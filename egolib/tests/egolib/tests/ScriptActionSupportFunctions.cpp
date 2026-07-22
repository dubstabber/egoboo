/// @file ScriptActionSupportFunctions.cpp
/// @brief Characterization tests completing script_functions_action.c: the seven legacy
///        speech-set functions (accepted no-ops), scr_CallForHelp, scr_DoActionOverride,
///        and scr_ShowTimer. Together with the KeepAction pair (ScriptLocomotionFunctions.cpp)
///        and the previously covered functions, every function in that translation unit now
///        has test coverage.

#include "gtest/gtest.h"

#include <cstdlib>
#include <initializer_list>
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
#include "egolib/Script/script.h"
#include "egolib/game/egoboo.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"
#include "ObjectGraphicsTestAccess.hpp"

namespace
{

using GraphicsAccess = Ego::Graphics::ObjectGraphicsTestAccess;
using ScriptFunction = uint8_t (*)(script_state_t&, ai_state_t&);

struct NamedFunction
{
    const char* name;
    ScriptFunction function;
};

/// The RTS speech registers are gone; the setters survive as accepted no-ops so legacy
/// scripts that call them keep running.
const NamedFunction kSpeechSetters[] = {
    {"SetSpeech", scr_SetSpeech},
    {"SetMoveSpeech", scr_SetMoveSpeech},
    {"SetSecondMoveSpeech", scr_SetSecondMoveSpeech},
    {"SetAttackSpeech", scr_SetAttackSpeech},
    {"SetAssistSpeech", scr_SetAssistSpeech},
    {"SetTerrainSpeech", scr_SetTerrainSpeech},
    {"SetSelectSpeech", scr_SetSelectSpeech},
};

const NamedFunction kSupportFunctions[] = {
    {"CallForHelp", scr_CallForHelp},
    {"DoActionOverride", scr_DoActionOverride},
    {"ShowTimer", scr_ShowTimer},
};

ModelAction findValidAction(const std::shared_ptr<Object>& object,
                            std::initializer_list<ModelAction> candidates,
                            ModelAction excluded = ACTION_COUNT)
{
    const auto& model = object->getGraphics().getModelDescriptor();
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

class ScriptActionSupportFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 63;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-action-support-function-tests.log";
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
        const bool began = session.beginModule(module, 63);
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

    /// Commit pending handler insertions so freshly spawned objects are iterable
    /// (Team::callForHelp walks the handler's ref iterator).
    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptActionSupportFunctionsFixture::s_runtime;

TEST_F(ScriptActionSupportFunctionsFixture, SpeechSettersAreAcceptedNoOpsForResolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6301);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 7;
    ai_state_t self = makeScriptSelf(actor);

    for (const NamedFunction& testCase : kSpeechSetters)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_TRUE(testCase.function(state, self));
    }
}

TEST_F(ScriptActionSupportFunctionsFixture, AllSupportFunctionsReturnFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto bystander = makeObject(module, "mp_objects/follower.obj", 6302);
    ASSERT_NE(bystander, nullptr);
    bystander->clearAIAlertBits(ALERTIF_CALLEDFORHELP);

    timeron = false;
    timervalue = 777;

    script_state_t state;
    state.argument = static_cast<int>(ACTION_DA);
    ai_state_t self = makeScriptSelf(nullptr);

    for (const NamedFunction& testCase : kSpeechSetters)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(testCase.function(state, self));
    }

    for (const NamedFunction& testCase : kSupportFunctions)
    {
        SCOPED_TRACE(testCase.name);
        EXPECT_FALSE(testCase.function(state, self));
    }

    // Nothing observable moved: no timer, no help call, no alert on anyone.
    EXPECT_FALSE(timeron);
    EXPECT_EQ(timervalue, 777u);
    EXPECT_EQ(module.getTeamCallerForHelpRef(static_cast<TEAM_REF>(Team::TEAM_NULL)), ObjectRef::Invalid);
    EXPECT_FALSE(bystander->hasAnyAIAlertBits(ALERTIF_CALLEDFORHELP));
}

TEST_F(ScriptActionSupportFunctionsFixture, CallForHelpPublishesCallerAndAlertsOthersButNotTheCaller)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 6303);
    auto teammate = makeObject(module, "mp_objects/follower.obj", 6304);
    ASSERT_NE(caller, nullptr);
    ASSERT_NE(teammate, nullptr);

    caller->clearAIAlertBits(ALERTIF_CALLEDFORHELP);
    teammate->clearAIAlertBits(ALERTIF_CALLEDFORHELP);
    flushObjectHandler(module);

    script_state_t state;
    ai_state_t self = makeScriptSelf(caller);

    EXPECT_TRUE(scr_CallForHelp(state, self));

    EXPECT_EQ(module.getTeamCallerForHelpRef(static_cast<TEAM_REF>(Team::TEAM_NULL)), caller->getObjRef());
    EXPECT_TRUE(teammate->hasAnyAIAlertBits(ALERTIF_CALLEDFORHELP));
    // The caller itself is skipped when the alert is published.
    EXPECT_FALSE(caller->hasAnyAIAlertBits(ALERTIF_CALLEDFORHELP));
}

TEST_F(ScriptActionSupportFunctionsFixture, DoActionOverrideStartsResolvedActionEvenWhenUninterruptible)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6305);
    ASSERT_NE(actor, nullptr);

    const ModelAction current = actor->getCurrentAnimation();
    const ModelAction action = findValidAction(actor, {ACTION_WA, ACTION_WB, ACTION_WC, ACTION_DB, ACTION_DC, ACTION_DA}, current);
    ASSERT_NE(action, ACTION_COUNT);

    // "Override" means the action starts even if the current animation forbids interruption.
    GraphicsAccess::setCanBeInterrupted(actor->getGraphics(), false);

    script_state_t state;
    state.argument = static_cast<int>(action);
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_DoActionOverride(state, self));
    EXPECT_EQ(actor->getCurrentAnimation(), action);
}

TEST_F(ScriptActionSupportFunctionsFixture, ShowTimerEnablesModuleTimerWithGivenValue)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6306);
    ASSERT_NE(actor, nullptr);

    timeron = false;
    timervalue = 0;

    script_state_t state;
    state.argument = 1234;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_ShowTimer(state, self));
    EXPECT_TRUE(timeron);
    EXPECT_EQ(timervalue, 1234u);
}

} // namespace
