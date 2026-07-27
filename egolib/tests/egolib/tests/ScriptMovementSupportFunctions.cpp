/// @file ScriptMovementSupportFunctions.cpp
/// @brief Characterization tests for the remaining untested script movement/navigation
///        support functions: from egolib/game/script_functions_movement.c
///        (scr_AddWaypoint, scr_ClearWaypoints, scr_Compass, scr_IfAtLastWaypoint,
///        scr_IfAtWaypoint) and from egolib/game/script_functions_movement_physics.c
///        (scr_AddXY, scr_GetXY, scr_SetSpeedPercent, scr_SetXY). Before this file, none
///        of these 9 functions had any test reference. The sibling
///        ScriptMovementFunctions.cpp already covers the turn-mode/latch/teleport/
///        velocity/reload/shadow/frame/FindPath functions from the same two
///        translation units.

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptMovementSupportFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 80;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-movement-support-function-tests.log";
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
        const bool began = session.beginModule(module, 80);
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

std::unique_ptr<ContentRuntimeBootstrap> ScriptMovementSupportFunctionsFixture::s_runtime;

// NOTE on a guard-divergence hypothesis: scr_AddWaypoint's entry guard is
// hasResolvedSelf() (-> Ego::Entities::activeObjectExists(), backed by
// GameModule::hasObject() -> ObjectHandler::exists()), while its internal
// ai_state_t::get_wp() call gates on isRuntimeObjectAlive() (backed by
// tryInventoryHolder() -> ObjectRoleAccess::tryActiveObject() -> the very same
// GameModule::tryObject() -> ObjectHandler::exists() check). Both routes bottom
// out in the identical ObjectHandler::exists() test on the identical Object
// (which unconditionally implements IInventoryHolder), so for any real spawned
// actor there is no reachable divergence: whenever the outer guard passes,
// get_wp() always successfully refreshes wp/wp_valid. The tests below pin that
// observed (non-diverging) behavior rather than a hypothetical stale-cache case.

TEST_F(ScriptMovementSupportFunctionsFixture, AddWaypointRefreshesCachedWaypointOnlyToTheOldestUnvisitedEntry)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6601);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    waypoint_list_t::clear(self.wp_lst);
    self.wp_valid = false;
    script_state_t state;

    state.x = 100;
    state.y = 200;
    EXPECT_TRUE(scr_AddWaypoint(state, self));
    EXPECT_TRUE(self.wp_valid);
    EXPECT_FLOAT_EQ(self.wp[0], 100.0f);
    EXPECT_FLOAT_EQ(self.wp[1], 200.0f);
    EXPECT_EQ(self.wp_lst._head, 1);
    EXPECT_EQ(self.wp_lst._tail, 0);

    // Quirk pinned deliberately: the second Add pushes a NEW waypoint (head
    // advances and the point is stored in the list), but waypoint_list_t::peek
    // keeps reporting the OLDEST (tail) waypoint since _tail never moved --
    // the cache does not track the "most recently added" waypoint.
    state.x = 300;
    state.y = 400;
    EXPECT_TRUE(scr_AddWaypoint(state, self));
    EXPECT_EQ(self.wp_lst._head, 2);
    EXPECT_TRUE(self.wp_valid);
    EXPECT_FLOAT_EQ(self.wp[0], 100.0f); // still the FIRST point, not (300, 400)
    EXPECT_FLOAT_EQ(self.wp[1], 200.0f);
    EXPECT_FLOAT_EQ(self.wp_lst._pos[1][0], 300.0f); // the second point IS stored in the list
    EXPECT_FLOAT_EQ(self.wp_lst._pos[1][1], 400.0f);
}

TEST_F(ScriptMovementSupportFunctionsFixture, AddWaypointOverwritesFinalSlotOnceListIsFullAndAlwaysReturnsTrue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6602);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    waypoint_list_t::clear(self.wp_lst);
    script_state_t state;

    // MAXWAY == 8: pushing 9 distinct waypoints demonstrates the silent
    // overwrite-final-slot behavior. ::AddWaypoint never signals failure past
    // the guard, so every one of the 9 calls returns true.
    for (int i = 1; i <= 9; ++i)
    {
        state.x = 1000 + i;
        state.y = 2000 + i;
        EXPECT_TRUE(scr_AddWaypoint(state, self)) << "push #" << i;
    }

    EXPECT_EQ(self.wp_lst._head, 7); // clamped at MAXWAY - 1
    // Slot 6 (the 7th push) is untouched by the overflow.
    EXPECT_FLOAT_EQ(self.wp_lst._pos[6][0], 1007.0f);
    EXPECT_FLOAT_EQ(self.wp_lst._pos[6][1], 2007.0f);
    // Slot 7 was overwritten by the 9th push; the 8th push's value is lost.
    EXPECT_FLOAT_EQ(self.wp_lst._pos[7][0], 1009.0f);
    EXPECT_FLOAT_EQ(self.wp_lst._pos[7][1], 2009.0f);
}

TEST_F(ScriptMovementSupportFunctionsFixture, ClearWaypointsResetsHeadAndTailButLeavesCachedWaypointStale)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6603);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    waypoint_list_t::clear(self.wp_lst);
    script_state_t state;

    state.x = 10;
    state.y = 20;
    EXPECT_TRUE(scr_AddWaypoint(state, self));
    EXPECT_TRUE(self.wp_valid);
    EXPECT_FALSE(waypoint_list_t::empty(self.wp_lst));

    EXPECT_TRUE(scr_ClearWaypoints(state, self));
    EXPECT_TRUE(waypoint_list_t::empty(self.wp_lst));
    EXPECT_TRUE(waypoint_list_t::finished(self.wp_lst));
    EXPECT_EQ(self.wp_lst._head, 0);
    EXPECT_EQ(self.wp_lst._tail, 0);

    // Quirk pinned deliberately: ClearWaypoints only resets _head/_tail; it does
    // NOT touch wp_valid/wp, so the character's cached waypoint stays valid and
    // stale until the next get_wp()/ensure_wp() refresh.
    EXPECT_TRUE(self.wp_valid);
    EXPECT_FLOAT_EQ(self.wp[0], 10.0f);
    EXPECT_FLOAT_EQ(self.wp[1], 20.0f);
}

TEST_F(ScriptMovementSupportFunctionsFixture, WaypointAlertChecksProceedExactlyOnTheirOwnBitAndDoNotClearIt)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6604);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    self.alert = 0;
    EXPECT_FALSE(scr_IfAtWaypoint(state, self));
    EXPECT_FALSE(scr_IfAtLastWaypoint(state, self));

    self.alert = ALERTIF_ATWAYPOINT;
    EXPECT_TRUE(scr_IfAtWaypoint(state, self));
    EXPECT_FALSE(scr_IfAtLastWaypoint(state, self));
    // Querying does not consume/clear the bit.
    EXPECT_TRUE(scr_IfAtWaypoint(state, self));

    // "At last waypoint" is defined purely by the alert bit, independent of
    // wp_lst contents -- this fixture's self has an empty waypoint list here,
    // yet the check still proceeds.
    EXPECT_TRUE(waypoint_list_t::empty(self.wp_lst));
    self.alert = ALERTIF_ATLASTWAYPOINT;
    EXPECT_FALSE(scr_IfAtWaypoint(state, self));
    EXPECT_TRUE(scr_IfAtLastWaypoint(state, self));

    // ALERTIF_PUTAWAY aliases the exact same bit as ALERTIF_ATLASTWAYPOINT.
    self.alert = ALERTIF_PUTAWAY;
    EXPECT_TRUE(scr_IfAtLastWaypoint(state, self));

    // Masked AND: unrelated bits alongside do not affect the result either way.
    self.alert = ALERTIF_ATWAYPOINT | ALERTIF_ATLASTWAYPOINT | (1 << 20);
    EXPECT_TRUE(scr_IfAtWaypoint(state, self));
    EXPECT_TRUE(scr_IfAtLastWaypoint(state, self));
}

TEST_F(ScriptMovementSupportFunctionsFixture, CompassSubtractsQuantizedTrigOffsetAndTruncatesTowardZero)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6605);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    // turn == 0: cos == 1 exactly, sin == 0 exactly -> pure "-x" motion (MINUS,
    // not plus: the compass points opposite the facing).
    state.x = 100;
    state.y = 50;
    state.turn = 0;
    state.distance = 10;
    EXPECT_TRUE(scr_Compass(state, self));
    EXPECT_EQ(state.x, 90);
    EXPECT_EQ(state.y, 50);
    EXPECT_EQ(state.turn, 0);      // turn register is never written back
    EXPECT_EQ(state.distance, 10); // distance register is never written back

    // Trig quantization: the lookup index is (facing >> 2), so turn values
    // 0..3 all collapse to the exact same bucket as turn == 0.
    state.x = 100;
    state.y = 50;
    state.turn = 3;
    state.distance = 10;
    EXPECT_TRUE(scr_Compass(state, self));
    EXPECT_EQ(state.x, 90);
    EXPECT_EQ(state.y, 50);
    EXPECT_EQ(state.turn, 3);

    // Facing canonicalization wraps by 65535, not 65536 (an off-by-one-looking
    // quirk): turn == 65536 canonicalizes to 1, which still falls in bucket 0
    // and therefore reproduces the turn == 0 result exactly.
    state.x = 100;
    state.y = 50;
    state.turn = 65536;
    state.distance = 10;
    EXPECT_TRUE(scr_Compass(state, self));
    EXPECT_EQ(state.x, 90);
    EXPECT_EQ(state.y, 50);
    EXPECT_EQ(state.turn, 65536);

    // turn == -1 canonicalizes to 65534, while turn == 65535 is NOT wrapped
    // (the wrap condition is strictly "> max", not ">="), staying 65535. Both
    // canonical values land in the SAME final trig bucket ((65534>>2)&mask ==
    // (65535>>2)&mask == 16383), so the two calls are indistinguishable even
    // though their canonical Facing values differ.
    state.x = 0;
    state.y = 0;
    state.turn = -1;
    state.distance = 1;
    EXPECT_TRUE(scr_Compass(state, self));
    const int xFromNegativeOne = state.x;
    const int yFromNegativeOne = state.y;

    state.x = 0;
    state.y = 0;
    state.turn = 65535;
    state.distance = 1;
    EXPECT_TRUE(scr_Compass(state, self));
    EXPECT_EQ(state.x, xFromNegativeOne);
    EXPECT_EQ(state.y, yFromNegativeOne);

    // Truncation toward zero, not floor: turn == 8192 lands on bucket 2048,
    // i.e. a 45-degree offset, so cos/sin are both a fraction strictly between
    // 0 and 1. distance == 1 therefore produces a small negative fractional
    // position that truncates to 0, not -1.
    state.x = 0;
    state.y = 0;
    state.turn = 8192;
    state.distance = 1;
    EXPECT_TRUE(scr_Compass(state, self));
    EXPECT_EQ(state.x, 0);
    EXPECT_EQ(state.y, 0);
    EXPECT_EQ(state.turn, 8192);
}

TEST_F(ScriptMovementSupportFunctionsFixture, StorageSlotIndexMaskAliasesArgumentModulo16)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6606);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    // Docstring says "8 permanent storage variable slots"; the actual mask
    // (STOR_AND == 15) provides 16 slots (STOR_COUNT == 1 << STOR_BITS == 16).
    // argument == 16 aliases slot 0 (16 & 15 == 0).
    state.argument = 0;
    state.x = 111;
    state.y = 222;
    EXPECT_TRUE(scr_SetXY(state, self));

    state.argument = 16;
    state.x = 0;
    state.y = 0;
    EXPECT_TRUE(scr_GetXY(state, self));
    EXPECT_EQ(state.x, 111);
    EXPECT_EQ(state.y, 222);
    EXPECT_EQ(self.x[0], 111);
    EXPECT_EQ(self.y[0], 222);

    // argument == -1 selects slot 15 via a plain signed bitwise AND
    // (two's-complement: -1 & 15 == 15).
    state.argument = -1;
    state.x = 7;
    state.y = 8;
    EXPECT_TRUE(scr_SetXY(state, self));
    EXPECT_EQ(self.x[15], 7);
    EXPECT_EQ(self.y[15], 8);

    state.argument = 15;
    state.x = 0;
    state.y = 0;
    EXPECT_TRUE(scr_GetXY(state, self));
    EXPECT_EQ(state.x, 7);
    EXPECT_EQ(state.y, 8);

    // Natural composed pin: SetXY(idx, a, b) then AddXY(idx, c, d) then
    // GetXY(idx) yields (a + c, b + d). argument == 17 aliases slot 1
    // (17 & 15 == 1); a freshly constructed ai_state_t zero-fills every slot.
    EXPECT_EQ(self.x[1], 0);
    EXPECT_EQ(self.y[1], 0);

    state.argument = 17;
    state.x = 3;
    state.y = 4;
    EXPECT_TRUE(scr_AddXY(state, self)); // slot 1: 0,0 -> 3,4
    EXPECT_EQ(self.x[1], 3);
    EXPECT_EQ(self.y[1], 4);

    state.argument = 1; // same slot 1, argument without the alias bit
    state.x = 10;
    state.y = 20;
    EXPECT_TRUE(scr_AddXY(state, self)); // slot 1: 3,4 -> 13,24 (read-modify-write accumulation)
    EXPECT_EQ(self.x[1], 13);
    EXPECT_EQ(self.y[1], 24);

    state.argument = 1;
    state.x = 0;
    state.y = 0;
    EXPECT_TRUE(scr_GetXY(state, self));
    EXPECT_EQ(state.x, 13);
    EXPECT_EQ(state.y, 24);
}

TEST_F(ScriptMovementSupportFunctionsFixture, SetSpeedPercentDividesArgumentByOneHundredWithZeroFloorAndNoUpperClamp)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6607);
    ASSERT_NE(actor, nullptr);

    ai_state_t self = makeScriptSelf(actor);
    script_state_t state;

    state.argument = 100;
    EXPECT_TRUE(scr_SetSpeedPercent(state, self));
    EXPECT_FLOAT_EQ(self.maxSpeed, 1.0f);

    state.argument = 150;
    EXPECT_TRUE(scr_SetSpeedPercent(state, self));
    EXPECT_FLOAT_EQ(self.maxSpeed, 1.5f);

    // Clamp is one-sided: negative arguments floor to exactly 0.0f.
    state.argument = -1;
    EXPECT_TRUE(scr_SetSpeedPercent(state, self));
    EXPECT_FLOAT_EQ(self.maxSpeed, 0.0f);

    // ...but there is no upper bound.
    state.argument = 100000;
    EXPECT_TRUE(scr_SetSpeedPercent(state, self));
    EXPECT_FLOAT_EQ(self.maxSpeed, 1000.0f);
}

TEST_F(ScriptMovementSupportFunctionsFixture, NavigationFunctionsReturnFalseForUnresolvedSelfAndLeaveStateUntouched)
{
    beginActiveTestModule();

    // Deliberately pre-satisfy every underlying predicate so a pass could only
    // come from skipping the resolved-self guard.
    ai_state_t self = makeScriptSelf(nullptr);
    waypoint_list_t::clear(self.wp_lst);
    waypoint_list_t::push(self.wp_lst, 55, 66); // non-empty list: would satisfy Clear if the guard were skipped
    self.wp_valid = true;
    self.wp[0] = 55.0f;
    self.wp[1] = 66.0f;
    self.wp[2] = 0.0f;
    self.alert = ALERTIF_ATWAYPOINT | ALERTIF_ATLASTWAYPOINT; // would satisfy both predicates if the guard were skipped

    script_state_t state;
    state.x = 10;
    state.y = 20;
    state.turn = 0;
    state.distance = 5;

    EXPECT_FALSE(scr_IfAtWaypoint(state, self));
    EXPECT_FALSE(scr_IfAtLastWaypoint(state, self));

    const int headBefore = self.wp_lst._head;
    const int tailBefore = self.wp_lst._tail;

    EXPECT_FALSE(scr_ClearWaypoints(state, self));
    EXPECT_EQ(self.wp_lst._head, headBefore); // list NOT cleared
    EXPECT_EQ(self.wp_lst._tail, tailBefore);

    EXPECT_FALSE(scr_AddWaypoint(state, self));
    EXPECT_EQ(self.wp_lst._head, headBefore); // no push happened

    EXPECT_FALSE(scr_Compass(state, self));
    EXPECT_EQ(state.x, 10); // registers untouched
    EXPECT_EQ(state.y, 20);
    EXPECT_EQ(state.turn, 0);
    EXPECT_EQ(state.distance, 5);
}

TEST_F(ScriptMovementSupportFunctionsFixture, StorageAndSpeedFunctionsReturnFalseForUnresolvedSelfAndLeaveStateUntouched)
{
    beginActiveTestModule();

    ai_state_t self = makeScriptSelf(nullptr);
    self.x[5] = 111;
    self.y[5] = 222;
    self.maxSpeed = 42.0f; // sentinel: a default-constructed ai_state_t leaves maxSpeed indeterminate

    script_state_t state;
    state.argument = 5;
    state.x = 999;
    state.y = 888;

    EXPECT_FALSE(scr_SetXY(state, self));
    EXPECT_EQ(self.x[5], 111);
    EXPECT_EQ(self.y[5], 222);

    EXPECT_FALSE(scr_AddXY(state, self));
    EXPECT_EQ(self.x[5], 111);
    EXPECT_EQ(self.y[5], 222);

    state.x = 555; // sentinel output registers
    state.y = 666;
    EXPECT_FALSE(scr_GetXY(state, self));
    EXPECT_EQ(state.x, 555); // register left with its prior content, not zeroed
    EXPECT_EQ(state.y, 666);

    state.argument = 100;
    EXPECT_FALSE(scr_SetSpeedPercent(state, self));
    EXPECT_FLOAT_EQ(self.maxSpeed, 42.0f);
}

} // namespace
