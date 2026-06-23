#include "gtest/gtest.h"

#include <algorithm>
#include <cstdlib>
#include <array>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define protected public
#define private public
#include "egolib/Core/System.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Logic/PerkHandler.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Graphics/GraphicsSystem.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GUI/InventorySlot.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/GUI/MessageLog.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Module/Passage.hpp"
#undef private
#undef protected
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "TestGraphicsSystem.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Logic/QuestLog.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp"
#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

void scr_systems_set_follow_link_by_modname_for_test(bool (*fn)(const std::string&, bool));

namespace
{

struct InputUpdateCalled final : std::runtime_error
{
    InputUpdateCalled() :
        std::runtime_error("input update called")
    {}
};

struct FollowLinkStubState
{
    int callCount = 0;
    std::string moduleName;
    bool pushCurrentModule = false;
    bool returnValue = false;
};

FollowLinkStubState* g_followLinkStubState = nullptr;

bool followLinkStub(const std::string& moduleName, bool pushCurrentModule)
{
    if (g_followLinkStubState == nullptr)
    {
        return false;
    }

    ++g_followLinkStubState->callCount;
    g_followLinkStubState->moduleName = moduleName;
    g_followLinkStubState->pushCurrentModule = pushCurrentModule;
    return g_followLinkStubState->returnValue;
}

class ScopedFollowLinkStub
{
public:
    explicit ScopedFollowLinkStub(FollowLinkStubState& state)
    {
        g_followLinkStubState = &state;
        scr_systems_set_follow_link_by_modname_for_test(&followLinkStub);
    }

    ~ScopedFollowLinkStub()
    {
        scr_systems_set_follow_link_by_modname_for_test(nullptr);
        g_followLinkStubState = nullptr;
    }
};

class ScopedTestModuleMenuOverrideCleanup
{
public:
    ScopedTestModuleMenuOverrideCleanup()
    {
        clear();
    }

    ~ScopedTestModuleMenuOverrideCleanup()
    {
        clear();
    }

private:
    static void clear()
    {
        vfs_delete_file("/modules/test.mod/gamedat/menu.txt");
    }
};

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

    void title(const std::string& title) override
    {
        _title = title;
    }

    std::string title() const override
    {
        return _title;
    }

    void grab_enabled(bool enabled) override
    {
        _grabEnabled = enabled;
    }

    bool grab_enabled() const override
    {
        return _grabEnabled;
    }

    idlib::vector_2s size() const override
    {
        return _size;
    }

    void size(const idlib::vector_2s& size) override
    {
        _size = size;
    }

    idlib::point_2s position() const override
    {
        return _position;
    }

    void position(const idlib::point_2s& position) override
    {
        _position = position;
    }

    void center() override {}

    idlib::vector_2s drawable_size() const override
    {
        return _size;
    }

    void update() override {}

    SDL_Window* get() override
    {
        return nullptr;
    }

    void setIcon(SDL_Surface*) override {}

    int getDisplayIndex() const override
    {
        return 0;
    }

    std::shared_ptr<SDL_Surface> getContents() const override
    {
        return nullptr;
    }

private:
    std::string _title;
    bool _grabEnabled = false;
    idlib::vector_2s _size;
    idlib::point_2s _position;
};

class StubInputSystem : public Ego::Input::IInputSystem
{
public:
    void update() override
    {
        ++updateCalls;
        if (throwOnUpdate)
        {
            throw InputUpdateCalled();
        }
    }

    const Ego::Vector2f& getMouseMovement() const override
    {
        return mouseMovement;
    }

    bool isMouseButtonDown(MouseButton button) const override
    {
        return mouseButtons[button];
    }

    bool isKeyDown(SDL_Keycode key) const override
    {
        return pressedKeys.count(key) != 0;
    }

    Ego::ModifierKeys getModifierKeys() const override
    {
        return modifierKeys;
    }

    void setKeyDown(SDL_Keycode key, bool down = true)
    {
        if (down)
        {
            pressedKeys.insert(key);
        }
        else
        {
            pressedKeys.erase(key);
        }
    }

    int updateCalls = 0;
    bool throwOnUpdate = false;
    Ego::Vector2f mouseMovement{0.0f, 0.0f};
    std::array<bool, Ego::Input::IInputSystem::NR_OF_MOUSE_BUTTONS> mouseButtons{};
    Ego::ModifierKeys modifierKeys{};
    std::unordered_set<SDL_Keycode> pressedKeys;
};

class StubBillboardSystem : public Ego::Graphics::IBillboardSystem
{
public:
    void update() override {}

    void reset() override {}

    std::shared_ptr<Ego::Graphics::Billboard> makeBillboard(ObjectRef,
                                                            const std::string&,
                                                            const Ego::Colour4f&,
                                                            const Ego::Colour4f&,
                                                            int,
                                                            BIT_FIELD,
                                                            float) override
    {
        return nullptr;
    }
};

class ScopedPlayingStateHarness
{
public:
    ScopedPlayingStateHarness() :
        _window({640, 480}),
        _fakeCoreSystem(static_cast<Ego::Core::System*>(::operator new(sizeof(Ego::Core::System)))),
        _fakeSystemService(static_cast<Ego::Core::SystemService*>(::operator new(sizeof(Ego::Core::SystemService)))),
        _fakeGraphicsSystem(static_cast<Ego::GraphicsSystem*>(::operator new(sizeof(Ego::GraphicsSystem)))),
        _fakeUiManager(static_cast<Ego::GUI::UIManager*>(::operator new(sizeof(Ego::GUI::UIManager))))
    {
        auto& context = EngineContext::get();
        context.setEngine(std::make_unique<GameEngine>());
        context.clearCameraSystem();

        GameEngine& engine = context.engine();
        engine._uiManager.reset(_fakeUiManager);
        // Publish the fake through the GUI-layer seam so widget uiManager() access (Component::uiManager()
        // -> activeUIManager()) resolves it, mirroring how EngineContext::uiManager() reads engine._uiManager.
        Ego::GUI::installActiveUIManager(*_fakeUiManager);

        _fakeCoreSystem->systemService = _fakeSystemService;
        _fakeCoreSystem->videoService = nullptr;
        _fakeCoreSystem->audioService = nullptr;
        _fakeCoreSystem->inputService = nullptr;
        _previousCoreSystem = CoreSystemAccess::instance.exchange(_fakeCoreSystem);

        _fakeGraphicsSystem->window = &_window;
        _fakeGraphicsSystem->context = nullptr;
        _previousGraphicsSystem = GraphicsSystemAccess::instance.exchange(_fakeGraphicsSystem);

        _mockGraphicsSystem = std::make_unique<Ego::Test::MockGraphicsSystem>(&_window);
        context.installGraphicsSystem(*_mockGraphicsSystem);

        CameraSystem::initialize();
        context.installCameraSystem(CameraSystem::get());
        context.installBillboardSystem(_billboardSystem);

        engine._currentGameState = std::make_shared<PlayingState>();
        _playingState = std::dynamic_pointer_cast<PlayingState>(engine._currentGameState);
    }

    ~ScopedPlayingStateHarness()
    {
        auto& context = EngineContext::get();
        Ego::GUI::clearActiveUIManager();
        if (GameEngine* engine = EngineContext::get().tryEngine())
        {
            engine->_currentGameState.reset();
            engine->_uiManager.release();
        }

        context.clearBillboardSystem();
        EngineContext::get().clearCameraSystem();
        CameraSystem::uninitialize();
        context.clearEngine();
        context.installAudioSystem(AudioSystem::get());
        context.installParticleHandler(ParticleHandler::get());
        context.installImageManager(Ego::ImageManager::get());
        context.installPerkHandler(Ego::Perks::PerkHandler::get());
        context.installProfileSystem(ProfileSystem::get());
        CoreSystemAccess::instance.store(_previousCoreSystem);
        GraphicsSystemAccess::instance.store(_previousGraphicsSystem);
        ::operator delete(_fakeCoreSystem);
        ::operator delete(_fakeSystemService);
        ::operator delete(_fakeGraphicsSystem);
        ::operator delete(_fakeUiManager);
        _mockGraphicsSystem.reset();
    }

    const std::shared_ptr<PlayingState>& playingState() const
    {
        return _playingState;
    }

    std::vector<std::string> messageTexts() const
    {
        std::vector<std::string> texts;
        if (_playingState == nullptr)
        {
            return texts;
        }

        for (const auto& message : _playingState->getMessageLog()->_messages)
        {
            texts.push_back(message.text);
        }
        return texts;
    }

private:
    StubGraphicsWindow _window;
    std::unique_ptr<Ego::Test::MockGraphicsSystem> _mockGraphicsSystem;
    Ego::Core::System* _fakeCoreSystem = nullptr;
    Ego::Core::System* _previousCoreSystem = nullptr;
    Ego::Core::SystemService* _fakeSystemService = nullptr;
    Ego::GraphicsSystem* _fakeGraphicsSystem = nullptr;
    Ego::GraphicsSystem* _previousGraphicsSystem = nullptr;
    Ego::GUI::UIManager* _fakeUiManager = nullptr;
    StubBillboardSystem _billboardSystem;
    std::shared_ptr<PlayingState> _playingState;
};

class ScriptSystemsFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 53;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-systems-function-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
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
        session.publishLocalPlayerPerception(LocalPlayerPerceptionState{});
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        setup_clear_module_vfs_paths();
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

    ObjectProfileRef loadProfile(const std::string& profilePath, int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
    }

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position = Ego::Vector3f(64.0f, 64.0f, 0.0f)) const
    {
        const ObjectProfileRef profile = loadProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return module.spawnObject(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
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
        const bool began = session.beginModule(module, 53);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject,
                              const std::shared_ptr<Object>& targetObject = nullptr) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(targetObject ? targetObject->getObjRef() : ObjectRef::Invalid);
        return self;
    }

    std::pair<std::shared_ptr<Passage>, int> addPassage(GameModule& module,
                                                        uint8_t mask = EMPTY_BIT_FIELD) const
    {
        auto passage = std::make_shared<Passage>(module, 0, 0, 4, 4, mask);
        module._passages.push_back(passage);
        return {passage, static_cast<int>(module._passages.size() - 1)};
    }

    std::shared_ptr<Object> makeAmmoItem(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/weapons/knife.obj",
            "mp_data/globalobjects/weapons/cknife.obj",
            "mp_data/globalobjects/items/gem.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->getAmmoMax() > 1)
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load an ammo-bearing item fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeInventoryItem(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/items/torch.obj",
            "mp_data/globalobjects/items/gem.obj",
            "mp_data/globalobjects/items/shovel.obj",
            "mp_data/globalobjects/armor/atshield.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->isAlive())
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load an inventory item fixture";
        return nullptr;
    }

    std::shared_ptr<Ego::Enchantment> addHealRemovableEnchant(GameModule& module,
                                                              const std::shared_ptr<Object>& target,
                                                              int slotBase) const
    {
        struct Candidate
        {
            const char* objectPath;
            const char* enchantPath;
        };

        static const std::vector<Candidate> candidates = {
            {"mp_data/globalobjects/weapons/stiletto.obj", "mp_data/globalobjects/weapons/stiletto.obj/enchant.txt"},
            {"mp_data/globalobjects/potions/ppotion.obj", "mp_data/globalobjects/potions/ppotion.obj/enchant.txt"},
            {"mp_data/globalobjects/items/mushroom.obj", "mp_data/globalobjects/items/mushroom.obj/enchant.txt"}
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto source = makeObject(module, candidates[i].objectPath, slotBase + static_cast<int>(i));
            if (!source)
            {
                continue;
            }

            const auto enchantRef = EngineContext::get().profileSystem().loadEnchantProfile(candidates[i].enchantPath,
                                                                                            INVALID_EVE_REF);
            auto enchant = target->addEnchant(enchantRef,
                                              source->getProfileID().get(),
                                              source->getObjRef(),
                                              source->getObjRef());
            if (enchant)
            {
                return enchant;
            }
        }

        ADD_FAILURE() << "unable to add a [HEAL]-removable enchant fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeEnchantSpawner(GameModule& module, int slotBase, ENC_REF& enchantRef) const
    {
        struct Candidate
        {
            const char* objectPath;
            const char* enchantPath;
        };

        static const std::vector<Candidate> candidates = {
            {"mp_data/globalobjects/weapons/stiletto.obj", "mp_data/globalobjects/weapons/stiletto.obj/enchant.txt"},
            {"mp_data/globalobjects/potions/ppotion.obj", "mp_data/globalobjects/potions/ppotion.obj/enchant.txt"},
            {"mp_data/globalobjects/items/mushroom.obj", "mp_data/globalobjects/items/mushroom.obj/enchant.txt"}
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto source = makeObject(module, candidates[i].objectPath, slotBase + static_cast<int>(i));
            if (!source)
            {
                continue;
            }

            const ENC_REF candidateEnchantRef = EngineContext::get().profileSystem().loadEnchantProfile(
                candidates[i].enchantPath, INVALID_EVE_REF);
            if (candidateEnchantRef >= ENCHANTPROFILES_MAX)
            {
                continue;
            }

            enchantRef = candidateEnchantRef;
            return source;
        }

        enchantRef = ENCHANTPROFILES_MAX;
        ADD_FAILURE() << "unable to load an enchant-capable object fixture";
        return nullptr;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptSystemsFunctionsFixture::s_runtime;

uint16_t tileTypeForIndex(const GameModule& module, Index1D tileIndex)
{
    if (!module._mesh)
    {
        throw std::runtime_error("test module mesh not initialized");
    }
    if (Index1D::Invalid == tileIndex)
    {
        throw std::runtime_error("invalid test tile index");
    }

    return module._mesh->getTileInfo(tileIndex)._img & TILE_LOWER_MASK;
}

uint16_t tileTypeAtPosition(const GameModule& module, const Ego::Vector2f& position)
{
    if (!module._mesh)
    {
        throw std::runtime_error("test module mesh not initialized");
    }

    const Index1D tileIndex = module._mesh->getTileIndex(position);
    return tileTypeForIndex(module, tileIndex);
}

uint16_t findAlternateTileType(const GameModule& module, const Ego::Vector2f& origin)
{
    const uint16_t currentType = tileTypeAtPosition(module, origin);
    const float gridSize = Info<float>::Grid::Size();

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const Ego::Vector2f candidate(origin.x() + x * gridSize, origin.y() + y * gridSize);
            if (!module.isInside(candidate.x(), candidate.y()))
            {
                continue;
            }

            const uint16_t candidateType = tileTypeAtPosition(module, candidate);
            if (candidateType != currentType)
            {
                return candidateType;
            }
        }
    }

    throw std::runtime_error("unable to find alternate tile type in test fixture");
}

TEST_F(ScriptSystemsFunctionsFixture, ChangeTileUsesModuleTileHelperForActorTile)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5590, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    const uint16_t originalType = tileTypeForIndex(module, actor->getTile());
    const uint16_t replacementType = findAlternateTileType(module, Ego::Vector2f(actor->getPosX(), actor->getPosY()));
    ASSERT_NE(originalType, replacementType);

    script_state_t state;
    state.argument = replacementType;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_ChangeTile(state, self));
    EXPECT_EQ(tileTypeForIndex(module, actor->getTile()), replacementType);
}

TEST_F(ScriptSystemsFunctionsFixture, ChangeTileFailsQuietlyForInvalidSelfWithoutMutatingTile)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5590, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    const uint16_t originalType = tileTypeForIndex(module, actor->getTile());
    const uint16_t replacementType = findAlternateTileType(module, Ego::Vector2f(actor->getPosX(), actor->getPosY()));
    ASSERT_NE(originalType, replacementType);

    script_state_t state;
    state.argument = replacementType;
    ai_state_t self = makeScriptSelf(nullptr);

    EXPECT_FALSE(scr_ChangeTile(state, self));
    EXPECT_EQ(tileTypeForIndex(module, actor->getTile()), originalType);
}

TEST_F(ScriptSystemsFunctionsFixture, GetTileXYReturnsMaskedTileTypeThroughModuleHelper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5591, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.x = static_cast<int32_t>(actor->getPosX());
    state.y = static_cast<int32_t>(actor->getPosY());
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_GetTileXY(state, self));
    EXPECT_EQ(state.argument,
              tileTypeAtPosition(module, Ego::Vector2f(static_cast<float>(state.x), static_cast<float>(state.y))));
}

TEST_F(ScriptSystemsFunctionsFixture, SetTileXYUpdatesTargetTileThroughModuleHelper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5592, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    const Ego::Vector2f targetPosition(actor->getPosX(), actor->getPosY());
    const uint16_t originalType = tileTypeAtPosition(module, targetPosition);
    const uint16_t replacementType = findAlternateTileType(module, targetPosition);
    ASSERT_NE(originalType, replacementType);

    script_state_t state;
    state.x = static_cast<int32_t>(targetPosition.x());
    state.y = static_cast<int32_t>(targetPosition.y());
    state.argument = replacementType;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetTileXY(state, self));
    EXPECT_EQ(tileTypeAtPosition(module, targetPosition), replacementType);
}

TEST_F(ScriptSystemsFunctionsFixture, PitsFallEnablesTeleportInsidePitBounds)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5593, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(module._mesh, nullptr);

    script_state_t state;
    state.x = EDGE + 1;
    state.y = EDGE + 2;
    state.distance = 33;
    ai_state_t self = makeScriptSelf(actor);

    module.enablePitsKill();

    EXPECT_TRUE(scr_PitsFall(state, self));
    EXPECT_TRUE(module._pitsTeleport);
    EXPECT_FALSE(module._pitsKill);
    EXPECT_FLOAT_EQ(module._pitsTeleportPos.x(), static_cast<float>(state.x));
    EXPECT_FLOAT_EQ(module._pitsTeleportPos.y(), static_cast<float>(state.y));
    EXPECT_FLOAT_EQ(module._pitsTeleportPos.z(), static_cast<float>(state.distance));
}

TEST_F(ScriptSystemsFunctionsFixture, PitsFallEnablesKillOutsidePitBounds)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5594, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.x = EDGE;
    state.y = EDGE;
    state.distance = 44;
    ai_state_t self = makeScriptSelf(actor);

    module.enablePitsTeleport(Ego::Vector3f(1.0f, 2.0f, 3.0f));

    EXPECT_TRUE(scr_PitsFall(state, self));
    EXPECT_FALSE(module._pitsTeleport);
    EXPECT_TRUE(module._pitsKill);
}

TEST_F(ScriptSystemsFunctionsFixture, FollowLinkReturnsFalseForInvalidMessageIdWithoutInvokingLinkFollow)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5600);

    ASSERT_NE(actor, nullptr);
    ASSERT_FALSE(actor->getProfile()->isValidMessageID(9999));

    script_state_t state;
    state.argument = 9999;
    ai_state_t self = makeScriptSelf(actor);

    FollowLinkStubState followLinkState;
    ScopedFollowLinkStub followLinkStub(followLinkState);

    EXPECT_FALSE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 0);
    EXPECT_EQ(&GameSessionContext::get().activeModule(), &module);
}

TEST_F(ScriptSystemsFunctionsFixture, FollowLinkInvalidMessageIdDoesNotPublishScaryMessage)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5603);

    ASSERT_NE(actor, nullptr);
    ASSERT_FALSE(actor->getProfile()->isValidMessageID(9999));

    script_state_t state;
    state.argument = 9999;
    ai_state_t self = makeScriptSelf(actor);

    FollowLinkStubState followLinkState;
    ScopedFollowLinkStub followLinkStub(followLinkState);
    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    EXPECT_FALSE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 0);
    EXPECT_TRUE(playingStateHarness.messageTexts().empty());
    EXPECT_EQ(&GameSessionContext::get().activeModule(), &module);
}

TEST_F(ScriptSystemsFunctionsFixture, FollowLinkUsesResolvedMessageAndPreservesSuccessfulFollowPath)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5604);

    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("test-next.mod"));

    script_state_t state;
    state.argument = messageId;
    ai_state_t self = makeScriptSelf(actor);

    FollowLinkStubState followLinkState;
    followLinkState.returnValue = true;
    ScopedFollowLinkStub followLinkStub(followLinkState);

    EXPECT_TRUE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 1);
    EXPECT_EQ(followLinkState.moduleName, "test-next.mod");
    EXPECT_TRUE(followLinkState.pushCurrentModule);
}

TEST_F(ScriptSystemsFunctionsFixture, FollowLinkFailurePublishesExistingScaryMessage)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5608);

    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("too-scary.mod"));

    script_state_t state;
    state.argument = messageId;
    ai_state_t self = makeScriptSelf(actor);

    FollowLinkStubState followLinkState;
    ScopedFollowLinkStub followLinkStub(followLinkState);
    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    EXPECT_FALSE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 1);
    EXPECT_EQ(followLinkState.moduleName, "too-scary.mod");
    EXPECT_TRUE(followLinkState.pushCurrentModule);

    const std::vector<std::string> messages = playingStateHarness.messageTexts();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front(), "That's too scary for " + actor->getName());
}

TEST_F(ScriptSystemsFunctionsFixture, FollowLinkFailureWithoutActivePlayingStateDoesNotCrash)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5609);

    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("too-scary.mod"));

    script_state_t state;
    state.argument = messageId;
    ai_state_t self = makeScriptSelf(actor);

    FollowLinkStubState followLinkState;
    ScopedFollowLinkStub followLinkStub(followLinkState);

    EXPECT_FALSE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 1);
    EXPECT_EQ(followLinkState.moduleName, "too-scary.mod");
    EXPECT_TRUE(followLinkState.pushCurrentModule);
    EXPECT_EQ(&GameSessionContext::get().activeModule(), &module);
    EXPECT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);
}

TEST_F(ScriptSystemsFunctionsFixture, EnableListenSkillRemainsLoggedNoOp)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5612);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    testing::internal::CaptureStdout();
    EXPECT_FALSE(scr_EnableListenSkill(state, self));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("deprecated script function"), std::string::npos);
    EXPECT_NE(output.find("EnableListenSkill"), std::string::npos);
    EXPECT_NE(output.find(actor->getProfile()->getClassName()), std::string::npos);
    EXPECT_TRUE(playingStateHarness.messageTexts().empty());
}

TEST_F(ScriptSystemsFunctionsFixture, SelfCompatibilityOpcodesFailQuietlyWhenSelfRefIsInvalid)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5613);

    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("Done."));

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    const SKIN_T initialSkin = actor->getSkin();
    const DamageType initialDamageType = actor->getDamageTargetType();
    const int initialMoney = actor->getMoney();
    g_endText.setText("Prefix:");

    state.argument = static_cast<int>(DamageType::DAMAGE_FIRE);
    EXPECT_FALSE(scr_SetDamageType(state, self));
    EXPECT_EQ(actor->getDamageTargetType(), initialDamageType);

    EXPECT_FALSE(scr_Equip(state, self));
    EXPECT_FALSE(actor->isEquipped());

    state.argument = 3;
    state.x = 77;
    EXPECT_FALSE(scr_ChangeArmor(state, self));
    EXPECT_EQ(actor->getSkin(), initialSkin);
    EXPECT_EQ(state.argument, 3);
    EXPECT_EQ(state.x, 77);

    state.argument = 25;
    EXPECT_FALSE(scr_DropMoney(state, self));
    EXPECT_EQ(actor->getMoney(), initialMoney);

    state.argument = 25;
    EXPECT_FALSE(scr_SetMoney(state, self));
    EXPECT_FALSE(scr_JoinGoodTeam(state, self));

    state.argument = messageId;
    EXPECT_FALSE(scr_AddEndMessage(state, self));
    EXPECT_EQ(g_endText.getText(), "Prefix:");

    FollowLinkStubState followLinkState;
    ScopedFollowLinkStub followLinkStub(followLinkState);
    EXPECT_FALSE(scr_FollowLink(state, self));
    EXPECT_EQ(followLinkState.callCount, 0);
    EXPECT_EQ(&GameSessionContext::get().activeModule(), &module);
}

TEST_F(ScriptSystemsFunctionsFixture, SelfProfileHelpersFailQuietlyWhenSelfRefIsInvalid)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/magic/summonspellii.obj", 56132);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    const ObjectProfileRef initialProfile = actor->getProfileID();
    const SKIN_T initialSkin = actor->getSkin();

    EXPECT_FALSE(scr_BecomeSpellbook(state, self));
    EXPECT_EQ(actor->getProfileID(), initialProfile);
    EXPECT_EQ(actor->getSkin(), initialSkin);
}

TEST_F(ScriptSystemsFunctionsFixture, SelfPresentationCompatibilityHelpersPreserveUiNoOpsAndInvalidSelfFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 56131);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    auto& config = EngineContext::get().config();
    const bool originalShowStatusBars = config.hud_displayStatusBars.getValue();
    config.hud_displayStatusBars.setValue(true);

    actor->setShowStatus(false);
    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_NULL));
    actor->giveMoney(-static_cast<int>(actor->getMoney()));
    actor->giveMoney(12);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("Done."));
    g_endText.setText("Prefix:");

    script_state_t state;
    state.x = 64;
    state.y = 64;
    state.argument = 1;
    ai_state_t validSelf = makeScriptSelf(actor);

    EXPECT_FALSE(scr_ShowMap(state, validSelf));
    EXPECT_TRUE(scr_ShowYouAreHere(state, validSelf));
    EXPECT_TRUE(scr_ShowBlipXY(state, validSelf));
    EXPECT_TRUE(scr_AddStat(state, validSelf));
    EXPECT_FALSE(actor->getShowStatus());
    EXPECT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    ai_state_t invalidSelf = makeScriptSelf(nullptr);

    state.argument = 33;
    EXPECT_FALSE(scr_SetMoney(state, invalidSelf));
    EXPECT_EQ(actor->getMoney(), 12);

    state.argument = static_cast<int>(Team::TEAM_GOOD);
    EXPECT_FALSE(scr_JoinGoodTeam(state, invalidSelf));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_NULL));

    state.argument = messageId;
    EXPECT_FALSE(scr_AddEndMessage(state, invalidSelf));
    EXPECT_EQ(g_endText.getText(), "Prefix:");

    config.hud_displayStatusBars.setValue(originalShowStatusBars);
}

TEST_F(ScriptSystemsFunctionsFixture, ModuleEnvironmentHelpersPreserveWaterFogAndFlagPublication)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5614, Ego::Vector3f(64.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    state.argument = 85;
    EXPECT_TRUE(scr_SetWaterLevel(state, self));
    EXPECT_FLOAT_EQ(module.getWater()._douse_level, 8.5f);

    state.argument = 0;
    EXPECT_TRUE(scr_GetWaterLevel(state, self));
    EXPECT_EQ(state.argument, 85);

    auto& fog = GameSessionContext::get().fog();
    const float originalTop = fog._top;
    const float originalDistance = fog._distance;
    EngineContext::get().config().graphic_fog_enable.setValue(true);

    state.argument = 42;
    EXPECT_TRUE(scr_SetFogLevel(state, self));
    EXPECT_FLOAT_EQ(fog._top, 4.2f);
    EXPECT_FLOAT_EQ(fog._distance, originalDistance + (4.2f - originalTop));
    EXPECT_TRUE(scr_GetFogLevel(state, self));
    EXPECT_EQ(state.argument, 42);

    state.turn = 300;
    state.argument = -5;
    state.distance = 128;
    EXPECT_TRUE(scr_SetFogTAD(state, self));
    EXPECT_EQ(fog._red, 255);
    EXPECT_EQ(fog._grn, 0);
    EXPECT_EQ(fog._blu, 128);

    const float originalBottom = fog._bottom;
    const float distanceAfterTop = fog._distance;
    state.argument = 18;
    EXPECT_TRUE(scr_SetFogBottomLevel(state, self));
    EXPECT_NEAR(fog._bottom, 1.8f, 0.0001f);
    EXPECT_FLOAT_EQ(fog._distance, distanceAfterTop - (1.8f - originalBottom));
    EXPECT_TRUE(scr_GetFogBottomLevel(state, self));
    EXPECT_EQ(state.argument, 18);

    module._isBeaten = false;
    EXPECT_TRUE(scr_BeatModule(state, self));
    EXPECT_TRUE(module._isBeaten);

    module._exportValid = true;
    EXPECT_TRUE(scr_DisableExport(state, self));
    EXPECT_FALSE(module._exportValid);
    EXPECT_TRUE(scr_EnableExport(state, self));
    EXPECT_TRUE(module._exportValid);

    module.enablePitsTeleport(Ego::Vector3f(1.0f, 2.0f, 3.0f));
    EXPECT_TRUE(scr_PitsKill(state, self));
    EXPECT_FALSE(module._pitsTeleport);
    EXPECT_TRUE(module._pitsKill);
}

TEST_F(ScriptSystemsFunctionsFixture, AddIDSZPublishesExpansionToActiveModuleMenu)
{
    const ScopedTestModuleMenuOverrideCleanup generatedMenuCleanup;
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5624);

    ASSERT_NE(actor, nullptr);

    const IDSZ2 idsz('T', 'S', 'Z', '1');
    ASSERT_FALSE(ModuleProfile::moduleHasIDSZ(module.getPath(), idsz));

    script_state_t state;
    state.argument = static_cast<int32_t>(idsz.toUint32());
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_AddIDSZ(state, self));
    EXPECT_TRUE(ModuleProfile::moduleHasIDSZ(module.getPath(), idsz));
}

TEST_F(ScriptSystemsFunctionsFixture, ShowMapHelpersUsePlayingStateMiniMapAdapters)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5615);

    ASSERT_NE(actor, nullptr);

    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    const auto minimap = playingStateHarness.playingState()->getMiniMap();
    ASSERT_NE(minimap, nullptr);
    EXPECT_FALSE(minimap->isVisible());
    EXPECT_FALSE(minimap->_showPlayerPosition);

    script_state_t state;
    state.x = 64;
    state.y = 64;
    state.argument = 1;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_ShowMap(state, self));
    EXPECT_TRUE(minimap->isVisible());

    EXPECT_TRUE(scr_ShowYouAreHere(state, self));
    EXPECT_TRUE(minimap->_showPlayerPosition);
}

TEST_F(ScriptSystemsFunctionsFixture, ShowMapHelpersNoOpWithoutActivePlayingState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5617);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    script_state_t state;
    state.x = 64;
    state.y = 64;
    state.argument = 1;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_FALSE(scr_ShowMap(state, self));
    EXPECT_TRUE(scr_ShowYouAreHere(state, self));
    EXPECT_TRUE(scr_ShowBlipXY(state, self));
    EXPECT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);
}

TEST_F(ScriptSystemsFunctionsFixture, MiniMapEnemySenseQueueSkipsTerminatedObservedObjects)
{
    auto& module = beginActiveTestModule();
    auto liveEnemy = makeObject(module, "mp_objects/follower.obj", 5621, Ego::Vector3f(48.0f, 64.0f, 0.0f));
    auto terminatedEnemy = makeObject(module, "mp_objects/follower.obj", 5622, Ego::Vector3f(96.0f, 64.0f, 0.0f));

    ASSERT_NE(liveEnemy, nullptr);
    ASSERT_NE(terminatedEnemy, nullptr);

    {
        auto objects = module.getObjectHandler().iterator();
        (void)objects;
    }

    liveEnemy->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    terminatedEnemy->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    terminatedEnemy->requestTerminate();

    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    const auto minimap = playingStateHarness.playingState()->getMiniMap();
    ASSERT_NE(minimap, nullptr);

    minimap->_blips.clear();
    GameSessionContext::get().publishEnemySense(EnemySenseState(static_cast<TEAM_REF>(Team::TEAM_GOOD), IDSZ2::None));
    minimap->queueEnemySenseBlips(GameSessionContext::get().enemySense(), module);

    ASSERT_EQ(minimap->_blips.size(), 1u);
    EXPECT_FLOAT_EQ(minimap->_blips.front().x, liveEnemy->getPosX());
    EXPECT_FLOAT_EQ(minimap->_blips.front().y, liveEnemy->getPosY());
    EXPECT_EQ(minimap->_blips.front().color, COLOR_RED);
}

TEST_F(ScriptSystemsFunctionsFixture, InventorySlotResolvesLiveItemByRefAndSkipsTerminatedItems)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5623);
    auto inventoryItem = makeAmmoItem(module, 5724);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(inventoryItem, nullptr);

    const size_t slot = actor->getFirstFreeInventorySlot();
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), slot, true));

    Ego::GUI::InventorySlot inventorySlot(actor->getObjRef(), slot, nullptr);
    EXPECT_EQ(inventorySlot.tryObservedItem(), inventoryItem.get());

    inventoryItem->requestTerminate();
    EXPECT_EQ(inventorySlot.tryObservedItem(), nullptr);
}

TEST_F(ScriptSystemsFunctionsFixture, EndTextHelpersPreserveClearAndAppendBehavior)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5616);

    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("Done."));

    script_state_t state;
    state.argument = messageId;
    ai_state_t self = makeScriptSelf(actor);

    g_endText.setText("Prefix:");
    EXPECT_TRUE(scr_AddEndMessage(state, self));
    EXPECT_EQ(g_endText.getText(), "Prefix:Done.");

    state.argument = messageId + 1000;
    EXPECT_FALSE(scr_AddEndMessage(state, self));
    EXPECT_EQ(g_endText.getText(), "Prefix:Done.");

    EXPECT_TRUE(scr_ClearEndMessage(state, self));
    EXPECT_TRUE(g_endText.getText().empty());

    g_endText.setText("Prefix:Done.");
    actor->requestTerminate();
    state.argument = messageId;
    EXPECT_FALSE(scr_AddEndMessage(state, self));
    EXPECT_EQ(g_endText.getText(), "Prefix:Done.");
    g_endText.setText("");
}

TEST_F(ScriptSystemsFunctionsFixture, AddStatPublishesStatusMonitorThroughPlayingStateAdapter)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5618);

    ASSERT_NE(actor, nullptr);

    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);
    ASSERT_EQ(playingStateHarness.playingState()->getStatusCharacterRef(0), ObjectRef::Invalid);

    auto& config = EngineContext::get().config();
    const bool originalShowStatusBars = config.hud_displayStatusBars.getValue();
    config.hud_displayStatusBars.setValue(true);

    actor->setShowStatus(false);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_AddStat(state, self));
    EXPECT_TRUE(actor->getShowStatus());
    ASSERT_NE(playingStateHarness.playingState()->getStatusCharacterRef(0), ObjectRef::Invalid);
    EXPECT_EQ(playingStateHarness.playingState()->getStatusCharacterRef(0), actor->getObjRef());

    config.hud_displayStatusBars.setValue(originalShowStatusBars);
}

TEST_F(ScriptSystemsFunctionsFixture, DisplayCharacterWindowIgnoresTerminatedStatusCharacter)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5620);

    ASSERT_NE(actor, nullptr);

    ScopedPlayingStateHarness playingStateHarness;
    ASSERT_NE(playingStateHarness.playingState(), nullptr);

    auto& config = EngineContext::get().config();
    const bool originalShowStatusBars = config.hud_displayStatusBars.getValue();
    config.hud_displayStatusBars.setValue(true);

    actor->setShowStatus(false);
    playingStateHarness.playingState()->addStatusMonitor(actor->getObjRef());
    ASSERT_EQ(playingStateHarness.playingState()->getStatusCharacterRef(0), actor->getObjRef());

    const size_t componentCountBefore = playingStateHarness.playingState()->getComponentCount();
    actor->requestTerminate();

    playingStateHarness.playingState()->displayCharacterWindow(0);

    EXPECT_EQ(playingStateHarness.playingState()->getComponentCount(), componentCountBefore);

    config.hud_displayStatusBars.setValue(originalShowStatusBars);
}

TEST_F(ScriptSystemsFunctionsFixture, AddStatNoOpsWithoutActivePlayingState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5619);

    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    auto& config = EngineContext::get().config();
    const bool originalShowStatusBars = config.hud_displayStatusBars.getValue();
    config.hud_displayStatusBars.setValue(true);

    actor->setShowStatus(false);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_AddStat(state, self));
    EXPECT_FALSE(actor->getShowStatus());
    EXPECT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    config.hud_displayStatusBars.setValue(originalShowStatusBars);
}

TEST_F(ScriptSystemsFunctionsFixture, PlayingStateUpdateUsesInstalledInputSystem)
{
    beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    StubInputSystem inputSystem;
    inputSystem.throwOnUpdate = true;
    EngineContext::get().installInputSystem(inputSystem);

    ASSERT_NE(playingStateHarness.playingState(), nullptr);
    EXPECT_THROW(playingStateHarness.playingState()->update(), InputUpdateCalled);

    EXPECT_EQ(inputSystem.updateCalls, 1);
}

TEST_F(ScriptSystemsFunctionsFixture, MainLoopCheckStatsUsesInstalledInputSystemForShowMapCheat)
{
    beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    StubInputSystem inputSystem;
    inputSystem.setKeyDown(SDLK_m);
    inputSystem.setKeyDown(SDLK_LSHIFT);
    EngineContext::get().installInputSystem(inputSystem);

    const auto minimap = playingStateHarness.playingState()->getMiniMap();
    ASSERT_NE(minimap, nullptr);
    ASSERT_FALSE(minimap->isVisible());
    ASSERT_FALSE(minimap->_showPlayerPosition);

    auto& config = EngineContext::get().config();
    const bool originalDeveloperMode = config.debug_developerMode_enable.getValue();
    config.debug_developerMode_enable.setValue(true);

    MainLoop::check_stats();

    EXPECT_TRUE(minimap->isVisible());
    EXPECT_TRUE(minimap->_showPlayerPosition);

    config.debug_developerMode_enable.setValue(originalDeveloperMode);
}

TEST_F(ScriptSystemsFunctionsFixture, MainLoopCheckStatsXpCheatUsesLivePlayerAndSkipsTerminatedPlayerObject)
{
    auto& module = beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    auto actor = makeObject(module, "mp_objects/follower.obj", 5618);
    ASSERT_NE(actor, nullptr);
    ASSERT_TRUE(module.addPlayer(actor, Ego::Input::InputDevice::DeviceList[0]));

    StubInputSystem idleInputSystem;
    EngineContext::get().clearInputSystem();
    EngineContext::get().installInputSystem(idleInputSystem);
    for (int i = 0; i < 8; ++i)
    {
        MainLoop::check_stats();
    }

    StubInputSystem inputSystem;
    inputSystem.setKeyDown(SDLK_x);
    inputSystem.setKeyDown(SDLK_1);
    EngineContext::get().clearInputSystem();
    EngineContext::get().installInputSystem(inputSystem);

    auto& config = EngineContext::get().config();
    const bool originalDeveloperMode = config.debug_developerMode_enable.getValue();
    config.debug_developerMode_enable.setValue(true);

    actor->setExperience(0);
    const uint32_t experienceBeforeCheat = actor->getExperience();

    MainLoop::check_stats();
    EXPECT_GT(actor->getExperience(), experienceBeforeCheat);

    actor->requestTerminate();
    const uint32_t experienceAfterLiveCheat = actor->getExperience();
    MainLoop::check_stats();
    EXPECT_EQ(actor->getExperience(), experienceAfterLiveCheat);

    config.debug_developerMode_enable.setValue(originalDeveloperMode);
    EngineContext::get().clearInputSystem();
}

TEST_F(ScriptSystemsFunctionsFixture, MainLoopCheckStatsLifeCheatUsesLivePlayerAndSkipsTerminatedPlayerObject)
{
    auto& module = beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    auto actor = makeObject(module, "mp_objects/follower.obj", 5619);
    ASSERT_NE(actor, nullptr);
    ASSERT_TRUE(module.addPlayer(actor, Ego::Input::InputDevice::DeviceList[0]));

    StubInputSystem idleInputSystem;
    EngineContext::get().clearInputSystem();
    EngineContext::get().installInputSystem(idleInputSystem);
    for (int i = 0; i < 8; ++i)
    {
        MainLoop::check_stats();
    }

    StubInputSystem inputSystem;
    inputSystem.setKeyDown(SDLK_z);
    inputSystem.setKeyDown(SDLK_1);
    EngineContext::get().clearInputSystem();
    EngineContext::get().installInputSystem(inputSystem);

    auto& config = EngineContext::get().config();
    const bool originalDeveloperMode = config.debug_developerMode_enable.getValue();
    config.debug_developerMode_enable.setValue(true);

    actor->_currentLife = std::max(1.0f, actor->getAttribute(Ego::Attribute::MAX_LIFE) - 10.0f);
    const float lifeBeforeHeal = actor->getLife();

    MainLoop::check_stats();
    EXPECT_GT(actor->getLife(), lifeBeforeHeal);

    actor->requestTerminate();
    const float lifeAfterLiveCheat = actor->getLife();
    MainLoop::check_stats();
    EXPECT_FLOAT_EQ(actor->getLife(), lifeAfterLiveCheat);

    config.debug_developerMode_enable.setValue(originalDeveloperMode);
    EngineContext::get().clearInputSystem();
}

TEST_F(ScriptSystemsFunctionsFixture, GameEngineScreenshotHotkeyUsesInstalledInputSystem)
{
    auto& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubInputSystem inputSystem;
    inputSystem.setKeyDown(SDLK_F11);
    context.installInputSystem(inputSystem);

    GameEngine& engine = context.engine();
    EXPECT_FALSE(engine._screenshotRequested);

    engine.updateScreenshotRequest();

    EXPECT_TRUE(engine._screenshotRequested);

    context.clearEngine();
    context.installAudioSystem(AudioSystem::get());
    context.installParticleHandler(ParticleHandler::get());
    context.installImageManager(Ego::ImageManager::get());
    context.installPerkHandler(Ego::Perks::PerkHandler::get());
    context.installProfileSystem(ProfileSystem::get());
}

TEST_F(ScriptSystemsFunctionsFixture, CostTargetItemIDConsumesHeldAmmoThroughRoleLookups)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5601);
    auto target = makeObject(module, "mp_objects/follower.obj", 5602);
    auto heldItem = makeAmmoItem(module, 5603);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(target->getObjRef(), GRIP_LEFT));

    heldItem->setAmmo(2);

    script_state_t state;
    state.argument = heldItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_CostTargetItemID(state, self));
    EXPECT_EQ(heldItem->getAmmo(), 1);
    EXPECT_FALSE(heldItem->isTerminated());
}

TEST_F(ScriptSystemsFunctionsFixture, CostTargetItemIDPoofsInventoryItemWhenOwnerMatchesTarget)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5611);
    auto inventoryItem = makeInventoryItem(module, 5612);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    inventoryItem->setAmmo(1);
    IInventoryHolder& actorInventory = *actor;

    script_state_t state;
    state.argument = inventoryItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, actor);

    EXPECT_TRUE(scr_CostTargetItemID(state, self));
    EXPECT_TRUE(inventoryItem->isTerminated());
    EXPECT_EQ(actorInventory.getInventoryItemRef(0), ObjectRef::Invalid);
}

TEST_F(ScriptSystemsFunctionsFixture, InventoryRoleHelpersReturnFalseWhenTargetIsMissing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5615);

    ASSERT_NE(actor, nullptr);

    const uint32_t sentinelType = IDSZ2('T', 'E', 'S', 'T').toUint32();

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(ObjectRef::Invalid);

    state.argument = sentinelType;
    EXPECT_FALSE(scr_CostTargetItemID(state, self));
    EXPECT_EQ(state.argument, sentinelType);

    state.argument = sentinelType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, sentinelType);

    state.argument = sentinelType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, sentinelType);
}

TEST_F(ScriptSystemsFunctionsFixture, InventoryRoleHelpersReturnFalseWhenSelfIsMissing)
{
    auto& module = beginActiveTestModule();
    auto target = makeObject(module, "mp_objects/follower.obj", 5616);
    auto heldItem = makeAmmoItem(module, 5617);

    ASSERT_NE(target, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(target->getObjRef(), GRIP_LEFT));

    heldItem->setAmmo(heldItem->getAmmoMax() - 2);
    heldItem->setKursed(true);

    const uint32_t matchingType = heldItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr, target);

    state.argument = matchingType;
    EXPECT_FALSE(scr_CostTargetItemID(state, self));
    EXPECT_EQ(state.argument, matchingType);
    EXPECT_EQ(heldItem->getAmmo(), heldItem->getAmmoMax() - 2);
    EXPECT_TRUE(heldItem->isKursed());

    state.argument = matchingType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, matchingType);
    EXPECT_EQ(heldItem->getAmmo(), heldItem->getAmmoMax() - 2);
    EXPECT_TRUE(heldItem->isKursed());

    state.argument = matchingType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, matchingType);
    EXPECT_EQ(heldItem->getAmmo(), heldItem->getAmmoMax() - 2);
    EXPECT_TRUE(heldItem->isKursed());

    EXPECT_FALSE(scr_UnkurseTargetInventory(state, self));
    EXPECT_EQ(heldItem->getAmmo(), heldItem->getAmmoMax() - 2);
    EXPECT_TRUE(heldItem->isKursed());
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDAllUsesTargetHandsAndActorInventory)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5621);
    auto target = makeObject(module, "mp_objects/follower.obj", 5622);
    auto leftItem = makeAmmoItem(module, 5623);
    auto rightItem = makeAmmoItem(module, 5626);
    auto inventoryItem = makeAmmoItem(module, 5629);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);

    script_state_t state;
    state.argument = leftItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, 6);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax());
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax());
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax());
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDFirstPreservesLeftRightThenActorInventoryOrder)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5631);
    auto target = makeObject(module, "mp_objects/follower.obj", 5632);
    auto leftItem = makeAmmoItem(module, 5633);
    auto rightItem = makeAmmoItem(module, 5636);
    auto inventoryItem = makeAmmoItem(module, 5639);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    const uint32_t matchingType = leftItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 1);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax());
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax() - 2);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);

    leftItem->setAmmo(leftItem->getAmmoMax());
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 2);
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax());
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);

    leftItem->setAmmo(leftItem->getAmmoMax());
    rightItem->setAmmo(rightItem->getAmmoMax());
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 3);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax());
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDFirstReturnsFalseWhenNoHeldOrInventoryItemMatches)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5640);
    auto target = makeObject(module, "mp_objects/follower.obj", 5641);
    auto leftItem = makeAmmoItem(module, 5642);
    auto rightItem = makeAmmoItem(module, 5645);
    auto inventoryItem = makeAmmoItem(module, 5648);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);

    script_state_t state;
    state.argument = IDSZ2('Z', 'Z', 'Z', 'Z').toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_FALSE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax() - 1);
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax() - 2);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDAllReturnsFalseWhenNoHeldOrInventoryItemMatches)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5649);
    auto target = makeObject(module, "mp_objects/follower.obj", 5650);
    auto leftItem = makeAmmoItem(module, 5654);
    auto rightItem = makeAmmoItem(module, 5657);
    auto inventoryItem = makeAmmoItem(module, 5660);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);

    script_state_t state;
    state.argument = IDSZ2('Z', 'Z', 'Z', 'Z').toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_FALSE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax() - 1);
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax() - 2);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);
}

TEST_F(ScriptSystemsFunctionsFixture, InventoryCompatibilityHelpersIgnoreTargetPocketItems)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5664);
    auto target = makeObject(module, "mp_objects/follower.obj", 5665);
    auto targetPocketItem = makeAmmoItem(module, 5666);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(targetPocketItem, nullptr);
    ASSERT_TRUE(Inventory::add_item(*target, targetPocketItem->getObjRef(), target->getFirstFreeInventorySlot(), true));

    targetPocketItem->setAmmo(targetPocketItem->getAmmoMax() - 2);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    const uint32_t matchingType = targetPocketItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();

    state.argument = matchingType;
    EXPECT_FALSE(scr_CostTargetItemID(state, self));
    EXPECT_EQ(state.argument, matchingType);
    EXPECT_FALSE(targetPocketItem->isTerminated());
    EXPECT_EQ(targetPocketItem->getAmmo(), targetPocketItem->getAmmoMax() - 2);

    state.argument = matchingType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(targetPocketItem->getAmmo(), targetPocketItem->getAmmoMax() - 2);

    targetPocketItem->setAmmo(targetPocketItem->getAmmoMax() - 2);
    state.argument = matchingType;
    EXPECT_FALSE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(targetPocketItem->getAmmo(), targetPocketItem->getAmmoMax() - 2);
}

TEST_F(ScriptSystemsFunctionsFixture, QuestHelpersResolvePlayersThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5651);
    auto target = makeObject(module, "mp_objects/follower.obj", 5652);
    auto nonPlayerTarget = makeObject(module, "mp_objects/follower.obj", 5653);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(nonPlayerTarget, nullptr);
    ASSERT_TRUE(module.addPlayer(target, Ego::Input::InputDevice::DeviceList[0]));

    const IDSZ2 questId('T', 'Q', 'S', 'T');
    auto& questLog = module.getPlayer(target->getPlayerNumber())->getQuestLog();

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nonPlayerTarget);

    state.argument = questId.toUint32();
    state.distance = 3;
    EXPECT_FALSE(scr_AddQuest(state, self));
    EXPECT_EQ(questLog[questId], Ego::QuestLog::QUEST_NONE);

    self.setTarget(target->getObjRef());
    EXPECT_TRUE(scr_AddQuest(state, self));
    EXPECT_EQ(questLog[questId], 3);

    state.distance = 8;
    EXPECT_FALSE(scr_AddQuest(state, self));
    EXPECT_EQ(questLog[questId], 3);

    questLog.setQuestProgress(questId, Ego::QuestLog::QUEST_BEATEN);
    state.distance = 5;
    EXPECT_FALSE(scr_AddQuest(state, self));
    EXPECT_EQ(questLog[questId], Ego::QuestLog::QUEST_BEATEN);
}

TEST_F(ScriptSystemsFunctionsFixture, SetQuestLevelResolvesPlayersThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5661);
    auto target = makeObject(module, "mp_objects/follower.obj", 5662);
    auto nonPlayerTarget = makeObject(module, "mp_objects/follower.obj", 5663);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(nonPlayerTarget, nullptr);
    ASSERT_TRUE(module.addPlayer(target, Ego::Input::InputDevice::DeviceList[1]));

    const IDSZ2 questId('L', 'V', 'L', 'Q');
    auto& questLog = module.getPlayer(target->getPlayerNumber())->getQuestLog();
    questLog.setQuestProgress(questId, 4);

    script_state_t state;
    state.argument = questId.toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    state.distance = 0;
    EXPECT_FALSE(scr_SetQuestLevel(state, self));
    EXPECT_EQ(questLog[questId], 4);

    state.distance = -2;
    EXPECT_TRUE(scr_SetQuestLevel(state, self));
    EXPECT_EQ(questLog[questId], 2);

    self.setTarget(nonPlayerTarget->getObjRef());
    state.distance = 5;
    EXPECT_FALSE(scr_SetQuestLevel(state, self));
    EXPECT_EQ(questLog[questId], 2);

    self.setTarget(target->getObjRef());
    questLog.setQuestProgress(questId, Ego::QuestLog::QUEST_NONE);
    state.distance = 3;
    EXPECT_FALSE(scr_SetQuestLevel(state, self));
    EXPECT_EQ(questLog[questId], Ego::QuestLog::QUEST_NONE);
}

TEST_F(ScriptSystemsFunctionsFixture, BeatQuestAllPlayersOnlyBeatsActiveQuests)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5664);
    auto firstPlayer = makeObject(module, "mp_objects/follower.obj", 5665);
    auto secondPlayer = makeObject(module, "mp_objects/follower.obj", 5666);
    auto beatenPlayer = makeObject(module, "mp_objects/follower.obj", 5667);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(firstPlayer, nullptr);
    ASSERT_NE(secondPlayer, nullptr);
    ASSERT_NE(beatenPlayer, nullptr);
    ASSERT_TRUE(module.addPlayer(firstPlayer, Ego::Input::InputDevice::DeviceList[0]));
    ASSERT_TRUE(module.addPlayer(secondPlayer, Ego::Input::InputDevice::DeviceList[1]));
    ASSERT_TRUE(module.addPlayer(beatenPlayer, Ego::Input::InputDevice::DeviceList[2]));

    const IDSZ2 questId('B', 'E', 'A', 'T');
    auto& firstQuestLog = module.getPlayer(firstPlayer->getPlayerNumber())->getQuestLog();
    auto& secondQuestLog = module.getPlayer(secondPlayer->getPlayerNumber())->getQuestLog();
    auto& beatenQuestLog = module.getPlayer(beatenPlayer->getPlayerNumber())->getQuestLog();
    firstQuestLog.setQuestProgress(questId, 2);
    secondQuestLog.setQuestProgress(questId, 5);
    beatenQuestLog.setQuestProgress(questId, Ego::QuestLog::QUEST_BEATEN);

    script_state_t state;
    state.argument = questId.toUint32();
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_BeatQuestAllPlayers(state, self));
    EXPECT_EQ(firstQuestLog[questId], Ego::QuestLog::QUEST_BEATEN);
    EXPECT_EQ(secondQuestLog[questId], Ego::QuestLog::QUEST_BEATEN);
    EXPECT_EQ(beatenQuestLog[questId], Ego::QuestLog::QUEST_BEATEN);

    EXPECT_FALSE(scr_BeatQuestAllPlayers(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, AddQuestAllPlayersOnlyRaisesNonBeatenQuestProgress)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5668);
    auto lowPlayer = makeObject(module, "mp_objects/follower.obj", 5669);
    auto highPlayer = makeObject(module, "mp_objects/follower.obj", 5670);
    auto beatenPlayer = makeObject(module, "mp_objects/follower.obj", 5671);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(lowPlayer, nullptr);
    ASSERT_NE(highPlayer, nullptr);
    ASSERT_NE(beatenPlayer, nullptr);
    ASSERT_TRUE(module.addPlayer(lowPlayer, Ego::Input::InputDevice::DeviceList[0]));
    ASSERT_TRUE(module.addPlayer(highPlayer, Ego::Input::InputDevice::DeviceList[1]));
    ASSERT_TRUE(module.addPlayer(beatenPlayer, Ego::Input::InputDevice::DeviceList[2]));

    const IDSZ2 questId('A', 'L', 'L', 'Q');
    auto& lowQuestLog = module.getPlayer(lowPlayer->getPlayerNumber())->getQuestLog();
    auto& highQuestLog = module.getPlayer(highPlayer->getPlayerNumber())->getQuestLog();
    auto& beatenQuestLog = module.getPlayer(beatenPlayer->getPlayerNumber())->getQuestLog();
    lowQuestLog.setQuestProgress(questId, 2);
    highQuestLog.setQuestProgress(questId, 7);
    beatenQuestLog.setQuestProgress(questId, Ego::QuestLog::QUEST_BEATEN);

    script_state_t state;
    state.argument = questId.toUint32();
    state.distance = 5;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_AddQuestAllPlayers(state, self));
    EXPECT_EQ(lowQuestLog[questId], 5);
    EXPECT_EQ(highQuestLog[questId], 7);
    EXPECT_EQ(beatenQuestLog[questId], Ego::QuestLog::QUEST_BEATEN);

    state.distance = 4;
    EXPECT_FALSE(scr_AddQuestAllPlayers(state, self));
    EXPECT_EQ(lowQuestLog[questId], 5);
    EXPECT_EQ(highQuestLog[questId], 7);

    state.distance = 0;
    EXPECT_FALSE(scr_AddQuestAllPlayers(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, AllPlayerQuestHelpersReturnFalseWhenNoLocalPlayersExist)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5672);

    ASSERT_NE(actor, nullptr);

    const IDSZ2 questId('N', 'O', 'P', 'L');
    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    state.argument = questId.toUint32();
    state.distance = 4;

    EXPECT_FALSE(scr_BeatQuestAllPlayers(state, self));
    EXPECT_FALSE(scr_AddQuestAllPlayers(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, DamageAndKillTargetUseDamageableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5641);
    auto damageTarget = makeObject(module, "mp_objects/follower.obj", 5642);
    auto killTarget = makeObject(module, "mp_objects/follower.obj", 5643);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(damageTarget, nullptr);
    ASSERT_NE(killTarget, nullptr);

    actor->setDamageTargetType(DamageType::DAMAGE_FIRE);
    auto& config = EngineContext::get().config();
    const auto previousFeedback = config.hud_feedback.getValue();
    config.hud_feedback.setValue(Ego::FeedbackType::None);

    script_state_t state;
    state.argument = 128;
    ai_state_t self = makeScriptSelf(actor, damageTarget);

    const float initialLife = damageTarget->getLife();
    EXPECT_TRUE(scr_DamageTarget(state, self));
    EXPECT_LT(damageTarget->getLife(), initialLife);

    self.setTarget(killTarget->getObjRef());
    EXPECT_TRUE(scr_KillTarget(state, self));
    EXPECT_FALSE(killTarget->isAlive());

    auto heldWeapon = makeInventoryItem(module, 5644);
    auto weaponKillTarget = makeObject(module, "mp_objects/follower.obj", 5645);

    ASSERT_NE(heldWeapon, nullptr);
    ASSERT_NE(weaponKillTarget, nullptr);
    ASSERT_TRUE(heldWeapon->attachToObject(actor->getObjRef(), GRIP_RIGHT));

    ai_state_t weaponSelf = makeScriptSelf(heldWeapon, weaponKillTarget);
    EXPECT_TRUE(scr_KillTarget(state, weaponSelf));
    EXPECT_FALSE(weaponKillTarget->isAlive());

    config.hud_feedback.setValue(previousFeedback);
}

TEST_F(ScriptSystemsFunctionsFixture, SetDamageTypeUsesLocalizedSelfHelper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5646);

    ASSERT_NE(actor, nullptr);
    actor->setDamageTargetType(DamageType::DAMAGE_DIRECT);

    script_state_t state;
    state.argument = DAMAGE_FIRE;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetDamageType(state, self));
    EXPECT_EQ(actor->getDamageTargetType(), DamageType::DAMAGE_FIRE);
}

TEST_F(ScriptSystemsFunctionsFixture, EquipUsesLocalizedSelfHelper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5647);

    ASSERT_NE(actor, nullptr);
    actor->setEquipped(false);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_Equip(state, self));
    EXPECT_TRUE(actor->isEquipped());
}

TEST_F(ScriptSystemsFunctionsFixture, KillTargetHandlesSelfHeldByMount)
{
    auto& module = beginActiveTestModule();
    auto mount = makeObject(module, "mp_data/globalobjects/magic/mount.obj", 5662);
    auto mountedChild = makeObject(module, "mp_objects/follower.obj", 5663);
    auto mountKillTarget = makeObject(module, "mp_objects/follower.obj", 5667);

    ASSERT_NE(mount, nullptr);
    ASSERT_NE(mountedChild, nullptr);
    ASSERT_NE(mountKillTarget, nullptr);
    ASSERT_TRUE(mount->isMount());
    mount->setHeldObject(SLOT_LEFT, ObjectRef::Invalid);
    mount->setHeldObject(SLOT_RIGHT, mountedChild->getObjRef());
    mountedChild->setHolderRef(mount->getObjRef());

    const auto setTeamRefs = [](const std::shared_ptr<Object>& object, TEAM_REF team)
    {
        object->setTeam(team);
        object->setTeamRef(team);
        object->setBaseTeamRef(team);
    };

    setTeamRefs(mount, static_cast<TEAM_REF>(Team::TEAM_GOOD));
    setTeamRefs(mountedChild, static_cast<TEAM_REF>(Team::TEAM_GOOD));
    setTeamRefs(mountKillTarget, static_cast<TEAM_REF>(Team::TEAM_EVIL));

    script_state_t state;
    ai_state_t mountedChildSelf = makeScriptSelf(mountedChild, mountKillTarget);
    EXPECT_TRUE(scr_KillTarget(state, mountedChildSelf));
    EXPECT_FALSE(mountKillTarget->isAlive());
}

TEST_F(ScriptSystemsFunctionsFixture, GiveExperienceToTargetUsesCharacterStateRoleAndPreservesMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5646);
    auto target = makeObject(module, "mp_objects/follower.obj", 5647);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    ICharacterState& targetState = *target;

    script_state_t state;
    state.argument = 48;
    state.distance = static_cast<int>(XP_DIRECT);
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_FALSE(scr_GiveExperienceToTarget(state, self));

    self.setTarget(target->getObjRef());
    const uint32_t experienceBefore = targetState.getExperience();
    EXPECT_TRUE(scr_GiveExperienceToTarget(state, self));
    EXPECT_GT(targetState.getExperience(), experienceBefore);
}

TEST_F(ScriptSystemsFunctionsFixture, HealSelfAndTargetUseDamageableRoleAndPreserveHealEnchantCleanup)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5651);
    auto target = makeObject(module, "mp_objects/follower.obj", 5652);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setLife(-19.0f);
    target->setLife(-19.0f);

    auto enchant = addHealRemovableEnchant(module, target, 5653);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());

    script_state_t state;
    state.argument = 512;
    ai_state_t self = makeScriptSelf(actor, target);

    const float actorLifeBeforeHeal = actor->getLife();
    EXPECT_TRUE(scr_HealSelf(state, self));
    EXPECT_GT(actor->getLife(), actorLifeBeforeHeal);

    const float targetLifeBeforeHeal = target->getLife();
    EXPECT_TRUE(scr_HealTarget(state, self));
    EXPECT_GT(target->getLife(), targetLifeBeforeHeal);
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_TRUE(target->getFirstActiveEnchant()->isTerminated());
}

TEST_F(ScriptSystemsFunctionsFixture, ManaAmmoAndKurseHelpersUseCharacterStateRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5661);
    auto manaTarget = makeObject(module, "mp_objects/follower.obj", 5662);
    auto ammoActor = makeAmmoItem(module, 5663);
    auto ammoTarget = makeAmmoItem(module, 5666);
    auto itemTarget = makeInventoryItem(module, 5669);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(manaTarget, nullptr);
    ASSERT_NE(ammoActor, nullptr);
    ASSERT_NE(ammoTarget, nullptr);
    ASSERT_NE(itemTarget, nullptr);

    script_state_t state;
    ai_state_t manaSelf = makeScriptSelf(actor, manaTarget);

    const float manaBeforeCost = manaTarget->getMana();
    state.argument = FLOAT_TO_FP8(1.0f);
    EXPECT_TRUE(scr_CostTargetMana(state, manaSelf));
    EXPECT_LT(manaTarget->getMana(), manaBeforeCost);

    manaTarget->setMana(0.0f);
    const float manaBeforePump = manaTarget->getMana();
    state.argument = FLOAT_TO_FP8(1.0f);
    EXPECT_TRUE(scr_PumpTarget(state, manaSelf));
    EXPECT_GT(manaTarget->getMana(), manaBeforePump);

    ammoActor->setAmmo(ammoActor->getAmmoMax() - 1);
    ai_state_t ammoSelf = makeScriptSelf(ammoActor, ammoTarget);

    EXPECT_TRUE(scr_IncreaseAmmo(state, ammoSelf));
    EXPECT_EQ(ammoActor->getAmmo(), ammoActor->getAmmoMax());

    EXPECT_TRUE(scr_CostAmmo(state, ammoSelf));
    EXPECT_EQ(ammoActor->getAmmo(), ammoActor->getAmmoMax() - 1);

    ammoTarget->setAmmo(0);
    state.argument = ammoTarget->getAmmoMax() + 5;
    EXPECT_TRUE(scr_SetTargetAmmo(state, ammoSelf));
    EXPECT_EQ(ammoTarget->getAmmo(), ammoTarget->getAmmoMax());

    ai_state_t kurseSelf = makeScriptSelf(actor, itemTarget);
    state.argument = 0;
    EXPECT_FALSE(itemTarget->isKursed());
    EXPECT_TRUE(scr_KurseTarget(state, kurseSelf));
    EXPECT_TRUE(itemTarget->isKursed());
}

TEST_F(ScriptSystemsFunctionsFixture, PumpTargetUsesSelfRefAsManaSource)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 56612);
    auto target = makeObject(module, "mp_objects/follower.obj", 56613);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setMana(target->getMaxMana());
    target->setLife(-5.0f);
    target->setBaseAttribute(Ego::Attribute::CHANNEL_LIFE, 1.0f);
    target->setAILastAttacker(ObjectRef::Invalid);
    target->careful_timer = 0;

    script_state_t state;
    state.argument = FLOAT_TO_FP8(3.0f);
    ai_state_t self = makeScriptSelf(actor, target);

    const float lifeBefore = target->getLife();

    EXPECT_TRUE(scr_PumpTarget(state, self));
    EXPECT_GT(target->getLife(), lifeBefore);
    EXPECT_EQ(target->getAILastAttacker(), actor->getObjRef());
}

TEST_F(ScriptSystemsFunctionsFixture, UnkurseTargetUsesCharacterStateRoleAndPreservesMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5670);
    auto itemTarget = makeInventoryItem(module, 5671);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(itemTarget, nullptr);

    itemTarget->setKursed(true);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, itemTarget);

    EXPECT_TRUE(scr_UnkurseTarget(state, self));
    EXPECT_FALSE(itemTarget->isKursed());

    itemTarget->setKursed(true);
    self.setTarget(ObjectRef::Invalid);

    EXPECT_FALSE(scr_UnkurseTarget(state, self));
    EXPECT_TRUE(itemTarget->isKursed());
}

TEST_F(ScriptSystemsFunctionsFixture, UnkurseTargetInventoryUsesRoleLookupsAndPreservesActorPocketBehavior)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5674);
    auto target = makeObject(module, "mp_objects/follower.obj", 5675);
    auto leftHeldItem = makeInventoryItem(module, 5676);
    auto rightHeldItem = makeInventoryItem(module, 5679);
    auto actorPocketItem = makeInventoryItem(module, 5682);
    auto targetPocketItem = makeInventoryItem(module, 5685);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftHeldItem, nullptr);
    ASSERT_NE(rightHeldItem, nullptr);
    ASSERT_NE(actorPocketItem, nullptr);
    ASSERT_NE(targetPocketItem, nullptr);
    ASSERT_TRUE(leftHeldItem->attachToObject(target->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightHeldItem->attachToObject(target->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, actorPocketItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));
    ASSERT_TRUE(Inventory::add_item(*target, targetPocketItem->getObjRef(), target->getFirstFreeInventorySlot(), true));

    leftHeldItem->setKursed(true);
    rightHeldItem->setKursed(true);
    actorPocketItem->setKursed(true);
    targetPocketItem->setKursed(true);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_UnkurseTargetInventory(state, self));
    EXPECT_FALSE(leftHeldItem->isKursed());
    EXPECT_FALSE(rightHeldItem->isKursed());
    EXPECT_FALSE(actorPocketItem->isKursed());
    EXPECT_TRUE(targetPocketItem->isKursed());
}

TEST_F(ScriptSystemsFunctionsFixture, AddBlipAllEnemiesPublishesAndResetsEnemySense)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5674);
    auto target = makeObject(module, "mp_objects/follower.obj", 5675);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));

    script_state_t state;
    state.argument = IDSZ2('U', 'N', 'D', 'E').toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_AddBlipAllEnemies(state, self));
    const EnemySenseState& published = GameSessionContext::get().enemySense();
    EXPECT_EQ(published.team, target->getTeamRef());
    EXPECT_EQ(published.idsz, IDSZ2('U', 'N', 'D', 'E'));

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_AddBlipAllEnemies(state, self));
    const EnemySenseState& reset = GameSessionContext::get().enemySense();
    EXPECT_EQ(reset.team, static_cast<TEAM_REF>(Team::TEAM_MAX));
    EXPECT_EQ(reset.idsz, IDSZ2::None);
}

TEST_F(ScriptSystemsFunctionsFixture, TargetDamageSelfUsesTargetDamageTypeAttribution)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5676);
    auto target = makeObject(module, "mp_objects/follower.obj", 5677);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    actor->setAILastAttacker(ObjectRef::Invalid);
    actor->setAILastDamageType(DamageType::DAMAGE_DIRECT);

    auto& config = EngineContext::get().config();
    const auto previousFeedback = config.hud_feedback.getValue();
    config.hud_feedback.setValue(Ego::FeedbackType::None);

    script_state_t state;
    state.argument = 512;
    state.distance = static_cast<int>(DamageType::DAMAGE_FIRE);
    ai_state_t self = makeScriptSelf(actor, target);

    const float lifeBefore = actor->getLife();
    EXPECT_TRUE(scr_TargetDamageSelf(state, self));
    EXPECT_LT(actor->getLife(), lifeBefore);
    EXPECT_EQ(actor->getAILastDamageType(), DamageType::DAMAGE_FIRE);

    config.hud_feedback.setValue(previousFeedback);
}

TEST_F(ScriptSystemsFunctionsFixture, TargetDamageSelfReturnsFalseWhenTargetIsMissing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5678);

    ASSERT_NE(actor, nullptr);

    actor->setAILastDamageType(DamageType::DAMAGE_DIRECT);

    script_state_t state;
    state.argument = 512;
    state.distance = static_cast<int>(DamageType::DAMAGE_FIRE);
    ai_state_t self = makeScriptSelf(actor);

    const float lifeBefore = actor->getLife();
    EXPECT_FALSE(scr_TargetDamageSelf(state, self));
    EXPECT_FLOAT_EQ(actor->getLife(), lifeBefore);
    EXPECT_EQ(actor->getAILastDamageType(), DamageType::DAMAGE_DIRECT);
}

TEST_F(ScriptSystemsFunctionsFixture, TargetStateCompatibilityHelpersPreserveMissingTargetNoOps)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 56781);
    auto target = makeObject(module, "mp_objects/follower.obj", 56782);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    auto enchant = addHealRemovableEnchant(module, target, 56783);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);

    const float spellPowerBefore = target->getBaseAttribute(Ego::Attribute::SPELL_POWER);
    const float manaRegenBefore = target->getBaseAttribute(Ego::Attribute::MANA_REGEN);

    script_state_t state;
    state.argument = FLOAT_TO_FP8(1.0f);
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_GiveManaFlowToTarget(state, self));
    EXPECT_FLOAT_EQ(target->getBaseAttribute(Ego::Attribute::SPELL_POWER), spellPowerBefore);

    EXPECT_TRUE(scr_GiveManaReturnToTarget(state, self));
    EXPECT_FLOAT_EQ(target->getBaseAttribute(Ego::Attribute::MANA_REGEN), manaRegenBefore);

    state.argument = IDSZ2('H', 'E', 'A', 'L').toUint32();
    EXPECT_FALSE(scr_DispelTargetEnchantID(state, self));
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_FALSE(target->getFirstActiveEnchant()->isTerminated());
}

TEST_F(ScriptSystemsFunctionsFixture, TargetStateCompatibilityHelpersTreatTerminatedTargetsAsMissing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 56791);
    auto target = makeObject(module, "mp_objects/follower.obj", 56792);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    auto enchant = addHealRemovableEnchant(module, target, 56793);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);

    actor->setAILastDamageType(DamageType::DAMAGE_DIRECT);
    const float actorLifeBefore = actor->getLife();
    const float spellPowerBefore = target->getBaseAttribute(Ego::Attribute::SPELL_POWER);
    const float manaRegenBefore = target->getBaseAttribute(Ego::Attribute::MANA_REGEN);

    target->requestTerminate();
    ASSERT_TRUE(target->isTerminated());

    script_state_t state;
    state.argument = FLOAT_TO_FP8(1.0f);
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GiveManaFlowToTarget(state, self));
    EXPECT_FLOAT_EQ(target->getBaseAttribute(Ego::Attribute::SPELL_POWER), spellPowerBefore);

    EXPECT_TRUE(scr_GiveManaReturnToTarget(state, self));
    EXPECT_FLOAT_EQ(target->getBaseAttribute(Ego::Attribute::MANA_REGEN), manaRegenBefore);

    state.argument = IDSZ2('H', 'E', 'A', 'L').toUint32();
    EXPECT_FALSE(scr_DispelTargetEnchantID(state, self));
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_FALSE(target->getFirstActiveEnchant()->isTerminated());

    state.argument = 512;
    state.distance = static_cast<int>(DamageType::DAMAGE_FIRE);
    EXPECT_FALSE(scr_TargetDamageSelf(state, self));
    EXPECT_FLOAT_EQ(actor->getLife(), actorLifeBefore);
    EXPECT_EQ(actor->getAILastDamageType(), DamageType::DAMAGE_DIRECT);
}

TEST_F(ScriptSystemsFunctionsFixture, AttributeTimerEnchantAndPerkHelpersUseCharacterStateRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5671);
    auto target = makeObject(module, "mp_objects/follower.obj", 5672);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(target->getProfile()->canBeGrogged());
    ASSERT_TRUE(target->getProfile()->canBeDazed());

    auto enchant = addHealRemovableEnchant(module, target, 5673);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());

    const float mightBefore = target->getBaseAttribute(Ego::Attribute::MIGHT);
    const float intellectBefore = target->getBaseAttribute(Ego::Attribute::INTELLECT);
    const float agilityBefore = target->getBaseAttribute(Ego::Attribute::AGILITY);
    const float maxLifeBefore = target->getBaseAttribute(Ego::Attribute::MAX_LIFE);
    const float maxManaBefore = target->getBaseAttribute(Ego::Attribute::MAX_MANA);
    const float spellPowerBefore = target->getBaseAttribute(Ego::Attribute::SPELL_POWER);
    const float manaRegenBefore = target->getBaseAttribute(Ego::Attribute::MANA_REGEN);

    target->setLife(-19.0f);
    target->setMana(0.0f);

    script_state_t state;
    state.argument = FLOAT_TO_FP8(1.0f);
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GiveStrengthToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::MIGHT), mightBefore);

    EXPECT_TRUE(scr_GiveIntelligenceToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::INTELLECT), intellectBefore);

    EXPECT_TRUE(scr_GiveDexterityToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::AGILITY), agilityBefore);

    const float lifeBefore = target->getLife();
    EXPECT_TRUE(scr_GiveLifeToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::MAX_LIFE), maxLifeBefore);
    EXPECT_GT(target->getLife(), lifeBefore);

    const float manaBefore = target->getMana();
    EXPECT_TRUE(scr_GiveManaToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::MAX_MANA), maxManaBefore);
    EXPECT_GT(target->getMana(), manaBefore);

    EXPECT_TRUE(scr_GiveManaFlowToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::SPELL_POWER), spellPowerBefore);

    EXPECT_TRUE(scr_GiveManaReturnToTarget(state, self));
    EXPECT_GT(target->getBaseAttribute(Ego::Attribute::MANA_REGEN), manaRegenBefore);

    target->setGrogTimer(1);
    state.argument = 4;
    EXPECT_TRUE(scr_GrogTarget(state, self));
    EXPECT_EQ(target->getGrogTimer(), 5);

    target->setDazeTimer(2);
    state.argument = 3;
    EXPECT_TRUE(scr_DazeTarget(state, self));
    EXPECT_EQ(target->getDazeTimer(), 5);

    state.argument = IDSZ2('H', 'E', 'A', 'L').toUint32();
    EXPECT_TRUE(scr_DispelTargetEnchantID(state, self));
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_TRUE(target->getFirstActiveEnchant()->isTerminated());

    state.argument = IDSZ2::caseLabel('D', 'A', 'R', 'K');
    EXPECT_TRUE(scr_GiveSkillToTarget(state, self));
    EXPECT_TRUE(target->hasPerk(Ego::Perks::NIGHT_VISION));
}

TEST_F(ScriptSystemsFunctionsFixture, GiveSkillToTargetPreservesLegacyUnknownSkillNoOp)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5673);
    auto target = makeObject(module, "mp_objects/follower.obj", 5674);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_FALSE(target->hasPerk(Ego::Perks::TRAP_LORE));

    script_state_t state;
    state.argument = IDSZ2('N', 'O', 'P', 'E').toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GiveSkillToTarget(state, self));
    EXPECT_FALSE(target->hasPerk(Ego::Perks::TRAP_LORE));
}

TEST_F(ScriptSystemsFunctionsFixture, TargetCompatibilityOpcodesFailQuietlyWhenTargetRefIsInvalid)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 56741);
    auto target = makeObject(module, "mp_objects/follower.obj", 56742);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(target->getProfile()->canBeGrogged());
    ASSERT_TRUE(target->getProfile()->canBeDazed());

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    target->setMana(target->getMaxMana());
    target->setAmmo(2);
    target->setGrogTimer(3);
    target->setDazeTimer(4);
    ASSERT_FALSE(target->hasPerk(Ego::Perks::NIGHT_VISION));

    auto enchant = addHealRemovableEnchant(module, target, 56743);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_FALSE(target->getFirstActiveEnchant()->isTerminated());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    self.setTarget(ObjectRef::Invalid);

    const TEAM_REF actorTeamBefore = actor->getTeamRef();
    const TEAM_REF targetTeamBefore = target->getTeamRef();
    const float targetManaBefore = target->getMana();
    const uint16_t targetAmmoBefore = target->getAmmo();
    const int targetGrogBefore = target->getGrogTimer();
    const int targetDazeBefore = target->getDazeTimer();

    state.argument = static_cast<int>(Team::TEAM_NULL);
    EXPECT_FALSE(scr_JoinTargetTeam(state, self));
    EXPECT_FALSE(scr_TargetJoinTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), actorTeamBefore);
    EXPECT_EQ(target->getTeamRef(), targetTeamBefore);

    state.argument = FLOAT_TO_FP8(1.0f);
    EXPECT_FALSE(scr_CostTargetMana(state, self));
    EXPECT_FLOAT_EQ(target->getMana(), targetManaBefore);

    state.argument = 99;
    EXPECT_FALSE(scr_SetTargetAmmo(state, self));
    EXPECT_EQ(target->getAmmo(), targetAmmoBefore);

    state.argument = 5;
    EXPECT_FALSE(scr_GrogTarget(state, self));
    EXPECT_FALSE(scr_DazeTarget(state, self));
    EXPECT_EQ(target->getGrogTimer(), targetGrogBefore);
    EXPECT_EQ(target->getDazeTimer(), targetDazeBefore);

    state.argument = IDSZ2::caseLabel('D', 'A', 'R', 'K');
    EXPECT_FALSE(scr_GiveSkillToTarget(state, self));
    EXPECT_FALSE(target->hasPerk(Ego::Perks::NIGHT_VISION));

    EXPECT_FALSE(scr_DisenchantTarget(state, self));
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_FALSE(target->getFirstActiveEnchant()->isTerminated());
}

TEST_F(ScriptSystemsFunctionsFixture, ExportCharacterWritesPerkAndPoolNamesThroughInstalledPerkService)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/zombor.obj", 5675);

    ASSERT_NE(actor, nullptr);
    actor->addPerk(Ego::Perks::NIGHT_VISION);

    const std::string exportPath = "/debug/exported-zombor-data.txt";
    ASSERT_TRUE(ObjectProfile::exportCharacterToFile(exportPath, actor.get()));

    std::string exported;
    vfs_readEntireFile(exportPath, [&exported](size_t length, const char* bytes)
    {
        exported.assign(bytes, length);
    });

    const std::string masteredBaseline = ": [PERK] Weapon_Proficiency";
    const std::string masteredAdded = ": [PERK] Night_Vision";
    const std::string firstPoolEntry = ": [POOL] Toughness";
    const auto baselinePos = exported.find(masteredBaseline);
    const auto addedPos = exported.find(masteredAdded);
    const auto poolPos = exported.find(firstPoolEntry);

    EXPECT_NE(baselinePos, std::string::npos);
    EXPECT_NE(addedPos, std::string::npos);
    EXPECT_NE(poolPos, std::string::npos);
    EXPECT_LT(baselinePos, addedPos);
    EXPECT_LT(addedPos, poolPos);
}

TEST_F(ScriptSystemsFunctionsFixture, EnchantLifecycleHelpersUseEnchantableRole)
{
    auto& module = beginActiveTestModule();
    ENC_REF enchantRef = ENCHANTPROFILES_MAX;
    auto actor = makeEnchantSpawner(module, 188, enchantRef);
    auto target = makeObject(module, "mp_objects/follower.obj", 5695);
    auto child = makeObject(module, "mp_objects/follower.obj", 5696);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(child, nullptr);
    ASSERT_LT(enchantRef, ENCHANTPROFILES_MAX);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    self.owner = actor->getObjRef();
    self.child = child->getObjRef();

    EXPECT_FALSE(target->hasActiveEnchants());
    EXPECT_TRUE(scr_EnchantTarget(state, self));
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_EQ(actor->getLastEnchantmentSpawned(), target->getFirstActiveEnchant());

    auto actorEnchant = addHealRemovableEnchant(module, actor, 5697);
    ASSERT_NE(actorEnchant, nullptr);
    ASSERT_TRUE(actor->hasActiveEnchants());

    state.argument = FLOAT_TO_FP8(1.0f);
    state.distance = FLOAT_TO_FP8(2.0f);
    state.x = FLOAT_TO_FP8(3.0f);
    state.y = FLOAT_TO_FP8(4.0f);
    EXPECT_TRUE(scr_SetEnchantBoostValues(state, self));
    ASSERT_NE(actor->getFirstActiveEnchant(), nullptr);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getOwnerManaSustain(), 1.0f);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getOwnerLifeSustain(), 2.0f);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getTargetManaDrain(), 3.0f);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getTargetLifeDrain(), 4.0f);

    EXPECT_TRUE(scr_UndoEnchant(state, self));
    EXPECT_TRUE(actor->getLastEnchantmentSpawned()->isTerminated());
    EXPECT_FALSE(scr_UndoEnchant(state, self));

    EXPECT_TRUE(scr_EnchantChild(state, self));
    ASSERT_TRUE(child->hasActiveEnchants());

    EXPECT_TRUE(scr_EnchantTarget(state, self));
    ASSERT_TRUE(target->hasActiveEnchants());
    EXPECT_TRUE(scr_DisenchantTarget(state, self));
    EXPECT_TRUE(target->getFirstActiveEnchant()->isTerminated());
    EXPECT_FALSE(scr_DisenchantTarget(state, self));

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_DisenchantTarget(state, self));

    self.setTarget(target->getObjRef());
    EXPECT_TRUE(scr_EnchantTarget(state, self));
    EXPECT_TRUE(scr_EnchantChild(state, self));
    EXPECT_TRUE(scr_DisenchantAll(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, EnchantTargetAndChildReturnFalseWhenOwnerOrSpawnerCannotBeResolved)
{
    auto& module = beginActiveTestModule();
    ENC_REF enchantRef = ENCHANTPROFILES_MAX;
    auto actor = makeEnchantSpawner(module, 5702, enchantRef);
    auto target = makeObject(module, "mp_objects/follower.obj", 5705);
    auto child = makeObject(module, "mp_objects/follower.obj", 5706);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(child, nullptr);
    ASSERT_LT(enchantRef, ENCHANTPROFILES_MAX);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    self.child = child->getObjRef();

    self.owner = ObjectRef::Invalid;
    EXPECT_FALSE(scr_EnchantTarget(state, self));
    EXPECT_FALSE(scr_EnchantChild(state, self));
    EXPECT_FALSE(target->hasActiveEnchants());
    EXPECT_FALSE(child->hasActiveEnchants());

    self.owner = actor->getObjRef();
    self.setSelf(ObjectRef::Invalid);
    EXPECT_FALSE(scr_EnchantTarget(state, self));
    EXPECT_FALSE(scr_EnchantChild(state, self));
    EXPECT_FALSE(target->hasActiveEnchants());
    EXPECT_FALSE(child->hasActiveEnchants());
}

TEST_F(ScriptSystemsFunctionsFixture, DisenchantAllHandlesMixedEnchantedAndPlainObjects)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5698);
    auto enchanted = makeObject(module, "mp_objects/follower.obj", 5699);
    auto plain = makeObject(module, "mp_objects/follower.obj", 5700);
    auto enchant = addHealRemovableEnchant(module, enchanted, 5701);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(enchanted, nullptr);
    ASSERT_NE(plain, nullptr);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(enchanted->hasActiveEnchants());
    EXPECT_FALSE(plain->hasActiveEnchants());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, enchanted);
    for (const auto& object : module.getObjectHandler().iterator())
    {
        (void)object;
    }

    EXPECT_TRUE(scr_DisenchantAll(state, self));
    ASSERT_NE(enchanted->getFirstActiveEnchant(), nullptr);
    EXPECT_TRUE(enchanted->getFirstActiveEnchant()->isTerminated());
    EXPECT_TRUE(enchant->isTerminated());
    EXPECT_FALSE(plain->hasActiveEnchants());
}

TEST_F(ScriptSystemsFunctionsFixture, TeamHelpersUseTeamMemberRoleSeams)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5681);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5682);
    auto ally = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5683);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(ally, nullptr);

    actor->setItem(false);
    actor->setInvincible(false);
    target->setItem(false);
    target->setInvincible(false);
    ally->setItem(false);
    ally->setInvincible(false);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_TRUE(scr_JoinTargetTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));

    state.argument = static_cast<int>(Team::TEAM_GOOD);
    EXPECT_TRUE(scr_JoinTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));

    EXPECT_TRUE(scr_TargetJoinTeam(state, self));
    EXPECT_EQ(target->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_NULL));
    EXPECT_TRUE(scr_JoinGoodTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));

    EXPECT_TRUE(scr_JoinEvilTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));

    EXPECT_TRUE(scr_JoinNullTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_NULL));

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    ally->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    module.getTeamList()[Team::TEAM_GOOD].clearLeader();
    EXPECT_TRUE(scr_BecomeLeader(state, self));
    EXPECT_EQ(module.getTeamList()[Team::TEAM_GOOD].getLeaderRef(), actor->getObjRef());
    EXPECT_TRUE(scr_IfLeaderIsAlive(state, self));
    EXPECT_EQ(module.getTeamLeaderRef(static_cast<TEAM_REF>(Team::TEAM_GOOD)), actor->getObjRef());

    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    state.argument = 96;
    state.distance = static_cast<int>(XP_TEAMKILL);
    EXPECT_TRUE(scr_GiveExperienceToTargetTeam(state, self));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(ally->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(target->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));

    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    const int goodTeamXpBefore = actor->getExperience();
    state.argument = 48;
    EXPECT_TRUE(scr_GiveExperienceToGoodTeam(state, self));
    EXPECT_GT(actor->getExperience(), goodTeamXpBefore);

    module.getTeamList()[Team::TEAM_GOOD].clearLeader();
    EXPECT_FALSE(scr_IfLeaderIsAlive(state, self));
    EXPECT_EQ(module.getTeamLeaderRef(static_cast<TEAM_REF>(Team::TEAM_GOOD)), ObjectRef::Invalid);

    actor->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_MAX));
    actor->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_MAX));
    EXPECT_FALSE(scr_IfLeaderIsAlive(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, SelfRoleResidueHelpersFailQuietlyWhenSelfRefIsInvalid)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 56811);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 56812);
    auto ammoActor = makeAmmoItem(module, 56813);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(ammoActor, nullptr);

    actor->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    target->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    ammoActor->setAmmo(ammoActor->getAmmoMax());

    auto enchant = addHealRemovableEnchant(module, actor, 56814);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(actor->hasActiveEnchants());
    ASSERT_NE(actor->getFirstActiveEnchant(), nullptr);
    const float ownerManaSustainBefore = actor->getFirstActiveEnchant()->getOwnerManaSustain();
    const float ownerLifeSustainBefore = actor->getFirstActiveEnchant()->getOwnerLifeSustain();
    const float targetManaDrainBefore = actor->getFirstActiveEnchant()->getTargetManaDrain();
    const float targetLifeDrainBefore = actor->getFirstActiveEnchant()->getTargetLifeDrain();

    script_state_t state;

    ai_state_t invalidTeamSelf = makeScriptSelf(actor, target);
    invalidTeamSelf.setSelf(ObjectRef::Invalid);
    EXPECT_FALSE(scr_JoinTargetTeam(state, invalidTeamSelf));
    EXPECT_EQ(actor->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));

    module.getTeamList()[Team::TEAM_GOOD].clearLeader();
    EXPECT_FALSE(scr_BecomeLeader(state, invalidTeamSelf));
    EXPECT_EQ(module.getTeamLeaderRef(static_cast<TEAM_REF>(Team::TEAM_GOOD)), ObjectRef::Invalid);
    EXPECT_FALSE(scr_IfLeaderIsAlive(state, invalidTeamSelf));

    ai_state_t invalidAmmoSelf = makeScriptSelf(ammoActor, target);
    invalidAmmoSelf.setSelf(ObjectRef::Invalid);
    EXPECT_FALSE(scr_IncreaseAmmo(state, invalidAmmoSelf));
    EXPECT_EQ(ammoActor->getAmmo(), ammoActor->getAmmoMax());
    EXPECT_FALSE(scr_CostAmmo(state, invalidAmmoSelf));
    EXPECT_EQ(ammoActor->getAmmo(), ammoActor->getAmmoMax());

    ai_state_t invalidEnchantSelf = makeScriptSelf(actor, target);
    invalidEnchantSelf.setSelf(ObjectRef::Invalid);
    state.argument = FLOAT_TO_FP8(1.0f);
    state.distance = FLOAT_TO_FP8(2.0f);
    state.x = FLOAT_TO_FP8(3.0f);
    state.y = FLOAT_TO_FP8(4.0f);
    EXPECT_FALSE(scr_SetEnchantBoostValues(state, invalidEnchantSelf));
    ASSERT_NE(actor->getFirstActiveEnchant(), nullptr);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getOwnerManaSustain(), ownerManaSustainBefore);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getOwnerLifeSustain(), ownerLifeSustainBefore);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getTargetManaDrain(), targetManaDrainBefore);
    EXPECT_FLOAT_EQ(actor->getFirstActiveEnchant()->getTargetLifeDrain(), targetLifeDrainBefore);
}

TEST_F(ScriptSystemsFunctionsFixture, WalletHelpersUseWalletRoleSeamsAndPreserveClampSemantics)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5691);
    auto target = makeObject(module, "mp_objects/follower.obj", 5692);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->giveMoney(100);
    target->giveMoney(40);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    state.argument = 30;
    EXPECT_TRUE(scr_GiveMoneyToTarget(state, self));
    EXPECT_EQ(state.argument, 30);
    EXPECT_EQ(actor->getMoney(), 70);
    EXPECT_EQ(target->getMoney(), 70);

    state.argument = 500;
    EXPECT_TRUE(scr_GiveMoneyToTarget(state, self));
    EXPECT_EQ(state.argument, 70);
    EXPECT_EQ(actor->getMoney(), 0);
    EXPECT_EQ(target->getMoney(), 140);

    target->giveMoney(-90);
    state.argument = -200;
    EXPECT_TRUE(scr_GiveMoneyToTarget(state, self));
    EXPECT_EQ(state.argument, -50);
    EXPECT_EQ(actor->getMoney(), 50);
    EXPECT_EQ(target->getMoney(), 0);

    state.argument = 15;
    EXPECT_TRUE(scr_DropTargetMoney(state, self));
    EXPECT_EQ(target->getMoney(), 0);

    actor->giveMoney(60);
    state.argument = 20;
    EXPECT_TRUE(scr_DropMoney(state, self));
    EXPECT_EQ(actor->getMoney(), 90);

    state.argument = 33;
    EXPECT_TRUE(scr_SetMoney(state, self));
    EXPECT_EQ(actor->getMoney(), 33);

    state.argument = Object::MAXMONEY + 25;
    EXPECT_TRUE(scr_SetMoney(state, self));
    EXPECT_EQ(actor->getMoney(), Object::MAXMONEY);

    state.argument = -5;
    EXPECT_TRUE(scr_SetMoney(state, self));
    EXPECT_EQ(actor->getMoney(), 0);
}

TEST_F(ScriptSystemsFunctionsFixture, TargetEconomyHelpersReturnFalseWhenTargetIsMissing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5693);

    ASSERT_NE(actor, nullptr);
    actor->giveMoney(100);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    const int initialMoney = actor->getMoney();
    const SKIN_T initialSkin = actor->getSkin();

    state.argument = 2;
    state.x = 77;
    EXPECT_FALSE(scr_GetTargetArmorPrice(state, self));
    EXPECT_EQ(state.argument, 2);
    EXPECT_EQ(state.x, 77);

    state.argument = 2;
    state.x = 88;
    EXPECT_FALSE(scr_ChangeTargetArmor(state, self));
    EXPECT_EQ(state.argument, 2);
    EXPECT_EQ(state.x, 88);
    EXPECT_EQ(actor->getSkin(), initialSkin);

    state.argument = 30;
    EXPECT_FALSE(scr_GiveMoneyToTarget(state, self));
    EXPECT_EQ(state.argument, 30);
    EXPECT_EQ(actor->getMoney(), initialMoney);

    state.argument = 15;
    EXPECT_FALSE(scr_DropTargetMoney(state, self));
    EXPECT_EQ(actor->getMoney(), initialMoney);

    state.argument = 3;
    state.x = 44;
    state.y = 55;
    EXPECT_FALSE(scr_TargetPayForArmor(state, self));
    EXPECT_EQ(state.argument, 3);
    EXPECT_EQ(state.x, 44);
    EXPECT_EQ(state.y, 55);
    EXPECT_EQ(actor->getMoney(), initialMoney);
}

TEST_F(ScriptSystemsFunctionsFixture, ArmorHelpersUseAppearanceProfileSeamAndPreserveLegacyOutputs)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5701);
    auto target = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5702);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(actor->setSkin(0));
    ASSERT_TRUE(target->setSkin(0));

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    state.argument = 2;
    EXPECT_TRUE(scr_GetTargetArmorPrice(state, self));
    EXPECT_EQ(state.x, target->getProfile()->getSkinInfo(2).cost);

    state.argument = 2;
    EXPECT_TRUE(scr_ChangeTargetArmor(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(state.x, 1);
    EXPECT_EQ(target->getSkin(), 2);

    state.argument = 3;
    EXPECT_TRUE(scr_ChangeArmor(state, self));
    EXPECT_EQ(state.argument, 0);
    EXPECT_EQ(state.x, 3);
    EXPECT_EQ(actor->getSkin(), 3);

    ASSERT_TRUE(target->setSkin(3));
    target->giveMoney(-static_cast<int>(target->getMoney()));
    state.argument = 0;
    EXPECT_TRUE(scr_TargetPayForArmor(state, self));
    EXPECT_EQ(state.y, target->getProfile()->getSkinInfo(0).cost);
    EXPECT_EQ(state.x, 0);
    EXPECT_EQ(target->getMoney(), 995);

    ASSERT_TRUE(target->setSkin(1));
    target->giveMoney(-static_cast<int>(target->getMoney()));
    target->giveMoney(600);
    state.argument = 2;
    EXPECT_TRUE(scr_TargetPayForArmor(state, self));
    EXPECT_EQ(state.y, target->getProfile()->getSkinInfo(2).cost);
    EXPECT_EQ(state.x, 0);
    EXPECT_EQ(target->getMoney(), 50);

    ASSERT_TRUE(target->setSkin(0));
    target->giveMoney(-static_cast<int>(target->getMoney()));
    target->giveMoney(100);
    state.argument = 3;
    EXPECT_FALSE(scr_TargetPayForArmor(state, self));
    EXPECT_EQ(state.y, target->getProfile()->getSkinInfo(3).cost);
    EXPECT_EQ(state.x, 895);
    EXPECT_EQ(target->getMoney(), 100);
}

TEST_F(ScriptSystemsFunctionsFixture, BecomeSpellUsesEnchantableAndMorphRolesAndResetsScriptState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/book.obj", 5703);

    ASSERT_NE(actor, nullptr);

    const auto enchant = addHealRemovableEnchant(module, actor, 5704);
    ASSERT_NE(enchant, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    const ObjectProfileRef spellProfile = loadProfile("mp_data/globalobjects/players/rogue.obj", 5706);
    ASSERT_NE(spellProfile, ObjectProfileRef::Invalid);
    self.content = spellProfile.get();
    self.state = 41;

    const ObjectProfileRef previousBaseModel = actor->getBaseModelRef();

    EXPECT_TRUE(scr_BecomeSpell(state, self));
    EXPECT_TRUE(enchant->isTerminated());
    if (actor->getFirstActiveEnchant() != nullptr)
    {
        EXPECT_TRUE(actor->getFirstActiveEnchant()->isTerminated());
    }
    EXPECT_EQ(actor->getProfileID(), spellProfile);
    EXPECT_EQ(actor->getBaseModelRef(), previousBaseModel);
    EXPECT_EQ(self.content, 0);
    EXPECT_EQ(self.state, 0);
}

TEST_F(ScriptSystemsFunctionsFixture, BecomeSpellbookUsesEnchantableMorphAndAnimationRoles)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/magic/summonspellii.obj", 5707);

    ASSERT_NE(actor, nullptr);

    const auto enchant = addHealRemovableEnchant(module, actor, 5708);
    ASSERT_NE(enchant, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.state = 17;
    self.content = 99;

    const ObjectProfileRef previousProfile = actor->getProfileID();
    const ObjectProfileRef previousBaseModel = actor->getBaseModelRef();
    const SKIN_T previousSpellEffectSkin = actor->getProfile()->getSpellEffectType();

    EXPECT_TRUE(scr_BecomeSpellbook(state, self));
    EXPECT_TRUE(enchant->isTerminated());
    if (actor->getFirstActiveEnchant() != nullptr)
    {
        EXPECT_TRUE(actor->getFirstActiveEnchant()->isTerminated());
    }
    EXPECT_EQ(actor->getProfileID(), ObjectProfileRef(SPELLBOOK));
    EXPECT_EQ(actor->getBaseModelRef(), previousBaseModel);
    EXPECT_EQ(actor->getSkin(), previousSpellEffectSkin);
    EXPECT_EQ(self.content, REF_TO_INT(previousProfile.get()));
    EXPECT_EQ(self.state, 0);
    EXPECT_EQ(actor->getCurrentAnimation(), ACTION_JB);
}

TEST_F(ScriptSystemsFunctionsFixture, IfCharacterWasABookPreservesBaseModelAndCurrentProfileSemantics)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/healer.obj", 5709);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    const ObjectProfileRef initialBaseModel = actor->getBaseModelRef();
    ASSERT_EQ(initialBaseModel, actor->getProfileID());
    EXPECT_TRUE(scr_IfCharacterWasABook(state, self));

    const ObjectProfileRef alternateProfile = loadProfile("mp_data/globalobjects/players/rogue.obj", 5710);
    ASSERT_NE(alternateProfile, ObjectProfileRef::Invalid);
    ASSERT_NE(alternateProfile, actor->getProfileID());

    actor->polymorphObject(alternateProfile, 0);
    EXPECT_EQ(actor->getBaseModelRef(), initialBaseModel);
    EXPECT_EQ(actor->getProfileID(), alternateProfile);
    EXPECT_FALSE(scr_IfCharacterWasABook(state, self));

    actor->setBaseModelRef(ObjectProfileRef(SPELLBOOK));
    EXPECT_EQ(actor->getProfileID(), alternateProfile);
    EXPECT_TRUE(scr_IfCharacterWasABook(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, ChangeTargetClassUsesMorphControlAndPublishesBaseModel)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/healer.obj", 5703);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    const ObjectProfileRef previousProfile = actor->getProfileID();
    const ObjectProfileRef previousBaseModel = actor->getBaseModelRef();
    const ObjectProfileRef nextProfile = loadProfile("mp_data/globalobjects/players/rogue.obj", 5705);
    ASSERT_NE(nextProfile, ObjectProfileRef::Invalid);

    state.argument = nextProfile.get();
    EXPECT_TRUE(scr_ChangeTargetClass(state, self));
    EXPECT_EQ(actor->getProfileID(), nextProfile);
    EXPECT_EQ(actor->getBaseModelRef(), nextProfile);

    PRO_REF unloadedProfile = INVALID_PRO_REF;
    for (PRO_REF candidate = 0; candidate < INVALID_PRO_REF; ++candidate)
    {
        if (!EngineContext::get().profileSystem().isLoaded(candidate))
        {
            unloadedProfile = candidate;
            break;
        }
    }

    ASSERT_NE(unloadedProfile, INVALID_PRO_REF);
    state.argument = unloadedProfile;
    EXPECT_FALSE(scr_ChangeTargetClass(state, self));
    EXPECT_EQ(actor->getProfileID(), nextProfile);
    EXPECT_EQ(actor->getBaseModelRef(), nextProfile);
    EXPECT_NE(previousProfile, nextProfile);
    EXPECT_NE(previousBaseModel, nextProfile);
}

TEST_F(ScriptSystemsFunctionsFixture, ChangeTargetClassFailsQuietlyWhenSelfRefIsInvalid)
{
    beginActiveTestModule();

    script_state_t state;
    state.argument = ObjectProfileRef(SPELLBOOK).get();
    ai_state_t self;
    self.setSelf(ObjectRef::Invalid);

    EXPECT_FALSE(scr_ChangeTargetClass(state, self));
}

TEST_F(ScriptSystemsFunctionsFixture, PassageMutatorsPreserveExistingPassageBehavior)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5680);

    ASSERT_NE(actor, nullptr);

    auto [passage, passageId] = addPassage(module);
    ASSERT_NE(passage, nullptr);
    EXPECT_TRUE(passage->isOpen());

    script_state_t state;
    state.argument = passageId;
    state.distance = 9;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfPassageOpen(state, self));
    EXPECT_TRUE(scr_ClosePassage(state, self));
    EXPECT_TRUE(passage->isOpen());
    EXPECT_TRUE(scr_IfPassageOpen(state, self));
    EXPECT_TRUE(scr_OpenPassage(state, self));
    EXPECT_TRUE(passage->isOpen());
    EXPECT_TRUE(scr_IfPassageOpen(state, self));

    EXPECT_TRUE(scr_AddShopPassage(state, self));
    EXPECT_TRUE(passage->isShop());
    EXPECT_EQ(passage->getShopOwner(), actor->getObjRef());

    state.argument = 99;
    EXPECT_FALSE(scr_OpenPassage(state, self));
    EXPECT_FALSE(scr_ClosePassage(state, self));
    EXPECT_FALSE(scr_IfPassageOpen(state, self));
    EXPECT_TRUE(scr_FlashPassage(state, self));
}

} // namespace
