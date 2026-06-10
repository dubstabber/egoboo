#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define protected public
#define private public
#include "egolib/Core/System.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Graphics/GraphicsSystem.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/game/GUI/MessageLog.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#undef private
#undef protected
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "TestGraphicsSystem.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/game/Graphics/ICameraSystem.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/graphic.h"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

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
        GameEngine& engine = context.engine();
        _previousGameState = engine._currentGameState;
        _previousUiManager = engine._uiManager.release();
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
        EngineContext::get().installGraphicsSystem(*_mockGraphicsSystem);

        engine._currentGameState = std::make_shared<PlayingState>();
        _playingState = std::dynamic_pointer_cast<PlayingState>(engine._currentGameState);
    }

    ~ScopedPlayingStateHarness()
    {
        GameEngine& engine = EngineContext::get().engine();
        engine._currentGameState = _previousGameState;
        engine._uiManager.release();
        if (_previousUiManager != nullptr)
        {
            engine._uiManager.reset(_previousUiManager);
            Ego::GUI::installActiveUIManager(*_previousUiManager);
        }
        else
        {
            Ego::GUI::clearActiveUIManager();
        }

        EngineContext::get().clearGraphicsSystem();
        _mockGraphicsSystem.reset();
        CoreSystemAccess::instance.store(_previousCoreSystem);
        GraphicsSystemAccess::instance.store(_previousGraphicsSystem);
        ::operator delete(_fakeCoreSystem);
        ::operator delete(_fakeSystemService);
        ::operator delete(_fakeGraphicsSystem);
        ::operator delete(_fakeUiManager);
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
    Ego::GUI::UIManager* _previousUiManager = nullptr;
    std::shared_ptr<GameState> _previousGameState;
    std::shared_ptr<PlayingState> _playingState;
};

class StubAudioSystem : public IAudioSystem
{
public:
    struct SpatialSoundCall
    {
        Ego::Vector3f position;
        SoundID soundID = INVALID_SOUND_ID;
    };

    struct LoopSoundCall
    {
        SoundID soundID = INVALID_SOUND_ID;
        ObjectRef ownerRef = ObjectRef::Invalid;
    };

    struct StopLoopCall
    {
        ObjectRef ownerRef = ObjectRef::Invalid;
        SoundID soundID = INVALID_SOUND_ID;
    };

    struct MusicCall
    {
        MusicID musicID = 0;
        uint16_t fadeTime = 0;
    };

    void reset()
    {
        playedSounds.clear();
        loopedSounds.clear();
        stoppedLoopSounds.clear();
        playedFullSounds.clear();
        playedMusicIds.clear();
        playedSongNames.clear();
        fadeAllCalls = 0;
        stopMusicCalls = 0;
        setMusicVolumeCalls = 0;
        setSoundEffectVolumeCalls = 0;
        updateCalls = 0;
        nextSoundChannel = 7;
        maxHearingDistance = 0.0f;
    }

    void playMusic(MusicID musicID, uint16_t fadeTime = 0) override
    {
        playedMusicIds.push_back({musicID, fadeTime});
    }

    SoundID loadSound(const std::string&) override
    {
        return INVALID_SOUND_ID;
    }

    void playMusic(const std::string& songName, uint16_t fadeTime = 0) override
    {
        playedSongNames.push_back({songName, fadeTime});
    }

    void stopMusic() override
    {
        ++stopMusicCalls;
    }

    void fadeAllSounds() override
    {
        ++fadeAllCalls;
    }

    int playSound(const Ego::Vector3f& position, SoundID soundID) override
    {
        playedSounds.push_back({position, soundID});
        return nextSoundChannel;
    }

    void playSoundLooped(SoundID soundID, ObjectRef ownerRef) override
    {
        loopedSounds.push_back({soundID, ownerRef});
    }

    size_t stopObjectLoopingSounds(ObjectRef ownerRef, SoundID soundID = -1) override
    {
        stoppedLoopSounds.push_back({ownerRef, soundID});
        return 1;
    }

    int playSoundFull(SoundID soundID) override
    {
        playedFullSounds.push_back(soundID);
        return nextSoundChannel;
    }

    SoundID getGlobalSound(GlobalSound id) const override
    {
        return 1000 + static_cast<SoundID>(id);
    }

    void setMaxHearingDistance(float distance) override
    {
        maxHearingDistance = distance;
    }

    void setMusicVolume(int) override
    {
        ++setMusicVolumeCalls;
    }

    void setSoundEffectVolume(int) override
    {
        ++setSoundEffectVolumeCalls;
    }

    void update() override
    {
        ++updateCalls;
    }

    void loadGlobalSounds() override {}
    void loadAllMusic() override {}

    std::vector<SpatialSoundCall> playedSounds;
    std::vector<LoopSoundCall> loopedSounds;
    std::vector<StopLoopCall> stoppedLoopSounds;
    std::vector<SoundID> playedFullSounds;
    std::vector<MusicCall> playedMusicIds;
    std::vector<std::pair<std::string, uint16_t>> playedSongNames;
    int fadeAllCalls = 0;
    int stopMusicCalls = 0;
    int setMusicVolumeCalls = 0;
    int setSoundEffectVolumeCalls = 0;
    int updateCalls = 0;
    int nextSoundChannel = 7;
    float maxHearingDistance = 0.0f;
};

class StubCameraSystem : public ICameraSystem
{
public:
    void reset()
    {
        _cameraList.clear();
        updateAllCalls = 0;
        lastSetCameraCount = 0;
    }

    void setSingleTrackPosition(const Ego::Vector3f& position)
    {
        CameraOptions options{};
        auto camera = std::make_shared<Camera>(options);
        camera->_trackPos = position;
        _cameraList = {camera};
    }

    void updateAll(const ego_mesh_t*) override
    {
        ++updateAllCalls;
    }

    void setNumberOfCameras(size_t numberOfCameras) override
    {
        lastSetCameraCount = numberOfCameras;
    }

    const std::vector<std::shared_ptr<Camera>>& getCameraList() const override
    {
        return _cameraList;
    }

    std::shared_ptr<Camera> getMainCamera() const override
    {
        return _cameraList.empty() ? nullptr : _cameraList.front();
    }

    std::shared_ptr<Camera> getCamera(ObjectRef) const override
    {
        return _cameraList.empty() ? nullptr : _cameraList.front();
    }

    CameraOptions& getCameraOptions() override
    {
        return _cameraOptions;
    }

    void renderAll(std::function<void(std::shared_ptr<Camera>, std::shared_ptr<Ego::Graphics::TileList>, std::shared_ptr<Ego::Graphics::EntityList>)>) override
    {
    }

    bool isTileInMainCameraRenderList(const Index1D&) const override { return false; }

    size_t updateAllCalls = 0;
    size_t lastSetCameraCount = 0;

private:
    std::vector<std::shared_ptr<Camera>> _cameraList;
    CameraOptions _cameraOptions;
};

class StubBillboardSystem : public Ego::Graphics::IBillboardSystem
{
public:
    struct Call
    {
        ObjectRef objectRef = ObjectRef::Invalid;
        std::string text;
        Ego::Colour4f textColor = Ego::Colour4f::white();
        Ego::Colour4f tint = Ego::Colour4f::white();
        int lifetime = 0;
        BIT_FIELD flags = EMPTY_BIT_FIELD;
        float size = 0.0f;

        Call() = default;

        Call(ObjectRef objectRef,
             std::string text,
             const Ego::Colour4f& textColor,
             const Ego::Colour4f& tint,
             int lifetime,
             BIT_FIELD flags,
             float size) :
            objectRef(objectRef),
            text(std::move(text)),
            textColor(textColor),
            tint(tint),
            lifetime(lifetime),
            flags(flags),
            size(size)
        {}
    };

    void reset() override
    {
        ++resetCalls;
    }

    void update() override
    {
        ++updateCalls;
    }

    std::shared_ptr<Ego::Graphics::Billboard> makeBillboard(ObjectRef objectRef,
                                                            const std::string& text,
                                                            const Ego::Colour4f& textColor,
                                                            const Ego::Colour4f& tint,
                                                            int lifetime_secs,
                                                            BIT_FIELD opt_bits,
                                                            float size) override
    {
        calls.emplace_back(objectRef, text, textColor, tint, lifetime_secs, opt_bits, size);
        return std::make_shared<Ego::Graphics::Billboard>(Time::Ticks(), nullptr, size);
    }

    std::vector<Call> calls;
    int resetCalls = 0;
    int updateCalls = 0;
};

class ScriptActionFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 61;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-action-function-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        EngineContext::get().setEngine(std::make_unique<GameEngine>());
        AudioSystem::initialize();
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
        AudioSystem::uninitialize();
        EngineContext::get().clearEngine();
        s_runtime.reset();
    }

    void SetUp() override
    {
        auto& context = EngineContext::get();
        context.clearAudioSystem();
        context.clearCameraSystem();
        context.clearBillboardSystem();
        context.installAudioSystem(audioSystem);
        context.installCameraSystem(cameraSystem);
        context.installBillboardSystem(billboardSystem);
        cameraSystem.reset();
        billboardSystem.calls.clear();
        billboardSystem.resetCalls = 0;
        billboardSystem.updateCalls = 0;

        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        context.profileSystem().reset();
        context.profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
        session.publishLocalPlayerPerception(LocalPlayerPerceptionState{});
        audioSystem.reset();
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearCameraSystem();
        EngineContext::get().clearBillboardSystem();
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

    GameModule& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        auto& session = GameSessionContext::get();
        const bool began = session.beginModule(module, 61);
        EXPECT_TRUE(began);
        return session.activeModule();
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

    std::pair<std::shared_ptr<Object>, int> makeSoundingObject(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_objects/follower.obj",
            "mp_data/globalobjects/items/torch.obj",
            "mp_data/globalobjects/weapons/stiletto.obj",
            "mp_data/globalobjects/armor/atshield.obj"
        };

        for (size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex)
        {
            auto object = makeObject(module, candidates[candidateIndex], slotBase + static_cast<int>(candidateIndex));
            if (!object)
            {
                continue;
            }

            for (int soundIndex = 0; soundIndex < 64; ++soundIndex)
            {
                const SoundID soundID = object->getProfile()->getSoundID(soundIndex);
                if (soundID != INVALID_SOUND_ID)
                {
                    return {object, soundIndex};
                }
            }

            constexpr int injectedSoundIndex = 3;
            constexpr SoundID injectedSoundID = 4242;
            object->getProfile()->_soundMap[injectedSoundIndex] = injectedSoundID;
            return {object, injectedSoundIndex};
        }

        ADD_FAILURE() << "unable to load an object fixture with a valid sound entry";
        return {nullptr, -1};
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(ObjectRef::Invalid);
        return self;
    }

    std::pair<std::shared_ptr<Passage>, int> addPassage(GameModule& module,
                                                        uint8_t mask = EMPTY_BIT_FIELD) const
    {
        auto passage = std::make_shared<Passage>(module, 0, 0, 4, 4, mask);
        module._passages.push_back(passage);
        return {passage, static_cast<int>(module._passages.size() - 1)};
    }

    void moveObjectToOldZ(const std::shared_ptr<Object>& object, float oldZ) const
    {
        ASSERT_NE(object, nullptr);
        const Ego::Vector3f position = object->getPosition();
        EXPECT_TRUE(object->setPosition(position.x(), position.y(), oldZ));
        EXPECT_TRUE(object->setPosition(position.x(), position.y(), oldZ + 1.0f));
    }

    StubAudioSystem audioSystem;
    StubCameraSystem cameraSystem;
    StubBillboardSystem billboardSystem;
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptActionFunctionsFixture::s_runtime;

ModelAction findValidAction(const std::shared_ptr<Object>& object,
                            std::initializer_list<ModelAction> candidates,
                            ModelAction excluded = ACTION_COUNT)
{
    const auto& model = object->inst.getModelDescriptor();
    if (!model)
    {
        return ACTION_COUNT;
    }

    for (const ModelAction action : candidates)
    {
        if (action != excluded && model->isActionValid(action))
        {
            return action;
        }
    }

    return ACTION_COUNT;
}

TEST_F(ScriptActionFunctionsFixture, PlaySoundUsesInstalledAudioSystemOnlyAbovePitNoSoundPlane)
{
    auto& module = beginActiveTestModule();
    const auto [actor, soundIndex] = makeSoundingObject(module, 5701);

    ASSERT_NE(actor, nullptr);
    ASSERT_GE(soundIndex, 0);

    script_state_t state;
    state.argument = soundIndex;
    ai_state_t self = makeScriptSelf(actor);

    moveObjectToOldZ(actor, PITNOSOUND + 8.0f);
    EXPECT_TRUE(scr_PlaySound(state, self));
    ASSERT_EQ(audioSystem.playedSounds.size(), 1u);
    EXPECT_EQ(audioSystem.playedSounds.front().soundID, actor->getProfile()->getSoundID(soundIndex));

    audioSystem.reset();
    moveObjectToOldZ(actor, PITNOSOUND - 8.0f);
    EXPECT_TRUE(scr_PlaySound(state, self));
    EXPECT_TRUE(audioSystem.playedSounds.empty());
}

TEST_F(ScriptActionFunctionsFixture, SendMessageAndUsageKnownUseSelfCompatibilityContext)
{
    auto& module = beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    auto actor = makeObject(module, "mp_objects/follower.obj", 5704);

    ASSERT_NE(actor, nullptr);
    const int messageId = static_cast<int>(actor->getProfile()->addMessage("self context text"));
    ASSERT_TRUE(actor->getProfile()->isValidMessageID(messageId));

    ai_state_t self = makeScriptSelf(actor);
    script_state_t usageState;
    script_state_t messageState;
    messageState.argument = messageId;

    EXPECT_TRUE(scr_MakeUsageKnown(usageState, self));
    EXPECT_TRUE(actor->getProfile()->isUsageKnown());

    const size_t initialMessageCount = playingStateHarness.messageTexts().size();
    EXPECT_TRUE(scr_SendMessage(messageState, self));
    const auto messages = playingStateHarness.messageTexts();
    ASSERT_EQ(messages.size(), initialMessageCount + 1);
    EXPECT_EQ(messages.back(), "Self context text");
}

TEST_F(ScriptActionFunctionsFixture, MusicPassageHelpersUpdatePassageMusic)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5900);

    ASSERT_NE(actor, nullptr);

    auto [passage, passageId] = addPassage(module);
    ASSERT_NE(passage, nullptr);
    EXPECT_EQ(passage->_music, Passage::NO_MUSIC);

    script_state_t state;
    state.argument = passageId;
    state.distance = 7;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetMusicPassage(state, self));
    EXPECT_EQ(passage->_music, 7);

    EXPECT_TRUE(scr_ClearMusicPassage(state, self));
    EXPECT_EQ(passage->_music, Passage::NO_MUSIC);
}

TEST_F(ScriptActionFunctionsFixture, MusicPassageHelpersIgnoreMissingPassagesWithoutFailing)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5901);

    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(module.getPassageCount(), 0);

    script_state_t state;
    state.argument = 99;
    state.distance = 3;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetMusicPassage(state, self));
    EXPECT_TRUE(scr_ClearMusicPassage(state, self));
}

TEST_F(ScriptActionFunctionsFixture, CheckPassageMusicUsesLivePlayerObservation)
{
    auto& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_objects/follower.obj", 5902, Ego::Vector3f(2.0f, 2.0f, 0.0f));

    ASSERT_NE(player, nullptr);
    ASSERT_TRUE(module.addPlayer(player, Ego::Input::InputDevice::DeviceList[0]));

    auto [passage, passageId] = addPassage(module);
    ASSERT_NE(passage, nullptr);
    EXPECT_EQ(passageId, 0);

    passage->setMusic(11);
    module.checkPassageMusic();

    ASSERT_EQ(audioSystem.playedMusicIds.size(), 1u);
    EXPECT_EQ(audioSystem.playedMusicIds.front().musicID, 11);
    EXPECT_EQ(audioSystem.playedMusicIds.front().fadeTime, 0u);

    audioSystem.reset();
    player->requestTerminate();
    module.checkPassageMusic();
    EXPECT_TRUE(audioSystem.playedMusicIds.empty());
}

TEST_F(ScriptActionFunctionsFixture, PlaySoundLoopedAndStopSoundUseInstalledAudioSystem)
{
    auto& module = beginActiveTestModule();
    const auto [actor, soundIndex] = makeSoundingObject(module, 5711);

    ASSERT_NE(actor, nullptr);
    ASSERT_GE(soundIndex, 0);

    script_state_t state;
    state.argument = soundIndex;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_PlaySoundLooped(state, self));
    ASSERT_EQ(audioSystem.loopedSounds.size(), 1u);
    EXPECT_EQ(audioSystem.loopedSounds.front().soundID, actor->getProfile()->getSoundID(soundIndex));
    EXPECT_EQ(audioSystem.loopedSounds.front().ownerRef, actor->getObjRef());

    state.argument = 999;
    EXPECT_TRUE(scr_PlaySoundLooped(state, self));
    ASSERT_EQ(audioSystem.stoppedLoopSounds.size(), 1u);
    EXPECT_EQ(audioSystem.stoppedLoopSounds.front().ownerRef, actor->getObjRef());
    EXPECT_EQ(audioSystem.stoppedLoopSounds.front().soundID, INVALID_SOUND_ID);

    state.argument = soundIndex;
    EXPECT_TRUE(scr_StopSound(state, self));
    ASSERT_EQ(audioSystem.stoppedLoopSounds.size(), 2u);
    EXPECT_EQ(audioSystem.stoppedLoopSounds.back().ownerRef, actor->getObjRef());
    EXPECT_EQ(audioSystem.stoppedLoopSounds.back().soundID, actor->getProfile()->getSoundID(soundIndex));
}

TEST_F(ScriptActionFunctionsFixture, PlaySoundVolumeUsesInstalledAudioSystemAndSkipsNonPositiveDistance)
{
    auto& module = beginActiveTestModule();
    const auto [actor, soundIndex] = makeSoundingObject(module, 5716);

    ASSERT_NE(actor, nullptr);
    ASSERT_GE(soundIndex, 0);

    script_state_t state;
    state.argument = soundIndex;
    state.distance = 75;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_PlaySoundVolume(state, self));
    ASSERT_EQ(audioSystem.playedSounds.size(), 1u);
    EXPECT_EQ(audioSystem.playedSounds.front().soundID, actor->getProfile()->getSoundID(soundIndex));

    audioSystem.reset();
    state.distance = 0;
    EXPECT_TRUE(scr_PlaySoundVolume(state, self));
    EXPECT_TRUE(audioSystem.playedSounds.empty());
}

TEST_F(ScriptActionFunctionsFixture, PlayFullSoundPlayMusicAndStopMusicUseInstalledAudioSystem)
{
    auto& module = beginActiveTestModule();
    const auto [actor, soundIndex] = makeSoundingObject(module, 5721);

    ASSERT_NE(actor, nullptr);
    ASSERT_GE(soundIndex, 0);

    ai_state_t self = makeScriptSelf(actor);

    script_state_t fullSoundState;
    fullSoundState.argument = soundIndex;
    EXPECT_TRUE(scr_PlayFullSound(fullSoundState, self));
    ASSERT_EQ(audioSystem.playedFullSounds.size(), 1u);
    EXPECT_EQ(audioSystem.playedFullSounds.front(), actor->getProfile()->getSoundID(soundIndex));

    script_state_t musicState;
    musicState.argument = 7;
    musicState.distance = 120;
    EXPECT_TRUE(scr_PlayMusic(musicState, self));
    ASSERT_EQ(audioSystem.playedMusicIds.size(), 1u);
    EXPECT_EQ(audioSystem.playedMusicIds.front().musicID, 7);
    EXPECT_EQ(audioSystem.playedMusicIds.front().fadeTime, 120);

    script_state_t stopState;
    EXPECT_TRUE(scr_StopMusic(stopState, self));
    EXPECT_EQ(audioSystem.stopMusicCalls, 1);
}

TEST_F(ScriptActionFunctionsFixture, QuitModuleFadesInstalledAudioSystem)
{
    beginActiveTestModule();
    auto& session = GameSessionContext::get();

    ASSERT_TRUE(session.hasActiveModule());
    audioSystem.reset();

    session.quitModule();

    EXPECT_FALSE(session.hasActiveModule());
    EXPECT_EQ(audioSystem.fadeAllCalls, 1);
}

TEST_F(ScriptActionFunctionsFixture, DoActionUsesAnimationRoleForSuccessAndBlockedState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5731);
    ASSERT_NE(actor, nullptr);

    const ModelAction action = findValidAction(actor, {ACTION_WA, ACTION_WB, ACTION_WC, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(action, ACTION_COUNT);

    script_state_t state;
    state.argument = static_cast<int>(action);
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_DoAction(state, self));
    EXPECT_EQ(actor->getCurrentAnimation(), action);

    actor->inst._canBeInterrupted = false;
    state.argument = static_cast<int>(findValidAction(actor, {ACTION_DA, ACTION_DB, ACTION_DC, ACTION_WA, ACTION_WB}, action));
    ASSERT_NE(state.argument, ACTION_COUNT);
    EXPECT_FALSE(scr_DoAction(state, self));
}

TEST_F(ScriptActionFunctionsFixture, TargetAndChildActionHelpersUseAnimationRoleAndPreserveDeadTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5741);
    auto target = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5742);
    auto child = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5743);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(child, nullptr);

    const ModelAction targetAction = findValidAction(target, {ACTION_WA, ACTION_WB, ACTION_DA, ACTION_DB});
    const ModelAction childAction = findValidAction(child, {ACTION_WB, ACTION_WC, ACTION_DA, ACTION_DB});
    ASSERT_NE(targetAction, ACTION_COUNT);
    ASSERT_NE(childAction, ACTION_COUNT);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    state.argument = static_cast<int>(targetAction);
    EXPECT_TRUE(scr_TargetDoAction(state, self));
    EXPECT_EQ(target->getCurrentAnimation(), targetAction);

    target->_isAlive = false;
    EXPECT_FALSE(scr_TargetDoAction(state, self));
    target->_isAlive = true;

    self.child = child->getObjRef();
    state.argument = static_cast<int>(childAction);
    EXPECT_TRUE(scr_ChildDoActionOverride(state, self));
    EXPECT_EQ(child->getCurrentAnimation(), childAction);
}

TEST_F(ScriptActionFunctionsFixture, TargetDoActionSetFrameSnapsInterpolationToFirstFrame)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5751);
    auto target = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5752);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    const ModelAction currentAction = findValidAction(target, {ACTION_WC, ACTION_WA, ACTION_DB, ACTION_DC});
    const ModelAction nextAction = findValidAction(target, {ACTION_DA, ACTION_DB, ACTION_DC, ACTION_WA}, currentAction);
    ASSERT_NE(currentAction, ACTION_COUNT);
    ASSERT_NE(nextAction, ACTION_COUNT);

    const auto& model = target->inst.getModelDescriptor();
    target->inst._currentAnimation = currentAction;
    target->inst._nextAnimation = ACTION_WB;
    target->inst._canBeInterrupted = false;
    target->inst._sourceFrameIndex = model->getFirstFrame(currentAction);
    target->inst._targetFrameIndex = model->getLastFrame(currentAction);
    target->inst._animationProgressInteger = 3;
    target->inst._animationProgress = 0.75f;

    script_state_t state;
    state.argument = static_cast<int>(nextAction);
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_TargetDoActionSetFrame(state, self));
    EXPECT_EQ(target->getCurrentAnimation(), nextAction);
    EXPECT_EQ(target->inst._sourceFrameIndex, model->getFirstFrame(nextAction));
    EXPECT_EQ(target->inst._targetFrameIndex, model->getFirstFrame(nextAction));
    EXPECT_EQ(target->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(target->inst._animationProgress, 0.0f);
}

TEST_F(ScriptActionFunctionsFixture, CorrectActionForHandUsesAttachmentSlotBands)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5761);
    auto actor = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5762);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(actor, nullptr);

    holder->setHeldObject(SLOT_LEFT, actor->getObjRef());
    actor->setHolderRef(holder->getObjRef());

    script_state_t state;
    state.argument = ACTION_DA;
    ai_state_t self = makeScriptSelf(actor);

    actor->setAttachmentSlot(SLOT_LEFT);
    EXPECT_TRUE(scr_CorrectActionForHand(state, self));
    EXPECT_GE(state.argument, ACTION_DA);
    EXPECT_LE(state.argument, ACTION_DA + 1);

    state.argument = ACTION_DA;
    actor->setAttachmentSlot(SLOT_RIGHT);
    EXPECT_TRUE(scr_CorrectActionForHand(state, self));
    EXPECT_GE(state.argument, ACTION_DA + 2);
    EXPECT_LE(state.argument, ACTION_DA + 3);

    state.argument = ACTION_DA;
    actor->setHolderRef(ObjectRef::Invalid);
    EXPECT_TRUE(scr_CorrectActionForHand(state, self));
    EXPECT_EQ(state.argument, ACTION_DA);
}

TEST_F(ScriptActionFunctionsFixture, VisualIdentityHelpersPreserveShiftFlagAndSparkleSemantics)
{
    constexpr int blueSparkle = 4;
    constexpr int colorMax = 6;

    auto& module = beginActiveTestModule();
    const ObjectProfileRef sharedProfile = loadProfile("mp_objects/follower.obj", 5763);
    ASSERT_NE(sharedProfile, ObjectProfileRef::Invalid);

    auto actor = module.spawnObject(Ego::Vector3f(64.0f, 64.0f, 0.0f), sharedProfile,
                                    static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
    auto peer = module.spawnObject(Ego::Vector3f(96.0f, 64.0f, 0.0f), sharedProfile,
                                   static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
    auto other = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5765);
    auto terminatedPeer = module.spawnObject(Ego::Vector3f(128.0f, 64.0f, 0.0f), sharedProfile,
                                             static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(peer, nullptr);
    ASSERT_NE(other, nullptr);
    ASSERT_NE(terminatedPeer, nullptr);

    actor->setNameKnown(false);
    actor->setAmmoKnown(false);
    actor->setSparkle(3);
    peer->setNameKnown(false);
    other->setNameKnown(false);
    terminatedPeer->setNameKnown(false);
    terminatedPeer->requestTerminate();
    ASSERT_TRUE(terminatedPeer->isTerminated());

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    state.argument = -4;
    EXPECT_TRUE(scr_SetRedShift(state, self));
    EXPECT_FLOAT_EQ(actor->getBaseAttribute(Ego::Attribute::RED_SHIFT), 0.0f);

    state.argument = 99;
    EXPECT_TRUE(scr_SetGreenShift(state, self));
    EXPECT_FLOAT_EQ(actor->getBaseAttribute(Ego::Attribute::GREEN_SHIFT), 6.0f);

    state.argument = 5;
    EXPECT_TRUE(scr_SetBlueShift(state, self));
    EXPECT_FLOAT_EQ(actor->getBaseAttribute(Ego::Attribute::BLUE_SHIFT), 5.0f);

    state.argument = 117;
    EXPECT_TRUE(scr_SetLight(state, self));
    EXPECT_EQ(actor->getLight(), 117);

    state.argument = 149;
    EXPECT_TRUE(scr_SetAlpha(state, self));
    EXPECT_EQ(actor->getAlpha(), 149);

    EXPECT_TRUE(scr_MakeNameKnown(state, self));
    EXPECT_TRUE(actor->isNameKnown());

    EXPECT_TRUE(scr_MakeAmmoKnown(state, self));
    EXPECT_TRUE(actor->isAmmoKnown());

    state.argument = -4;
    EXPECT_TRUE(scr_SparkleIcon(state, self));
    EXPECT_EQ(actor->getSparkle(), NOSPARKLE);

    state.argument = -1;
    EXPECT_TRUE(scr_SparkleIcon(state, self));
    EXPECT_EQ(actor->getSparkle(), NOSPARKLE);

    state.argument = blueSparkle;
    EXPECT_TRUE(scr_SparkleIcon(state, self));
    EXPECT_EQ(actor->getSparkle(), blueSparkle);

    state.argument = colorMax;
    EXPECT_TRUE(scr_SparkleIcon(state, self));
    EXPECT_EQ(actor->getSparkle(), blueSparkle);

    EXPECT_TRUE(scr_UnsparkleIcon(state, self));
    EXPECT_EQ(actor->getSparkle(), NOSPARKLE);

    EXPECT_TRUE(scr_MakeNameUnknown(state, self));
    EXPECT_FALSE(actor->isNameKnown());

    EXPECT_TRUE(scr_MakeSimilarNamesKnown(state, self));
    EXPECT_FALSE(other->isNameKnown());
    EXPECT_FALSE(peer->isNameKnown());
    EXPECT_FALSE(terminatedPeer->isNameKnown());
}

TEST_F(ScriptActionFunctionsFixture, FlashTargetAndBlackTargetUseVisualRoleAndPreserveMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5767);
    auto target = makeObject(module, "mp_objects/follower.obj", 5768);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_FALSE(target->inst._vertexList.empty());

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    const int flashedLight = static_cast<int>(255 * idlib::fraction<float, 1, 255>());

    EXPECT_TRUE(scr_FlashTarget(state, self));
    EXPECT_EQ(target->getAmbientColour(), flashedLight);
    EXPECT_EQ(target->getVertex(0).color_dir, flashedLight);

    EXPECT_TRUE(scr_BlackTarget(state, self));
    EXPECT_EQ(target->getAmbientColour(), 0);
    EXPECT_EQ(target->getVertex(0).color_dir, 0);

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_FlashTarget(state, self));
    EXPECT_FALSE(scr_BlackTarget(state, self));
}

TEST_F(ScriptActionFunctionsFixture, TakePictureReturnsFalseWithoutInstalledUIManager)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5768);
    ASSERT_NE(actor, nullptr);
    ASSERT_EQ(EngineContext::get().tryUIManager(), nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    EXPECT_FALSE(scr_TakePicture(state, self));
}

TEST_F(ScriptActionFunctionsFixture, DrawBillboardRejectsInvalidMessageBeforeTouchingGraphics)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5769);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;
    state.argument = 9999;
    state.distance = 4;
    state.turn = 3;

    ASSERT_FALSE(actor->getProfile()->isValidMessageID(state.argument));
    EXPECT_FALSE(scr_DrawBillboard(state, self));
}

TEST_F(ScriptActionFunctionsFixture, SendMessageNearUsesInstalledCameraDistanceGate)
{
    auto& module = beginActiveTestModule();
    ScopedPlayingStateHarness playingStateHarness;
    auto actor = makeObject(module, "mp_objects/follower.obj", 5770);
    ASSERT_NE(actor, nullptr);
    const int messageId = static_cast<int>(actor->getProfile()->addMessage("near camera text"));
    ASSERT_TRUE(actor->getProfile()->isValidMessageID(messageId));

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;
    state.argument = messageId;

    const size_t initialMessageCount = playingStateHarness.messageTexts().size();

    cameraSystem.setSingleTrackPosition(actor->getOldPosition());
    EXPECT_TRUE(scr_SendMessageNear(state, self));
    const auto afterNear = playingStateHarness.messageTexts();
    ASSERT_EQ(afterNear.size(), initialMessageCount + 1);
    EXPECT_EQ(afterNear.back(), "Near camera text");

    cameraSystem.setSingleTrackPosition(actor->getOldPosition() + Ego::Vector3f(MSGDISTANCE * 2.0f, MSGDISTANCE * 2.0f, 0.0f));
    EXPECT_TRUE(scr_SendMessageNear(state, self));
    EXPECT_EQ(playingStateHarness.messageTexts().size(), initialMessageCount + 1);
}

TEST_F(ScriptActionFunctionsFixture, DrawBillboardUsesInstalledBillboardSystemForValidMessages)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 57701);
    ASSERT_NE(actor, nullptr);

    const int messageId = static_cast<int>(actor->getProfile()->addMessage("billboard text"));
    ASSERT_TRUE(actor->getProfile()->isValidMessageID(messageId));

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;
    state.argument = messageId;
    state.distance = 4;
    state.turn = COLOR_GREEN;

    EXPECT_TRUE(scr_DrawBillboard(state, self));
    ASSERT_EQ(billboardSystem.calls.size(), 1u);
    EXPECT_EQ(billboardSystem.calls.front().objectRef, actor->getObjRef());
    EXPECT_EQ(billboardSystem.calls.front().text, "billboard text");
    EXPECT_EQ(billboardSystem.calls.front().lifetime, 4);
    EXPECT_EQ(billboardSystem.calls.front().flags, Ego::Graphics::Billboard::Flags::Fade);
}

TEST_F(ScriptActionFunctionsFixture, DisplayChargeUsesPlayerOrHolderPlayerAndRejectsInvalidArguments)
{
    auto& module = beginActiveTestModule();
    auto player = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5771);
    auto heldItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 5772);
    ASSERT_NE(player, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(module.addPlayer(player, Ego::Input::InputDevice::DeviceList[0]));

    player->setHeldObject(SLOT_LEFT, heldItem->getObjRef());
    heldItem->setHolderRef(player->getObjRef());
    heldItem->setAttachmentSlot(SLOT_LEFT);

    script_state_t playerState;
    playerState.argument = 4;
    playerState.distance = 10;
    playerState.turn = 3;
    ai_state_t playerSelf = makeScriptSelf(player);

    EXPECT_TRUE(scr_DisplayCharge(playerState, playerSelf));
    const std::shared_ptr<Ego::Player>& playerEntry = module.getPlayer(player->getPlayerNumber());
    ASSERT_NE(playerEntry, nullptr);
    EXPECT_EQ(playerEntry->getBarCurrentCharge(), 4u);
    EXPECT_EQ(playerEntry->getBarMaxCharge(), 10u);
    EXPECT_EQ(playerEntry->getBarPipWidth(), 3u);

    script_state_t heldState;
    heldState.argument = 6;
    heldState.distance = 12;
    heldState.turn = 5;
    ai_state_t heldSelf = makeScriptSelf(heldItem);

    EXPECT_TRUE(scr_DisplayCharge(heldState, heldSelf));
    EXPECT_EQ(playerEntry->getBarCurrentCharge(), 6u);
    EXPECT_EQ(playerEntry->getBarMaxCharge(), 12u);
    EXPECT_EQ(playerEntry->getBarPipWidth(), 5u);

    script_state_t invalidState;
    invalidState.argument = -1;
    invalidState.distance = 0;
    ai_state_t invalidSelf = makeScriptSelf(player);

    EXPECT_FALSE(scr_DisplayCharge(invalidState, invalidSelf));

    auto nonPlayer = makeObject(module, "mp_objects/follower.obj", 5773);
    ASSERT_NE(nonPlayer, nullptr);

    script_state_t nonPlayerState;
    nonPlayerState.argument = 3;
    nonPlayerState.distance = 8;
    nonPlayerState.turn = 2;
    ai_state_t nonPlayerSelf = makeScriptSelf(nonPlayer);

    EXPECT_FALSE(scr_DisplayCharge(nonPlayerState, nonPlayerSelf));
}

} // namespace
