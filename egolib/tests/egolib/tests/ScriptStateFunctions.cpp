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
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptStateFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 47;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-state-function-tests.log";
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
        const bool began = session.beginModule(module, 47);
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

std::unique_ptr<ContentRuntimeBootstrap> ScriptStateFunctionsFixture::s_runtime;

TEST_F(ScriptStateFunctionsFixture, SetStatePublishesStateThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5501);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 41;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetState(state, self));
    EXPECT_EQ(actor->getAIStateValue(), 41);
}

TEST_F(ScriptStateFunctionsFixture, SetChildStatePublishesStateThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5511);
    auto child = makeObject(module, "mp_objects/follower.obj", 5512);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(child, nullptr);

    script_state_t state;
    state.argument = 42;
    ai_state_t self = makeScriptSelf(actor);
    self.child = child->getObjRef();

    EXPECT_TRUE(scr_SetChildState(state, self));
    EXPECT_EQ(child->getAIStateValue(), 42);
}

TEST_F(ScriptStateFunctionsFixture, SetChildContentPublishesContentThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5521);
    auto child = makeObject(module, "mp_objects/follower.obj", 5522);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(child, nullptr);

    script_state_t state;
    state.argument = 43;
    ai_state_t self = makeScriptSelf(actor);
    self.child = child->getObjRef();

    EXPECT_TRUE(scr_SetChildContent(state, self));
    EXPECT_EQ(child->getAIContent(), 43);
}

TEST_F(ScriptStateFunctionsFixture, IfHolderBlockedReadsAlertAndLastAttackerThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5531);
    auto holder = makeObject(module, "mp_objects/follower.obj", 5532);
    auto attacker = makeObject(module, "mp_objects/follower.obj", 5533);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(attacker, nullptr);

    actor->setHolderRef(holder->getObjRef());
    holder->setAIAlertBits(ALERTIF_BLOCKED);
    holder->setAILastAttacker(attacker->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHolderBlocked(state, self));
    EXPECT_EQ(self.getTarget(), attacker->getObjRef());
}

TEST_F(ScriptStateFunctionsFixture, SetDamageTimePublishesThroughDamageableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5541);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 255;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SetDamageTime(state, self));
    EXPECT_EQ(actor->getDamageTimer(), 255);
}

TEST_F(ScriptStateFunctionsFixture, EnableAndDisableInvictusUseDamageableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5551);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    actor->setInvincible(false);
    EXPECT_TRUE(scr_EnableInvictus(state, self));
    EXPECT_TRUE(actor->isInvincible());

    EXPECT_TRUE(scr_DisableInvictus(state, self));
    EXPECT_FALSE(actor->isInvincible());
}

TEST_F(ScriptStateFunctionsFixture, SetFogFunctionsRespectInstalledConfigToggle)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5561);

    ASSERT_NE(actor, nullptr);

    auto& config = EngineContext::get().config();
    const bool previousFogEnabled = config.graphic_fog_enable.getValue();
    auto& fog = GameSessionContext::get().fog();
    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    state.argument = 30;
    fog._top = 0.0f;
    fog._bottom = 0.0f;
    fog._distance = 10.0f;

    config.graphic_fog_enable.setValue(false);
    EXPECT_TRUE(scr_SetFogLevel(state, self));
    EXPECT_FALSE(fog._on);

    config.graphic_fog_enable.setValue(true);
    EXPECT_TRUE(scr_SetFogLevel(state, self));
    EXPECT_TRUE(fog._on);

    state.argument = 10;
    EXPECT_TRUE(scr_SetFogBottomLevel(state, self));
    EXPECT_TRUE(fog._on);

    config.graphic_fog_enable.setValue(previousFogEnabled);
}

} // namespace
