/// @file ScriptLocomotionFunctions.cpp
/// @brief Characterization tests for the script locomotion-mode family: the four maxSpeed
///        setters scr_Run/scr_Walk/scr_Sneak/scr_Stop (script_functions_movement_locomotion.c)
///        and the animation keep pair scr_KeepAction/scr_UnkeepAction
///        (script_functions_action.c). None of the six had any test reference before this file.

#include "gtest/gtest.h"

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
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"
#include "ObjectGraphicsTestAccess.hpp"

namespace
{

using GraphicsAccess = Ego::Graphics::ObjectGraphicsTestAccess;
using ScriptFunction = uint8_t (*)(script_state_t&, ai_state_t&);

struct LocomotionModeCase
{
    const char* name;
    ScriptFunction function;
    float maxSpeed;
};

/// Each locomotion mode publishes a fixed fraction of the character's maximum acceleration
/// into the script runtime's maxSpeed.
const LocomotionModeCase kLocomotionModeCases[] = {
    {"Run", scr_Run, 1.0f},
    {"Walk", scr_Walk, 0.66f},
    {"Sneak", scr_Sneak, 0.33f},
    {"Stop", scr_Stop, 0.0f},
};

class ScriptLocomotionFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 62;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-locomotion-function-tests.log";
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
        const bool began = session.beginModule(module, 62);
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

std::unique_ptr<ContentRuntimeBootstrap> ScriptLocomotionFunctionsFixture::s_runtime;

TEST_F(ScriptLocomotionFunctionsFixture, LocomotionModesPublishExactMaxSpeedFractions)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6201);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    for (const LocomotionModeCase& testCase : kLocomotionModeCases)
    {
        SCOPED_TRACE(testCase.name);

        self.maxSpeed = -1.0f;
        EXPECT_TRUE(testCase.function(state, self));
        EXPECT_FLOAT_EQ(self.maxSpeed, testCase.maxSpeed);
    }
}

TEST_F(ScriptLocomotionFunctionsFixture, LocomotionModesLeaveMaxSpeedUntouchedForUnresolvedSelf)
{
    beginActiveTestModule();

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    for (const LocomotionModeCase& testCase : kLocomotionModeCases)
    {
        SCOPED_TRACE(testCase.name);

        self.maxSpeed = 0.5f;
        EXPECT_FALSE(testCase.function(state, self));
        EXPECT_FLOAT_EQ(self.maxSpeed, 0.5f);
    }
}

TEST_F(ScriptLocomotionFunctionsFixture, KeepActionFreezesAnimationAndUnkeepReleasesIt)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 6202);
    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    actor->getGraphics().setActionKeep(false);

    EXPECT_TRUE(scr_KeepAction(state, self));
    EXPECT_TRUE(GraphicsAccess::freezeAtLastFrame(actor->getGraphics()));

    EXPECT_TRUE(scr_UnkeepAction(state, self));
    EXPECT_FALSE(GraphicsAccess::freezeAtLastFrame(actor->getGraphics()));
}

TEST_F(ScriptLocomotionFunctionsFixture, KeepActionPairReturnsFalseForUnresolvedSelf)
{
    auto& module = beginActiveTestModule();
    auto bystander = makeObject(module, "mp_objects/follower.obj", 6203);
    ASSERT_NE(bystander, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(nullptr);

    bystander->getGraphics().setActionKeep(true);

    EXPECT_FALSE(scr_KeepAction(state, self));
    EXPECT_FALSE(scr_UnkeepAction(state, self));

    // No resolved self, so no animation state anywhere was touched.
    EXPECT_TRUE(GraphicsAccess::freezeAtLastFrame(bystander->getGraphics()));
}

} // namespace
