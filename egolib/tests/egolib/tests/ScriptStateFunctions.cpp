#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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

    std::shared_ptr<Object> makeRangedWeapon(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/weapons/xbow.obj",
            "mp_data/globalobjects/weapons/lbow.obj",
            "mp_data/globalobjects/weapons/pistol.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->getProfile()->isRangedWeapon())
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load a ranged-weapon fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeMeleeWeapon(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/weapons/stiletto.obj",
            "mp_data/globalobjects/weapons/broadsword.obj",
            "mp_data/globalobjects/weapons/mace.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && !item->getProfile()->isRangedWeapon() && item->getProfile()->getWeaponAction() != ACTION_PA)
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load a melee-weapon fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeShield(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/armor/atshield.obj",
            "mp_data/globalobjects/armor/kiteshield.obj",
            "mp_data/globalobjects/armor/rshield.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->getProfile()->getWeaponAction() == ACTION_PA)
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load a shield fixture";
        return nullptr;
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

TEST_F(ScriptStateFunctionsFixture, SetChildStateFailsWithoutChild)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5516);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 99;
    ai_state_t self = makeScriptSelf(actor);
    self.child = ObjectRef::Invalid;
    const int previousStateValue = actor->getAIStateValue();

    EXPECT_FALSE(scr_SetChildState(state, self));
    EXPECT_EQ(actor->getAIStateValue(), previousStateValue);
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

TEST_F(ScriptStateFunctionsFixture, SetChildContentFailsWithoutChild)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5526);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 98;
    ai_state_t self = makeScriptSelf(actor);
    self.child = ObjectRef::Invalid;
    const int previousContentValue = actor->getAIContent();

    EXPECT_FALSE(scr_SetChildContent(state, self));
    EXPECT_EQ(actor->getAIContent(), previousContentValue);
}

TEST_F(ScriptStateFunctionsFixture, PoofTargetDefersSelfPoofByOneUpdate)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5527);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(actor->getObjRef());

    const int32_t previousTargetPoofTime = actor->getAIPoofTime();
    const auto updateCount = static_cast<int32_t>(GameSessionContext::get().worldUpdateCount());

    EXPECT_TRUE(scr_PoofTarget(state, self));
    EXPECT_EQ(self.poof_time, updateCount + 1);
    EXPECT_EQ(actor->getAIPoofTime(), previousTargetPoofTime);
    EXPECT_EQ(self.getTarget(), actor->getObjRef());
}

TEST_F(ScriptStateFunctionsFixture, PoofTargetPublishesImmediatePoofTimeAndRetargetsToSelf)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5528);
    auto target = makeObject(module, "mp_objects/follower.obj", 5529);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    const int32_t previousSelfPoofTime = self.poof_time;
    const auto updateCount = static_cast<int32_t>(GameSessionContext::get().worldUpdateCount());

    EXPECT_TRUE(scr_PoofTarget(state, self));
    EXPECT_EQ(target->getAIPoofTime(), updateCount);
    EXPECT_EQ(self.poof_time, previousSelfPoofTime);
    EXPECT_EQ(self.getTarget(), actor->getObjRef());
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

TEST_F(ScriptStateFunctionsFixture, IfInvisibleReadsAlphaThroughRenderableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5536);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    actor->setAlpha(INVISIBLE);
    EXPECT_TRUE(scr_IfInvisible(state, self));

    actor->setAlpha(INVISIBLE + 1);
    EXPECT_FALSE(scr_IfInvisible(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfUnarmedReadsHeldSlotsThroughInventoryHolderRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5537);
    auto leftHandItem = makeObject(module, "mp_objects/follower.obj", 5538);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftHandItem, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfUnarmed(state, self));

    actor->setHeldObject(SLOT_LEFT, leftHandItem->getObjRef());
    EXPECT_FALSE(scr_IfUnarmed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfHoldingItemIDReadsHeldItemsThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5539);
    auto item = makeMeleeWeapon(module, 5540);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(item, nullptr);
    ASSERT_TRUE(item->attachToObject(actor, GRIP_RIGHT));

    script_state_t state;
    state.argument = item->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHoldingItemID(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfHoldingRangedWeaponUsesHeldSlotRoleAndAmmoGate)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5543);
    auto rightHandItem = makeRangedWeapon(module, 5544);
    auto leftHandItem = makeRangedWeapon(module, 5547);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(rightHandItem, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_TRUE(rightHandItem->attachToObject(actor, GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor, GRIP_LEFT));

    rightHandItem->setAmmo(0);
    if (leftHandItem->getAmmoMax() > 0)
    {
        leftHandItem->setAmmo(1);
    }

    script_state_t state;
    state.argument = 0;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHoldingRangedWeapon(state, self));
    EXPECT_EQ(state.argument, LATCHBUTTON_LEFT);
}

TEST_F(ScriptStateFunctionsFixture, IfHoldingMeleeWeaponPrefersRightHandThroughInventoryRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5553);
    auto rightHandItem = makeMeleeWeapon(module, 5554);
    auto leftHandItem = makeMeleeWeapon(module, 5557);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(rightHandItem, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_TRUE(rightHandItem->attachToObject(actor, GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor, GRIP_LEFT));

    script_state_t state;
    state.argument = 0;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHoldingMeleeWeapon(state, self));
    EXPECT_EQ(state.argument, LATCHBUTTON_RIGHT);
}

TEST_F(ScriptStateFunctionsFixture, IfHoldingShieldPrefersRightHandThroughInventoryRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5563);
    auto rightHandItem = makeShield(module, 5564);
    auto leftHandItem = makeShield(module, 5567);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(rightHandItem, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_TRUE(rightHandItem->attachToObject(actor, GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor, GRIP_LEFT));

    script_state_t state;
    state.argument = 0;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHoldingShield(state, self));
    EXPECT_EQ(state.argument, LATCHBUTTON_RIGHT);
}

TEST_F(ScriptStateFunctionsFixture, IfHeldInLeftHandReadsHolderSlotThroughInventoryRole)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 5573);
    auto heldItem = makeMeleeWeapon(module, 5574);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(holder, GRIP_LEFT));

    script_state_t state;
    ai_state_t self = makeScriptSelf(heldItem);

    EXPECT_TRUE(scr_IfHeldInLeftHand(state, self));
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
