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
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#undef private
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptTargetFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 41;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-target-function-tests.log";
        opts.logLevel = Log::Level::Warning;

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);

        setenv("EGOBOO_DISABLE_AUDIO", "1", 1);
        AudioSystem::initialize();
        ParticleHandler::initialize();
        EngineContext::get().installParticleHandler(ParticleHandler::get());
    }

    static void TearDownTestSuite()
    {
        EngineContext::get().clearParticleHandler();
        ParticleHandler::uninitialize();
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
        const bool began = session.beginModule(module, 41);
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
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptTargetFunctionsFixture::s_runtime;

TEST_F(ScriptTargetFunctionsFixture, OrderTargetPublishesOrderThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5301);
    auto target = makeObject(module, "mp_objects/follower.obj", 5302);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    state.argument = 77;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_OrderTarget(state, self));
    EXPECT_TRUE(target->hasAnyAIAlertBits(ALERTIF_ORDERED));
    EXPECT_EQ(Ego::Script::runtimeState(*target).order_value, 77u);
    EXPECT_EQ(Ego::Script::runtimeState(*target).order_counter, 0);
}

TEST_F(ScriptTargetFunctionsFixture, SetTargetToTargetHandsReadsThroughInventoryHolderRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5305);
    auto target = makeObject(module, "mp_objects/follower.obj", 5306);
    auto leftHandItem = makeObject(module, "mp_objects/follower.obj", 5307);
    auto rightHandItem = makeObject(module, "mp_objects/follower.obj", 5308);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_NE(rightHandItem, nullptr);

    IInventoryHolder& inventoryHolder = *target;
    inventoryHolder.setHeldObject(SLOT_LEFT, leftHandItem->getObjRef());
    inventoryHolder.setHeldObject(SLOT_RIGHT, rightHandItem->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_SetTargetToTargetLeftHand(state, self));
    EXPECT_EQ(self.getTarget(), leftHandItem->getObjRef());

    self.setTarget(target->getObjRef());
    EXPECT_TRUE(scr_SetTargetToTargetRightHand(state, self));
    EXPECT_EQ(self.getTarget(), rightHandItem->getObjRef());
}

TEST_F(ScriptTargetFunctionsFixture, TargetStateAndContentQueriesReadThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5311);
    auto target = makeObject(module, "mp_objects/follower.obj", 5312);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    IScriptable& scriptableTarget = *target;
    scriptableTarget.setAIStateValue(31);
    scriptableTarget.setAIContent(32);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GetTargetState(state, self));
    EXPECT_EQ(state.argument, 31);

    state.argument = 0;
    EXPECT_TRUE(scr_GetTargetContent(state, self));
    EXPECT_EQ(state.argument, 32);
}

TEST_F(ScriptTargetFunctionsFixture, TargetDamageTypeQueryReadsThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5321);
    auto target = makeObject(module, "mp_objects/follower.obj", 5322);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    IScriptable& scriptableTarget = *target;
    scriptableTarget.setAILastDamageType(DamageType::DAMAGE_FIRE);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_GetTargetDamageType(state, self));
    EXPECT_EQ(state.argument, static_cast<int>(DamageType::DAMAGE_FIRE));
}

TEST_F(ScriptTargetFunctionsFixture, IfTargetKilledReturnsFalseForLiveTargetAndTrueForDeadTarget)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5331);
    auto target = makeObject(module, "mp_objects/follower.obj", 5332);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_FALSE(scr_IfTargetKilled(state, self));

    IDamageable& damageableTarget = *target;
    damageableTarget.kill(actor, true);
    ASSERT_FALSE(target->isAlive());

    EXPECT_TRUE(scr_IfTargetKilled(state, self));
}

} // namespace
