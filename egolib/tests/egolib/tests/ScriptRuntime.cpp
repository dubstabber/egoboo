#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Script/script.h"
#include "egolib/vfs.h"

namespace
{

class ScriptRuntimeFixture : public ::testing::Test
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
        opts.randomSeed = 67;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-runtime-tests.log";
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

    std::shared_ptr<Object> makeObject(GameModule& module,
                                       const std::string& profilePath,
                                       int slot,
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
        const bool began = session.beginModule(module, 67);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    static void clearOrderState(Object& object)
    {
        auto& aiState = Ego::Script::runtimeState(object);
        aiState.order_value = 0;
        aiState.order_counter = 0;
        aiState.alert = 0;
    }

    static void flushSpawnedObjects(GameModule& module)
    {
        auto objects = module.getObjectHandler().iterator();
        (void)objects;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptRuntimeFixture::s_runtime;

TEST_F(ScriptRuntimeFixture, RunCharacterScriptResetsInvisibleTargetAndAppliesWaypointVelocity)
{
    auto& module = beginActiveTestModule();
    module.getObjectHandler().clear();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5801, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto target = makeObject(module, "mp_objects/follower.obj", 5802, Ego::Vector3f(128.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_FALSE(actor->isPlayer());

    actor->setInvincible(false);
    actor->setAlpha(200);
    actor->setLight(200);
    target->setAlpha(0);
    target->setLight(0);
    ASSERT_FALSE(actor->canSeeObject(target));

    auto& aiScript = actor->getProfile()->getAIScript();
    aiScript._name = "runtime-test";
    aiScript._instructions.clear();

    auto& aiState = Ego::Script::runtimeState(*actor);
    aiState.setTarget(target->getObjRef());
    aiState.wp_valid = false;
    waypoint_list_t::clear(aiState.wp_lst);
    waypoint_list_t::push(aiState.wp_lst, actor->getPosX() + Info<float>::Grid::Size(), actor->getPosY());

    scr_run_chr_script(actor.get());

    EXPECT_EQ(aiState.getTarget(), actor->getObjRef());
    EXPECT_FLOAT_EQ(actor->getDesiredVelocity().x(), 1.0f);
    EXPECT_FLOAT_EQ(actor->getDesiredVelocity().y(), 0.0f);
}

TEST_F(ScriptRuntimeFixture, SetAlertsPublishesLastWaypointAlertForNonEquipmentObjects)
{
    auto& module = beginActiveTestModule();
    module.getObjectHandler().clear();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5811);

    ASSERT_NE(actor, nullptr);
    ASSERT_FALSE(actor->getProfile()->isEquipment());

    auto& actorState = Ego::Script::runtimeState(*actor);
    actorState.alert = 0;
    actorState.wp_valid = false;
    waypoint_list_t::clear(actorState.wp_lst);
    waypoint_list_t::push(actorState.wp_lst, actor->getPosX(), actor->getPosY());

    set_alerts(actor->getObjRef());

    EXPECT_TRUE(HAS_SOME_BITS(actorState.alert, ALERTIF_ATWAYPOINT));
    EXPECT_FALSE(HAS_SOME_BITS(actorState.alert, ALERTIF_ATLASTWAYPOINT));

    set_alerts(actor->getObjRef());

    EXPECT_TRUE(HAS_SOME_BITS(actorState.alert, ALERTIF_ATLASTWAYPOINT));
}

TEST_F(ScriptRuntimeFixture, RunOperandLeaderVariablesFallBackToSelfWhenTeamLeaderMissing)
{
    auto& module = beginActiveTestModule();
    module.getObjectHandler().clear();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5815,
                            Ego::Vector3f(96.0f, 160.0f, 0.0f));

    ASSERT_NE(actor, nullptr);

    constexpr TEAM_REF goodTeam = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    actor->setTeam(goodTeam);
    module.getTeamList()[goodTeam].setLeader(nullptr);
    EXPECT_EQ(module.getTeamList()[goodTeam].getLeader(), nullptr);

    auto& aiState = Ego::Script::runtimeState(*actor);
    aiState.setSelf(actor->getObjRef());

    script_state_t scriptState{};
    script_info_t script;
    const auto constantIndex = script._instructions.getConstantPool().getOrCreateConstant(Ego::Script::VARLEADERX);
    script._instructions.append(Instruction((static_cast<uint32_t>(Ego::Script::OPADD) << 27) | constantIndex));

    scriptState.operationsum = 0;
    scriptState.run_operand(aiState, script);
    EXPECT_EQ(static_cast<int32_t>(scriptState.operationsum), static_cast<int32_t>(actor->getPosX()));

    script._instructions.clear();
    const auto distanceIndex = script._instructions.getConstantPool().getOrCreateConstant(Ego::Script::VARLEADERDISTANCE);
    script._instructions.append(Instruction((static_cast<uint32_t>(Ego::Script::OPADD) << 27) | distanceIndex));

    scriptState.operationsum = 0;
    scriptState.run_operand(aiState, script);
    EXPECT_EQ(static_cast<int32_t>(scriptState.operationsum), 0x7FFFFFFF);
}

TEST_F(ScriptRuntimeFixture, RunOperandLeaderVariablesUseResolvedTeamLeaderWhenPresent)
{
    auto& module = beginActiveTestModule();
    module.getObjectHandler().clear();
    auto actor = makeObject(module, "mp_data/globalobjects/players/rogue.obj", 5816,
                            Ego::Vector3f(96.0f, 160.0f, 0.0f));
    auto leader = makeObject(module, "mp_objects/follower.obj", 5817,
                             Ego::Vector3f(224.0f, 160.0f, 0.0f));

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leader, nullptr);

    constexpr TEAM_REF goodTeam = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    actor->setTeam(goodTeam);
    leader->setTeam(goodTeam);
    module.getTeamList()[goodTeam].setLeader(leader);
    EXPECT_EQ(module.getTeamLeaderRef(goodTeam), leader->getObjRef());

    auto& aiState = Ego::Script::runtimeState(*actor);
    aiState.setSelf(actor->getObjRef());

    script_state_t scriptState{};
    script_info_t script;
    const auto xIndex = script._instructions.getConstantPool().getOrCreateConstant(Ego::Script::VARLEADERX);
    script._instructions.append(Instruction((static_cast<uint32_t>(Ego::Script::OPADD) << 27) | xIndex));

    scriptState.operationsum = 0;
    scriptState.run_operand(aiState, script);
    EXPECT_EQ(static_cast<int32_t>(scriptState.operationsum), static_cast<int32_t>(leader->getPosX()));

    script._instructions.clear();
    const auto distanceIndex = script._instructions.getConstantPool().getOrCreateConstant(Ego::Script::VARLEADERDISTANCE);
    script._instructions.append(Instruction((static_cast<uint32_t>(Ego::Script::OPADD) << 27) | distanceIndex));

    scriptState.operationsum = 0;
    scriptState.run_operand(aiState, script);
    EXPECT_EQ(static_cast<int32_t>(scriptState.operationsum), 128);
}

TEST_F(ScriptRuntimeFixture, IssueOrderPublishesToLiveSameTeamObjectsOnly)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 5821);
    auto teammate = makeObject(module, "mp_objects/follower.obj", 5822);
    auto outsider = makeObject(module, "mp_objects/follower.obj", 5823);
    auto terminated = makeObject(module, "mp_objects/follower.obj", 5824);

    ASSERT_NE(caller, nullptr);
    ASSERT_NE(teammate, nullptr);
    ASSERT_NE(outsider, nullptr);
    ASSERT_NE(terminated, nullptr);

    flushSpawnedObjects(module);

    constexpr TEAM_REF goodTeam = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    constexpr TEAM_REF evilTeam = static_cast<TEAM_REF>(Team::TEAM_EVIL);
    caller->setTeam(goodTeam);
    teammate->setTeam(goodTeam);
    outsider->setTeam(evilTeam);
    terminated->setTeam(goodTeam);

    clearOrderState(*caller);
    clearOrderState(*teammate);
    clearOrderState(*outsider);
    clearOrderState(*terminated);
    terminated->requestTerminate();

    issue_order(caller->getObjRef(), 77);

    const auto& callerState = Ego::Script::runtimeState(*caller);
    const auto& teammateState = Ego::Script::runtimeState(*teammate);
    const auto& outsiderState = Ego::Script::runtimeState(*outsider);
    const auto& terminatedState = Ego::Script::runtimeState(*terminated);
    EXPECT_EQ(callerState.order_value, 77u);
    EXPECT_TRUE(HAS_SOME_BITS(callerState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(teammateState.order_value, 77u);
    EXPECT_TRUE(HAS_SOME_BITS(teammateState.alert, ALERTIF_ORDERED));
    EXPECT_LT(callerState.order_counter, teammateState.order_counter);
    EXPECT_EQ(outsiderState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(outsiderState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(terminatedState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(terminatedState.alert, ALERTIF_ORDERED));
}

TEST_F(ScriptRuntimeFixture, IssueOrderQuietlyIgnoresInvalidCallerRef)
{
    auto& module = beginActiveTestModule();
    auto teammate = makeObject(module, "mp_objects/follower.obj", 5825);

    ASSERT_NE(teammate, nullptr);

    flushSpawnedObjects(module);

    teammate->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    clearOrderState(*teammate);

    issue_order(ObjectRef::Invalid, 77);

    const auto& teammateState = Ego::Script::runtimeState(*teammate);
    EXPECT_EQ(teammateState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(teammateState.alert, ALERTIF_ORDERED));
}

TEST_F(ScriptRuntimeFixture, IssueSpecialOrderPublishesToMatchingSpecialIdsOnly)
{
    auto& module = beginActiveTestModule();
    auto matchingA = makeObject(module, "mp_objects/follower.obj", 5831);
    auto matchingB = makeObject(module, "mp_objects/follower.obj", 5832);
    auto nonMatching = makeObject(module, "mp_data/globalobjects/items/torch.obj", 5833);
    auto terminatedMatch = makeObject(module, "mp_objects/follower.obj", 5834);

    ASSERT_NE(matchingA, nullptr);
    ASSERT_NE(matchingB, nullptr);
    ASSERT_NE(nonMatching, nullptr);
    ASSERT_NE(terminatedMatch, nullptr);

    flushSpawnedObjects(module);

    clearOrderState(*matchingA);
    clearOrderState(*matchingB);
    clearOrderState(*nonMatching);
    clearOrderState(*terminatedMatch);

    const IDSZ2 specialId = matchingA->getProfile()->getIDSZ(IDSZ_SPECIAL);
    ASSERT_NE(nonMatching->getProfile()->getIDSZ(IDSZ_SPECIAL), specialId);
    terminatedMatch->requestTerminate();

    issue_special_order(91, specialId);

    const auto& matchingAState = Ego::Script::runtimeState(*matchingA);
    const auto& matchingBState = Ego::Script::runtimeState(*matchingB);
    const auto& nonMatchingState = Ego::Script::runtimeState(*nonMatching);
    const auto& terminatedMatchState = Ego::Script::runtimeState(*terminatedMatch);
    EXPECT_EQ(matchingAState.order_value, 91u);
    EXPECT_TRUE(HAS_SOME_BITS(matchingAState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(matchingBState.order_value, 91u);
    EXPECT_TRUE(HAS_SOME_BITS(matchingBState.alert, ALERTIF_ORDERED));
    EXPECT_LT(matchingAState.order_counter, matchingBState.order_counter);
    EXPECT_EQ(terminatedMatchState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(terminatedMatchState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(nonMatchingState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(nonMatchingState.alert, ALERTIF_ORDERED));
}

} // namespace
