/// @file ScriptStateControlFunctions.cpp
/// @brief Characterization tests for the script state family split across
///        egolib/game/script_functions_state.c (21 functions) and
///        egolib/game/script_functions_state_inventory.c (13 functions): the
///        IfStateIs0..15 literal ladder, IfStateIsOdd parity, the generic
///        IfStateIs/IfStateIsNot/IfContentIs comparisons, SetContent/GetContent/
///        GetState round-trips, the tmpx/tmpy trichotomy, IfTimeOut/SetTime timer
///        arithmetic, SetWeatherTime, DebugMessage, DoNothing, End, and the
///        Linux/Macintosh platform-operator checks. Before this file, none of
///        these 34 functions had any test reference.

#include "gtest/gtest.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
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
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

using ScriptFunction = uint8_t (*)(script_state_t&, ai_state_t&);

struct StateLiteralCase
{
    const char* name;
    ScriptFunction function;
    int literal;
};

/// The 16 IfStateIs0..15 functions: 8 from script_functions_state.c and 8 from
/// script_functions_state_inventory.c. Each proceeds iff self.state equals its
/// own hard-coded literal, completely ignoring script_state_t.
const StateLiteralCase kStateLiteralCases[] = {
    {"IfStateIs0", scr_IfStateIs0, 0},
    {"IfStateIs1", scr_IfStateIs1, 1},
    {"IfStateIs2", scr_IfStateIs2, 2},
    {"IfStateIs3", scr_IfStateIs3, 3},
    {"IfStateIs4", scr_IfStateIs4, 4},
    {"IfStateIs5", scr_IfStateIs5, 5},
    {"IfStateIs6", scr_IfStateIs6, 6},
    {"IfStateIs7", scr_IfStateIs7, 7},
    {"IfStateIs8", scr_IfStateIs8, 8},
    {"IfStateIs9", scr_IfStateIs9, 9},
    {"IfStateIs10", scr_IfStateIs10, 10},
    {"IfStateIs11", scr_IfStateIs11, 11},
    {"IfStateIs12", scr_IfStateIs12, 12},
    {"IfStateIs13", scr_IfStateIs13, 13},
    {"IfStateIs14", scr_IfStateIs14, 14},
    {"IfStateIs15", scr_IfStateIs15, 15},
};

class ScriptStateControlFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 79;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-state-control-function-tests.log";
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
        const bool began = session.beginModule(module, 79);
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

std::unique_ptr<ContentRuntimeBootstrap> ScriptStateControlFunctionsFixture::s_runtime;

TEST_F(ScriptStateControlFunctionsFixture, StateIsLiteralLadderMatchesOwnValueAndIgnoresArgument)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6501);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    for (const StateLiteralCase& testCase : kStateLiteralCases)
    {
        SCOPED_TRACE(testCase.name);

        // Deliberately set tmpargument to something unrelated -- the IfStateIsN
        // family ignores script_state_t entirely, unlike the generic IfStateIs.
        state.argument = 999;

        self.state = testCase.literal;
        EXPECT_TRUE(testCase.function(state, self));
        EXPECT_EQ(state.argument, 999);  // still untouched: no side effects on match

        self.state = testCase.literal + 1;
        EXPECT_FALSE(testCase.function(state, self));

        self.state = -1;
        EXPECT_FALSE(testCase.function(state, self));
    }
}

TEST_F(ScriptStateControlFunctionsFixture, StateIsLiteralLadderReturnsFalseForUnresolvedSelfEvenWhenLiteralMatches)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6502);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);  // unresolved: self ref is Invalid

    for (const StateLiteralCase& testCase : kStateLiteralCases)
    {
        SCOPED_TRACE(testCase.name);

        // Pre-satisfy the underlying predicate: if the guard were skipped, this
        // would proceed. A pass can only come from the resolved-self check.
        self.state = testCase.literal;
        EXPECT_FALSE(testCase.function(state, self));
    }
}

TEST_F(ScriptStateControlFunctionsFixture, IfStateIsAndIfStateIsNotCompareArgumentAgainstStateWithSignedEquality)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6503);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    self.state = 5;
    state.argument = 5;
    EXPECT_TRUE(scr_IfStateIs(state, self));
    EXPECT_FALSE(scr_IfStateIsNot(state, self));

    state.argument = 6;
    EXPECT_FALSE(scr_IfStateIs(state, self));
    EXPECT_TRUE(scr_IfStateIsNot(state, self));

    // Negative states compare normally: both operands are plain signed int.
    self.state = -7;
    state.argument = -7;
    EXPECT_TRUE(scr_IfStateIs(state, self));
    EXPECT_FALSE(scr_IfStateIsNot(state, self));

    state.argument = 7;
    EXPECT_FALSE(scr_IfStateIs(state, self));
    EXPECT_TRUE(scr_IfStateIsNot(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, IfStateIsNotReturnsFalseForUnresolvedSelfDespiteVacuousTruth)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6504);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    // self.state (default 0) trivially differs from tmpargument -- "state is not
    // X" would be vacuously true if the guard were skipped. Pin that the guard
    // wins regardless: unresolved self returns FALSE here.
    self.state = 0;
    state.argument = 12345;
    EXPECT_FALSE(scr_IfStateIsNot(state, self));

    // state.argument == self.state would make IfStateIs pass if the guard were
    // skipped -- use equal operands here (unlike the unequal pair above, which
    // only exercises IfStateIsNot's guard non-vacuously) so this assertion can
    // actually distinguish the guard from the comparison.
    self.state = 42;
    state.argument = 42;
    EXPECT_FALSE(scr_IfStateIs(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, SetContentGetContentAndIfContentIsRoundTripArbitraryIntsIncludingNegatives)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6505);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    state.argument = -12345;
    EXPECT_TRUE(scr_SetContent(state, self));
    EXPECT_EQ(self.content, -12345);

    state.argument = 0;
    EXPECT_TRUE(scr_GetContent(state, self));
    EXPECT_EQ(state.argument, -12345);

    EXPECT_TRUE(scr_IfContentIs(state, self));  // tmpargument still -12345 == content

    state.argument = 999;
    EXPECT_FALSE(scr_IfContentIs(state, self));

    // Round-trip through a positive value too, to show the SetContent cast is
    // a plain identity copy over the full int range, not just negatives.
    state.argument = 2147483647;
    EXPECT_TRUE(scr_SetContent(state, self));
    EXPECT_EQ(self.content, 2147483647);
    EXPECT_TRUE(scr_IfContentIs(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, GetContentAndGetStateAndSetContentLeaveArgumentAndContentUntouchedForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6506);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    self.content = 55;
    self.state = 42;

    state.argument = 777;
    EXPECT_FALSE(scr_GetContent(state, self));
    EXPECT_EQ(state.argument, 777);  // untouched on failure

    state.argument = 777;
    EXPECT_FALSE(scr_GetState(state, self));
    EXPECT_EQ(state.argument, 777);

    state.argument = -1;
    EXPECT_FALSE(scr_SetContent(state, self));
    EXPECT_EQ(self.content, 55);  // untouched on failure

    // state.argument == self.content would make IfContentIs pass if the guard
    // were skipped; equalize them here so the assertion is non-vacuous.
    state.argument = 55;
    EXPECT_FALSE(scr_IfContentIs(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, GetStateReadsSelfStateIntoArgumentAcrossSignedRange)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6507);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    self.state = 42;
    state.argument = 0;
    EXPECT_TRUE(scr_GetState(state, self));
    EXPECT_EQ(state.argument, 42);

    self.state = -8;
    EXPECT_TRUE(scr_GetState(state, self));
    EXPECT_EQ(state.argument, -8);
}

TEST_F(ScriptStateControlFunctionsFixture, XYComparisonTrichotomyIsSignedIntegerCompare)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6508);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    state.x = 3;
    state.y = 5;
    EXPECT_TRUE(scr_IfXIsLessThanY(state, self));
    EXPECT_FALSE(scr_IfYIsLessThanX(state, self));
    EXPECT_FALSE(scr_IfXIsEqualToY(state, self));

    state.x = 5;
    state.y = 3;
    EXPECT_FALSE(scr_IfXIsLessThanY(state, self));
    EXPECT_TRUE(scr_IfYIsLessThanX(state, self));
    EXPECT_FALSE(scr_IfXIsEqualToY(state, self));

    state.x = 5;
    state.y = 5;
    EXPECT_FALSE(scr_IfXIsLessThanY(state, self));
    EXPECT_FALSE(scr_IfYIsLessThanX(state, self));
    EXPECT_TRUE(scr_IfXIsEqualToY(state, self));

    // Signed compare: negative values order normally.
    state.x = -5;
    state.y = -2;
    EXPECT_TRUE(scr_IfXIsLessThanY(state, self));
    EXPECT_FALSE(scr_IfYIsLessThanX(state, self));
    EXPECT_FALSE(scr_IfXIsEqualToY(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, XYComparisonFunctionsReturnFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6509);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    // x < y would make IfXIsLessThanY pass if the guard were skipped. Using
    // x == y here (as the other two sub-cases do) would be vacuous for this
    // function since (x < y) is false either way.
    state.x = 3;
    state.y = 5;
    EXPECT_FALSE(scr_IfXIsLessThanY(state, self));

    // y < x would make IfYIsLessThanX pass if the guard were skipped.
    state.x = 5;
    state.y = 3;
    EXPECT_FALSE(scr_IfYIsLessThanX(state, self));

    // x == y would make IfXIsEqualToY pass if the guard were skipped.
    state.x = 3;
    state.y = 3;
    EXPECT_FALSE(scr_IfXIsEqualToY(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, IfTimeOutIsStrictGreaterThanWithUnsignedWraparound)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6510);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    auto& worldUpdateCount = GameSessionContext::get().worldUpdateCount();

    // Fresh timer 0, count 0: equal counts do NOT time out (strict >, not >=).
    self.timer = 0;
    worldUpdateCount = 0;
    EXPECT_FALSE(scr_IfTimeOut(state, self));

    worldUpdateCount = 1;
    EXPECT_TRUE(scr_IfTimeOut(state, self));

    // Timer set near UINT32_MAX: stays FALSE until the unsigned counter would
    // need to wrap past it -- comparison is raw unsigned >, no signed reinterpret.
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    self.timer = kMax - 2;
    worldUpdateCount = 0;
    EXPECT_FALSE(scr_IfTimeOut(state, self));

    worldUpdateCount = kMax - 2;
    EXPECT_FALSE(scr_IfTimeOut(state, self));  // equal -> still false

    worldUpdateCount = kMax - 1;
    EXPECT_TRUE(scr_IfTimeOut(state, self));

    worldUpdateCount = kMax;
    EXPECT_TRUE(scr_IfTimeOut(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, IfTimeOutReturnsFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6511);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    auto& worldUpdateCount = GameSessionContext::get().worldUpdateCount();

    self.timer = 0;
    worldUpdateCount = 100;  // would proceed if the guard were skipped
    EXPECT_FALSE(scr_IfTimeOut(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, SetTimeIsANoOpForNonPositiveDelayAndWrapsForPositiveDelay)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6512);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    auto& worldUpdateCount = GameSessionContext::get().worldUpdateCount();

    // delay <= 0 is a silent no-op that still returns TRUE.
    self.timer = 500;
    state.argument = 0;
    EXPECT_TRUE(scr_SetTime(state, self));
    EXPECT_EQ(self.timer, 500u);

    state.argument = -10;
    EXPECT_TRUE(scr_SetTime(state, self));
    EXPECT_EQ(self.timer, 500u);

    // delay > 0: timer = worldUpdateCount() + delay.
    worldUpdateCount = 1000;
    state.argument = 50;  // 50 ticks == 1 second, per docs
    EXPECT_TRUE(scr_SetTime(state, self));
    EXPECT_EQ(self.timer, 1050u);

    // uint32_t arithmetic silently wraps.
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    worldUpdateCount = kMax - 2;
    state.argument = 5;
    EXPECT_TRUE(scr_SetTime(state, self));
    EXPECT_EQ(self.timer, 2u);  // (kMax - 2) + 5 wraps to 2
}

TEST_F(ScriptStateControlFunctionsFixture, SetTimeReturnsFalseForUnresolvedSelfAndLeavesTimerUntouched)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6513);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    self.timer = 123;
    state.argument = 50;

    EXPECT_FALSE(scr_SetTime(state, self));
    EXPECT_EQ(self.timer, 123u);
}

TEST_F(ScriptStateControlFunctionsFixture, SetWeatherTimeWritesBothTimerFieldsFromOneArgumentIncludingNegatives)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6514);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    WeatherState& weatherState = activeModuleEnvironment().weatherState();

    state.argument = 42;
    EXPECT_TRUE(scr_SetWeatherTime(state, self));
    EXPECT_EQ(weatherState.timer_reset, 42);
    EXPECT_EQ(weatherState.time, 42);

    // Negative argument is accepted and stored verbatim in both fields.
    state.argument = -7;
    EXPECT_TRUE(scr_SetWeatherTime(state, self));
    EXPECT_EQ(weatherState.timer_reset, -7);
    EXPECT_EQ(weatherState.time, -7);

    // 0 means "no weather" per WeatherState docs.
    state.argument = 0;
    EXPECT_TRUE(scr_SetWeatherTime(state, self));
    EXPECT_EQ(weatherState.timer_reset, 0);
    EXPECT_EQ(weatherState.time, 0);
}

TEST_F(ScriptStateControlFunctionsFixture, SetWeatherTimeGuardReturnsFalseWithoutTouchingModuleEnvironment)
{
    // Deliberately do NOT begin a module here: the resolved-self guard must
    // reject an unresolved self before scr_SetWeatherTime ever reaches
    // activeModuleEnvironment() -- which would throw std::logic_error if
    // consulted with no module active. If the guard ordering were reversed,
    // this call would throw instead of returning false.
    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    state.argument = 99;

    EXPECT_FALSE(scr_SetWeatherTime(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, DebugMessageReturnsTrueAndSilentlyDropsMessagesWithoutAnActivePlayingState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6515);
    ASSERT_NE(actor, nullptr);

    // The headless test process never installs an IPlayingStateController, so
    // DisplayMsg_printf's messages are silently dropped -- but DebugMessage
    // still reports success.
    ASSERT_EQ(EngineContext::get().tryActivePlayingState(), nullptr);

    script_state_t state;
    state.x = 3;
    state.y = 5;
    state.distance = 7;
    state.turn = 11;
    state.argument = 13;
    ai_state_t self = makeScriptSelf(actor);
    self.state = 1;
    self.content = 2;

    EXPECT_TRUE(scr_DebugMessage(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, DebugMessageReturnsFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6516);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    EXPECT_FALSE(scr_DebugMessage(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, DoNothingAlwaysReturnsTrueEvenWithoutAnyObjectWorldInstalled)
{
    // The ONLY function in this family with no resolved-self guard: it
    // returns true unconditionally, even for an invalid self and even before
    // any module/object-world is active.
    script_state_t state;
    ai_state_t self;  // default-constructed, self ref is Invalid

    EXPECT_TRUE(scr_DoNothing(state, self));

    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6517);
    ASSERT_NE(actor, nullptr);
    ai_state_t resolvedSelf = makeScriptSelf(actor);
    EXPECT_TRUE(scr_DoNothing(state, resolvedSelf));
}

TEST_F(ScriptStateControlFunctionsFixture, EndTerminatesSelfAndReturnsFalseOnlyWhenResolved)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6518);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.terminate = false;

    // End() "fails" (returns FALSE) even on success -- it terminates the script.
    EXPECT_FALSE(scr_End(state, self));
    EXPECT_TRUE(self.terminate);
}

TEST_F(ScriptStateControlFunctionsFixture, EndLeavesTerminateUntouchedForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6519);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    self.terminate = false;

    EXPECT_FALSE(scr_End(state, self));
    EXPECT_FALSE(self.terminate);  // a dead/unresolved self cannot terminate via End
}

TEST_F(ScriptStateControlFunctionsFixture, OperatorPlatformChecksReturnLinuxTrueMacFalseWhenResolved)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6520);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    // This repository's Linux build compiles ID_LINUX defined / ID_OSX
    // undefined, so with a resolved self these are unconditionally 1 / 0.
    EXPECT_TRUE(scr_IfOperatorIsLinux(state, self));
    EXPECT_FALSE(scr_IfOperatorIsMacintosh(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, OperatorPlatformChecksReturnFalseForUnresolvedSelfRegardlessOfPlatform)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6521);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    // Quirk: an OS check that requires a resolved self -- with unresolved self
    // it reports "not this platform" even on the platform the build targets.
    EXPECT_FALSE(scr_IfOperatorIsLinux(state, self));
    EXPECT_FALSE(scr_IfOperatorIsMacintosh(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, IfStateIsOddPinsTruncatingModuloForNegativeStates)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6522);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    self.state = 0;
    EXPECT_FALSE(scr_IfStateIsOdd(state, self));

    self.state = 1;
    EXPECT_TRUE(scr_IfStateIsOdd(state, self));

    self.state = -2;
    EXPECT_FALSE(scr_IfStateIsOdd(state, self));

    // C++'s truncating % gives -3 % 2 == -1 (not 1), so is_even(-3) is
    // (0 == -1) == false, and is_odd(-3) is therefore TRUE.
    self.state = -3;
    EXPECT_TRUE(scr_IfStateIsOdd(state, self));
}

TEST_F(ScriptStateControlFunctionsFixture, IfStateIsOddReturnsFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6523);
    ASSERT_NE(actor, nullptr);
    (void)actor;

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);
    self.state = 1;  // odd, would pass if the guard were skipped

    EXPECT_FALSE(scr_IfStateIsOdd(state, self));
}

} // namespace
