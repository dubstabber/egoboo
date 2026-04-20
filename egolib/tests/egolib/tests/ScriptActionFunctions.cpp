#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

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
        context.installAudioSystem(audioSystem);

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

    void moveObjectToOldZ(const std::shared_ptr<Object>& object, float oldZ) const
    {
        ASSERT_NE(object, nullptr);
        const Ego::Vector3f position = object->getPosition();
        EXPECT_TRUE(object->setPosition(position.x(), position.y(), oldZ));
        EXPECT_TRUE(object->setPosition(position.x(), position.y(), oldZ + 1.0f));
    }

    StubAudioSystem audioSystem;
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
}

} // namespace
