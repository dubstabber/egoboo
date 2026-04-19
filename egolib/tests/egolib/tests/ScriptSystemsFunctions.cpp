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
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "egolib/vfs.h"

namespace
{

class ScriptSystemsFunctionsFixture : public ::testing::Test
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
        opts.randomSeed = 53;
        opts.binaryPath = "";
        opts.logPath = "/debug/script-systems-function-tests.log";
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
        const bool began = session.beginModule(module, 53);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    ai_state_t makeScriptSelf(const std::shared_ptr<Object>& selfObject,
                              const std::shared_ptr<Object>& targetObject = nullptr) const
    {
        ai_state_t self;
        self.setSelf(selfObject ? selfObject->getObjRef() : ObjectRef::Invalid);
        self.setTarget(targetObject ? targetObject->getObjRef() : ObjectRef::Invalid);
        return self;
    }

    std::shared_ptr<Object> makeAmmoItem(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/weapons/knife.obj",
            "mp_data/globalobjects/weapons/cknife.obj",
            "mp_data/globalobjects/items/gem.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->getAmmoMax() > 1)
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load an ammo-bearing item fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeInventoryItem(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/items/torch.obj",
            "mp_data/globalobjects/items/gem.obj",
            "mp_data/globalobjects/items/shovel.obj",
            "mp_data/globalobjects/armor/atshield.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->isAlive())
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load an inventory item fixture";
        return nullptr;
    }

    std::shared_ptr<Ego::Enchantment> addHealRemovableEnchant(GameModule& module,
                                                              const std::shared_ptr<Object>& target,
                                                              int slotBase) const
    {
        struct Candidate
        {
            const char* objectPath;
            const char* enchantPath;
        };

        static const std::vector<Candidate> candidates = {
            {"mp_data/globalobjects/weapons/stiletto.obj", "mp_data/globalobjects/weapons/stiletto.obj/enchant.txt"},
            {"mp_data/globalobjects/potions/ppotion.obj", "mp_data/globalobjects/potions/ppotion.obj/enchant.txt"},
            {"mp_data/globalobjects/items/mushroom.obj", "mp_data/globalobjects/items/mushroom.obj/enchant.txt"}
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto source = makeObject(module, candidates[i].objectPath, slotBase + static_cast<int>(i));
            if (!source)
            {
                continue;
            }

            const auto enchantRef = EngineContext::get().profileSystem().loadEnchantProfile(candidates[i].enchantPath,
                                                                                            INVALID_EVE_REF);
            auto enchant = target->addEnchant(enchantRef,
                                              source->getProfileID().get(),
                                              source,
                                              source);
            if (enchant)
            {
                return enchant;
            }
        }

        ADD_FAILURE() << "unable to add a [HEAL]-removable enchant fixture";
        return nullptr;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ScriptSystemsFunctionsFixture::s_runtime;

TEST_F(ScriptSystemsFunctionsFixture, CostTargetItemIDConsumesHeldAmmoThroughRoleLookups)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5601);
    auto target = makeObject(module, "mp_objects/follower.obj", 5602);
    auto heldItem = makeAmmoItem(module, 5603);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(target, GRIP_LEFT));

    heldItem->setAmmo(2);

    script_state_t state;
    state.argument = heldItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_CostTargetItemID(state, self));
    EXPECT_EQ(heldItem->getAmmo(), 1);
    EXPECT_FALSE(heldItem->isTerminated());
}

TEST_F(ScriptSystemsFunctionsFixture, CostTargetItemIDPoofsInventoryItemWhenOwnerMatchesTarget)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5611);
    auto inventoryItem = makeInventoryItem(module, 5612);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem, actor->getFirstFreeInventorySlot(), true));

    inventoryItem->setAmmo(1);

    script_state_t state;
    state.argument = inventoryItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, actor);

    EXPECT_TRUE(scr_CostTargetItemID(state, self));
    EXPECT_TRUE(inventoryItem->isTerminated());
    EXPECT_EQ(actor->getInventoryItem(0), nullptr);
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDAllUsesTargetHandsAndActorInventory)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5621);
    auto target = makeObject(module, "mp_objects/follower.obj", 5622);
    auto leftItem = makeAmmoItem(module, 5623);
    auto rightItem = makeAmmoItem(module, 5626);
    auto inventoryItem = makeAmmoItem(module, 5629);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target, GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target, GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem, actor->getFirstFreeInventorySlot(), true));

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);

    script_state_t state;
    state.argument = leftItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();
    ai_state_t self = makeScriptSelf(actor, target);

    EXPECT_TRUE(scr_RestockTargetAmmoIDAll(state, self));
    EXPECT_EQ(state.argument, 6);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax());
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax());
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax());
}

TEST_F(ScriptSystemsFunctionsFixture, RestockTargetAmmoIDFirstPreservesLeftRightThenActorInventoryOrder)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5631);
    auto target = makeObject(module, "mp_objects/follower.obj", 5632);
    auto leftItem = makeAmmoItem(module, 5633);
    auto rightItem = makeAmmoItem(module, 5636);
    auto inventoryItem = makeAmmoItem(module, 5639);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(inventoryItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(target, GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(target, GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(*actor, inventoryItem, actor->getFirstFreeInventorySlot(), true));

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor, target);
    const uint32_t matchingType = leftItem->getProfile()->getIDSZ(IDSZ_TYPE).toUint32();

    leftItem->setAmmo(leftItem->getAmmoMax() - 1);
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 1);
    EXPECT_EQ(leftItem->getAmmo(), leftItem->getAmmoMax());
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax() - 2);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);

    leftItem->setAmmo(leftItem->getAmmoMax());
    rightItem->setAmmo(rightItem->getAmmoMax() - 2);
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 2);
    EXPECT_EQ(rightItem->getAmmo(), rightItem->getAmmoMax());
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax() - 3);

    leftItem->setAmmo(leftItem->getAmmoMax());
    rightItem->setAmmo(rightItem->getAmmoMax());
    inventoryItem->setAmmo(inventoryItem->getAmmoMax() - 3);
    state.argument = matchingType;
    EXPECT_TRUE(scr_RestockTargetAmmoIDFirst(state, self));
    EXPECT_EQ(state.argument, 3);
    EXPECT_EQ(inventoryItem->getAmmo(), inventoryItem->getAmmoMax());
}

TEST_F(ScriptSystemsFunctionsFixture, DamageAndKillTargetUseDamageableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5641);
    auto damageTarget = makeObject(module, "mp_objects/follower.obj", 5642);
    auto killTarget = makeObject(module, "mp_objects/follower.obj", 5643);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(damageTarget, nullptr);
    ASSERT_NE(killTarget, nullptr);

    actor->setDamageTargetType(DamageType::DAMAGE_FIRE);
    auto& config = EngineContext::get().config();
    const auto previousFeedback = config.hud_feedback.getValue();
    config.hud_feedback.setValue(Ego::FeedbackType::None);

    script_state_t state;
    state.argument = 128;
    ai_state_t self = makeScriptSelf(actor, damageTarget);

    const float initialLife = damageTarget->getLife();
    EXPECT_TRUE(scr_DamageTarget(state, self));
    EXPECT_LT(damageTarget->getLife(), initialLife);

    self.setTarget(killTarget->getObjRef());
    EXPECT_TRUE(scr_KillTarget(state, self));
    EXPECT_FALSE(killTarget->isAlive());

    config.hud_feedback.setValue(previousFeedback);
}

TEST_F(ScriptSystemsFunctionsFixture, HealSelfAndTargetUseDamageableRoleAndPreserveHealEnchantCleanup)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5651);
    auto target = makeObject(module, "mp_objects/follower.obj", 5652);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->setLife(-19.0f);
    target->setLife(-19.0f);

    auto enchant = addHealRemovableEnchant(module, target, 5653);
    ASSERT_NE(enchant, nullptr);
    ASSERT_TRUE(target->hasActiveEnchants());

    script_state_t state;
    state.argument = 512;
    ai_state_t self = makeScriptSelf(actor, target);

    const float actorLifeBeforeHeal = actor->getLife();
    EXPECT_TRUE(scr_HealSelf(state, self));
    EXPECT_GT(actor->getLife(), actorLifeBeforeHeal);

    const float targetLifeBeforeHeal = target->getLife();
    EXPECT_TRUE(scr_HealTarget(state, self));
    EXPECT_GT(target->getLife(), targetLifeBeforeHeal);
    ASSERT_TRUE(target->hasActiveEnchants());
    ASSERT_NE(target->getFirstActiveEnchant(), nullptr);
    EXPECT_TRUE(target->getFirstActiveEnchant()->isTerminated());
}

} // namespace
