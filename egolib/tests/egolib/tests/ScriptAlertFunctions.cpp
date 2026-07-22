/// @file ScriptAlertFunctions.cpp
/// @brief Characterization tests for the script alert-check family
///        (egolib/game/script_functions_alerts.c): the 27 one-bit ALERTIF_* checks, the four
///        raw-window IfHitFrom* direction checks, and IfSomeoneIsStealing. Before this file,
///        none of the 32 functions in that translation unit had any test reference.

#include "gtest/gtest.h"

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

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
#include "egolib/game/Module/Passage.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

using ScriptFunction = uint8_t (*)(script_state_t&, ai_state_t&);

struct AlertBitCase
{
    const char* name;
    ScriptFunction function;
    uint32_t bit;
};

/// The one-bit alert checks: each proceeds exactly when its own ALERTIF_* bit is set.
const AlertBitCase kAlertBitCases[] = {
    {"IfSpawned", scr_IfSpawned, ALERTIF_SPAWNED},
    {"IfHitVulnerable", scr_IfHitVulnerable, ALERTIF_HITVULNERABLE},
    {"IfAttacked", scr_IfAttacked, ALERTIF_ATTACKED},
    {"IfBumped", scr_IfBumped, ALERTIF_BUMPED},
    {"IfOrdered", scr_IfOrdered, ALERTIF_ORDERED},
    {"IfCalledForHelp", scr_IfCalledForHelp, ALERTIF_CALLEDFORHELP},
    {"IfKilled", scr_IfKilled, ALERTIF_KILLED},
    {"IfDropped", scr_IfDropped, ALERTIF_DROPPED},
    {"IfGrabbed", scr_IfGrabbed, ALERTIF_GRABBED},
    {"IfReaffirmed", scr_IfReaffirmed, ALERTIF_REAFFIRMED},
    {"IfUsed", scr_IfUsed, ALERTIF_USED},
    {"IfCleanedUp", scr_IfCleanedUp, ALERTIF_CLEANEDUP},
    {"IfScoredAHit", scr_IfScoredAHit, ALERTIF_SCOREDAHIT},
    {"IfHealed", scr_IfHealed, ALERTIF_HEALED},
    {"IfDisaffirmed", scr_IfDisaffirmed, ALERTIF_DISAFFIRMED},
    {"IfChanged", scr_IfChanged, ALERTIF_CHANGED},
    {"IfInWater", scr_IfInWater, ALERTIF_INWATER},
    {"IfBored", scr_IfBored, ALERTIF_BORED},
    {"IfTooMuchBaggage", scr_IfTooMuchBaggage, ALERTIF_TOOMUCHBAGGAGE},
    {"IfLevelUp", scr_IfLevelUp, ALERTIF_LEVELUP},
    {"IfHitGround", scr_IfHitGround, ALERTIF_HITGROUND},
    {"IfNotDropped", scr_IfNotDropped, ALERTIF_NOTDROPPED},
    {"IfBlocked", scr_IfBlocked, ALERTIF_BLOCKED},
    {"IfThrown", scr_IfThrown, ALERTIF_THROWN},
    {"IfCrushed", scr_IfCrushed, ALERTIF_CRUSHED},
    {"IfNotPutAway", scr_IfNotPutAway, ALERTIF_NOTPUTAWAY},
    {"IfTakenOut", scr_IfTakenOut, static_cast<uint32_t>(ALERTIF_TAKENOUT)},  // 1 << 31 is a negative int
};

struct QuadrantCase
{
    const char* name;
    ScriptFunction function;
    int32_t center;  ///< The raw ATK_* facing value the window is centered on.
};

const QuadrantCase kQuadrantCases[] = {
    {"IfHitFromFront", scr_IfHitFromFront, 0x0000},
    {"IfHitFromRight", scr_IfHitFromRight, 0x4000},
    {"IfHitFromBehind", scr_IfHitFromBehind, 0x8000},
    {"IfHitFromLeft", scr_IfHitFromLeft, 0xC000},
};

constexpr int32_t kQuadrantTolerance = 8192;  ///< ~45 degrees, mirrors the implementation.

class ScriptAlertFunctionsFixture : public ::testing::Test
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
        opts.logPath = "/debug/script-alert-function-tests.log";
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

    std::shared_ptr<Object> makeObject(GameModule& module, const std::string& profilePath, int slot) const
    {
        const ObjectProfileRef profile = EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        const ObjectRef objectRef = module.spawnObjectRef(Ego::Vector3f(64.0f, 64.0f, 0.0f), profile,
                                                          static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0),
                                                          "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(ObjectRef::Invalid);
        return self;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptAlertFunctionsFixture::s_runtime;

TEST_F(ScriptAlertFunctionsFixture, AlertBitChecksProceedExactlyOnTheirOwnBit)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6101);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    for (const AlertBitCase& testCase : kAlertBitCases)
    {
        SCOPED_TRACE(testCase.name);

        self.alert = 0;
        EXPECT_FALSE(testCase.function(state, self));

        self.alert = testCase.bit;
        EXPECT_TRUE(testCase.function(state, self));

        // Every alert bit EXCEPT the function's own: the checks are independent one-bit tests.
        self.alert = ~testCase.bit;
        EXPECT_FALSE(testCase.function(state, self));
    }
}

TEST_F(ScriptAlertFunctionsFixture, AlertChecksReturnFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6102);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    // Make every underlying predicate satisfied, so a pass could only come from
    // skipping the resolved-self guard.
    self.alert = ~static_cast<uint32_t>(0);
    self.directionlast = Facing(0x8000);
    self.order_value = Passage::SHOP_STOLEN;
    self.order_counter = Passage::SHOP_THEFT;

    for (const AlertBitCase& testCase : kAlertBitCases)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(testCase.function(state, self));
    }

    for (const QuadrantCase& testCase : kQuadrantCases)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(testCase.function(state, self));
    }

    EXPECT_FALSE(scr_IfSomeoneIsStealing(state, self));
}

TEST_F(ScriptAlertFunctionsFixture, HitFromChecksAcceptOnlyTheirRawQuadrantWindow)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6103);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    for (const QuadrantCase& testCase : kQuadrantCases)
    {
        SCOPED_TRACE(testCase.name);

        // The window is [center - tolerance, center + tolerance) over raw facing values.
        self.directionlast = Facing(testCase.center);
        EXPECT_TRUE(testCase.function(state, self));

        self.directionlast = Facing(testCase.center - kQuadrantTolerance);
        EXPECT_TRUE(testCase.function(state, self));

        self.directionlast = Facing(testCase.center - kQuadrantTolerance - 1);
        EXPECT_FALSE(testCase.function(state, self));

        self.directionlast = Facing(testCase.center + kQuadrantTolerance - 1);
        EXPECT_TRUE(testCase.function(state, self));

        self.directionlast = Facing(testCase.center + kQuadrantTolerance);
        EXPECT_FALSE(testCase.function(state, self));
    }
}

TEST_F(ScriptAlertFunctionsFixture, HitFromChecksCompareRawFacingValuesWithoutAngleWrap)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6104);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    // Legacy quirk pinned deliberately: Facing stores an unnormalized raw value and the
    // window checks compare those raw values. A raw -8192 sits inside the front window,
    // while the canonically equal 57344 (== -8192 mod 2^16) does not.
    self.directionlast = Facing(-8192);
    EXPECT_TRUE(scr_IfHitFromFront(state, self));

    self.directionlast = Facing(57344);
    EXPECT_FALSE(scr_IfHitFromFront(state, self));

    // Consequence: canonical facing values in [57344, 65535] fall in a dead zone that no
    // quadrant check accepts.
    self.directionlast = Facing(60000);
    for (const QuadrantCase& testCase : kQuadrantCases)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(testCase.function(state, self));
    }
}

TEST_F(ScriptAlertFunctionsFixture, SomeoneIsStealingRequiresBothTheftOrderValueAndCounter)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6105);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    self.order_value = Passage::SHOP_STOLEN;
    self.order_counter = Passage::SHOP_THEFT;
    EXPECT_TRUE(scr_IfSomeoneIsStealing(state, self));

    self.order_value = Passage::SHOP_STOLEN;
    self.order_counter = Passage::SHOP_SELL;
    EXPECT_FALSE(scr_IfSomeoneIsStealing(state, self));

    self.order_value = 0;
    self.order_counter = Passage::SHOP_THEFT;
    EXPECT_FALSE(scr_IfSomeoneIsStealing(state, self));

    self.order_value = 0;
    self.order_counter = 0;
    EXPECT_FALSE(scr_IfSomeoneIsStealing(state, self));
}

} // namespace
