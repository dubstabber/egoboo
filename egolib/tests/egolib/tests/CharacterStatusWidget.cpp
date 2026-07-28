#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define protected public
#define private public
#include "egolib/Core/System.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Graphics/GraphicsSystem.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#undef protected
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "TestGraphicsSystem.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/game/GUI/CharacterStatus.hpp"
#include "egolib/game/GUI/Container.hpp"

/// @file CharacterStatusWidget.cpp
/// @brief Characterization tests for Ego::GUI::CharacterStatus (egolib/game/GUI/CharacterStatus.{hpp,cpp}),
///        the in-HUD per-character status monitor that PlayingState attaches for every tracked
///        player/observed character.
///
/// CharacterStatus is the only one of the four untested HUD widgets (CharacterStatus, MiniMap-
/// adjacent InventorySlot excepted, ModuleSelector, CharacterWindow/LevelUpWindow) whose
/// constructor is completely service-free (CharacterStatus.cpp:47-52: it only stores an
/// ObjectRef and make_shared<ProgressBar>() -- no UIManager, EngineContext service, or renderer
/// touch). Its draw() body is otherwise deeply renderer-bound, but it opens with self-destroying
/// lifecycle gates that run and return BEFORE any uiManager()/texture access:
///   (a) no active session state (ISessionState seam) -> destroy() and return
///       (CharacterStatus.cpp:340-344);
///   (b) session active but the observed object is missing or terminated
///       (tryObservedObject/isTerminated gate) -> destroy() and return (CharacterStatus.cpp:
///       347-351, tryObservedObject at :40-44).
/// These are exactly the branches this file pins, composed with a plain Container (the same
/// membership contract characterized in GuiComponentBehavior.cpp) to confirm the self-destroying
/// widget also detaches itself from its parent -- the seam PlayingState relies on to garbage-
/// collect status HUDs for characters/players that leave the game.
///
/// A finding surfaced while pinning gate (b): tryObservedObject's own `!object->isTerminated()`
/// re-check (CharacterStatus.cpp:43) is currently unreachable dead code through this call chain,
/// and for a stronger reason than a mere flag check. Object::requestTerminate() calls
/// ObjectHandler::remove() (Object_lifecycle.cpp:190-193), which not only marks the terminate-
/// requested flag but SYNCHRONOUSLY erases the map entry from _internalCharacterList
/// (ObjectHandler.cpp:85-106, erase at :103) in the very same call. So ObjectHandler::exists(),
/// get(), and getHandle() (:108-120, :171-183, :185-197) all return false/nullptr for that ref
/// immediately afterwards -- there is no window in which a ref lookup finds the object "still
/// allocated but terminated". Only a shared_ptr the caller already held onto (e.g. a local test
/// variable) keeps the Object object alive past that point; nothing reachable through
/// Ego::Entities::tryActiveObject() (IObjectWorld.cpp:73-76, delegating through
/// GameModule::tryObject(), Module.hpp:189-192) can find it anymore. A "missing ref" and a
/// "terminated ref" are therefore fully indistinguishable from tryObservedObject's perspective:
/// both produce the exact same nullptr, through the exact same erased-from-map mechanism. The
/// test below pins the CharacterStatus-visible half of this (destroy() still fires for a
/// terminated ref) and additionally confirms the object handler itself no longer resolves that
/// ref, using only the caller's own kept-alive Object to prove requestTerminate() really ran.
///
/// The two gates above are NOT independently orderable through black-box observation: for any
/// combination of (session valid?, object valid?) the only two possible outcomes are "destroyed"
/// (if either condition is false) or "fully drawn" (if both are true) -- swapping the checks
/// produces identical externally observable results whenever at least one condition is false.
/// The order is pinned by source reference only (CharacterStatus.cpp:340 runs before :347).
///
/// This file also documents the headless wall: with a live, non-terminated observed object and
/// no UIManager installed (this fixture never installs one), draw() reaches
/// Component::uiManager() -> activeUIManager(), which throws std::logic_error("no active ui
/// manager") (UIManager.cpp:62-68) BEFORE any texture fetch. That throw matters operationally:
/// the very next texture fetches in this file (draw_one_character_icon/draw_one_bar/
/// draw_one_xp_bar via EngineContext::get().textureManager().getTexture(...), CharacterStatus.cpp
/// :66,113,261) would deadlock headless on a condition variable with no GL context
/// (TextureManager.cpp:120-129) -- so no test in this file may go further down the live-object
/// draw path than that throw.
///
/// Component derives from std::enable_shared_from_this and destroy() calls shared_from_this()
/// (Component.cpp:110-115), so every widget under test here is heap-allocated via
/// std::make_shared, matching the GuiComponentBehavior.cpp / CameraTracking.cpp precedent.

namespace
{

struct GraphicsSystemAccess : Ego::GraphicsSystem
{
    using idlib::singleton<Ego::GraphicsSystem>::instance;
};

struct CoreSystemAccess : Ego::Core::System
{
    using idlib::singleton<Ego::Core::System, Ego::Core::SystemCreateFunctor>::instance;
};

class StubGraphicsWindow : public Ego::GraphicsWindow
{
public:
    explicit StubGraphicsWindow(const idlib::vector_2s& size) :
        _size(size),
        _position(0, 0)
    {}

    void title(const std::string& title) override { _title = title; }
    std::string title() const override { return _title; }
    void grab_enabled(bool enabled) override { _grabEnabled = enabled; }
    bool grab_enabled() const override { return _grabEnabled; }
    idlib::vector_2s size() const override { return _size; }
    void size(const idlib::vector_2s& size) override { _size = size; }
    idlib::point_2s position() const override { return _position; }
    void position(const idlib::point_2s& position) override { _position = position; }
    void center() override {}
    idlib::vector_2s drawable_size() const override { return _size; }
    void update() override {}
    SDL_Window* get() override { return nullptr; }
    void setIcon(SDL_Surface*) override {}
    int getDisplayIndex() const override { return 0; }
    std::shared_ptr<SDL_Surface> getContents() const override { return nullptr; }

private:
    std::string _title;
    bool _grabEnabled = false;
    idlib::vector_2s _size;
    idlib::point_2s _position;
};

/// A concrete Container whose only abstract obligation (drawContainer) is a no-op, matching the
/// TestContainer idiom in GuiComponentBehavior.cpp -- never calls draw()/drawAll() itself, but is
/// used here as the parent a CharacterStatus is added to, so destroy()'s removeComponent() call
/// (Component.cpp:110-115) has somewhere real to detach from.
class TestContainer : public Ego::GUI::Container
{
public:
    void draw(Ego::GUI::DrawingContext&) override { /* no-op: never reaches the renderer */ }
    void drawContainer(Ego::GUI::DrawingContext&) override { /* no-op: never reaches the renderer */ }
};

class CharacterStatusWidgetFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = true;
        opts.configureLightweightProfileLoading = true;
        opts.initializeImageManager = true;
        opts.initializePerkHandler = true;
        opts.initializeProfileSystem = true;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.seedRandom = true;
        opts.randomSeed = 83;
        opts.binaryPath = "";
        opts.logPath = "/debug/character-status-widget-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize(EngineContext::get().config(), EngineContext::get().logTarget());
        EngineContext::get().installAudioSystem(AudioSystem::get());
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
        EngineContext::get().clearAudioSystem();
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");

        _fakeCoreSystem = static_cast<Ego::Core::System*>(::operator new(sizeof(Ego::Core::System)));
        _fakeSystemService = static_cast<Ego::Core::SystemService*>(::operator new(sizeof(Ego::Core::SystemService)));
        _fakeGraphicsSystem = static_cast<Ego::GraphicsSystem*>(::operator new(sizeof(Ego::GraphicsSystem)));

        _fakeCoreSystem->systemService = _fakeSystemService;
        _fakeCoreSystem->videoService = nullptr;
        _fakeCoreSystem->audioService = nullptr;
        _fakeCoreSystem->inputService = nullptr;
        _previousCoreSystem = CoreSystemAccess::instance.exchange(_fakeCoreSystem);

        _fakeGraphicsSystem->window = &_window;
        _fakeGraphicsSystem->context = nullptr;
        _previousGraphicsSystem = GraphicsSystemAccess::instance.exchange(_fakeGraphicsSystem);

        _mockGraphicsSystem = std::make_unique<Ego::Test::MockGraphicsSystem>(&_window);
        EngineContext::get().installGraphicsSystem(*_mockGraphicsSystem);
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        setup_clear_module_vfs_paths();
        EngineContext::get().clearGraphicsSystem();
        _mockGraphicsSystem.reset();
        CoreSystemAccess::instance.store(_previousCoreSystem);
        GraphicsSystemAccess::instance.store(_previousGraphicsSystem);
        ::operator delete(_fakeCoreSystem);
        ::operator delete(_fakeSystemService);
        ::operator delete(_fakeGraphicsSystem);
        _fakeCoreSystem = nullptr;
        _fakeSystemService = nullptr;
        _fakeGraphicsSystem = nullptr;
        _previousCoreSystem = nullptr;
        _previousGraphicsSystem = nullptr;
    }

    std::shared_ptr<ModuleProfile> findTestModule() const
    {
        for (const auto& module : EngineContext::get().profileSystem().getModuleProfiles())
        {
            if (module && module->getFolderName() == "test.mod")
            {
                return module;
            }
        }

        return nullptr;
    }

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 83);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position) const
    {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            throw std::runtime_error("profile load failed");
        }

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        auto object = module.getObjectHandler().getHandle(objectRef);
        EXPECT_NE(object, nullptr);
        if (!object)
        {
            throw std::runtime_error("spawn object failed");
        }

        return object;
    }

    StubGraphicsWindow _window{ idlib::vector_2s(640, 480) };
    std::unique_ptr<Ego::Test::MockGraphicsSystem> _mockGraphicsSystem;
    Ego::Core::System* _fakeCoreSystem = nullptr;
    Ego::Core::System* _previousCoreSystem = nullptr;
    Ego::Core::SystemService* _fakeSystemService = nullptr;
    Ego::GraphicsSystem* _fakeGraphicsSystem = nullptr;
    Ego::GraphicsSystem* _previousGraphicsSystem = nullptr;
};

std::unique_ptr<ContentRuntimeBootstrap> CharacterStatusWidgetFixture::s_runtime;

// -------------------------------------------------------------------------------------------
// Construction: service-free, zero GUI infrastructure needed.
// -------------------------------------------------------------------------------------------

TEST(CharacterStatusWidget, ConstructionIsServiceFreeAndStoresObjectRef)
{
    // CharacterStatus::CharacterStatus(ObjectRef) (CharacterStatus.cpp:47-52) only copies the
    // ref and default-constructs a ProgressBar via make_shared -- no UIManager, EngineContext
    // service, or renderer touch. No fixture/bootstrap of any kind is needed for this test.
    const ObjectRef objectRef(42);
    auto widget = std::make_shared<Ego::GUI::CharacterStatus>(objectRef);

    EXPECT_EQ(widget->getObjectRef(), objectRef);
    EXPECT_FALSE(widget->isDestroyed());
    EXPECT_TRUE(widget->isEnabled());
    EXPECT_TRUE(widget->isVisible());
    // Inherited default Component bounds (Component.cpp:8-13): 32x32 at the origin.
    EXPECT_FLOAT_EQ(widget->getX(), 0.0f);
    EXPECT_FLOAT_EQ(widget->getY(), 0.0f);
    EXPECT_FLOAT_EQ(widget->getWidth(), 32.0f);
    EXPECT_FLOAT_EQ(widget->getHeight(), 32.0f);
}

// -------------------------------------------------------------------------------------------
// draw() self-destroy gating, exercised against the live module/object harness.
// -------------------------------------------------------------------------------------------

TEST_F(CharacterStatusWidgetFixture, DrawWithNoActiveSessionDestroysAndDetachesFromParent)
{
    // SetUp() always quits any active module first, so tryActiveSessionState() == nullptr here
    // (GameSessionContext::beginModule installs the seam at GameSessionContext.cpp:276;
    // quitModule clears it at :297). Deliberately never calling beginActiveTestModule() in this
    // test exercises CharacterStatus.cpp's very first gate (:340-344).
    auto widget = std::make_shared<Ego::GUI::CharacterStatus>(ObjectRef::Invalid);
    auto container = std::make_shared<TestContainer>();
    container->addComponent(widget);
    ASSERT_EQ(container->getComponentCount(), 1u);
    ASSERT_EQ(widget->getParent(), container.get());

    Ego::GUI::DrawingContext drawingContext;
    widget->draw(drawingContext);

    EXPECT_TRUE(widget->isDestroyed());
    EXPECT_FALSE(widget->isEnabled());
    EXPECT_FALSE(widget->isVisible());
    EXPECT_EQ(widget->getParent(), nullptr);
    EXPECT_EQ(container->getComponentCount(), 0u);
}

TEST_F(CharacterStatusWidgetFixture, DrawWithActiveSessionButMissingObservedObjectDestroysAndDetachesFromParent)
{
    beginActiveTestModule();
    ASSERT_NE(tryActiveSessionState(), nullptr);

    // ObjectRef::Invalid never resolves through Ego::Entities::tryActiveObject (IObjectWorld.cpp:
    // 73-76 delegates to the world, which reports no such object), so this exercises the second
    // gate (CharacterStatus.cpp:347-351) with the session gate already satisfied.
    ASSERT_EQ(Ego::Entities::tryActiveObject(ObjectRef::Invalid), nullptr);

    auto widget = std::make_shared<Ego::GUI::CharacterStatus>(ObjectRef::Invalid);
    auto container = std::make_shared<TestContainer>();
    container->addComponent(widget);

    Ego::GUI::DrawingContext drawingContext;
    widget->draw(drawingContext);

    EXPECT_TRUE(widget->isDestroyed());
    EXPECT_EQ(widget->getParent(), nullptr);
    EXPECT_EQ(container->getComponentCount(), 0u);
}

TEST_F(CharacterStatusWidgetFixture, DrawWithActiveSessionButTerminatedObservedObjectDestroysAndDetachesFromParent)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6901, Ego::Vector3f(16.0f, 16.0f, 0.0f));
    const ObjectRef actorRef = actor->getObjRef();

    actor->requestTerminate();
    // Object::requestTerminate() -> ObjectHandler::remove() (Object_lifecycle.cpp:190-193,
    // ObjectHandler.cpp:85-106) marks the terminate-requested flag AND synchronously erases the
    // map entry (erase at ObjectHandler.cpp:103) in the same call -- so every ref-lookup surface
    // (exists()/get()/getHandle()/tryObject(), and therefore Ego::Entities::tryActiveObject(),
    // the seam CharacterStatus's tryObservedObject() uses) returns false/nullptr for this ref
    // immediately. Only our own kept-alive `actor` shared_ptr still proves the Object itself
    // really did get the terminate request; the handler no longer resolves the ref at all.
    ASSERT_TRUE(actor->isTerminated());
    ASSERT_EQ(module.getObjectHandler().getHandle(actorRef), nullptr);
    ASSERT_EQ(Ego::Entities::tryActiveObject(actorRef), nullptr);

    auto widget = std::make_shared<Ego::GUI::CharacterStatus>(actorRef);
    auto container = std::make_shared<TestContainer>();
    container->addComponent(widget);

    Ego::GUI::DrawingContext drawingContext;
    widget->draw(drawingContext);

    EXPECT_TRUE(widget->isDestroyed());
    EXPECT_EQ(widget->getParent(), nullptr);
    EXPECT_EQ(container->getComponentCount(), 0u);
}

// -------------------------------------------------------------------------------------------
// The deterministic headless wall on the live-object draw path.
// -------------------------------------------------------------------------------------------

TEST_F(CharacterStatusWidgetFixture, DrawWithLiveObjectAndNoUiManagerThrowsLogicErrorBeforeAnyTextureAccess)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6902, Ego::Vector3f(0.0f, 0.0f, 0.0f));
    ASSERT_FALSE(actor->isTerminated());

    auto widget = std::make_shared<Ego::GUI::CharacterStatus>(actor->getObjRef());
    auto container = std::make_shared<TestContainer>();
    container->addComponent(widget);

    // This fixture never installs a UIManager. Once both gates above pass, draw() next calls
    // Component::uiManager() (CharacterStatus.cpp:356), which resolves through
    // activeUIManager() (Component.cpp:129-131) and throws std::logic_error("no active ui
    // manager") (UIManager.cpp:62-68) -- strictly before pchr->getName()/getMoney() or any
    // EngineContext::get().textureManager().getTexture(...) call further down draw() (which,
    // headless, would deadlock on a condition variable with no GL context; TextureManager.cpp:
    // 120-129). This is the deterministic wall that keeps the rest of draw() unreachable here.
    Ego::GUI::DrawingContext drawingContext;
    EXPECT_THROW(widget->draw(drawingContext), std::logic_error);

    // A throw mid-draw() never reaches destroy() -- the widget and its parent link survive.
    EXPECT_FALSE(widget->isDestroyed());
    EXPECT_EQ(widget->getParent(), container.get());
    EXPECT_EQ(container->getComponentCount(), 1u);
}

} // namespace
