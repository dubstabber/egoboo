#include "gtest/gtest.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

    std::optional<Ego::Vector3f> findSpawnCharacterPosition(const std::string& profilePath,
                                                            int slot,
                                                            bool requireSafe,
                                                            std::optional<Ego::Vector3f> excludedPosition = std::nullopt)
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }

        auto& module = beginActiveTestModule();
        const ObjectProfileRef profile = loadProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            session.quitModule();
            return std::nullopt;
        }

        const std::vector<Ego::Vector3f> candidates = {
            Ego::Vector3f(64.0f, 64.0f, 0.0f),
            Ego::Vector3f(96.0f, 64.0f, 0.0f),
            Ego::Vector3f(64.0f, 96.0f, 0.0f),
            Ego::Vector3f(128.0f, 64.0f, 0.0f),
            Ego::Vector3f(128.0f, 96.0f, 0.0f),
            Ego::Vector3f(160.0f, 96.0f, 0.0f),
        };

        const auto matchesCandidate = [&](const Ego::Vector3f& position) -> bool
        {
            if (excludedPosition.has_value() && position == *excludedPosition)
            {
                return false;
            }

            auto probe = module.spawnObject(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
            if (!probe)
            {
                return false;
            }

            const bool matches = probe->hasSafePosition() == requireSafe;
            probe->requestTerminate();
            return matches;
        };

        for (const auto& position : candidates)
        {
            if (matchesCandidate(position))
            {
                session.quitModule();
                return position;
            }
        }

        for (int y = 32; y <= 256; y += 16)
        {
            for (int x = 32; x <= 256; x += 16)
            {
                const Ego::Vector3f position(static_cast<float>(x), static_cast<float>(y), 0.0f);
                if (matchesCandidate(position))
                {
                    session.quitModule();
                    return position;
                }
            }
        }

        session.quitModule();
        return std::nullopt;
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

    int findFirstLocalParticleProfile(const Object& object) const
    {
        const std::shared_ptr<ObjectProfile> profile = object.getProfile();
        if (!profile)
        {
            return -1;
        }

        for (LocalParticleProfileRef index(0); index.get() < 30; ++index)
        {
            if (profile->getParticleProfile(index) != INVALID_PIP_REF)
            {
                return index.get();
            }
        }

        return -1;
    }

    void clearParticles() const
    {
        ParticleHandler::get().clear();
    }

    std::shared_ptr<Ego::Particle> latestSpawnedParticle() const
    {
        auto& handler = ParticleHandler::get();
        if (!handler._pendingParticles.empty())
        {
            return handler._pendingParticles.back();
        }

        if (!handler._activeParticles.empty())
        {
            return handler._activeParticles.back();
        }

        return nullptr;
    }

    std::vector<ObjectRef> collectObjectRefs(GameModule& module) const
    {
        std::vector<ObjectRef> refs;
        for (const auto& object : module.getObjectHandler().getAllObjects())
        {
            if (object != nullptr)
            {
                refs.push_back(object->getObjRef());
            }
        }

        return refs;
    }

    std::shared_ptr<Object> findNewObject(GameModule& module,
                                          const std::vector<ObjectRef>& existingRefs) const
    {
        for (const auto& object : module.getObjectHandler().getAllObjects())
        {
            if (object == nullptr)
            {
                continue;
            }

            bool knownRef = false;
            for (const ObjectRef& ref : existingRefs)
            {
                if (ref == object->getObjRef())
                {
                    knownRef = true;
                    break;
                }
            }

            if (!knownRef)
            {
                return object;
            }
        }

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

TEST_F(ScriptStateFunctionsFixture, SetChildAmmoPublishesAmmoThroughChildCharacterState)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55261);
    auto child = makeRangedWeapon(module, 55262);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(child, nullptr);

    script_state_t state;
    state.argument = 7;
    ai_state_t self = makeScriptSelf(actor);
    self.child = child->getObjRef();

    EXPECT_TRUE(scr_SetChildAmmo(state, self));
    EXPECT_EQ(child->getAmmo(), 7);
}

TEST_F(ScriptStateFunctionsFixture, SetChildAmmoFailsWithoutChild)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55266);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.argument = 19;
    ai_state_t self = makeScriptSelf(actor);
    self.child = ObjectRef::Invalid;

    EXPECT_FALSE(scr_SetChildAmmo(state, self));
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

TEST_F(ScriptStateFunctionsFixture, GoPoofUsesPlayerImmunityAndPublishesSelfPoofTime)
{
    auto& module = beginActiveTestModule();
    auto nonPlayer = makeObject(module, "mp_objects/follower.obj", 55291);
    auto player = makeObject(module, "mp_data/globalobjects/players/ranger.obj", 55292);

    ASSERT_NE(nonPlayer, nullptr);
    ASSERT_NE(player, nullptr);

    script_state_t state;
    ai_state_t nonPlayerSelf = makeScriptSelf(nonPlayer);
    ai_state_t playerSelf = makeScriptSelf(player);
    const auto updateCount = static_cast<int32_t>(GameSessionContext::get().worldUpdateCount());
    const int32_t previousPlayerPoofTime = playerSelf.poof_time;

    player->setLocalPlayer(true);

    EXPECT_TRUE(scr_GoPoof(state, nonPlayerSelf));
    EXPECT_EQ(nonPlayerSelf.poof_time, updateCount);

    EXPECT_FALSE(scr_GoPoof(state, playerSelf));
    EXPECT_EQ(playerSelf.poof_time, previousPlayerPoofTime);
}

TEST_F(ScriptStateFunctionsFixture, RespawnHelpersUseLifecycleRoleAndPreserveTargetPosition)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55293);
    auto target = makeObject(module, "mp_objects/follower.obj", 55294);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    actor->kill(ObjectAttribution(), true);
    target->kill(ObjectAttribution(), true);
    const Ego::Vector3f targetPositionBeforeRespawn(88.0f, 44.0f, 3.0f);
    target->setPosition(targetPositionBeforeRespawn);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_RespawnCharacter(state, self));
    EXPECT_TRUE(actor->isAlive());

    EXPECT_TRUE(scr_RespawnTarget(state, self));
    EXPECT_TRUE(target->isAlive());
    EXPECT_FLOAT_EQ(target->getPosition().x(), targetPositionBeforeRespawn.x());
    EXPECT_FLOAT_EQ(target->getPosition().y(), targetPositionBeforeRespawn.y());
    EXPECT_FLOAT_EQ(target->getPosition().z(), targetPositionBeforeRespawn.z());
}

TEST_F(ScriptStateFunctionsFixture, DropWeaponsDetachesHeldItemsThroughRefLookups)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5530);
    auto leftItem = makeMeleeWeapon(module, 5531);
    auto rightItem = makeShield(module, 5534);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(actor->getObjRef(), GRIP_RIGHT));

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_DropWeapons(state, self));
    EXPECT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    EXPECT_EQ(actor->getHeldObject(SLOT_RIGHT), ObjectRef::Invalid);
    EXPECT_EQ(leftItem->getHolderRef(), ObjectRef::Invalid);
    EXPECT_EQ(rightItem->getHolderRef(), ObjectRef::Invalid);
}

TEST_F(ScriptStateFunctionsFixture, LifecycleMutationHelpersUseRoleLookups)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55305);
    auto leftItem = makeMeleeWeapon(module, 55306);
    auto rightItem = makeShield(module, 55309);
    auto keyItem = makeObject(module, "mp_data/globalobjects/items/keya.obj", 55312);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(keyItem, nullptr);
    ASSERT_TRUE(leftItem->attachToObject(actor->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(rightItem->attachToObject(actor->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(Inventory::add_item(actor->getObjRef(), keyItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_DropItems(state, self));
    EXPECT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    EXPECT_EQ(actor->getHeldObject(SLOT_RIGHT), ObjectRef::Invalid);

    actor->setItem(true);
    EXPECT_TRUE(scr_NotAnItem(state, self));
    EXPECT_FALSE(actor->isItem());

    EXPECT_TRUE(scr_MakeCrushValid(state, self));
    EXPECT_TRUE(actor->canBeCrushed());

    EXPECT_TRUE(scr_MakeCrushInvalid(state, self));
    EXPECT_FALSE(actor->canBeCrushed());

    state.argument = 9;
    EXPECT_TRUE(scr_SetDamageThreshold(state, self));
    EXPECT_EQ(actor->getDamageThreshold(), 9);

    EXPECT_TRUE(scr_DropKeys(state, self));
    EXPECT_EQ(keyItem->getInventoryHolderRef(), ObjectRef::Invalid);
}

TEST_F(ScriptStateFunctionsFixture, DetachFromHolderRequiresLiveHolderRef)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 55341);
    auto heldItem = makeMeleeWeapon(module, 55342);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(holder->getObjRef(), GRIP_LEFT));

    script_state_t state;
    ai_state_t self = makeScriptSelf(heldItem);

    EXPECT_TRUE(scr_DetachFromHolder(state, self));
    EXPECT_EQ(heldItem->getHolderRef(), ObjectRef::Invalid);

    EXPECT_FALSE(scr_DetachFromHolder(state, self));
}

TEST_F(ScriptStateFunctionsFixture, SpawnPoofUsesRefResolvedSelfObject)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/ranger.obj", 5538);

    ASSERT_NE(actor, nullptr);
    ASSERT_GT(actor->getProfile()->getParticlePoofAmount(), 0);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    IParticleHandler& particleHandler = EngineContext::get().particleHandler();
    const size_t particleCountBefore = particleHandler.getCount();

    EXPECT_TRUE(scr_SpawnPoof(state, self));
    EXPECT_GT(particleHandler.getCount(), particleCountBefore);
}

TEST_F(ScriptStateFunctionsFixture, SpawnPoofFailsWhenSelfIsNoLongerLive)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/ranger.obj", 55381);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    IParticleHandler& particleHandler = EngineContext::get().particleHandler();
    const size_t particleCountBefore = particleHandler.getCount();

    actor->requestTerminate();

    EXPECT_FALSE(scr_SpawnPoof(state, self));
    EXPECT_EQ(particleHandler.getCount(), particleCountBefore);
}

TEST_F(ScriptStateFunctionsFixture, SpawnPoofSpeedSpacingDamageUsesSelfProfileContext)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_data/globalobjects/players/ranger.obj", 55382);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(actor->getProfile(), nullptr);
    ASSERT_GT(actor->getProfile()->getParticlePoofAmount(), 0);
    ASSERT_NE(actor->getProfile()->getParticlePoofProfile(), INVALID_PIP_REF);

    actor->setTeam(Team::TEAM_GOOD);
    actor->setAIOwner(ObjectRef(777));

    clearParticles();

    script_state_t state;
    state.x = 3;
    state.y = 5;
    state.argument = FLOAT_TO_FP8(2.0f);
    ai_state_t self = makeScriptSelf(actor);

    IParticleHandler& particleHandler = EngineContext::get().particleHandler();
    EXPECT_TRUE(scr_SpawnPoofSpeedSpacingDamage(state, self));
    EXPECT_GT(particleHandler.getCount(), 0u);

    const auto particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, ObjectRef(777));
    EXPECT_EQ(particle->team, actor->getTeamRef());
    EXPECT_EQ(particle->getSpawnerProfile(), actor->getProfileID());
    EXPECT_FLOAT_EQ(particle->damage.base, FP8_TO_FLOAT(state.argument));
}

TEST_F(ScriptStateFunctionsFixture, SpawnParticleUsesResolvedHolderOwnerAndKeepsSelfAttachment)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 55370);
    auto actor = makeMeleeWeapon(module, 55371);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(actor, nullptr);
    ASSERT_TRUE(actor->attachToObject(holder->getObjRef(), GRIP_LEFT));

    const int particleProfileIndex = findFirstLocalParticleProfile(*actor);
    ASSERT_GE(particleProfileIndex, 0);

    clearParticles();

    script_state_t state;
    state.argument = particleProfileIndex;
    state.distance = 0;
    state.x = 4;
    state.y = 0;
    state.turn = 0;

    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SpawnParticle(state, self));

    const auto particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), actor->getObjRef());
    EXPECT_EQ(particle->getSpawnerProfile(), actor->getProfileID());
}

TEST_F(ScriptStateFunctionsFixture, SpawnCharacterPublishesChildStateThroughRoleSurfaces)
{
    const auto actorPosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55388, true);
    ASSERT_TRUE(actorPosition.has_value());

    const auto safePosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55389, true, actorPosition);
    ASSERT_TRUE(safePosition.has_value());

    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55388, *actorPosition);

    ASSERT_NE(actor, nullptr);

    actor->setFacingZ(Facing(1024));
    actor->setKursed(true);

    script_state_t state;
    state.x = static_cast<int>(safePosition->x());
    state.y = static_cast<int>(safePosition->y());
    state.turn = 2048;
    state.distance = 6;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(77);
    self.passage = 13;

    EXPECT_TRUE(scr_SpawnCharacter(state, self));
    ASSERT_NE(self.child, ObjectRef::Invalid);

    auto child = module.getObjectHandler().get(self.child);
    ASSERT_NE(child, nullptr);

    const Facing velocityFacing = actor->getFacingZ() + ATK_BEHIND;
    EXPECT_EQ(child->getProfileID(), actor->getProfileID());
    EXPECT_TRUE(child->isKursed());
    EXPECT_EQ(child->getAIOwner(), self.owner);
    EXPECT_EQ(child->getAIPassage(), self.passage);
    EXPECT_EQ(child->getDismountTimer(), Object::PHYS_DISMOUNT_TIME);
    EXPECT_EQ(child->getDismountObject(), actor->getObjRef());
    EXPECT_FLOAT_EQ(child->getVelocity().x(), std::cos(velocityFacing) * state.distance);
    EXPECT_FLOAT_EQ(child->getVelocity().y(), std::sin(velocityFacing) * state.distance);
    EXPECT_FLOAT_EQ(child->getVelocity().z(), 0.0f);
}

TEST_F(ScriptStateFunctionsFixture, SpawnCharacterLeavesChildInvalidWhenSpawnedIntoUnsafeLocation)
{
    const auto unsafePosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55389, false);
    ASSERT_TRUE(unsafePosition.has_value());

    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55389);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    state.x = static_cast<int>(unsafePosition->x());
    state.y = static_cast<int>(unsafePosition->y());

    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_SpawnCharacter(state, self));
    EXPECT_EQ(self.child, ObjectRef::Invalid);
}

TEST_F(ScriptStateFunctionsFixture, SpawnCharacterXYZPublishesChildStateThroughRoleSurfaces)
{
    const auto actorPosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55390, true);
    ASSERT_TRUE(actorPosition.has_value());

    const auto exactPosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55391, true, actorPosition);
    ASSERT_TRUE(exactPosition.has_value());

    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55390, *actorPosition);

    ASSERT_NE(actor, nullptr);

    actor->setKursed(true);

    script_state_t state;
    state.x = static_cast<int>(exactPosition->x());
    state.y = static_cast<int>(exactPosition->y());
    state.distance = 12;
    state.turn = 33333;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(91);
    self.passage = 15;

    EXPECT_TRUE(scr_SpawnCharacterXYZ(state, self));
    ASSERT_NE(self.child, ObjectRef::Invalid);

    auto child = module.getObjectHandler().get(self.child);
    ASSERT_NE(child, nullptr);

    EXPECT_EQ(child->getProfileID(), actor->getProfileID());
    EXPECT_TRUE(child->isKursed());
    EXPECT_EQ(child->getAIOwner(), self.owner);
    EXPECT_EQ(child->getAIPassage(), self.passage);
    EXPECT_EQ(child->getDismountTimer(), Object::PHYS_DISMOUNT_TIME);
    EXPECT_EQ(child->getDismountObject(), actor->getObjRef());
    EXPECT_EQ(child->getFacingZ(), Facing(Ego::Math::clipBits<16>(state.turn)));
    EXPECT_FLOAT_EQ(child->getPosX(), static_cast<float>(state.x));
    EXPECT_FLOAT_EQ(child->getPosY(), static_cast<float>(state.y));
    EXPECT_FLOAT_EQ(child->getPosZ(), static_cast<float>(state.distance));
    EXPECT_FLOAT_EQ(child->getVelocity().x(), 0.0f);
    EXPECT_FLOAT_EQ(child->getVelocity().y(), 0.0f);
    EXPECT_FLOAT_EQ(child->getVelocity().z(), 0.0f);
}

TEST_F(ScriptStateFunctionsFixture, SpawnExactCharacterXYZPublishesRequestedProfileAndChildState)
{
    const auto actorPosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55392, true);
    ASSERT_TRUE(actorPosition.has_value());

    const auto exactPosition = findSpawnCharacterPosition("mp_objects/follower.obj", 55393, true, actorPosition);
    ASSERT_TRUE(exactPosition.has_value());

    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55392, *actorPosition);

    ASSERT_NE(actor, nullptr);

    const ObjectProfileRef requestedProfile = loadProfile("mp_data/globalobjects/players/ranger.obj", 55394);
    ASSERT_NE(requestedProfile, ObjectProfileRef::Invalid);

    actor->setKursed(true);

    script_state_t state;
    state.argument = requestedProfile.get();
    state.x = static_cast<int>(exactPosition->x());
    state.y = static_cast<int>(exactPosition->y());
    state.distance = 18;
    state.turn = 44444;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(92);
    self.passage = 16;

    EXPECT_TRUE(scr_SpawnExactCharacterXYZ(state, self));
    ASSERT_NE(self.child, ObjectRef::Invalid);

    auto child = module.getObjectHandler().get(self.child);
    ASSERT_NE(child, nullptr);

    EXPECT_EQ(child->getProfileID(), requestedProfile);
    EXPECT_TRUE(child->isKursed());
    EXPECT_EQ(child->getAIOwner(), self.owner);
    EXPECT_EQ(child->getAIPassage(), self.passage);
    EXPECT_EQ(child->getDismountTimer(), Object::PHYS_DISMOUNT_TIME);
    EXPECT_EQ(child->getDismountObject(), actor->getObjRef());
    EXPECT_EQ(child->getFacingZ(), Facing(Ego::Math::clipBits<16>(state.turn)));
    EXPECT_FLOAT_EQ(child->getPosX(), static_cast<float>(state.x));
    EXPECT_FLOAT_EQ(child->getPosY(), static_cast<float>(state.y));
    EXPECT_FLOAT_EQ(child->getPosZ(), static_cast<float>(state.distance));
    EXPECT_FLOAT_EQ(child->getVelocity().x(), 0.0f);
    EXPECT_FLOAT_EQ(child->getVelocity().y(), 0.0f);
    EXPECT_FLOAT_EQ(child->getVelocity().z(), 0.0f);
}

TEST_F(ScriptStateFunctionsFixture, AttachedParticleHelpersUseResolvedOwnerFallbacks)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 55395);
    auto actor = makeMeleeWeapon(module, 55396);
    auto target = makeObject(module, "mp_objects/follower.obj", 55399);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_TRUE(actor->attachToObject(holder->getObjRef(), GRIP_LEFT));

    const int particleProfileIndex = findFirstLocalParticleProfile(*actor);
    ASSERT_GE(particleProfileIndex, 0);

    script_state_t state;
    state.argument = particleProfileIndex;
    state.distance = 0;
    state.turn = 777;
    state.x = 96;
    state.y = 128;

    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    clearParticles();
    EXPECT_TRUE(scr_SpawnAttachedParticle(state, self));
    auto particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), actor->getObjRef());

    clearParticles();
    EXPECT_TRUE(scr_SpawnAttachedSizedParticle(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), actor->getObjRef());
    EXPECT_EQ(particle->size, state.turn);

    clearParticles();
    EXPECT_TRUE(scr_SpawnAttachedFacedParticle(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), actor->getObjRef());

    clearParticles();
    EXPECT_TRUE(scr_SpawnAttachedHolderParticle(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), holder->getObjRef());

    clearParticles();
    EXPECT_TRUE(scr_SpawnExactParticle(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getAttachedObjectID(), ObjectRef::Invalid);

    clearParticles();
    EXPECT_TRUE(scr_SpawnExactChaseParticle(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->getTargetID(), target->getObjRef());

    clearParticles();
    EXPECT_TRUE(scr_SpawnExactParticleEndSpawn(state, self));
    particle = latestSpawnedParticle();
    ASSERT_NE(particle, nullptr);
    EXPECT_EQ(particle->owner_ref, holder->getObjRef());
    EXPECT_EQ(particle->endspawn_characterstate, state.turn);
}

TEST_F(ScriptStateFunctionsFixture, SpawnAttachedCharacterPreservesInventoryAndWieldAttachBehavior)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55400);
    auto target = makeObject(module, "mp_objects/follower.obj", 55401);
    auto existingRightItem = makeMeleeWeapon(module, 55402);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(existingRightItem, nullptr);
    ASSERT_TRUE(existingRightItem->attachToObject(target->getObjRef(), GRIP_RIGHT));

    const ObjectProfileRef requestedProfile = loadProfile("mp_data/globalobjects/weapons/stiletto.obj", 55410);
    ASSERT_NE(requestedProfile, ObjectProfileRef::Invalid);

    script_state_t state;
    state.argument = requestedProfile.get();
    state.x = 72;
    state.y = 84;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(314);
    self.passage = 27;
    self.setTarget(target->getObjRef());

    const auto firstInventorySlot = target->getFirstFreeInventorySlot();
    state.distance = ATTACH_INVENTORY;
    EXPECT_TRUE(scr_SpawnAttachedCharacter(state, self));
    ASSERT_NE(self.child, ObjectRef::Invalid);

    auto inventoryChild = module.getObjectHandler().get(self.child);
    ASSERT_NE(inventoryChild, nullptr);
    ASSERT_NE(target->getInventoryItemRef(firstInventorySlot), ObjectRef::Invalid);
    EXPECT_EQ(target->getInventoryItemRef(firstInventorySlot), inventoryChild->getObjRef());
    EXPECT_EQ(inventoryChild->getInventoryHolderRef(), target->getObjRef());
    EXPECT_EQ(inventoryChild->getAIOwner(), self.owner);
    EXPECT_EQ(inventoryChild->getAIPassage(), self.passage);

    self.child = ObjectRef::Invalid;
    state.distance = ATTACH_LEFT;
    EXPECT_TRUE(scr_SpawnAttachedCharacter(state, self));
    ASSERT_NE(self.child, ObjectRef::Invalid);
    EXPECT_EQ(target->getHeldObject(SLOT_LEFT), self.child);
    EXPECT_EQ(target->getHeldObject(SLOT_RIGHT), existingRightItem->getObjRef());
}

TEST_F(ScriptStateFunctionsFixture, SpawnAttachedCharacterTerminatesChildWhenTargetInventoryIsFull)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55412);
    auto target = makeObject(module, "mp_objects/follower.obj", 55413);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    std::vector<std::shared_ptr<Object>> fillerItems;
    while (target->getFirstFreeInventorySlot() < target->getInventoryMaxItems())
    {
        auto filler = makeMeleeWeapon(module, 55420 + static_cast<int>(fillerItems.size()) * 10);
        ASSERT_NE(filler, nullptr);
        ASSERT_TRUE(Inventory::add_item(target->getObjRef(),
                                        filler->getObjRef(),
                                        target->getFirstFreeInventorySlot(),
                                        true));
        fillerItems.push_back(filler);
    }

    const ObjectProfileRef requestedProfile = loadProfile("mp_data/globalobjects/weapons/stiletto.obj", 55490);
    ASSERT_NE(requestedProfile, ObjectProfileRef::Invalid);

    const auto refsBefore = collectObjectRefs(module);
    const size_t inventoryItemCountBefore = target->getInventoryItemRefs().size();
    const size_t objectCountBefore = module.getObjectHandler().getObjectCount();

    script_state_t state;
    state.argument = requestedProfile.get();
    state.x = 72;
    state.y = 84;
    state.distance = ATTACH_INVENTORY;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(401);
    self.passage = 31;
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_SpawnAttachedCharacter(state, self));
    EXPECT_EQ(self.child, ObjectRef::Invalid);
    EXPECT_EQ(target->getInventoryItemRefs().size(), inventoryItemCountBefore);
    EXPECT_EQ(module.getObjectHandler().getObjectCount(), objectCountBefore);
    EXPECT_EQ(findNewObject(module, refsBefore), nullptr);
}

TEST_F(ScriptStateFunctionsFixture, SpawnAttachedCharacterTerminatesChildWhenGripAlreadyUsed)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55491);
    auto target = makeObject(module, "mp_objects/follower.obj", 55492);
    auto existingLeftItem = makeMeleeWeapon(module, 55493);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(existingLeftItem, nullptr);
    ASSERT_TRUE(existingLeftItem->attachToObject(target->getObjRef(), GRIP_LEFT));

    const ObjectProfileRef requestedProfile = loadProfile("mp_data/globalobjects/weapons/stiletto.obj", 55500);
    ASSERT_NE(requestedProfile, ObjectProfileRef::Invalid);

    const auto refsBefore = collectObjectRefs(module);
    const ObjectRef previousLeftHand = target->getHeldObject(SLOT_LEFT);
    const size_t objectCountBefore = module.getObjectHandler().getObjectCount();

    script_state_t state;
    state.argument = requestedProfile.get();
    state.x = 96;
    state.y = 108;
    state.distance = ATTACH_LEFT;

    ai_state_t self = makeScriptSelf(actor);
    self.owner = ObjectRef(402);
    self.passage = 32;
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_SpawnAttachedCharacter(state, self));
    EXPECT_EQ(self.child, ObjectRef::Invalid);
    EXPECT_EQ(target->getHeldObject(SLOT_LEFT), previousLeftHand);
    EXPECT_EQ(module.getObjectHandler().getObjectCount(), objectCountBefore);
    EXPECT_EQ(findNewObject(module, refsBefore), nullptr);
}

TEST_F(ScriptStateFunctionsFixture, RespawnToggleHelpersUseModuleWrapper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55420);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    module.setRespawnValid(false);
    EXPECT_TRUE(scr_EnableRespawn(state, self));
    EXPECT_TRUE(module.isRespawnValid());

    EXPECT_TRUE(scr_DisableRespawn(state, self));
    EXPECT_FALSE(module.isRespawnValid());
}

TEST_F(ScriptStateFunctionsFixture, CleanUpTouchesOnlySameTeamAndOnlyTimersDeadListeners)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55381);
    auto aliveTeammate = makeObject(module, "mp_objects/follower.obj", 55382);
    auto deadTeammate = makeObject(module, "mp_objects/follower.obj", 55383);
    auto outsider = makeObject(module, "mp_objects/follower.obj", 55384);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(aliveTeammate, nullptr);
    ASSERT_NE(deadTeammate, nullptr);
    ASSERT_NE(outsider, nullptr);

    actor->setTeam(Team::TEAM_GOOD);
    aliveTeammate->setTeam(Team::TEAM_GOOD);
    deadTeammate->setTeam(Team::TEAM_GOOD);
    outsider->setTeam(Team::TEAM_EVIL);
    deadTeammate->kill(ObjectAttribution(), true);

    actor->setAIAlertBits(0);
    aliveTeammate->setAIAlertBits(0);
    deadTeammate->setAIAlertBits(0);
    outsider->setAIAlertBits(0);
    actor->setAITimer(0);
    aliveTeammate->setAITimer(0);
    deadTeammate->setAITimer(0);
    outsider->setAITimer(0);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    const auto updateCount = GameSessionContext::get().worldUpdateCount();

    EXPECT_TRUE(scr_CleanUp(state, self));
    EXPECT_NE(actor->getAIAlertBits() & ALERTIF_CLEANEDUP, 0u);
    EXPECT_NE(aliveTeammate->getAIAlertBits() & ALERTIF_CLEANEDUP, 0u);
    EXPECT_NE(deadTeammate->getAIAlertBits() & ALERTIF_CLEANEDUP, 0u);
    EXPECT_EQ(outsider->getAIAlertBits() & ALERTIF_CLEANEDUP, 0u);
    EXPECT_EQ(actor->getAITimer(), 0u);
    EXPECT_EQ(aliveTeammate->getAITimer(), 0u);
    EXPECT_EQ(deadTeammate->getAITimer(), updateCount + 2);
    EXPECT_EQ(outsider->getAITimer(), 0u);
}

TEST_F(ScriptStateFunctionsFixture, IdentifyAndTargetKeyDropUseRoleLookups)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55385);
    auto target = makeObject(module, "mp_objects/follower.obj", 55386);
    auto keyItem = makeObject(module, "mp_data/globalobjects/items/keya.obj", 55387);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(keyItem, nullptr);
    ASSERT_TRUE(Inventory::add_item(target->getObjRef(), keyItem->getObjRef(), target->getFirstFreeInventorySlot(), true));

    target->setAmmoMax(4);
    target->setAmmoKnown(false);
    target->setNameKnown(false);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_IdentifyTarget(state, self));
    EXPECT_TRUE(target->isAmmoKnown());
    EXPECT_TRUE(target->isNameKnown());
    EXPECT_TRUE(actor->getProfile()->isUsageKnown());

    EXPECT_TRUE(scr_DropTargetKeys(state, self));
    EXPECT_EQ(keyItem->getInventoryHolderRef(), ObjectRef::Invalid);
}

TEST_F(ScriptStateFunctionsFixture, ElseUsesSelfProfileContextForIndentComparison)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55388);

    ASSERT_NE(actor, nullptr);

    auto& aiScript = actor->getProfile()->getAIScript();
    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    aiScript.indent = 4;
    aiScript.indent_last = 4;
    EXPECT_TRUE(scr_Else(state, self));

    aiScript.indent = 3;
    aiScript.indent_last = 4;
    EXPECT_FALSE(scr_Else(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfUsageIsKnownUsesSelfProfileContext)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55389);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(actor->getProfile(), nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_EQ(scr_IfUsageIsKnown(state, self), actor->getProfile()->isUsageKnown());

    EXPECT_TRUE(scr_MakeUsageKnown(state, self));
    EXPECT_TRUE(scr_IfUsageIsKnown(state, self));
}

TEST_F(ScriptStateFunctionsFixture, MorphToTargetUsesMorphControlRoleAndPreservesMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55390);
    auto target = makeObject(module, "mp_data/globalobjects/players/healer.obj", 55391);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    const ObjectProfileRef actorBaseModelBefore = actor->getBaseModelRef();
    actor->setResizeTimeRemaining(7);
    target->setTargetFat(1.75f);
    const ObjectProfileRef targetBaseModel = target->getBaseModelRef();
    const SKIN_T targetSkin = target->getSkin();

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_MorphToTarget(state, self));
    EXPECT_EQ(actor->getProfileID(), targetBaseModel);
    EXPECT_EQ(actor->getBaseModelRef(), actorBaseModelBefore);
    EXPECT_EQ(actor->getSkin(), targetSkin);
    EXPECT_FLOAT_EQ(actor->getTargetFat(), target->getFat());
    EXPECT_EQ(actor->getResizeTimeRemaining(), Object::SIZETIME);

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_MorphToTarget(state, self));
}

TEST_F(ScriptStateFunctionsFixture, SetTargetSizeUsesMorphControlRoleAndPreservesMissingTargetFailure)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55392);
    auto target = makeObject(module, "mp_objects/follower.obj", 55393);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(target, nullptr);

    target->setTargetFat(2.0f);
    target->setResizeTimeRemaining(5);

    script_state_t state;
    state.argument = 150;
    ai_state_t self = makeScriptSelf(actor);
    self.setTarget(target->getObjRef());

    EXPECT_TRUE(scr_SetTargetSize(state, self));
    EXPECT_FLOAT_EQ(target->getTargetFat(), 3.0f);
    EXPECT_EQ(target->getResizeTimeRemaining(), 5 + Object::SIZETIME);

    self.setTarget(ObjectRef::Invalid);
    EXPECT_FALSE(scr_SetTargetSize(state, self));
}

TEST_F(ScriptStateFunctionsFixture, StealthHelpersUseLifecycleRoleAndPreserveEntryExitSemantics)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55388);

    ASSERT_NE(actor, nullptr);

    actor->addPerk(Ego::Perks::STEALTH);
    actor->_stealthTimer = 0;

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_EnableStealth(state, self));
    EXPECT_TRUE(actor->isStealthed());

    EXPECT_FALSE(scr_EnableStealth(state, self));
    EXPECT_TRUE(actor->isStealthed());

    EXPECT_TRUE(scr_DisableStealth(state, self));
    EXPECT_FALSE(actor->isStealthed());

    EXPECT_FALSE(scr_DisableStealth(state, self));
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

TEST_F(ScriptStateFunctionsFixture, IfSittingReadsHolderRefThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55321);
    auto holder = makeObject(module, "mp_objects/follower.obj", 55322);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(holder, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_FALSE(scr_IfSitting(state, self));

    actor->setHolderRef(holder->getObjRef());
    EXPECT_TRUE(scr_IfSitting(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfHolderBlockedFailsWithoutBlockedAlertOrLastAttacker)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55310);
    auto holder = makeObject(module, "mp_objects/follower.obj", 55311);
    auto attacker = makeObject(module, "mp_objects/follower.obj", 55312);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(attacker, nullptr);

    actor->setHolderRef(holder->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_FALSE(scr_IfHolderBlocked(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    holder->setAIAlertBits(ALERTIF_BLOCKED);
    EXPECT_FALSE(scr_IfHolderBlocked(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);

    holder->setAILastAttacker(attacker->getObjRef());
    attacker->requestTerminate();
    EXPECT_FALSE(scr_IfHolderBlocked(state, self));
    EXPECT_EQ(self.getTarget(), ObjectRef::Invalid);
}

TEST_F(ScriptStateFunctionsFixture, IfModuleHasIDSZUsesMessageSelectedModuleAndRejectsInvalidNames)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55323);

    ASSERT_NE(actor, nullptr);

    const IDSZ2 presentIdsz('T', 'Y', 'P', 'E');
    const IDSZ2 missingIdsz('Z', 'Z', 'Z', 'Z');
    const int validMessageId = static_cast<int>(actor->getProfile()->addMessage("test.mod"));
    const int invalidModuleMessageId = static_cast<int>(actor->getProfile()->addMessage("missing.mod"));

    script_state_t state;
    state.argument = validMessageId;
    state.distance = static_cast<int>(presentIdsz.toUint32());
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfModuleHasIDSZ(state, self));

    state.distance = static_cast<int>(missingIdsz.toUint32());
    EXPECT_FALSE(scr_IfModuleHasIDSZ(state, self));

    state.argument = invalidModuleMessageId;
    state.distance = static_cast<int>(presentIdsz.toUint32());
    EXPECT_FALSE(scr_IfModuleHasIDSZ(state, self));

    state.argument = 9999;
    ASSERT_FALSE(actor->getProfile()->isValidMessageID(state.argument));
    EXPECT_FALSE(scr_IfModuleHasIDSZ(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfBackstabbedReadsLastAttackerThroughScriptableRole)
{
    auto& module = beginActiveTestModule();
    auto victim = makeObject(module, "mp_objects/follower.obj", 5590);
    auto staleAiAttacker = makeObject(module, "mp_objects/follower.obj", 5591);
    auto backstabber = makeObject(module, "mp_objects/follower.obj", 5592);

    ASSERT_NE(victim, nullptr);
    ASSERT_NE(staleAiAttacker, nullptr);
    ASSERT_NE(backstabber, nullptr);

    backstabber->addPerk(Ego::Perks::BACKSTAB);
    victim->setAILastAttacker(backstabber->getObjRef());

    script_state_t state;
    ai_state_t self = makeScriptSelf(victim);
    self.alert = ALERTIF_ATTACKED;
    self.directionlast = ATK_BEHIND;
    self.damagetypelast = DamageType::DAMAGE_SLASH;
    self.setLastAttacker(staleAiAttacker->getObjRef());

    EXPECT_TRUE(scr_IfBackstabbed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfBackstabbedFailsWhenScriptableLastAttackerIsMissingOrTerminated)
{
    auto& module = beginActiveTestModule();
    auto victim = makeObject(module, "mp_objects/follower.obj", 5593);
    auto attacker = makeObject(module, "mp_objects/follower.obj", 5594);

    ASSERT_NE(victim, nullptr);
    ASSERT_NE(attacker, nullptr);

    attacker->addPerk(Ego::Perks::BACKSTAB);

    script_state_t state;
    ai_state_t self = makeScriptSelf(victim);
    self.alert = ALERTIF_ATTACKED;
    self.directionlast = ATK_BEHIND;
    self.damagetypelast = DamageType::DAMAGE_SLASH;
    self.setLastAttacker(attacker->getObjRef());

    victim->setAILastAttacker(ObjectRef::Invalid);
    EXPECT_FALSE(scr_IfBackstabbed(state, self));

    victim->setAILastAttacker(attacker->getObjRef());
    attacker->requestTerminate();
    EXPECT_FALSE(scr_IfBackstabbed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfInvisibleReadsAlphaThroughRenderableRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5595);

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
    auto actor = makeObject(module, "mp_objects/follower.obj", 5596);
    auto leftHandItem = makeObject(module, "mp_objects/follower.obj", 5597);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftHandItem, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfUnarmed(state, self));

    actor->setHeldObject(SLOT_LEFT, leftHandItem->getObjRef());
    EXPECT_FALSE(scr_IfUnarmed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfUnarmedTreatsTerminatedHeldRefsAsUnarmed)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5598);
    auto leftHandItem = makeObject(module, "mp_objects/follower.obj", 5599);
    auto rightHandItem = makeObject(module, "mp_objects/follower.obj", 5600);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftHandItem, nullptr);
    ASSERT_NE(rightHandItem, nullptr);

    actor->setHeldObject(SLOT_LEFT, leftHandItem->getObjRef());
    actor->setHeldObject(SLOT_RIGHT, rightHandItem->getObjRef());
    leftHandItem->requestTerminate();
    rightHandItem->requestTerminate();

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfUnarmed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfHoldingItemIDReadsHeldItemsThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5539);
    auto item = makeMeleeWeapon(module, 5540);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(item, nullptr);
    ASSERT_TRUE(item->attachToObject(actor->getObjRef(), GRIP_RIGHT));

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
    ASSERT_TRUE(rightHandItem->attachToObject(actor->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor->getObjRef(), GRIP_LEFT));

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
    ASSERT_TRUE(rightHandItem->attachToObject(actor->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor->getObjRef(), GRIP_LEFT));

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
    ASSERT_TRUE(rightHandItem->attachToObject(actor->getObjRef(), GRIP_RIGHT));
    ASSERT_TRUE(leftHandItem->attachToObject(actor->getObjRef(), GRIP_LEFT));

    script_state_t state;
    state.argument = 0;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfHoldingShield(state, self));
    EXPECT_EQ(state.argument, LATCHBUTTON_RIGHT);
}

TEST_F(ScriptStateFunctionsFixture, HoldingWeaponPredicatesReturnFalseWithoutHeldItems)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5570);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    state.argument = 77;
    EXPECT_FALSE(scr_IfHoldingRangedWeapon(state, self));
    EXPECT_EQ(state.argument, 0);

    state.argument = 77;
    EXPECT_FALSE(scr_IfHoldingMeleeWeapon(state, self));
    EXPECT_EQ(state.argument, 0);

    state.argument = 77;
    EXPECT_FALSE(scr_IfHoldingShield(state, self));
    EXPECT_EQ(state.argument, 0);
}

TEST_F(ScriptStateFunctionsFixture, IfHeldInLeftHandReadsHolderSlotThroughInventoryRole)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 5573);
    auto heldItem = makeMeleeWeapon(module, 5574);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(heldItem, nullptr);
    ASSERT_TRUE(heldItem->attachToObject(holder->getObjRef(), GRIP_LEFT));

    script_state_t state;
    ai_state_t self = makeScriptSelf(heldItem);

    EXPECT_TRUE(scr_IfHeldInLeftHand(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfHeldInLeftHandFailsWithoutMatchingLeftHandHolder)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 55730);
    auto heldItem = makeMeleeWeapon(module, 55731);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(heldItem, nullptr);

    ASSERT_TRUE(heldItem->attachToObject(holder->getObjRef(), GRIP_RIGHT));

    script_state_t state;
    ai_state_t self = makeScriptSelf(heldItem);

    EXPECT_FALSE(scr_IfHeldInLeftHand(state, self));

    heldItem->detachFromHolder(true, false);
    EXPECT_FALSE(scr_IfHeldInLeftHand(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfGroggedAndIfDazedReadTimersThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5578);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    actor->setGrogTimer(4);
    actor->setDazeTimer(6);
    self.alert = ALERTIF_CONFUSED;

    EXPECT_TRUE(scr_IfGrogged(state, self));
    EXPECT_TRUE(scr_IfDazed(state, self));

    self.alert = 0;
    EXPECT_FALSE(scr_IfGrogged(state, self));
    EXPECT_FALSE(scr_IfDazed(state, self));
}

TEST_F(ScriptStateFunctionsFixture, IfArmorIsReadsSkinThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5579);

    ASSERT_NE(actor, nullptr);

    const SKIN_T validSkin = actor->getProfile()->isValidSkin(1) ? 1 : 0;
    ASSERT_TRUE(actor->getProfile()->isValidSkin(validSkin));
    ASSERT_TRUE(actor->setSkin(validSkin));

    script_state_t state;
    state.argument = validSkin;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_IfArmorIs(state, self));

    state.argument = validSkin + 1;
    EXPECT_FALSE(scr_IfArmorIs(state, self));
}

TEST_F(ScriptStateFunctionsFixture, SelfQueryPredicatesReadThroughTargetInfoRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5580);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);

    actor->setNameKnown(true);
    actor->setKursed(true);
    actor->setAmmo(0);
    actor->setEquipped(true);
    actor->addPerk(Ego::Perks::STEALTH);

    EXPECT_TRUE(scr_IfNameIsKnown(state, self));
    EXPECT_TRUE(scr_IfKursed(state, self));
    EXPECT_TRUE(scr_IfAmmoOut(state, self));
    EXPECT_TRUE(scr_IfEquipped(state, self));
    EXPECT_EQ(scr_IfOverWater(state, self), actor->isOnWaterTile());

    actor->_stealth = true;
    EXPECT_TRUE(scr_IfStealthed(state, self));

    actor->_stealth = false;
    actor->setNameKnown(false);
    actor->setKursed(false);
    actor->setAmmo(3);
    actor->setEquipped(false);

    EXPECT_FALSE(scr_IfNameIsKnown(state, self));
    EXPECT_FALSE(scr_IfKursed(state, self));
    EXPECT_FALSE(scr_IfAmmoOut(state, self));
    EXPECT_FALSE(scr_IfEquipped(state, self));
    EXPECT_FALSE(scr_IfStealthed(state, self));
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

TEST_F(ScriptStateFunctionsFixture, FlashVariableAndFlashVariableHeightUseVisualRole)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5542);

    ASSERT_NE(actor, nullptr);
    ASSERT_GE(actor->getVertexCount(), 3u);

    GraphicsAccess::setVertexZ(actor->getGraphics(), 0, 5.0f);
    GraphicsAccess::setVertexZ(actor->getGraphics(), 1, 15.0f);
    GraphicsAccess::setVertexZ(actor->getGraphics(), 2, 25.0f);

    const int flashedLight = static_cast<int>(255 * idlib::fraction<float, 1, 255>());

    script_state_t flashState;
    flashState.argument = 255;
    ai_state_t self = makeScriptSelf(actor);

    EXPECT_TRUE(scr_FlashVariable(flashState, self));
    EXPECT_EQ(actor->getAmbientColour(), flashedLight);
    EXPECT_EQ(actor->getVertex(0).color_dir, flashedLight);

    script_state_t heightState;
    heightState.turn = 0;
    heightState.x = 10;
    heightState.distance = 100;
    heightState.y = 20;

    EXPECT_TRUE(scr_FlashVariableHeight(heightState, self));
    EXPECT_FLOAT_EQ(actor->getVertex(0).col[RR], 0.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(0).col[GG], 0.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(0).col[BB], 0.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(1).col[RR], 50.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(1).col[GG], 50.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(1).col[BB], 50.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(2).col[RR], 100.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(2).col[GG], 100.0f);
    EXPECT_FLOAT_EQ(actor->getVertex(2).col[BB], 100.0f);
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

TEST_F(ScriptStateFunctionsFixture, SetTargetToChildUsesChildResolutionHelper)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55511);
    auto child = makeObject(module, "mp_objects/follower.obj", 55512);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(child, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.child = child->getObjRef();

    EXPECT_TRUE(scr_SetTargetToChild(state, self));
    EXPECT_EQ(self.getTarget(), child->getObjRef());
}

TEST_F(ScriptStateFunctionsFixture, SetTargetToChildFailsWithoutChild)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 55513);

    ASSERT_NE(actor, nullptr);

    script_state_t state;
    ai_state_t self = makeScriptSelf(actor);
    self.child = ObjectRef::Invalid;
    self.setTarget(actor->getObjRef());

    EXPECT_FALSE(scr_SetTargetToChild(state, self));
    EXPECT_EQ(self.getTarget(), actor->getObjRef());
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
