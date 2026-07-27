/// @file ScriptTargetSupportFunctions.cpp
/// @brief Characterization tests for the 12 previously-untested scr_* functions of the
///        script target family:
///          * script_functions_target.c:        scr_IfDistanceIsMoreThanTurn,
///                                               scr_IfTargetIsOldTarget, scr_IfTargetIsSelf
///          * script_functions_target_orders.c: scr_CreateOrder, scr_GetAttackTurn,
///                                               scr_GetDamageType, scr_IssueOrder,
///                                               scr_OrderSpecialID, scr_SetOldTarget
///          * script_functions_target_select.c: scr_SetOwnerToTarget,
///                                               scr_SetTargetToNearestLifeform,
///                                               scr_SetTargetToSelf
///        These functions previously had no direct test reference.

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/script_implementation.h"
#undef private
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptTargetSupportFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 81;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-target-support-function-tests.log";
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

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot,
                                       const Ego::Vector3f& position = Ego::Vector3f(64.0f, 64.0f, 0.0f)) const
    {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
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
        const bool began = session.beginModule(module, 81);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject, const std::shared_ptr<Object>& targetObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(targetObject ? targetObject->getObjRef() : ObjectRef::Invalid);
        return self;
    }

    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
    }

    static void clearOrderState(Object& object)
    {
        auto& aiState = Ego::Script::runtimeState(object);
        aiState.order_value = 0;
        aiState.order_counter = 0;
        aiState.alert = 0;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptTargetSupportFunctionsFixture::s_runtime;

//--------------------------------------------------------------------------------------------
// script_functions_target.c
//--------------------------------------------------------------------------------------------

TEST_F(ScriptTargetSupportFunctionsFixture, IfDistanceIsMoreThanTurnComparesRawSignedRegistersStrictly)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6701);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    state.distance = 5;
    state.turn = 5;
    EXPECT_FALSE(scr_IfDistanceIsMoreThanTurn(state, self)); // strict >, equal registers -> false

    state.distance = 6;
    state.turn = 5;
    EXPECT_TRUE(scr_IfDistanceIsMoreThanTurn(state, self));

    // Raw signed int comparison -- no FACING wraparound even though tmpturn semantically holds
    // a facing value elsewhere in the script VM.
    state.distance = -1;
    state.turn = -2;
    EXPECT_TRUE(scr_IfDistanceIsMoreThanTurn(state, self));

    state.distance = 0;
    state.turn = -1;
    EXPECT_TRUE(scr_IfDistanceIsMoreThanTurn(state, self));

    state.distance = -5;
    state.turn = 0;
    EXPECT_FALSE(scr_IfDistanceIsMoreThanTurn(state, self));

    // The target is completely ignored -- no target resolution occurs at all.
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
}

TEST_F(ScriptTargetSupportFunctionsFixture, IfTargetIsOldTargetTreatsDefaultUnsetTargetAndOldTargetAsEqual)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6702);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    ASSERT_EQ(self.getTarget(), ObjectRef::Invalid);
    ASSERT_EQ(self.getOldTarget(), ObjectRef::Invalid);

    // Quirk pinned deliberately: "no target ever set" reads as "target is old target" because
    // ObjectRef::Invalid == ObjectRef::Invalid.
    EXPECT_TRUE(scr_IfTargetIsOldTarget(state, self));
}

TEST_F(ScriptTargetSupportFunctionsFixture, IfTargetIsOldTargetComparesRawRefValuesWithoutAnyLivenessCheck)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6703);
    auto targetA = makeObject(module, "mp_objects/follower.obj", 6704);
    auto targetB = makeObject(module, "mp_objects/follower.obj", 6705);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(targetA, nullptr);
    ASSERT_NE(targetB, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, targetA);

    self.setOldTarget(targetA->getObjRef());
    EXPECT_TRUE(scr_IfTargetIsOldTarget(state, self));

    self.setOldTarget(targetB->getObjRef());
    EXPECT_FALSE(scr_IfTargetIsOldTarget(state, self));

    // Quirk pinned deliberately: no liveness check on either ref -- a terminated target that
    // still equals the (also stale) old target still reads as "is old target".
    targetA->requestTerminate();
    self.setOldTarget(targetA->getObjRef());
    EXPECT_EQ(self.getTarget(), targetA->getObjRef());
    EXPECT_TRUE(scr_IfTargetIsOldTarget(state, self));
}

TEST_F(ScriptTargetSupportFunctionsFixture, IfTargetIsSelfComparesTargetRefAgainstSelfRefByRawValue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6706);
    auto other = makeObject(module, "mp_objects/follower.obj", 6707);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(other, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    self.setTarget(actor->getObjRef());
    EXPECT_TRUE(scr_IfTargetIsSelf(state, self));

    self.setTarget(other->getObjRef());
    EXPECT_FALSE(scr_IfTargetIsSelf(state, self));

    // Unlike IfTargetIsOldTarget's vacuous-truth quirk, the guard already proves self is a live
    // ref != Invalid, so an Invalid target can never equal self.
    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_IfTargetIsSelf(state, self));
}

TEST_F(ScriptTargetSupportFunctionsFixture, TargetStatePredicatesReturnFalseForUnresolvedSelfRegardlessOfOperands)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6708);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.distance = 100;
    state.turn = -100; // would satisfy distance > turn if the guard were skipped

    ai_state_t self = makeScriptSelf(actor, nullptr);
    // target and oldTarget are both default-constructed ObjectRef::Invalid here -- already
    // equal, so would satisfy IfTargetIsOldTarget's raw equality if the guard were skipped.
    self.setSelf(ObjectRef::Invalid);
    // self.getSelf() is now also Invalid, matching the still-Invalid target -- would satisfy
    // IfTargetIsSelf's raw equality if the guard were skipped.

    EXPECT_FALSE(scr_IfDistanceIsMoreThanTurn(state, self));
    EXPECT_FALSE(scr_IfTargetIsOldTarget(state, self));
    EXPECT_FALSE(scr_IfTargetIsSelf(state, self));

    EXPECT_EQ(state.distance, 100);
    EXPECT_EQ(state.turn, -100);
}

TEST_F(ScriptTargetSupportFunctionsFixture, TargetStatePredicatesReturnFalseForTerminatedSelfEvenThoughRefStillExistsInHandler)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6709);
    ASSERT_NE(actor, nullptr);
    actor->requestTerminate();

    script_state_t state;
    state.distance = 5;
    state.turn = 1; // 5 > 1 would be true if the guard were skipped

    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.setTarget(actor->getObjRef());    // target == self ref -- would satisfy IfTargetIsSelf
    self.setOldTarget(actor->getObjRef()); // target == oldTarget -- would satisfy IfTargetIsOldTarget

    // ObjectHandler::exists() returns false for terminated objects even though get() would
    // still resolve the ref, so the self guard fails for all three predicates.
    EXPECT_FALSE(scr_IfDistanceIsMoreThanTurn(state, self));
    EXPECT_FALSE(scr_IfTargetIsOldTarget(state, self));
    EXPECT_FALSE(scr_IfTargetIsSelf(state, self));
}

//--------------------------------------------------------------------------------------------
// script_functions_target_orders.c
//--------------------------------------------------------------------------------------------

TEST_F(ScriptTargetSupportFunctionsFixture, CreateOrderTruncatesTargetByteRegardlessOfTargetRefValue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6710);
    auto targetA = makeObject(module, "mp_objects/follower.obj", 6711);
    auto targetB = makeObject(module, "mp_objects/follower.obj", 6712);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(targetA, nullptr);
    ASSERT_NE(targetB, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, targetA);

    constexpr int x = 3 * 64; // (x >> 6) == 3, low 2 bits survive fully
    constexpr int y = 5 * 64; // (y >> 6) == 5
    constexpr int arg = 9;
    constexpr int expectedTruncatedArgument = ((x >> 6) & 0x3) << 14 | ((y >> 6) & 0x03FF) << 4 | (arg & 0x000F);
    ASSERT_EQ(expectedTruncatedArgument, 0xC059); // 49241 -- concrete pinned value

    state.x = x;
    state.y = y;
    state.argument = arg;
    EXPECT_TRUE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, expectedTruncatedArgument);

    // Bug pinned deliberately: the target byte ((ref & 0xFF) << 24) is entirely lost to the
    // uint16_t accumulator, so swapping the target -- or clearing it to Invalid -- produces the
    // IDENTICAL packed result. CreateOrder never validates the target either.
    self.setTarget(targetB->getObjRef());
    state.x = x;
    state.y = y;
    state.argument = arg;
    EXPECT_TRUE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, expectedTruncatedArgument);

    self.setTarget(ObjectRef::Invalid);
    state.x = x;
    state.y = y;
    state.argument = arg;
    EXPECT_TRUE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, expectedTruncatedArgument);
}

TEST_F(ScriptTargetSupportFunctionsFixture, CreateOrderKeepsOnlyTheLowTwoBitsOfTheXField)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6713);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // (x >> 6) == 8 == 0b1000; its low 2 bits are 0, so the entire 10-bit x contribution vanishes.
    state.x = 8 * 64;
    state.y = 0;
    state.argument = 0;
    EXPECT_TRUE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, 0);

    // (x >> 6) == 3 == 0b011; its low 2 bits are 3, so it survives fully as bits 14-15.
    state.x = 3 * 64;
    state.y = 0;
    state.argument = 0;
    EXPECT_TRUE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, 3 << 14);
}

TEST_F(ScriptTargetSupportFunctionsFixture, GetAttackTurnPublishesRawDirectionLastValueAndAlwaysReturnsTrueEvenWhenZero)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6714);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // Default ai_state_t already carries directionlast == Facing(0); GetAttackTurn still
    // returns true even though the published turn ends up 0 (unlike the GrogTime/DazeTime
    // siblings, which gate their return value on the published number being nonzero).
    EXPECT_TRUE(scr_GetAttackTurn(state, self));
    EXPECT_EQ(state.turn, 0);

    self.directionlast = Facing(54321);
    EXPECT_TRUE(scr_GetAttackTurn(state, self));
    EXPECT_EQ(state.turn, 54321);

    self.directionlast = Facing(65535);
    EXPECT_TRUE(scr_GetAttackTurn(state, self));
    EXPECT_EQ(state.turn, 65535); // full uint16_t range, never sign-extended
}

TEST_F(ScriptTargetSupportFunctionsFixture, GetDamageTypePublishesLastDamageTypeIncludingSentinelDefault)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6715);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    // Default ai_state_t already carries damagetypelast == DamageType::DAMAGE_DIRECT (0xFF),
    // the "never hit yet" sentinel -- NOT a member of the 0-7 SLASH..ZAP range. GetDamageType
    // still returns true and publishes it verbatim.
    EXPECT_TRUE(scr_GetDamageType(state, self));
    EXPECT_EQ(state.argument, 255);

    self.damagetypelast = DamageType::DAMAGE_FIRE;
    EXPECT_TRUE(scr_GetDamageType(state, self));
    EXPECT_EQ(state.argument, static_cast<int>(DamageType::DAMAGE_FIRE));
}

TEST_F(ScriptTargetSupportFunctionsFixture, IssueOrderBroadcastsToLiveSameTeamMembersIncludingCallerAndSkipsOutsidersAndTerminated)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 6716);
    auto teammate = makeObject(module, "mp_objects/follower.obj", 6717);
    auto outsider = makeObject(module, "mp_objects/follower.obj", 6718);
    auto terminated = makeObject(module, "mp_objects/follower.obj", 6719);
    ASSERT_NE(caller, nullptr);
    ASSERT_NE(teammate, nullptr);
    ASSERT_NE(outsider, nullptr);
    ASSERT_NE(terminated, nullptr);

    flushObjectHandler(module);

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

    script_state_t state;
    state.argument = 77;
    ai_state_t self = makeScriptSelf(caller, nullptr);

    EXPECT_TRUE(scr_IssueOrder(state, self));

    const auto& callerState = Ego::Script::runtimeState(*caller);
    const auto& teammateState = Ego::Script::runtimeState(*teammate);
    const auto& outsiderState = Ego::Script::runtimeState(*outsider);
    const auto& terminatedState = Ego::Script::runtimeState(*terminated);

    // Quirk pinned deliberately: the caller itself is a valid recipient of its own order (same
    // team as itself), not just its teammates.
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

TEST_F(ScriptTargetSupportFunctionsFixture, IssueOrderOverwritesAlreadyOrderedRecipientAndStillConsumesCounterSlot)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 6720);
    auto teammateA = makeObject(module, "mp_objects/follower.obj", 6721);
    auto teammateB = makeObject(module, "mp_objects/follower.obj", 6722);
    ASSERT_NE(caller, nullptr);
    ASSERT_NE(teammateA, nullptr);
    ASSERT_NE(teammateB, nullptr);

    flushObjectHandler(module);

    constexpr TEAM_REF goodTeam = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    caller->setTeam(goodTeam);
    teammateA->setTeam(goodTeam);
    teammateB->setTeam(goodTeam);

    clearOrderState(*caller);
    clearOrderState(*teammateB);

    auto& teammateAState = Ego::Script::runtimeState(*teammateA);
    teammateAState.order_value = 999;
    teammateAState.order_counter = 42;
    teammateAState.alert = ALERTIF_ORDERED; // already-ordered before the new broadcast arrives

    script_state_t state;
    state.argument = 123;
    ai_state_t self = makeScriptSelf(caller, nullptr);

    EXPECT_TRUE(scr_IssueOrder(state, self));

    // Quirk pinned deliberately: add_order's "was this new" bool return is discarded by the
    // broadcast loop -- an already-ordered recipient still gets its order_value/order_counter
    // overwritten, and it still consumes a counter slot in iteration order.
    EXPECT_EQ(teammateAState.order_value, 123u);
    EXPECT_TRUE(HAS_SOME_BITS(teammateAState.alert, ALERTIF_ORDERED));
    EXPECT_NE(teammateAState.order_counter, 42);

    const auto& teammateBState = Ego::Script::runtimeState(*teammateB);
    EXPECT_EQ(teammateBState.order_value, 123u);
    EXPECT_GT(teammateBState.order_counter, teammateAState.order_counter);
}

TEST_F(ScriptTargetSupportFunctionsFixture, OrderSpecialIdBroadcastsToLiveMatchingProfilesRegardlessOfTeamIncludingSelf)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 6723);
    auto matchingA = makeObject(module, "mp_objects/follower.obj", 6724);
    auto matchingB = makeObject(module, "mp_objects/follower.obj", 6725);
    auto nonMatching = makeObject(module, "mp_data/globalobjects/items/torch.obj", 6726);
    auto terminatedMatch = makeObject(module, "mp_objects/follower.obj", 6727);

    ASSERT_NE(caller, nullptr);
    ASSERT_NE(matchingA, nullptr);
    ASSERT_NE(matchingB, nullptr);
    ASSERT_NE(nonMatching, nullptr);
    ASSERT_NE(terminatedMatch, nullptr);

    flushObjectHandler(module);

    // No team filter for special orders: put caller and matchingA/B on different teams to
    // prove team membership is irrelevant -- only the profile's [SPEC] IDSZ matters.
    caller->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    matchingA->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    matchingB->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    nonMatching->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    terminatedMatch->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));

    clearOrderState(*caller);
    clearOrderState(*matchingA);
    clearOrderState(*matchingB);
    clearOrderState(*nonMatching);
    clearOrderState(*terminatedMatch);

    const IDSZ2 specialId = caller->getProfile()->getIDSZ(IDSZ_SPECIAL);
    ASSERT_EQ(matchingA->getProfile()->getIDSZ(IDSZ_SPECIAL), specialId);
    ASSERT_NE(nonMatching->getProfile()->getIDSZ(IDSZ_SPECIAL), specialId);
    terminatedMatch->requestTerminate();

    script_state_t state;
    state.argument = 91;
    state.distance = static_cast<int>(specialId.toUint32());
    ai_state_t self = makeScriptSelf(caller, nullptr);

    EXPECT_TRUE(scr_OrderSpecialID(state, self));

    const auto& callerState = Ego::Script::runtimeState(*caller);
    const auto& matchingAState = Ego::Script::runtimeState(*matchingA);
    const auto& matchingBState = Ego::Script::runtimeState(*matchingB);
    const auto& nonMatchingState = Ego::Script::runtimeState(*nonMatching);
    const auto& terminatedMatchState = Ego::Script::runtimeState(*terminatedMatch);

    // Quirk pinned deliberately: the caller matches its own profile's special IDSZ, so it
    // receives its own order too -- there is no self-exclusion, unlike some target selectors.
    EXPECT_EQ(callerState.order_value, 91u);
    EXPECT_TRUE(HAS_SOME_BITS(callerState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(matchingAState.order_value, 91u);
    EXPECT_TRUE(HAS_SOME_BITS(matchingAState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(matchingBState.order_value, 91u);
    EXPECT_TRUE(HAS_SOME_BITS(matchingBState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(nonMatchingState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(nonMatchingState.alert, ALERTIF_ORDERED));
    EXPECT_EQ(terminatedMatchState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(terminatedMatchState.alert, ALERTIF_ORDERED));
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetOldTargetCopiesTargetVerbatimEvenWhenInvalidOrStale)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6728);
    auto target = makeObject(module, "mp_objects/follower.obj", 6729);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_SetOldTarget(state, self));
    EXPECT_EQ(self.getOldTarget(), target->getObjRef());

    self.setTarget(ObjectRef::Invalid);
    EXPECT_TRUE(scr_SetOldTarget(state, self));
    EXPECT_EQ(self.getOldTarget(), ObjectRef::Invalid);

    // Quirk pinned deliberately: no target validation at all -- a terminated/stale target ref
    // is copied into _oldTarget verbatim and the call still returns true.
    target->requestTerminate();
    self.setTarget(target->getObjRef());
    EXPECT_TRUE(scr_SetOldTarget(state, self));
    EXPECT_EQ(self.getOldTarget(), target->getObjRef());
}

TEST_F(ScriptTargetSupportFunctionsFixture, AllOrderFunctionsReturnFalseForUnresolvedSelfAndProduceNoSideEffects)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6730);
    auto other = makeObject(module, "mp_objects/follower.obj", 6731);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(other, nullptr);

    constexpr TEAM_REF teamRef = static_cast<TEAM_REF>(Team::TEAM_GOOD);
    actor->setTeam(teamRef);
    other->setTeam(teamRef);
    flushObjectHandler(module);

    clearOrderState(*other);
    const auto& otherState = Ego::Script::runtimeState(*other);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, other);
    self.setSelf(ObjectRef::Invalid);

    state.argument = 555;
    EXPECT_FALSE(scr_CreateOrder(state, self));
    EXPECT_EQ(state.argument, 555);

    state.argument = 666;
    EXPECT_FALSE(scr_GetDamageType(state, self));
    EXPECT_EQ(state.argument, 666);

    state.turn = 111;
    EXPECT_FALSE(scr_GetAttackTurn(state, self));
    EXPECT_EQ(state.turn, 111);

    self.setOldTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetOldTarget(state, self));
    EXPECT_EQ(self.getOldTarget(), ObjectRef::Invalid);

    // "other" is a live same-team recipient -- it would receive the order if the guard were
    // skipped.
    state.argument = 77;
    EXPECT_FALSE(scr_IssueOrder(state, self));

    // "other" also matches its own profile's special IDSZ -- it would receive this order too
    // if the guard were skipped.
    state.argument = 88;
    state.distance = static_cast<int>(other->getProfile()->getIDSZ(IDSZ_SPECIAL).toUint32());
    EXPECT_FALSE(scr_OrderSpecialID(state, self));

    EXPECT_EQ(otherState.order_value, 0u);
    EXPECT_FALSE(HAS_SOME_BITS(otherState.alert, ALERTIF_ORDERED));
}

//--------------------------------------------------------------------------------------------
// script_functions_target_select.c
//--------------------------------------------------------------------------------------------

TEST_F(ScriptTargetSupportFunctionsFixture, SetOwnerToTargetCopiesTargetRefUnconditionallyWithoutValidation)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6732);
    auto target = makeObject(module, "mp_objects/follower.obj", 6733);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    self.owner = ObjectRef::Invalid;

    EXPECT_TRUE(scr_SetOwnerToTarget(state, self));
    EXPECT_EQ(self.owner, target->getObjRef());

    // Quirk pinned deliberately: the target is never validated -- an Invalid target is still
    // copied verbatim into owner, and the call still returns true.
    self.setTarget(ObjectRef::Invalid);
    self.owner = actor->getObjRef();
    EXPECT_TRUE(scr_SetOwnerToTarget(state, self));
    EXPECT_EQ(self.owner, ObjectRef::Invalid);

    // A terminated/stale target ref is likewise copied verbatim.
    target->requestTerminate();
    self.setTarget(target->getObjRef());
    self.owner = ObjectRef::Invalid;
    EXPECT_TRUE(scr_SetOwnerToTarget(state, self));
    EXPECT_EQ(self.owner, target->getObjRef());
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetTargetToSelfOverwritesTargetWithSelfRefButLeavesOldTargetUntouched)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6734);
    auto other = makeObject(module, "mp_objects/follower.obj", 6735);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(other, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, other);
    self.setOldTarget(other->getObjRef());

    EXPECT_TRUE(scr_SetTargetToSelf(state, self));
    EXPECT_EQ(self.getTarget(), actor->getObjRef());
    // _oldTarget bookkeeping lives elsewhere in the script-engine update cycle, not in this
    // function.
    EXPECT_EQ(self.getOldTarget(), other->getObjRef());
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetTargetToNearestLifeformSelectsStrictlyNearerVisibleCandidateAndIncludesGroundItems)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6736, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto farCharacter = makeObject(module, "mp_objects/follower.obj", 6737, Ego::Vector3f(224.0f, 64.0f, 0.0f));
    auto nearItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 6738, Ego::Vector3f(96.0f, 64.0f, 0.0f));

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(farCharacter, nullptr);
    ASSERT_NE(nearItem, nullptr);

    // The headless test fixture has no real terrain/LOS data available, so make the actor
    // instance-invincible to bypass chr_find_target's line-of-sight veto (production code
    // grants the same bypass to any invincible source) -- this does not affect any of the
    // exclusion rules being pinned below.
    actor->setInvincible(true);
    farCharacter->setInvincible(false);
    farCharacter->setAlpha(200);
    farCharacter->setLight(200);
    nearItem->setInvincible(false);
    nearItem->setAlpha(200);
    nearItem->setLight(200);
    ASSERT_TRUE(nearItem->isItem());

    flushObjectHandler(module);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);
    self.setOldTarget(ObjectRef::Invalid);

    EXPECT_TRUE(scr_SetTargetToNearestLifeform(state, self));
    // Quirk pinned deliberately: TARGET_ITEMS is set internally, so a ground item counts as an
    // eligible "lifeform" and (being strictly nearer) wins over the farther live character.
    EXPECT_EQ(self.getTarget(), nearItem->getObjRef());
    EXPECT_EQ(self.getOldTarget(), ObjectRef::Invalid); // setTarget never touches _oldTarget
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetTargetToNearestLifeformExcludesHeldAndDeadCandidatesButStillIncludesInvincibleOnes)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6739, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto heldItem = makeObject(module, "mp_data/globalobjects/weapons/stiletto.obj", 6740, Ego::Vector3f(80.0f, 64.0f, 0.0f));
    auto deadCandidate = makeObject(module, "mp_objects/follower.obj", 6741, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    // second nearest slot reused deliberately below for the invincible candidate's position

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_NE(deadCandidate, nullptr);

    auto invincibleCandidate = makeObject(module, "mp_objects/follower.obj", 6742, Ego::Vector3f(128.0f, 64.0f, 0.0f));
    ASSERT_NE(invincibleCandidate, nullptr);

    // The headless test fixture has no real terrain/LOS data available, so make the actor
    // instance-invincible to bypass chr_find_target's line-of-sight veto (production code
    // grants the same bypass to any invincible source) -- this is unrelated to the candidate's
    // own instance-invincible flag being pinned below.
    actor->setInvincible(true);
    deadCandidate->setInvincible(false);
    deadCandidate->setAlpha(200);
    deadCandidate->setLight(200);
    invincibleCandidate->setAlpha(200);
    invincibleCandidate->setLight(200);
    invincibleCandidate->setInvincible(true);

    // Mark heldItem as being held (the holder's own bookkeeping array is irrelevant to
    // isBeingHeld() -- only the item's own holder ref matters).
    heldItem->setHolderRef(invincibleCandidate->getObjRef());

    flushObjectHandler(module);

    IDamageable& damageableDead = *deadCandidate;
    damageableDead.kill(actor->attribution(), true);
    ASSERT_FALSE(deadCandidate->isAlive());
    ASSERT_TRUE(heldItem->isBeingHeld());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_TRUE(scr_SetTargetToNearestLifeform(state, self));
    // Quirks pinned deliberately: the closer held item is skipped (isBeingHeld short-circuit
    // before any other check), the closer dead character is skipped (TARGET_DEAD bit is absent,
    // so only ALIVE targets pass), so the farther INVINCIBLE candidate wins even though
    // isInvincible() would normally exclude a candidate -- TARGET_ITEMS covers both the
    // "is an item" and "is invincible" exemptions together.
    EXPECT_EQ(self.getTarget(), invincibleCandidate->getObjRef());
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetTargetToNearestLifeformIgnoresUnflushedPendingSpawnsUntilHandlerIsFlushed)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6743, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    ASSERT_NE(actor, nullptr);
    // The headless test fixture has no real terrain/LOS data available, so make the actor
    // instance-invincible to bypass chr_find_target's line-of-sight veto (production code
    // grants the same bypass to any invincible source) -- unrelated to the flush behavior
    // being pinned below.
    actor->setInvincible(true);
    flushObjectHandler(module);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, nullptr);

    EXPECT_FALSE(scr_SetTargetToNearestLifeform(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    auto pendingCandidate = makeObject(module, "mp_objects/follower.obj", 6744, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    ASSERT_NE(pendingCandidate, nullptr);
    pendingCandidate->setInvincible(false);
    pendingCandidate->setAlpha(200);
    pendingCandidate->setLight(200);

    // Not flushed yet: pendingCandidate sits in the deferred-add list and is invisible to the
    // objectRefIterator()-based search used by the NEAREST branch of chr_find_target.
    EXPECT_FALSE(scr_SetTargetToNearestLifeform(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    flushObjectHandler(module);
    EXPECT_TRUE(scr_SetTargetToNearestLifeform(state, self));
    EXPECT_EQ(self.getTarget(), pendingCandidate->getObjRef());
}

TEST_F(ScriptTargetSupportFunctionsFixture, SetTargetToNearestLifeformFailsAndPreservesTargetWhenNoVisibleCandidateExists)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6745, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto invisibleCandidate = makeObject(module, "mp_objects/follower.obj", 6746, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(invisibleCandidate, nullptr);

    invisibleCandidate->setAlpha(0);
    invisibleCandidate->setLight(0);
    flushObjectHandler(module);
    ASSERT_FALSE(actor->canSeeObject(invisibleCandidate->getObjRef()));

    script_state_t state;
    // Pre-existing target set to prove preservation on failure.
    ai_state_t self = makeScriptSelf(actor, invisibleCandidate);
    self.setOldTarget(ObjectRef::Invalid);

    EXPECT_FALSE(scr_SetTargetToNearestLifeform(state, self));
    EXPECT_EQ(self.getTarget(), invisibleCandidate->getObjRef()); // untouched on failure
}

TEST_F(ScriptTargetSupportFunctionsFixture, AllTargetSelectionFunctionsReturnFalseForUnresolvedSelfAndProduceNoSideEffects)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6747, Ego::Vector3f(64.0f, 64.0f, 0.0f));
    auto other = makeObject(module, "mp_objects/follower.obj", 6748, Ego::Vector3f(96.0f, 64.0f, 0.0f));
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(other, nullptr);

    other->setInvincible(false);
    other->setAlpha(200);
    other->setLight(200);
    flushObjectHandler(module);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, other);
    self.setSelf(ObjectRef::Invalid);

    // SetOwnerToTarget: would copy the live target ref into owner if the guard were skipped.
    self.owner = actor->getObjRef();
    EXPECT_FALSE(scr_SetOwnerToTarget(state, self));
    EXPECT_EQ(self.owner, actor->getObjRef());

    // SetTargetToSelf: self.getSelf() is Invalid here, so it would overwrite a live target with
    // Invalid if the guard were skipped -- a visible, checkable change either way.
    self.setTarget(other->getObjRef());
    EXPECT_FALSE(scr_SetTargetToSelf(state, self));
    EXPECT_EQ(self.getTarget(), other->getObjRef());

    // SetTargetToNearestLifeform: unlike the two sub-cases above, this one does NOT distinguish
    // the resolveSelfContext guard from a downstream failure. makeSelfTargetSelectorContext reads
    // its search-source ref straight from self.getSelf() -- the very same Invalid ref the guard
    // above just rejected -- so chr_find_target's own `tryActiveObject(sourceRef)` lookup
    // (game_targeting.c) returns nullptr and the search bails out before any candidate is ever
    // considered, regardless of whether the resolveSelfContext guard ran or was skipped. "other"
    // is still set up alive, visible, and not self to show that even a maximally-eligible
    // candidate makes no difference here; this sub-case pins that the call stays safe (returns
    // false, target untouched) for an unresolved self, not that the top-level guard specifically
    // is what stops it -- that guard is unreachable-to-distinguish for this function.
    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetTargetToNearestLifeform(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
}

} // namespace
