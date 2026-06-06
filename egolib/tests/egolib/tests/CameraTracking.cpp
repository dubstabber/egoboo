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
#include "egolib/game/Graphics/Camera.hpp"
#undef private
#undef protected
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "TestGraphicsSystem.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/game.h"

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

class CameraTrackingFixture : public ::testing::Test
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
        opts.randomSeed = 73;
        opts.binaryPath = "";
        opts.logPath = "/debug/camera-tracking-tests.log";
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
        const bool began = session.beginModule(module, 73);
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

        auto object = module.spawnObject(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        EXPECT_NE(object, nullptr);
        if (!object)
        {
            throw std::runtime_error("spawn object failed");
        }

        return object;
    }

    CameraOptions cameraOptions() const
    {
        CameraOptions options{};
        options.swing = 0;
        options.swingRate = 0;
        options.swingAmp = 0.0f;
        options.turnMode = CameraTurnMode::Auto;
        return options;
    }

    StubGraphicsWindow _window{ idlib::vector_2s(640, 480) };
    std::unique_ptr<Ego::Test::MockGraphicsSystem> _mockGraphicsSystem;
    Ego::Core::System* _fakeCoreSystem = nullptr;
    Ego::Core::System* _previousCoreSystem = nullptr;
    Ego::Core::SystemService* _fakeSystemService = nullptr;
    Ego::GraphicsSystem* _fakeGraphicsSystem = nullptr;
    Ego::GraphicsSystem* _previousGraphicsSystem = nullptr;
};

std::unique_ptr<ContentRuntimeBootstrap> CameraTrackingFixture::s_runtime;

TEST_F(CameraTrackingFixture, SingleLiveTrackedPlayerKeepsExactTrackedPosition)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6200, Ego::Vector3f(32.0f, 48.0f, 0.0f));

    Camera camera(cameraOptions());
    camera.addTrackTarget(actor->getObjRef());
    camera.updateTrack();

    EXPECT_FLOAT_EQ(camera.getTrackPosition().x(), actor->getPosition().x());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().y(), actor->getPosition().y());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().z(), actor->getPosition().z());
}

TEST_F(CameraTrackingFixture, MissingOrTerminatedTrackedRefsAreIgnored)
{
    auto& module = beginActiveTestModule();
    auto liveActor = makeObject(module, "mp_objects/follower.obj", 6201, Ego::Vector3f(48.0f, 64.0f, 0.0f));
    auto terminatedActor = makeObject(module, "mp_objects/follower.obj", 6202, Ego::Vector3f(192.0f, 224.0f, 0.0f));

    Camera camera(cameraOptions());
    camera.addTrackTarget(liveActor->getObjRef());
    camera.addTrackTarget(terminatedActor->getObjRef());
    terminatedActor->requestTerminate();
    camera._trackList.push_front(ObjectRef::Invalid);
    camera.updateTrack();

    EXPECT_FLOAT_EQ(camera.getTrackPosition().x(), liveActor->getPosition().x());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().y(), liveActor->getPosition().y());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().z(), liveActor->getPosition().z());
}

TEST_F(CameraTrackingFixture, MultiPlayerTrackingKeepsWeightedBlendBehaviorForLivePlayers)
{
    auto& module = beginActiveTestModule();
    auto firstActor = makeObject(module, "mp_objects/follower.obj", 6203, Ego::Vector3f(20.0f, 40.0f, 0.0f));
    auto secondActor = makeObject(module, "mp_objects/follower.obj", 6204, Ego::Vector3f(120.0f, 240.0f, 0.0f));
    firstActor->setVelocity(Ego::Vector3f(10.0f, 0.0f, 0.0f));
    secondActor->setVelocity(Ego::Vector3f(20.0f, 0.0f, 0.0f));

    Camera camera(cameraOptions());
    camera.addTrackTarget(firstActor->getObjRef());
    camera.addTrackTarget(secondActor->getObjRef());
    camera.updateTrack();

    constexpr float firstWeight = 100.0f;
    constexpr float secondWeight = 400.0f;
    const Ego::Vector3f weightedAverage =
        (firstActor->getPosition() * firstWeight + secondActor->getPosition() * secondWeight) / (firstWeight + secondWeight);
    const Ego::Vector3f expectedTrack = firstActor->getPosition() * 0.9f + weightedAverage * 0.1f;

    EXPECT_FLOAT_EQ(camera.getTrackPosition().x(), expectedTrack.x());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().y(), expectedTrack.y());
    EXPECT_FLOAT_EQ(camera.getTrackPosition().z(), expectedTrack.z());
}

} // namespace
