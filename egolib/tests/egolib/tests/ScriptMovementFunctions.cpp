#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#undef private
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Physics/PhysicalConstants.hpp"  // Ego::Physics::g_environment
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptMovementFunctionsFixture : public ::testing::Test
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
        opts.logPath = "/debug/script-movement-function-tests.log";
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

        auto object = module.getObjectHandler().insert(profile);
        if (object != nullptr)
        {
            object->setPosition(position);
        }

        return object;
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

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(ObjectRef::Invalid);
        return self;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptMovementFunctionsFixture::s_runtime;

TEST_F(ScriptMovementFunctionsFixture, TurnModeHeightAndFlyHeightHelpersMutateSelfThroughMovementRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5601);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    EXPECT_TRUE(scr_SetTurnModeToWatch(state, self));
    EXPECT_EQ(actor->getTurnMode(), TURNMODE_WATCH);

    EXPECT_TRUE(scr_SetTurnModeToSpin(state, self));
    EXPECT_EQ(actor->getTurnMode(), TURNMODE_SPIN);

    EXPECT_TRUE(scr_SetTurnModeToVelocity(state, self));
    EXPECT_EQ(actor->getTurnMode(), TURNMODE_VELOCITY);

    EXPECT_TRUE(scr_SetTurnModeToWatchTarget(state, self));
    EXPECT_EQ(actor->getTurnMode(), TURNMODE_WATCHTARGET);

    state.argument = 17;
    EXPECT_TRUE(scr_SetBumpHeight(state, self));

    state.argument = 0;
    EXPECT_TRUE(scr_GetBumpHeight(state, self));
    EXPECT_EQ(state.argument, static_cast<int>(actor->getCurrentBump().height));

    const float initialSize = actor->getCurrentBump().size;
    state.argument = 33;
    EXPECT_TRUE(scr_SetBumpSize(state, self));
    EXPECT_FLOAT_EQ(actor->getCurrentBump().size, initialSize);

    state.argument = 12;
    EXPECT_TRUE(scr_SetFlyHeight(state, self));
    EXPECT_FLOAT_EQ(actor->getBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT), 12.0f);
}

TEST_F(ScriptMovementFunctionsFixture, LatchHelpersPublishButtonPressesForSelfAndTarget)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5602);
    auto target = makeObject(module, "mp_objects/follower.obj", 5603);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    state.argument = LATCHBUTTON_LEFT;
    EXPECT_TRUE(scr_PressLatchButton(state, self));
    EXPECT_TRUE(actor->_inputLatchesPressed[LATCHBUTTON_LEFT]);

    state.argument = LATCHBUTTON_RIGHT;
    EXPECT_TRUE(scr_PressTargetLatchButton(state, self));
    EXPECT_TRUE(target->_inputLatchesPressed[LATCHBUTTON_RIGHT]);
}

TEST_F(ScriptMovementFunctionsFixture, TeleportHelpersMoveSelfAndTargetAndPreserveMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5604, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto target = makeObject(module, "mp_objects/follower.obj", 5605, Ego::Vector3f(72.0f, 72.0f, 0.0f));
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setFacingZ(Facing(444));

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    state.x = 72;
    state.y = 72;
    state.distance = 0;
    state.turn = 222;
    EXPECT_TRUE(scr_TeleportTarget(state, self));
    EXPECT_FLOAT_EQ(target->getPosX(), 72.0f);
    EXPECT_FLOAT_EQ(target->getPosY(), 72.0f);
    EXPECT_FLOAT_EQ(target->getPosZ(), 0.0f);
    EXPECT_EQ(target->getFacingZ(), Facing(222));

    state.x = 64;
    state.y = 64;
    EXPECT_TRUE(scr_Teleport(state, self));
    EXPECT_FLOAT_EQ(actor->getPosX(), 64.0f);
    EXPECT_FLOAT_EQ(actor->getPosY(), 64.0f);
    EXPECT_FLOAT_EQ(actor->getPosZ(), 0.0f);
    EXPECT_EQ(actor->getFacingZ(), Facing(444));

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_TeleportTarget(state, self));
}

TEST_F(ScriptMovementFunctionsFixture, VelocityHelpersApplyTargetAccelerationAndPositiveZClamp)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5606);
    auto target = makeObject(module, "mp_objects/follower.obj", 5607);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    target->setVelocity(Ego::Vector3f(3.0f, 4.0f, 5.0f));
    EXPECT_TRUE(scr_StopTargetMovement(state, self));
    EXPECT_FLOAT_EQ(target->getVelocity().x(), 0.0f);
    EXPECT_FLOAT_EQ(target->getVelocity().y(), 0.0f);
    EXPECT_FLOAT_EQ(target->getVelocity().z(), Ego::Physics::g_environment.gravity);

    target->setVelocity(Ego::Vector3f(1.0f, 2.0f, 0.0f));
    state.x = 6;
    state.y = 7;
    EXPECT_TRUE(scr_AccelerateTarget(state, self));
    EXPECT_FLOAT_EQ(target->getVelocity().x(), 7.0f);
    EXPECT_FLOAT_EQ(target->getVelocity().y(), 9.0f);
    EXPECT_FLOAT_EQ(target->getVelocity().z(), 0.0f);

    actor->setVelocity(Ego::Vector3f(0.0f, 0.0f, 1.0f));
    state.argument = 50;
    EXPECT_TRUE(scr_AccelerateUp(state, self));
    EXPECT_FLOAT_EQ(actor->getVelocity().z(), 1.5f);

    target->setVelocity(Ego::Vector3f(0.0f, 0.0f, 1.0f));
    state.argument = 25;
    EXPECT_TRUE(scr_AccelerateTargetUp(state, self));
    EXPECT_FLOAT_EQ(target->getVelocity().z(), 1.25f);
}

TEST_F(ScriptMovementFunctionsFixture, ReloadAndShadowHelpersRoundTripThroughMovementRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5608);
    auto target = makeObject(module, "mp_objects/follower.obj", 5609);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());
    script_state_t state;

    state.argument = 15;
    EXPECT_TRUE(scr_SetReloadTime(state, self));
    EXPECT_EQ(actor->getReloadTimer(), 15);

    state.argument = -1;
    EXPECT_TRUE(scr_SetTargetReloadTime(state, self));
    EXPECT_EQ(target->getReloadTimer(), 0);

    state.argument = 13;
    EXPECT_TRUE(scr_SetTargetReloadTime(state, self));
    EXPECT_EQ(target->getReloadTimer(), 13);

    state.argument = 9;
    EXPECT_TRUE(scr_SetShadowSize(state, self));
    EXPECT_EQ(actor->getSavedShadowSize(), 9u);
    EXPECT_EQ(actor->getShadowSize(), static_cast<uint32_t>(9.0f * actor->getFat()));
}

TEST_F(ScriptMovementFunctionsFixture, SetFramePublishesEncodedDaAnimationFrame)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/monsters/zombi.obj", 5610);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(actor->inst.getModelDescriptor(), nullptr);
    ASSERT_TRUE(actor->inst.getModelDescriptor()->isActionValid(ACTION_DA));

    const auto& model = actor->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(ACTION_DA);
    const int lastFrame = model->getLastFrame(ACTION_DA);
    const int frameAlong = std::min(1, std::max(0, lastFrame - firstFrame));
    constexpr int interpolationStep = 2;

    actor->inst._sourceFrameIndex = firstFrame;
    actor->inst._targetFrameIndex = lastFrame;
    actor->inst._animationProgressInteger = 3;
    actor->inst._animationProgress = 0.75f;

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;
    state.argument = (frameAlong << 2) | interpolationStep;

    EXPECT_TRUE(scr_SetFrame(state, self));
    EXPECT_EQ(actor->getCurrentAnimation(), ACTION_DA);
    EXPECT_EQ(actor->inst._targetFrameIndex, std::min(firstFrame + frameAlong, lastFrame));
    EXPECT_EQ(actor->inst._animationProgressInteger, interpolationStep);
    EXPECT_FLOAT_EQ(actor->inst._animationProgress, 0.5f);
}

TEST_F(ScriptMovementFunctionsFixture, FindPathUsesResolvedSelfPhysicalStateAndPreservesInvalidSelfFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5611, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    waypoint_list_t::clear(self.wp_lst);

    script_state_t state;
    state.x = actor->getPosX() + Info<float>::Grid::Size();
    state.y = actor->getPosY();

    EXPECT_TRUE(scr_FindPath(state, self));
    EXPECT_FALSE(waypoint_list_t::empty(self.wp_lst));

    ai_state_t invalidSelf = makeScriptSelf(nullptr);
    waypoint_list_t::clear(invalidSelf.wp_lst);
    waypoint_list_t::push(invalidSelf.wp_lst, 12, 34);

    EXPECT_FALSE(scr_FindPath(state, invalidSelf));
    EXPECT_FALSE(waypoint_list_t::empty(invalidSelf.wp_lst));
}

} // namespace
