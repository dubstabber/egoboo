#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#undef private
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace
{
IMovementControl& movementControl(Object& object)
{
    return object;
}

const IMovementControl& movementControl(const Object& object)
{
    return object;
}

class ObjectAccessorFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    ObjectHandler _objectHandler;

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
        opts.randomSeed = 17;
        opts.binaryPath = "";
        opts.logPath = "/debug/object-accessor-tests.log";
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
        if (GameSessionContext::get().hasActiveModule())
        {
            GameSessionContext::get().quitModule();
        }

        EngineContext::get().profileSystem().reset();
        EngineContext::get().profileSystem().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
        GameSessionContext::get().publishLocalPlayerPerception(LocalPlayerPerceptionState{});
    }

    void TearDown() override
    {
        if (GameSessionContext::get().hasActiveModule())
        {
            GameSessionContext::get().quitModule();
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

    ObjectProfileRef loadFollowerProfile(int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile("mp_objects/follower.obj", slot);
    }

    ObjectProfileRef loadProfile(const std::string& profilePath, int slot) const
    {
        return EngineContext::get().profileSystem().loadOneProfile(profilePath, slot);
    }

    std::shared_ptr<Object> makeFollower(ObjectHandler& objectHandler, int slot) const
    {
        const ObjectProfileRef profile = loadFollowerProfile(slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return objectHandler.insert(profile);
    }

    std::shared_ptr<Object> makeObject(ObjectHandler& objectHandler, const std::string& profilePath, int slot) const
    {
        const ObjectProfileRef profile = loadProfile(profilePath, slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return objectHandler.insert(profile);
    }

    std::shared_ptr<Ego::Enchantment> addEnchantFixture(const std::shared_ptr<Object>& target,
                                                        const std::shared_ptr<Object>& spawner,
                                                        const char* enchantPath) const
    {
        EXPECT_NE(target, nullptr);
        EXPECT_NE(spawner, nullptr);
        if (target == nullptr || spawner == nullptr)
        {
            return nullptr;
        }

        const ENC_REF enchantRef = EngineContext::get().profileSystem().loadEnchantProfile(enchantPath, INVALID_EVE_REF);
        EXPECT_LT(enchantRef, ENCHANTPROFILES_MAX);
        if (enchantRef >= ENCHANTPROFILES_MAX)
        {
            return nullptr;
        }

        return target->addEnchant(enchantRef, spawner->getProfileID().get(), spawner, spawner);
    }

    std::shared_ptr<Object> makeFollower(int slot)
    {
        return makeFollower(_objectHandler, slot);
    }

    ObjectHandler& beginActiveTestModule()
    {
        auto module = findTestModule();
        EXPECT_NE(module, nullptr);
        if (module == nullptr)
        {
            throw std::runtime_error("test.mod profile not found");
        }

        const bool began = GameSessionContext::get().beginModule(module, 17);
        EXPECT_TRUE(began);
        return GameSessionContext::get().objectHandler();
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ObjectAccessorFixture::s_runtime;

ModelAction findValidAction(const std::shared_ptr<Object>& object,
                            std::initializer_list<ModelAction> candidates,
                            ModelAction excluded = ACTION_COUNT)
{
    const auto& model = object->inst.getModelDescriptor();
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

ModelAction findLoopingAction(const std::shared_ptr<Object>& object,
                              std::initializer_list<ModelAction> candidates,
                              ModelAction excluded = ACTION_COUNT)
{
    const auto& model = object->inst.getModelDescriptor();
    if (!model)
    {
        return ACTION_COUNT;
    }

    for (const ModelAction action : candidates)
    {
        if (action == excluded || !model->isActionValid(action))
        {
            continue;
        }

        if (model->getLastFrame(action) > model->getFirstFrame(action))
        {
            return action;
        }
    }

    return ACTION_COUNT;
}

ModelAction findActionWithMinimumFrameCount(const std::shared_ptr<Object>& object,
                                            std::initializer_list<ModelAction> candidates,
                                            int minimumFrameCount,
                                            ModelAction excluded = ACTION_COUNT)
{
    const auto& model = object->inst.getModelDescriptor();
    if (!model)
    {
        return ACTION_COUNT;
    }

    for (const ModelAction action : candidates)
    {
        if (action == excluded || !model->isActionValid(action))
        {
            continue;
        }

        const int frameCount = 1 + (model->getLastFrame(action) - model->getFirstFrame(action));
        if (frameCount >= minimumFrameCount)
        {
            return action;
        }
    }

    return ACTION_COUNT;
}

bool findHealableInvalidAction(const std::shared_ptr<Object>& object,
                               ModelAction& invalidAction,
                               ModelAction& healedAction)
{
    const auto& model = object->inst.getModelDescriptor();
    if (!model)
    {
        return false;
    }

    for (int rawAction = 0; rawAction < ACTION_COUNT; ++rawAction)
    {
        const auto candidate = static_cast<ModelAction>(rawAction);
        if (model->isActionValid(candidate))
        {
            continue;
        }

        const ModelAction healedCandidate = model->getAction(rawAction);
        if (healedCandidate != ACTION_COUNT)
        {
            invalidAction = candidate;
            healedAction = healedCandidate;
            return true;
        }
    }

    return false;
}

TEST_F(ObjectAccessorFixture, SelectedObjectRefsDefaultToInvalidAndRoundTripThroughAccessors)
{
    auto object = makeFollower(301);
    ASSERT_NE(object, nullptr);

    EXPECT_EQ(object->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    EXPECT_EQ(object->getHeldObject(SLOT_RIGHT), ObjectRef::Invalid);
    EXPECT_EQ(object->getEquipment(INVEN_PACK), ObjectRef::Invalid);
    EXPECT_EQ(object->getEquipment(INVEN_NECK), ObjectRef::Invalid);

    const ObjectRef leftHand(17);
    const ObjectRef rightHand(18);
    const ObjectRef packItem(19);
    const ObjectRef neckItem(20);

    object->setHeldObject(SLOT_LEFT, leftHand);
    object->setHeldObject(SLOT_RIGHT, rightHand);
    object->setEquipment(INVEN_PACK, packItem);
    object->setEquipment(INVEN_NECK, neckItem);

    EXPECT_EQ(object->getHeldObject(SLOT_LEFT), leftHand);
    EXPECT_EQ(object->getHeldObject(SLOT_RIGHT), rightHand);
    EXPECT_EQ(object->getEquipment(INVEN_PACK), packItem);
    EXPECT_EQ(object->getEquipment(INVEN_NECK), neckItem);
}

TEST_F(ObjectAccessorFixture, ScalarAccessorsRoundTripSelectedEncapsulatedState)
{
    auto object = makeFollower(302);
    ASSERT_NE(object, nullptr);

    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    object->setJumpTimer(9);
    object->setJumpNumber(3);
    object->setJumpReady(true);
    object->setBaseFat(1.25f);
    object->setTargetFat(2.25f);
    object->setResizeTimeRemaining(33);
    object->setDamageTargetType(DamageType::DAMAGE_FIRE);
    object->setReaffirmDamageType(DamageType::DAMAGE_ZAP);
    object->setDamageThreshold(77);

    EXPECT_EQ(object->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(object->getBaseTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(object->getJumpTimer(), 9);
    EXPECT_EQ(object->getJumpNumber(), 3);
    EXPECT_TRUE(object->isJumpReady());
    EXPECT_FLOAT_EQ(object->getBaseFat(), 1.25f);
    EXPECT_FLOAT_EQ(object->getTargetFat(), 2.25f);
    EXPECT_EQ(object->getResizeTimeRemaining(), 33);
    EXPECT_EQ(object->getDamageTargetType(), DamageType::DAMAGE_FIRE);
    EXPECT_EQ(object->getReaffirmDamageType(), DamageType::DAMAGE_ZAP);
    EXPECT_EQ(object->getDamageThreshold(), 77);
}

TEST_F(ObjectAccessorFixture, TeamIntentBecomeLeaderAssignsSelfToCurrentTeam)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3021);
    ASSERT_NE(object, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    module.getTeamList()[Team::TEAM_GOOD].setLeader(Object::INVALID_OBJECT);

    object->becomeTeamLeader();

    EXPECT_EQ(module.getTeamList()[Team::TEAM_GOOD].getLeader(), object);
}

TEST_F(ObjectAccessorFixture, SetTeamTransfersMoraleAndLeadershipThroughObjectSeam)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3022);
    ASSERT_NE(object, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    Team& goodTeam = module.getTeamList()[Team::TEAM_GOOD];
    Team& evilTeam = module.getTeamList()[Team::TEAM_EVIL];

    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setItem(false);
    object->setInvincible(false);
    object->_isAlive = true;

    goodTeam.increaseMorale();
    goodTeam.setLeader(object);
    evilTeam.setLeader(Object::INVALID_OBJECT);

    const auto goodMoraleBefore = goodTeam.getMorale();
    const auto evilMoraleBefore = evilTeam.getMorale();

    object->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL), true);

    EXPECT_EQ(object->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(object->getBaseTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(goodTeam.getMorale(), goodMoraleBefore - 1);
    EXPECT_EQ(evilTeam.getMorale(), evilMoraleBefore + 1);
    EXPECT_EQ(goodTeam.getLeader(), Object::INVALID_OBJECT);
    EXPECT_EQ(evilTeam.getLeader(), object);
}

TEST_F(ObjectAccessorFixture, TeamIntentCallForHelpPublishesCallerOnCurrentTeam)
{
    auto& objectHandler = beginActiveTestModule();
    auto caller = makeFollower(objectHandler, 3023);
    ASSERT_NE(caller, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    caller->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    module.getTeamList()[Team::TEAM_GOOD].setLeader(Object::INVALID_OBJECT);

    caller->callTeamForHelp();

    EXPECT_EQ(module.getTeamList()[Team::TEAM_GOOD].getSissy(), caller);
}

TEST_F(ObjectAccessorFixture, TeamMemberRoleSurfaceSupportsTeamMutationLeadershipAndTeamExperience)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/players/rogue.obj", 3024);
    auto ally = makeObject(objectHandler, "mp_data/globalobjects/players/rogue.obj", 3025);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(ally, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    Team& goodTeam = module.getTeamList()[Team::TEAM_GOOD];
    Team& evilTeam = module.getTeamList()[Team::TEAM_EVIL];

    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    ally->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    ally->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setItem(false);
    object->setInvincible(false);
    object->_isAlive = true;
    ally->setItem(false);
    ally->setInvincible(false);
    ally->_isAlive = true;

    ITeamMember& teamRole = *object;
    teamRole.setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(object->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(object->getBaseTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));

    evilTeam.setLeader(Object::INVALID_OBJECT);
    teamRole.becomeTeamLeader();
    EXPECT_EQ(evilTeam.getLeader(), object);

    teamRole.setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    goodTeam.setLeader(Object::INVALID_OBJECT);
    teamRole.becomeTeamLeader();
    teamRole.callTeamForHelp();
    EXPECT_EQ(goodTeam.getSissy(), object);
    teamRole.giveTeamExperience(64, XP_TEAMKILL);
}

TEST_F(ObjectAccessorFixture, WalletRoleSurfaceSupportsBoundedMoneyQueriesAndMutations)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3027);
    ASSERT_NE(object, nullptr);

    IWallet& wallet = *object;
    wallet.giveMoney(150);
    EXPECT_EQ(wallet.getMoney(), 150);

    wallet.giveMoney(-25);
    EXPECT_EQ(wallet.getMoney(), 125);

    wallet.dropMoney(40);
    EXPECT_EQ(wallet.getMoney(), 85);
}

TEST_F(ObjectAccessorFixture, LifecycleRoleSurfaceSupportsRespawnInPlaceAndBoundedStateMutation)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3601);
    ASSERT_NE(object, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    Team& goodTeam = module.getTeamList()[Team::TEAM_GOOD];
    ILifecycleControl& lifecycle = *object;

    lifecycle.setItem(true);
    lifecycle.setCanBeCrushed(true);
    lifecycle.setDamageThreshold(7);

    EXPECT_TRUE(object->isItem());
    EXPECT_TRUE(object->canBeCrushed());
    EXPECT_EQ(object->getDamageThreshold(), 7);

    object->addPerk(Ego::Perks::STEALTH);
    object->_stealthTimer = 0;
    object->setAlpha(12);

    EXPECT_TRUE(lifecycle.activateStealth());
    EXPECT_TRUE(object->isStealthed());

    lifecycle.deactivateStealth();
    EXPECT_FALSE(object->isStealthed());
    EXPECT_EQ(object->getAlpha(), 0xFF);

    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->_isAlive = false;
    object->setInvincible(false);
    object->setPosition(Ego::Vector3f(91.0f, 37.0f, 5.0f));
    goodTeam.setLeader(Object::INVALID_OBJECT);
    const auto moraleBefore = goodTeam.getMorale();
    const Ego::Vector3f positionBeforeRespawn = object->getPosition();

    lifecycle.respawnInPlace();

    EXPECT_TRUE(object->isAlive());
    EXPECT_FLOAT_EQ(object->getPosition().x(), positionBeforeRespawn.x());
    EXPECT_FLOAT_EQ(object->getPosition().y(), positionBeforeRespawn.y());
    EXPECT_FLOAT_EQ(object->getPosition().z(), positionBeforeRespawn.z());
    EXPECT_EQ(goodTeam.getMorale(), moraleBefore + 1);
    EXPECT_EQ(goodTeam.getLeader(), object);
    EXPECT_FALSE(object->canBeCrushed());
}

TEST_F(ObjectAccessorFixture, LifecycleRoleSurfaceSupportsKeyAndInventoryDrops)
{
    auto& objectHandler = beginActiveTestModule();
    auto actor = makeFollower(objectHandler, 3602);
    auto leftItem = makeObject(objectHandler, "mp_data/globalobjects/weapons/stiletto.obj", 3603);
    auto rightItem = makeObject(objectHandler, "mp_data/globalobjects/armor/atshield.obj", 3604);
    auto packItem = makeObject(objectHandler, "mp_data/globalobjects/weapons/stiletto.obj", 3605);
    auto keyItem = makeObject(objectHandler, "mp_data/globalobjects/items/keya.obj", 3606);

    ASSERT_NE(actor, nullptr);
    ASSERT_NE(leftItem, nullptr);
    ASSERT_NE(rightItem, nullptr);
    ASSERT_NE(packItem, nullptr);
    ASSERT_NE(keyItem, nullptr);
    actor->setHeldObject(SLOT_LEFT, leftItem->getObjRef());
    leftItem->setHolderRef(actor->getObjRef());
    leftItem->setAttachmentSlot(SLOT_LEFT);
    actor->setHeldObject(SLOT_RIGHT, rightItem->getObjRef());
    rightItem->setHolderRef(actor->getObjRef());
    rightItem->setAttachmentSlot(SLOT_RIGHT);
    ASSERT_TRUE(Inventory::add_item(actor->getObjRef(), packItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));
    ASSERT_TRUE(Inventory::add_item(actor->getObjRef(), keyItem->getObjRef(), actor->getFirstFreeInventorySlot(), true));

    ILifecycleControl& lifecycle = *actor;

    ASSERT_NO_THROW(lifecycle.dropKeys());

    EXPECT_EQ(keyItem->getInventoryHolderRef(), ObjectRef::Invalid);
    const std::vector<ObjectRef> remainingAfterKeys = actor->getInventoryItemRefs();
    ASSERT_EQ(remainingAfterKeys.size(), 1u);
    EXPECT_EQ(remainingAfterKeys.front(), packItem->getObjRef());

    ASSERT_NO_THROW(lifecycle.dropAllItems());

    EXPECT_EQ(actor->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    EXPECT_EQ(actor->getHeldObject(SLOT_RIGHT), ObjectRef::Invalid);
    EXPECT_EQ(leftItem->getHolderRef(), ObjectRef::Invalid);
    EXPECT_EQ(rightItem->getHolderRef(), ObjectRef::Invalid);
    EXPECT_EQ(packItem->getInventoryHolderRef(), ObjectRef::Invalid);
    EXPECT_TRUE(actor->getInventoryItemRefs().empty());
}

TEST_F(ObjectAccessorFixture, LifecycleRoleSurfaceSupportsDismountPublication)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3606);
    ASSERT_NE(object, nullptr);

    ILifecycleControl& lifecycle = *object;

    lifecycle.setDismountTimer(17);
    lifecycle.setDismountObject(ObjectRef(88));

    EXPECT_EQ(object->getDismountTimer(), 17);
    EXPECT_EQ(object->getDismountObject(), ObjectRef(88));
}

TEST_F(ObjectAccessorFixture, LifecycleRoleSurfaceSupportsTerminationRequest)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3607);
    ASSERT_NE(object, nullptr);

    ILifecycleControl& lifecycle = *object;
    const ObjectRef objectRef = object->getObjRef();

    EXPECT_TRUE(objectHandler.exists(objectRef));

    lifecycle.requestTerminate();

    EXPECT_TRUE(object->isTerminated());
    EXPECT_FALSE(objectHandler.exists(objectRef));
}

TEST_F(ObjectAccessorFixture, RespawnRestoresMoraleAndClaimsLeadershipWhenUnset)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3026);
    ASSERT_NE(object, nullptr);

    GameModule& module = GameSessionContext::get().activeModule();
    Team& goodTeam = module.getTeamList()[Team::TEAM_GOOD];

    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->_isAlive = false;
    object->setInvincible(false);
    goodTeam.setLeader(Object::INVALID_OBJECT);

    const auto moraleBefore = goodTeam.getMorale();

    object->respawn();

    EXPECT_TRUE(object->isAlive());
    EXPECT_EQ(object->getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(goodTeam.getMorale(), moraleBefore + 1);
    EXPECT_EQ(goodTeam.getLeader(), object);
}

TEST_F(ObjectAccessorFixture, FlagAndPlayerAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(303);
    ASSERT_NE(object, nullptr);

    object->setPlayerNumber(2);
    object->setLocalPlayer(true);
    object->setInvincible(true);
    object->setKursed(true);
    object->setNameKnown(true);
    object->setAmmoKnown(true);
    object->setHitReady(false);
    object->setEquipped(true);
    object->setItem(true);
    object->setShopItem(true);
    object->setCanBeCrushed(true);
    object->setSparkle(7);

    EXPECT_EQ(object->getPlayerNumber(), 2);
    EXPECT_TRUE(object->isPlayer());
    EXPECT_TRUE(object->isLocalPlayer());
    EXPECT_TRUE(object->isInvincible());
    EXPECT_TRUE(object->isKursed());
    EXPECT_TRUE(object->isNameKnown());
    EXPECT_TRUE(object->isAmmoKnown());
    EXPECT_FALSE(object->isHitReady());
    EXPECT_TRUE(object->isEquipped());
    EXPECT_TRUE(object->isItem());
    EXPECT_TRUE(object->isShopItem());
    EXPECT_TRUE(object->canBeCrushed());
    EXPECT_EQ(object->getSparkle(), 7);
}

TEST_F(ObjectAccessorFixture, AttachmentAndPlatformAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(304);
    ASSERT_NE(object, nullptr);

    EXPECT_EQ(object->getHolderRef(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAttachedPlatformRef(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAttachmentSlot(), SLOT_LEFT);
    EXPECT_EQ(object->getInventoryHolderRef(), ObjectRef::Invalid);
    EXPECT_FALSE(object->isPlatform());
    EXPECT_FALSE(object->canUsePlatforms());
    EXPECT_EQ(object->getHoldingWeight(), 0);

    object->setHolderRef(ObjectRef(41));
    object->onwhichplatform_ref = ObjectRef(43);
    object->setAttachmentSlot(SLOT_RIGHT);
    object->setInventoryHolderRef(ObjectRef(42));
    object->setPlatform(true);
    object->setCanUsePlatforms(true);
    object->setHoldingWeight(7);
    object->adjustHoldingWeight(5);

    EXPECT_EQ(object->getHolderRef(), ObjectRef(41));
    EXPECT_EQ(object->getAttachedPlatformRef(), ObjectRef(43));
    EXPECT_EQ(object->getAttachmentSlot(), SLOT_RIGHT);
    EXPECT_EQ(object->getInventoryHolderRef(), ObjectRef(42));
    EXPECT_TRUE(object->isPlatform());
    EXPECT_TRUE(object->canUsePlatforms());
    EXPECT_EQ(object->getHoldingWeight(), 12);
}

TEST_F(ObjectAccessorFixture, TargetInfoRoleSurfaceExposesAttachmentAndHeldStateQueries)
{
    auto& objectHandler = beginActiveTestModule();
    auto holder = makeFollower(objectHandler, 3046);
    auto object = makeFollower(objectHandler, 3047);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(object, nullptr);

    holder->setHeldObject(SLOT_RIGHT, object->getObjRef());
    object->setHolderRef(holder->getObjRef());
    object->setAttachmentSlot(SLOT_RIGHT);

    const ITargetInfo& target = *object;

    EXPECT_EQ(target.getHolderRef(), holder->getObjRef());
    EXPECT_EQ(target.getAttachmentSlot(), SLOT_RIGHT);
    EXPECT_TRUE(target.isBeingHeld());
}

TEST_F(ObjectAccessorFixture, MissingHolderAndPlatformRefsRemainNullLikeThroughRuntimeLookups)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3045);
    ASSERT_NE(object, nullptr);

    object->setHolderRef(ObjectRef(41));
    object->onwhichplatform_ref = ObjectRef(42);

    EXPECT_FALSE(objectHandler[object->getHolderRef()]);
    EXPECT_FALSE(objectHandler[object->getAttachedPlatformRef()]);
    EXPECT_FALSE(object->isBeingHeld());
}

TEST_F(ObjectAccessorFixture, PhysicsForwardersClampDesiredVelocityAndExposeGroundContactState)
{
    auto object = makeFollower(3044);
    ASSERT_NE(object, nullptr);

    IMovementControl& movement = movementControl(*object);
    movement.setDesiredVelocity(Ego::Vector2f(2.0f, 0.0f));
    EXPECT_FLOAT_EQ(movement.getDesiredVelocity().x(), 1.0f);
    EXPECT_FLOAT_EQ(movement.getDesiredVelocity().y(), 0.0f);

    object->_objectPhysics._groundElevation = object->getPosZ();
    EXPECT_TRUE(object->isTouchingGround());

    object->_objectPhysics._groundElevation = object->getPosZ() + 100.0f;
    EXPECT_FALSE(object->isTouchingGround());
}

TEST_F(ObjectAccessorFixture, PhysicsForwardersExposeFiniteAndInfiniteMass)
{
    auto object = makeFollower(3045);
    ASSERT_NE(object, nullptr);

    object->phys.weight = 80;
    object->phys.bumpdampen = 0.5f;
    EXPECT_FLOAT_EQ(object->getMass(), 160.0f);

    object->phys.bumpdampen = 0.0f;
    EXPECT_LT(object->getMass(), 0.0f);
}

TEST_F(ObjectAccessorFixture, InventoryObservationHelpersExposeSlotCountItemsAndFirstFreeSlot)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3041);
    auto item0 = makeFollower(objectHandler, 3042);
    auto item2 = makeFollower(objectHandler, 3043);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(item0, nullptr);
    ASSERT_NE(item2, nullptr);

    EXPECT_EQ(object->getInventoryMaxItems(), Inventory::MAXNUMINPACK);
    EXPECT_EQ(object->getFirstFreeInventorySlot(), 0u);
    EXPECT_EQ(object->getInventoryItem(0), nullptr);
    EXPECT_EQ(object->getInventoryItem(1), nullptr);
    EXPECT_TRUE(object->getInventoryItemRefs().empty());

    object->setInventoryItem(0, item0);
    object->setInventoryItem(2, item2);

    EXPECT_EQ(object->getInventoryItem(0), item0);
    EXPECT_EQ(object->getInventoryItem(1), nullptr);
    EXPECT_EQ(object->getInventoryItem(2), item2);
    EXPECT_EQ(object->getInventoryItemRef(0), item0->getObjRef());
    EXPECT_EQ(object->getInventoryItemRef(1), ObjectRef::Invalid);
    EXPECT_EQ(object->getInventoryItemRef(2), item2->getObjRef());
    EXPECT_EQ(object->getFirstFreeInventorySlot(), 1u);

    const std::vector<ObjectRef> itemRefs = object->getInventoryItemRefs();
    ASSERT_EQ(itemRefs.size(), 2u);
    EXPECT_EQ(itemRefs[0], item0->getObjRef());
    EXPECT_EQ(itemRefs[1], item2->getObjRef());

    EXPECT_TRUE(object->removeInventoryItem(item0, true));
    EXPECT_EQ(object->getInventoryItem(0), nullptr);
    EXPECT_EQ(object->getInventoryItemRef(0), ObjectRef::Invalid);
    EXPECT_EQ(object->getFirstFreeInventorySlot(), 0u);
}

TEST_F(ObjectAccessorFixture, InventoryMutationHelpersSupportStaticInventoryOperations)
{
    auto& objectHandler = beginActiveTestModule();
    auto owner = makeFollower(objectHandler, 3046);
    auto inventoryItem = makeFollower(objectHandler, 3047);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(inventoryItem, nullptr);

    EXPECT_TRUE(Inventory::add_item(owner->getObjRef(), inventoryItem->getObjRef(), owner->getFirstFreeInventorySlot(), true));
    EXPECT_EQ(owner->getInventoryItem(0), inventoryItem);
    EXPECT_EQ(inventoryItem->getInventoryHolderRef(), owner->getObjRef());

    EXPECT_TRUE(Inventory::remove_item(owner->getObjRef(), 0, true));
    EXPECT_EQ(owner->getInventoryItem(0), nullptr);
    EXPECT_EQ(inventoryItem->getInventoryHolderRef(), ObjectRef::Invalid);

    // Empty hand/slot swaps are a stable no-op and should continue to succeed.
    EXPECT_TRUE(Inventory::swap_item(owner->getObjRef(), 0, SLOT_LEFT, true));
    EXPECT_EQ(owner->getInventoryItem(0), nullptr);
    EXPECT_EQ(owner->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
}

TEST_F(ObjectAccessorFixture, InventoryRoleSurfaceSupportsInterfaceBasedStaticOperations)
{
    auto& objectHandler = beginActiveTestModule();
    auto owner = makeFollower(objectHandler, 3048);
    auto inventoryItem = makeFollower(objectHandler, 3049);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(inventoryItem, nullptr);

    IInventoryHolder& inventoryHolder = *owner;
    owner->setHoldingWeight(9);

    EXPECT_EQ(inventoryHolder.getHoldingWeight(), 9);

    EXPECT_TRUE(Inventory::add_item(inventoryHolder, inventoryItem, inventoryHolder.getFirstFreeInventorySlot(), true));
    EXPECT_EQ(inventoryHolder.getInventoryItemRef(0), inventoryItem->getObjRef());
    EXPECT_EQ(inventoryHolder.getInventoryItem(0), inventoryItem);
    const std::vector<ObjectRef> inventoryItemRefs = inventoryHolder.getInventoryItemRefs();
    ASSERT_EQ(inventoryItemRefs.size(), 1u);
    EXPECT_EQ(inventoryItemRefs.front(), inventoryItem->getObjRef());

    // Empty hand/slot swaps remain a stable success path through the role overload.
    EXPECT_TRUE(Inventory::swap_item(inventoryHolder, 1, SLOT_LEFT, true));
    EXPECT_EQ(owner->getHeldObject(SLOT_LEFT), ObjectRef::Invalid);
    EXPECT_EQ(inventoryHolder.getInventoryItemRef(1), ObjectRef::Invalid);
    EXPECT_EQ(inventoryHolder.getInventoryItem(1), nullptr);

    EXPECT_TRUE(Inventory::remove_item(inventoryHolder, 0, true));
    EXPECT_EQ(inventoryHolder.getInventoryItemRef(0), ObjectRef::Invalid);
    EXPECT_EQ(inventoryHolder.getInventoryItem(0), nullptr);
}

TEST_F(ObjectAccessorFixture, ItemInfoRoleSurfaceMatchesProfileBackedClassification)
{
    auto& objectHandler = beginActiveTestModule();
    auto rangedWeapon = makeObject(objectHandler, "mp_data/globalobjects/weapons/xbow.obj", 3051);
    auto meleeWeapon = makeObject(objectHandler, "mp_data/globalobjects/weapons/stiletto.obj", 3052);
    auto shield = makeObject(objectHandler, "mp_data/globalobjects/armor/atshield.obj", 3053);
    ASSERT_NE(rangedWeapon, nullptr);
    ASSERT_NE(meleeWeapon, nullptr);
    ASSERT_NE(shield, nullptr);

    const IItemInfo& rangedInfo = *rangedWeapon;
    const IItemInfo& meleeInfo = *meleeWeapon;
    const IItemInfo& shieldInfo = *shield;

    EXPECT_TRUE(rangedInfo.hasTypeIDSZ(rangedWeapon->getProfile()->getIDSZ(IDSZ_TYPE)));
    EXPECT_TRUE(rangedInfo.isRangedWeapon());
    EXPECT_FALSE(rangedInfo.isMeleeWeapon());
    EXPECT_FALSE(rangedInfo.isShield());

    EXPECT_TRUE(meleeInfo.hasTypeIDSZ(meleeWeapon->getProfile()->getIDSZ(IDSZ_TYPE)));
    EXPECT_FALSE(meleeInfo.isRangedWeapon());
    EXPECT_TRUE(meleeInfo.isMeleeWeapon());
    EXPECT_FALSE(meleeInfo.isShield());

    EXPECT_TRUE(shieldInfo.hasTypeIDSZ(shield->getProfile()->getIDSZ(IDSZ_TYPE)));
    EXPECT_FALSE(shieldInfo.isRangedWeapon());
    EXPECT_FALSE(shieldInfo.isMeleeWeapon());
    EXPECT_TRUE(shieldInfo.isShield());
    EXPECT_FALSE(shieldInfo.hasTypeIDSZ(IDSZ2('Z', 'Z', 'Z', 'Z')));
}

TEST_F(ObjectAccessorFixture, TempAttributeHelpersRoundTripPresenceValueAndClearing)
{
    auto object = makeFollower(344);
    ASSERT_NE(object, nullptr);

    EXPECT_FALSE(object->hasTempAttribute(Ego::Attribute::MANA_REGEN));
    EXPECT_FLOAT_EQ(object->getTempAttributeValue(Ego::Attribute::MANA_REGEN), 0.0f);

    object->setTempAttribute(Ego::Attribute::MANA_REGEN, 1.5f);
    EXPECT_TRUE(object->hasTempAttribute(Ego::Attribute::MANA_REGEN));
    EXPECT_FLOAT_EQ(object->getTempAttributeValue(Ego::Attribute::MANA_REGEN), 1.5f);

    object->adjustTempAttribute(Ego::Attribute::MANA_REGEN, -0.25f);
    EXPECT_FLOAT_EQ(object->getTempAttributeValue(Ego::Attribute::MANA_REGEN), 1.25f);

    object->clearTempAttribute(Ego::Attribute::MANA_REGEN);
    EXPECT_FALSE(object->hasTempAttribute(Ego::Attribute::MANA_REGEN));
    EXPECT_FLOAT_EQ(object->getTempAttributeValue(Ego::Attribute::MANA_REGEN), 0.0f);
}

TEST_F(ObjectAccessorFixture, EnchantHelpersExposeReadOnlyListStateAndFrontEntry)
{
    auto& objectHandler = beginActiveTestModule();
    auto target = makeFollower(objectHandler, 345);
    auto spawner = makeObject(objectHandler, "mp_data/globalobjects/weapons/stiletto.obj", 346);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(spawner, nullptr);

    EXPECT_FALSE(target->hasActiveEnchants());
    EXPECT_EQ(target->getFirstActiveEnchant(), nullptr);
    EXPECT_TRUE(target->getActiveEnchants().empty());

    const auto enchant = addEnchantFixture(target, spawner, "mp_data/globalobjects/weapons/stiletto.obj/enchant.txt");
    ASSERT_NE(enchant, nullptr);

    EXPECT_TRUE(target->hasActiveEnchants());
    EXPECT_FALSE(target->getActiveEnchants().empty());
    EXPECT_EQ(target->getFirstActiveEnchant(), enchant);
    EXPECT_EQ(target->getActiveEnchants().front(), enchant);
    EXPECT_EQ(spawner->getLastEnchantmentSpawned(), enchant);
}

TEST_F(ObjectAccessorFixture, EnchantableRolePublishesAddDisenchantAndLastSpawnedState)
{
    auto& objectHandler = beginActiveTestModule();
    auto target = makeFollower(objectHandler, 347);
    auto spawner = makeObject(objectHandler, "mp_data/globalobjects/potions/ppotion.obj", 348);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(spawner, nullptr);

    IEnchantable& targetEnchantable = *target;
    IEnchantable& spawnerEnchantable = *spawner;

    EXPECT_FALSE(targetEnchantable.hasActiveEnchants());
    EXPECT_EQ(targetEnchantable.getFirstActiveEnchant(), nullptr);
    EXPECT_EQ(spawnerEnchantable.getLastEnchantmentSpawned(), nullptr);

    const auto enchant = targetEnchantable.addEnchant(EngineContext::get().profileSystem().loadEnchantProfile(
                                                          "mp_data/globalobjects/potions/ppotion.obj/enchant.txt",
                                                          INVALID_EVE_REF),
                                                      spawner->getProfileID().get(),
                                                      spawner,
                                                      spawner);
    ASSERT_NE(enchant, nullptr);

    EXPECT_TRUE(targetEnchantable.hasActiveEnchants());
    EXPECT_EQ(targetEnchantable.getFirstActiveEnchant(), enchant);
    EXPECT_EQ(spawnerEnchantable.getLastEnchantmentSpawned(), enchant);
    EXPECT_TRUE(targetEnchantable.disenchant());
    EXPECT_TRUE(enchant->isTerminated());
}

TEST_F(ObjectAccessorFixture, RuntimeTimerAndStatusAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(305);
    ASSERT_NE(object, nullptr);

    const int16_t initialBoredTimer = object->getBoredTimer();
    const uint8_t initialCarefulTimer = object->getCarefulTimer();

    EXPECT_EQ(object->getGrogTimer(), 0);
    EXPECT_EQ(object->getDazeTimer(), 0);
    EXPECT_GE(initialBoredTimer, 250);
    EXPECT_LE(initialBoredTimer, 800);
    EXPECT_EQ(initialCarefulTimer, 50);
    EXPECT_EQ(object->getReloadTimer(), 0);
    EXPECT_EQ(object->getDamageTimer(), 0);
    EXPECT_FALSE(object->shouldDrawIcon());
    EXPECT_FALSE(object->isInWater());
    EXPECT_EQ(object->getDismountTimer(), 0);
    EXPECT_EQ(object->getDismountObject(), ObjectRef::Invalid);

    object->setGrogTimer(8);
    object->setDazeTimer(10);
    object->setBoredTimer(12);
    object->setCarefulTimer(14);
    object->setReloadTimer(16);
    object->setDamageTimer(18);
    object->setDrawIcon(true);
    object->setInWater(true);
    object->setDismountTimer(20);
    object->setDismountObject(ObjectRef(21));

    EXPECT_EQ(object->getGrogTimer(), 8);
    EXPECT_EQ(object->getDazeTimer(), 10);
    EXPECT_EQ(object->getBoredTimer(), 12);
    EXPECT_EQ(object->getCarefulTimer(), 14);
    EXPECT_EQ(object->getReloadTimer(), 16);
    EXPECT_EQ(object->getDamageTimer(), 18);
    EXPECT_TRUE(object->shouldDrawIcon());
    EXPECT_TRUE(object->isInWater());
    EXPECT_EQ(object->getDismountTimer(), 20);
    EXPECT_EQ(object->getDismountObject(), ObjectRef(21));
}

TEST_F(ObjectAccessorFixture, TargetInfoRoleSurfaceExposesAnimationCombatAndSelfQueryState)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 30504);
    ASSERT_NE(object, nullptr);

    const ITargetInfo& targetInfo = *object;
    const SKIN_T validSkin = object->getProfile()->isValidSkin(1) ? 1 : 0;
    ASSERT_TRUE(object->getProfile()->isValidSkin(validSkin));

    object->inst._currentAnimation = ACTION_UA;
    object->inst._nextAnimation = ACTION_UA;
    object->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    object->setBaseTeamRef(static_cast<TEAM_REF>(Team::TEAM_EVIL));
    object->setNameKnown(true);
    object->setKursed(true);
    object->setEquipped(true);
    object->setAmmo(9);
    EXPECT_TRUE(object->setSkin(validSkin));
    object->addPerk(Ego::Perks::STEALTH);

    EXPECT_EQ(targetInfo.getCurrentAnimation(), ACTION_UA);
    EXPECT_TRUE(targetInfo.isAttacking());
    EXPECT_TRUE(targetInfo.isNameKnown());
    EXPECT_TRUE(targetInfo.isKursed());
    EXPECT_TRUE(targetInfo.isEquipped());
    EXPECT_EQ(targetInfo.getTeamRef(), static_cast<TEAM_REF>(Team::TEAM_GOOD));
    EXPECT_EQ(targetInfo.getBaseTeamRef(), static_cast<TEAM_REF>(Team::TEAM_EVIL));
    EXPECT_EQ(targetInfo.getTypeIDSZ(), object->getProfile()->getIDSZ(IDSZ_TYPE));
    EXPECT_EQ(targetInfo.getHateIDSZ(), object->getProfile()->getIDSZ(IDSZ_HATE));
    EXPECT_EQ(targetInfo.getAmmo(), 9);
    EXPECT_EQ(targetInfo.getSkin(), validSkin);
    EXPECT_TRUE(targetInfo.canBeGrogged());
    EXPECT_TRUE(targetInfo.canBeDazed());
    EXPECT_EQ(targetInfo.isOnWaterTile(), object->isOnWaterTile());
    object->_stealth = false;
    EXPECT_FALSE(targetInfo.isStealthed());

    object->_stealth = true;
    EXPECT_TRUE(targetInfo.isStealthed());

    object->inst._currentAnimation = ACTION_DA;
    object->inst._nextAnimation = ACTION_DA;
    object->setNameKnown(false);
    object->setKursed(false);
    object->setEquipped(false);
    object->setAmmo(0);
    object->_stealth = false;

    EXPECT_EQ(targetInfo.getCurrentAnimation(), ACTION_DA);
    EXPECT_FALSE(targetInfo.isAttacking());
    EXPECT_FALSE(targetInfo.isNameKnown());
    EXPECT_FALSE(targetInfo.isKursed());
    EXPECT_FALSE(targetInfo.isEquipped());
    EXPECT_EQ(targetInfo.getAmmo(), 0);
    EXPECT_FALSE(targetInfo.isStealthed());
}

TEST_F(ObjectAccessorFixture, CharacterStateRoleSurfaceSupportsMutableAmmoTimerKursePerkAndManaState)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 30506);
    ASSERT_NE(object, nullptr);

    ICharacterState& characterState = *object;
    object->setAmmoMax(9);
    object->setMana(0.0f);
    object->setLife(6.0f);
    object->setExperience(42);
    object->setExperienceLevelIndex(3);
    object->setReloadTimer(11);
    object->setBaseAttribute(Ego::Attribute::SPELL_POWER, 2.5f);

    const float mightBefore = object->getBaseAttribute(Ego::Attribute::MIGHT);
    const float manaBefore = object->getMana();

    characterState.setAmmo(4);
    characterState.setGrogTimer(6);
    characterState.setDazeTimer(8);
    characterState.setKursed(true);
    characterState.increaseBaseAttribute(Ego::Attribute::MIGHT, 1.5f);

    EXPECT_TRUE(characterState.costMana(-FLOAT_TO_FP8(1.0f), ObjectRef::Invalid));
    EXPECT_FALSE(characterState.hasPerk(Ego::Perks::NIGHT_VISION));
    characterState.addPerk(Ego::Perks::NIGHT_VISION);

    EXPECT_FLOAT_EQ(characterState.getLife(), object->getLife());
    EXPECT_FLOAT_EQ(characterState.getMana(), object->getMana());
    EXPECT_FLOAT_EQ(characterState.getAttribute(Ego::Attribute::SPELL_POWER), object->getAttribute(Ego::Attribute::SPELL_POWER));
    EXPECT_EQ(characterState.getExperience(), 42u);
    EXPECT_EQ(characterState.getExperienceLevelIndex(), 3);
    EXPECT_EQ(characterState.getReloadTimer(), 11);
    EXPECT_EQ(characterState.getAmmoMax(), 9);
    EXPECT_EQ(characterState.getAmmo(), 4);
    EXPECT_EQ(characterState.getGrogTimer(), 6);
    EXPECT_EQ(characterState.getDazeTimer(), 8);
    EXPECT_TRUE(characterState.isKursed());
    EXPECT_GT(object->getBaseAttribute(Ego::Attribute::MIGHT), mightBefore);
    EXPECT_GT(object->getMana(), manaBefore);
    EXPECT_TRUE(characterState.hasPerk(Ego::Perks::NIGHT_VISION));
    EXPECT_TRUE(object->hasPerk(Ego::Perks::NIGHT_VISION));
}

TEST_F(ObjectAccessorFixture, CharacterStateRoleSurfaceSupportsExperienceGrantParity)
{
    auto& objectHandler = beginActiveTestModule();
    auto directObject = makeFollower(objectHandler, 30507);
    auto roleObject = makeFollower(objectHandler, 30508);
    ASSERT_NE(directObject, nullptr);
    ASSERT_NE(roleObject, nullptr);

    directObject->setInvincible(false);
    roleObject->setInvincible(false);
    directObject->setExperience(0);
    roleObject->setExperience(0);
    directObject->setBaseAttribute(Ego::Attribute::INTELLECT, 10.0f);
    roleObject->setBaseAttribute(Ego::Attribute::INTELLECT, 10.0f);

    ICharacterState& characterState = *roleObject;

    directObject->giveExperience(48, XP_DIRECT, false);
    characterState.giveExperience(48, XP_DIRECT, false);

    EXPECT_EQ(characterState.getExperience(), roleObject->getExperience());
    EXPECT_EQ(roleObject->getExperience(), directObject->getExperience());
    EXPECT_GT(roleObject->getExperience(), 0u);
}

TEST_F(ObjectAccessorFixture, VisualControlRoleSurfaceSupportsShiftFlagAndRenderStateMutation)
{
    auto object = makeFollower(30507);
    ASSERT_NE(object, nullptr);

    IVisualControl& visual = *object;

    visual.setRedShift(2);
    visual.setGreenShift(4);
    visual.setBlueShift(6);
    visual.setLight(111);
    visual.setAlpha(143);
    visual.setNameKnown(true);
    visual.setAmmoKnown(true);
    visual.setSparkle(7);

    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::RED_SHIFT), 2.0f);
    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::GREEN_SHIFT), 4.0f);
    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::BLUE_SHIFT), 6.0f);
    EXPECT_EQ(object->getLight(), 111);
    EXPECT_EQ(object->getAlpha(), 143);
    EXPECT_TRUE(object->isNameKnown());
    EXPECT_TRUE(object->isAmmoKnown());
    EXPECT_EQ(object->getSparkle(), 7);

    ASSERT_GE(object->getVertexCount(), 3u);
    object->inst._vertexList[0].pos[ZZ] = 5.0f;
    object->inst._vertexList[1].pos[ZZ] = 15.0f;
    object->inst._vertexList[2].pos[ZZ] = 25.0f;

    const int flashedLight = static_cast<int>(255 * idlib::fraction<float, 1, 255>());
    visual.flash(255);
    EXPECT_EQ(object->getAmbientColour(), flashedLight);
    EXPECT_EQ(object->getVertex(0).color_dir, flashedLight);

    visual.flashVariableHeight(0, 10, 100, 20);
    EXPECT_FLOAT_EQ(object->getVertex(0).col[RR], 0.0f);
    EXPECT_FLOAT_EQ(object->getVertex(0).col[GG], 0.0f);
    EXPECT_FLOAT_EQ(object->getVertex(0).col[BB], 0.0f);
    EXPECT_FLOAT_EQ(object->getVertex(1).col[RR], 50.0f);
    EXPECT_FLOAT_EQ(object->getVertex(1).col[GG], 50.0f);
    EXPECT_FLOAT_EQ(object->getVertex(1).col[BB], 50.0f);
    EXPECT_FLOAT_EQ(object->getVertex(2).col[RR], 100.0f);
    EXPECT_FLOAT_EQ(object->getVertex(2).col[GG], 100.0f);
    EXPECT_FLOAT_EQ(object->getVertex(2).col[BB], 100.0f);
}

TEST_F(ObjectAccessorFixture, DamageableRoleSurfaceSupportsBoundedCombatQueriesAndCalls)
{
    auto& objectHandler = beginActiveTestModule();
    auto target = makeFollower(objectHandler, 30501);
    auto attacker = makeFollower(objectHandler, 30502);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(attacker, nullptr);

    IDamageable& damageable = *target;
    const IDamageable& constDamageable = *target;

    target->setDamageTargetType(DamageType::DAMAGE_FIRE);
    target->setReaffirmDamageType(DamageType::DAMAGE_ZAP);
    damageable.setDamageTimer(18);

    EXPECT_EQ(constDamageable.getObjRef(), target->getObjRef());
    EXPECT_TRUE(constDamageable.isAlive());
    EXPECT_FALSE(constDamageable.isInvincible());
    EXPECT_EQ(constDamageable.getDamageTimer(), 18);
    EXPECT_EQ(constDamageable.getDamageTargetType(), DamageType::DAMAGE_FIRE);
    EXPECT_EQ(constDamageable.getReaffirmDamageType(), DamageType::DAMAGE_ZAP);
    EXPECT_FLOAT_EQ(constDamageable.getDamageReduction(DAMAGE_SLASH), target->getDamageReduction(DAMAGE_SLASH));

    target->setInvincible(true);
    const IPair preventedDamage{FLOAT_TO_FP8(5.0f), 0};
    EXPECT_EQ(damageable.damage(ATK_FRONT, preventedDamage, DAMAGE_FIRE, attacker->getTeamRef(), attacker, false, false, false), 0);
    EXPECT_TRUE(target->isAlive());

    target->setInvincible(false);
    target->_currentLife = std::max(1.0f, target->getAttribute(Ego::Attribute::MAX_LIFE) - 5.0f);
    const float lifeBeforeHeal = target->getLife();
    EXPECT_TRUE(damageable.heal(attacker, FLOAT_TO_FP8(1.0f), true));
    EXPECT_GT(target->getLife(), lifeBeforeHeal);

    target->setInvincible(true);
    damageable.kill(attacker, false);
    EXPECT_TRUE(target->isAlive());
}

TEST_F(ObjectAccessorFixture, PhysicalRoleSurfaceExposesCollisionShapeAndOrientationState)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 30503);
    ASSERT_NE(object, nullptr);

    bumper_t initialBump;
    initialBump.size = 11.0f;
    initialBump.size_big = 13.0f;
    initialBump.height = 17.0f;

    bumper_t currentBump;
    currentBump.size = 19.0f;
    currentBump.size_big = 23.0f;
    currentBump.height = 29.0f;

    bumper_t looseBump;
    looseBump.size = 31.0f;
    looseBump.size_big = 37.0f;
    looseBump.height = 41.0f;

    oct_bb_t minCollisionVolume(oct_vec_v2_t(-1.0f, -2.0f, -3.0f, -4.0f, -5.0f));
    oct_bb_t maxCollisionVolume(oct_vec_v2_t(6.0f, 7.0f, 8.0f, 9.0f, 10.0f));
    std::array<oct_bb_t, SLOT_COUNT> slotCollisionVolumes;
    slotCollisionVolumes.fill(oct_bb_t());
    slotCollisionVolumes[SLOT_LEFT] = oct_bb_t(oct_vec_v2_t(11.0f, 12.0f, 13.0f, 14.0f, 15.0f));
    slotCollisionVolumes[SLOT_RIGHT] = oct_bb_t(oct_vec_v2_t(16.0f, 17.0f, 18.0f, 19.0f, 20.0f));

    object->initializeBaseBump(initialBump);
    object->setCurrentBump(currentBump);
    object->setLooseBump(looseBump);
    object->setCollisionVolumes(minCollisionVolume, maxCollisionVolume, slotCollisionVolumes);
    object->setPosition(17.0f, 19.0f, 23.0f);
    object->setSpawnPosition(Ego::Vector3f(29.0f, 31.0f, 37.0f));
    object->setVelocity(Ego::Vector3f(41.0f, 43.0f, 47.0f));
    object->setFacingZ(Facing(1111));
    object->setMapTwistFacingX(Facing(2222));
    object->setMapTwistFacingY(Facing(3333));
    object->setPreviousFacingZ(Facing(4444));

    const IPhysical& physical = *object;

    EXPECT_FLOAT_EQ(physical.getInitialBump().size, 11.0f);
    EXPECT_FLOAT_EQ(physical.getSavedBump().size, 11.0f);
    EXPECT_FLOAT_EQ(physical.getCurrentBump().size, 19.0f);
    EXPECT_FLOAT_EQ(physical.getLooseBump().size, 31.0f);
    EXPECT_FLOAT_EQ(physical.getPosZ(), 23.0f);
    EXPECT_FLOAT_EQ(physical.getVelocity().x(), 41.0f);
    EXPECT_FLOAT_EQ(physical.getVelocity().y(), 43.0f);
    EXPECT_FLOAT_EQ(physical.getVelocity().z(), 47.0f);
    EXPECT_FLOAT_EQ(physical.getSpawnPosition().x(), 29.0f);
    EXPECT_FLOAT_EQ(physical.getSpawnPosition().y(), 31.0f);
    EXPECT_FLOAT_EQ(physical.getSpawnPosition().z(), 37.0f);
    EXPECT_FLOAT_EQ(physical.getFloorElevation(), object->getFloorElevation());
    EXPECT_FLOAT_EQ(physical.getMinCollisionVolume()._mins[OCT_X], -1.0f);
    EXPECT_FLOAT_EQ(physical.getMaxCollisionVolume()._mins[OCT_X], 6.0f);
    EXPECT_FLOAT_EQ(physical.getSlotCollisionVolume(SLOT_LEFT)._mins[OCT_X], 11.0f);
    EXPECT_FLOAT_EQ(physical.getSlotCollisionVolume(SLOT_RIGHT)._mins[OCT_X], 16.0f);
    EXPECT_EQ(physical.getFacingZ(), Facing(1111));
    EXPECT_EQ(physical.getMapTwistFacingX(), Facing(2222));
    EXPECT_EQ(physical.getMapTwistFacingY(), Facing(3333));
    EXPECT_EQ(physical.getPreviousFacingZ(), Facing(4444));
}

TEST_F(ObjectAccessorFixture, MovementControlRoleSurfaceSupportsBoundedMotionMutation)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 30516);
    ASSERT_NE(object, nullptr);

    IMovementControl& movement = *object;

    object->setPosition(64.0f, 64.0f, 0.0f);
    movement.setTurnMode(TURNMODE_WATCHTARGET);
    movement.setBumpHeight(15.0f);
    movement.setBumpWidth(8.0f);
    movement.setLatchButton(LATCHBUTTON_LEFT, true);
    movement.setVelocity(Ego::Vector3f(4.0f, 5.0f, 6.0f));
    movement.setDesiredVelocity(Ego::Vector2f(2.0f, 0.0f));
    movement.setJumpTimer(13);
    movement.movePosition(1.0f, 2.0f, 3.0f);
    movement.setReloadTimer(9);
    movement.setShadowSize(21);
    movement.setSavedShadowSize(7);
    movement.setFlyHeight(11.0f);

    EXPECT_EQ(object->getTurnMode(), TURNMODE_WATCHTARGET);
    EXPECT_FLOAT_EQ(object->getCurrentBump().height, 15.0f * object->getFat());
    EXPECT_TRUE(object->_inputLatchesPressed[LATCHBUTTON_LEFT]);
    EXPECT_FLOAT_EQ(movement.getVelocity().x(), 4.0f);
    EXPECT_FLOAT_EQ(movement.getVelocity().y(), 5.0f);
    EXPECT_FLOAT_EQ(movement.getDesiredVelocity().x(), 1.0f);
    EXPECT_FLOAT_EQ(movement.getDesiredVelocity().y(), 0.0f);
    EXPECT_FLOAT_EQ(movement.getVelocity().z(), 6.0f);
    EXPECT_FLOAT_EQ(object->getVelocity().x(), 4.0f);
    EXPECT_FLOAT_EQ(object->getVelocity().y(), 5.0f);
    EXPECT_FLOAT_EQ(object->getVelocity().z(), 6.0f);
    EXPECT_EQ(object->getJumpTimer(), 13);
    EXPECT_FLOAT_EQ(object->getPosX(), 65.0f);
    EXPECT_FLOAT_EQ(object->getPosY(), 66.0f);
    EXPECT_FLOAT_EQ(object->getPosZ(), 3.0f);
    EXPECT_EQ(object->getReloadTimer(), 9);
    EXPECT_EQ(object->getShadowSize(), 21u);
    EXPECT_EQ(object->getSavedShadowSize(), 7u);
    EXPECT_FLOAT_EQ(object->getBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT), 11.0f);

    EXPECT_TRUE(movement.teleport(Ego::Vector3f(96.0f, 96.0f, 0.0f), Facing(1234)));
    EXPECT_FLOAT_EQ(object->getPosX(), 96.0f);
    EXPECT_FLOAT_EQ(object->getPosY(), 96.0f);
    EXPECT_EQ(object->getFacingZ(), Facing(1234));
}

TEST_F(ObjectAccessorFixture, AnimationControlRoleSurfaceSupportsActionResolutionAndAnimationMutation)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 30517);
    ASSERT_NE(object, nullptr);

    IAnimationControl& animation = *object;

    const ModelAction currentAction = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    const ModelAction nextAction = findValidAction(object, {ACTION_WA, ACTION_WB, ACTION_WC, ACTION_DA, ACTION_DB, ACTION_DC}, currentAction);
    ASSERT_NE(currentAction, ACTION_COUNT);
    ASSERT_NE(nextAction, ACTION_COUNT);

    EXPECT_EQ(animation.resolveModelAction(static_cast<int>(nextAction)), nextAction);

    const auto& model = object->inst.getModelDescriptor();
    const int currentLastFrame = model->getLastFrame(currentAction);
    const int nextFirstFrame = model->getFirstFrame(nextAction);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WB;
    object->inst._canBeInterrupted = false;
    object->inst._sourceFrameIndex = model->getFirstFrame(currentAction);
    object->inst._targetFrameIndex = currentLastFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;

    EXPECT_TRUE(animation.startAnimation(nextAction, true, true));
    animation.setActionKeep(true);
    animation.removeInterpolation();

    EXPECT_EQ(object->getCurrentAnimation(), nextAction);
    EXPECT_TRUE(object->inst._freezeAtLastFrame);
    EXPECT_EQ(object->inst._sourceFrameIndex, nextFirstFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, nextFirstFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, AIAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(3051);
    ASSERT_NE(object, nullptr);

    EXPECT_EQ(object->getAIAlertBits(), 0);
    EXPECT_EQ(object->getAIStateValue(), 0);
    EXPECT_EQ(object->getAIContent(), 0);
    EXPECT_EQ(object->getAIPassage(), 0);
    EXPECT_EQ(object->getAITimer(), 0u);
    EXPECT_EQ(object->getAIPoofTime(), -1);
    EXPECT_EQ(object->getAIOwner(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAIChild(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAITarget(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAILastAttacker(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAIBumped(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAILastItemUsed(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAILastHit(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAILastDamageType(), DamageType::DAMAGE_DIRECT);
    EXPECT_EQ(object->getAILastDirection(), Facing(0));

    object->addAIAlertBits(ALERTIF_BLOCKED | ALERTIF_GRABBED);
    object->clearAIAlertBits(ALERTIF_GRABBED);
    object->setAIStateValue(11);
    object->setAIContent(12);
    object->setAIPassage(13);
    object->setAITimer(14);
    object->setAIPoofTime(15);
    object->setAIOwner(ObjectRef(16));
    object->setAIChild(ObjectRef(17));
    object->setAITarget(ObjectRef(18));
    object->setAILastAttacker(ObjectRef(19));
    object->setAILastItemUsed(ObjectRef(20));
    object->setAILastHit(ObjectRef(21));
    object->setAILastDamageType(DamageType::DAMAGE_FIRE);
    object->setAILastDirection(Facing(22));
    object->setAIMaxSpeed(0.75f);

    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_BLOCKED));
    EXPECT_FALSE(object->hasAnyAIAlertBits(ALERTIF_GRABBED));
    EXPECT_EQ(object->getAIStateValue(), 11);
    EXPECT_EQ(object->getAIContent(), 12);
    EXPECT_EQ(object->getAIPassage(), 13);
    EXPECT_EQ(object->getAITimer(), 14u);
    EXPECT_EQ(object->getAIPoofTime(), 15);
    EXPECT_EQ(object->getAIOwner(), ObjectRef(16));
    EXPECT_EQ(object->getAIChild(), ObjectRef(17));
    EXPECT_EQ(object->getAITarget(), ObjectRef(18));
    EXPECT_EQ(object->getAILastAttacker(), ObjectRef(19));
    EXPECT_EQ(object->getAILastItemUsed(), ObjectRef(20));
    EXPECT_EQ(object->getAILastHit(), ObjectRef(21));
    EXPECT_EQ(object->getAILastDamageType(), DamageType::DAMAGE_FIRE);
    EXPECT_EQ(object->getAILastDirection(), Facing(22));
    EXPECT_FLOAT_EQ(object->getAIMaxSpeed(), 0.75f);
}

TEST_F(ObjectAccessorFixture, ScriptRoleSurfaceSupportsInterfaceBasedStatePublication)
{
    auto object = makeFollower(30511);
    ASSERT_NE(object, nullptr);

    IScriptable& scriptable = *object;
    const IScriptable& constScriptable = *object;

    scriptable.setAIStateValue(31);
    scriptable.setAIContent(32);
    scriptable.setAIPassage(33);
    scriptable.setAITimer(34);
    scriptable.setAIPoofTime(35);
    scriptable.setAIOwner(ObjectRef(36));
    scriptable.setAITarget(ObjectRef(37));
    scriptable.addAIAlertBits(ALERTIF_CHANGED | ALERTIF_ORDERED);

    EXPECT_EQ(constScriptable.getAIStateValue(), 31);
    EXPECT_EQ(constScriptable.getAIContent(), 32);
    EXPECT_EQ(constScriptable.getAIPassage(), 33);
    EXPECT_EQ(constScriptable.getAITimer(), 34u);
    EXPECT_EQ(constScriptable.getAIPoofTime(), 35);
    EXPECT_EQ(constScriptable.getAIOwner(), ObjectRef(36));
    EXPECT_EQ(constScriptable.getAITarget(), ObjectRef(37));
    EXPECT_TRUE(constScriptable.hasAnyAIAlertBits(ALERTIF_CHANGED));
    EXPECT_TRUE(constScriptable.hasAnyAIAlertBits(ALERTIF_ORDERED));
}

TEST_F(ObjectAccessorFixture, ScriptRuntimeStateAndPublicAccessorsStayInSync)
{
    auto object = makeFollower(30515);
    ASSERT_NE(object, nullptr);

    auto& aiState = Ego::Script::runtimeState(*object);
    aiState.poof_time = 61;
    aiState.owner = ObjectRef(62);
    aiState.setTarget(ObjectRef(63));
    aiState.setLastAttacker(ObjectRef(64));
    aiState.alert = ALERTIF_BLOCKED | ALERTIF_CHANGED;

    EXPECT_EQ(object->getAIPoofTime(), 61);
    EXPECT_EQ(object->getAIOwner(), ObjectRef(62));
    EXPECT_EQ(object->getAITarget(), ObjectRef(63));
    EXPECT_EQ(object->getAILastAttacker(), ObjectRef(64));
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_BLOCKED));
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_CHANGED));

    object->setAIPoofTime(71);
    object->setAIOwner(ObjectRef(72));
    object->setAITarget(ObjectRef(73));
    object->setAILastAttacker(ObjectRef(74));
    object->setAIAlertBits(ALERTIF_GRABBED);

    EXPECT_EQ(aiState.poof_time, 71);
    EXPECT_EQ(aiState.owner, ObjectRef(72));
    EXPECT_EQ(aiState.getTarget(), ObjectRef(73));
    EXPECT_EQ(aiState.getLastAttacker(), ObjectRef(74));
    EXPECT_EQ(aiState.alert, ALERTIF_GRABBED);
}

TEST_F(ObjectAccessorFixture, AIOrderHelperPublishesOrderedAlertAndTracksOutstandingOrder)
{
    auto object = makeFollower(3052);
    ASSERT_NE(object, nullptr);

    auto& aiState = Ego::Script::runtimeState(*object);
    EXPECT_FALSE(object->hasAnyAIAlertBits(ALERTIF_ORDERED));
    EXPECT_EQ(aiState.order_value, 0u);
    EXPECT_EQ(aiState.order_counter, 0);

    EXPECT_TRUE(object->addAIOrder(123u, 4));
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_ORDERED));
    EXPECT_EQ(aiState.order_value, 123u);
    EXPECT_EQ(aiState.order_counter, 4);

    EXPECT_FALSE(object->addAIOrder(456u, 7));
    EXPECT_EQ(aiState.order_value, 456u);
    EXPECT_EQ(aiState.order_counter, 7);
}

TEST_F(ObjectAccessorFixture, AIChangeHelperPublishesChangedStateUntilNoNewSignalIsNeeded)
{
    auto object = makeFollower(3053);
    ASSERT_NE(object, nullptr);

    auto& aiState = Ego::Script::runtimeState(*object);
    EXPECT_FALSE(aiState.changed);
    EXPECT_FALSE(object->hasAnyAIAlertBits(ALERTIF_CHANGED));

    EXPECT_TRUE(object->markAIChanged());
    EXPECT_TRUE(aiState.changed);
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_CHANGED));

    EXPECT_FALSE(object->markAIChanged());

    object->clearAIAlertBits(ALERTIF_CHANGED);
    EXPECT_TRUE(object->markAIChanged());
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_CHANGED));
}

TEST_F(ObjectAccessorFixture, AIBumpHelperRejectsInvalidObjectsAndThrottlesRepeatedAlerts)
{
    ObjectHandler& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3054);
    auto bumpedObject = makeFollower(objectHandler, 3055);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(bumpedObject, nullptr);

    GameSessionContext::get().worldUpdateCount() = 10;
    object->setAIAlertBits(0);

    EXPECT_FALSE(object->recordAIBump(ObjectRef::Invalid));
    EXPECT_FALSE(object->hasAnyAIAlertBits(ALERTIF_BUMPED));

    auto& aiState = Ego::Script::runtimeState(*object);
    EXPECT_TRUE(object->recordAIBump(bumpedObject->getObjRef()));
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_BUMPED));
    EXPECT_EQ(aiState.getBumped(), bumpedObject->getObjRef());
    EXPECT_EQ(aiState.bumplast_time, 10);

    object->clearAIAlertBits(ALERTIF_BUMPED);
    EXPECT_TRUE(object->recordAIBump(bumpedObject->getObjRef()));
    EXPECT_FALSE(object->hasAnyAIAlertBits(ALERTIF_BUMPED));
    EXPECT_EQ(aiState.bumplast_time, 10);

    GameSessionContext::get().worldUpdateCount() = 1010;
    EXPECT_TRUE(object->recordAIBump(bumpedObject->getObjRef()));
    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_BUMPED));
    EXPECT_EQ(aiState.bumplast_time, 1010);
}

TEST_F(ObjectAccessorFixture, AIResetAndSpawnHelpersRestoreDocumentedScriptStateDefaults)
{
    ObjectHandler& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 3056);
    ASSERT_NE(object, nullptr);

    auto& aiState = Ego::Script::runtimeState(*object);
    aiState.poof_time = 44;
    aiState.changed = true;
    aiState.terminate = true;
    aiState.setSelf(ObjectRef(31));
    aiState.setTarget(ObjectRef(32));
    aiState.setOldTarget(ObjectRef(33));
    aiState.setBumped(ObjectRef(34));
    aiState.setLastAttacker(ObjectRef(35));
    aiState.owner = ObjectRef(36);
    aiState.child = ObjectRef(37);
    aiState.alert = ALERTIF_BLOCKED | ALERTIF_ORDERED;
    aiState.state = 38;
    aiState.content = 39;
    aiState.passage = 40;
    aiState.timer = 41;
    aiState.x[0] = 42;
    aiState.y[1] = 43;
    aiState.maxSpeed = 0.5f;
    aiState.bumplast_time = 45;
    aiState.hitlast = ObjectRef(46);
    aiState.directionlast = Facing(47);
    aiState.damagetypelast = DamageType::DAMAGE_FIRE;
    aiState.lastitemused = ObjectRef(48);
    aiState.order_value = 49;
    aiState.order_counter = 50;
    aiState.wp_valid = true;
    aiState.astar_timer = 51;
    waypoint_list_t::push(aiState.wp_lst, 12, 34);

    object->resetAIState();

    EXPECT_EQ(aiState.poof_time, -1);
    EXPECT_FALSE(aiState.changed);
    EXPECT_FALSE(aiState.terminate);
    EXPECT_EQ(aiState.getSelf(), ObjectRef::Invalid);
    EXPECT_EQ(aiState.getTarget(), ObjectRef::Invalid);
    EXPECT_EQ(aiState.getOldTarget(), ObjectRef::Invalid);
    EXPECT_EQ(aiState.getBumped(), ObjectRef::Invalid);
    EXPECT_EQ(aiState.getLastAttacker(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAIOwner(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAIChild(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAIAlertBits(), 0);
    EXPECT_EQ(object->getAIStateValue(), 0);
    EXPECT_EQ(object->getAIContent(), 0);
    EXPECT_EQ(object->getAIPassage(), 0);
    EXPECT_EQ(object->getAITimer(), 0u);
    EXPECT_EQ(aiState.x[0], 0);
    EXPECT_EQ(aiState.y[1], 0);
    EXPECT_FLOAT_EQ(object->getAIMaxSpeed(), 1.0f);
    EXPECT_EQ(aiState.bumplast_time, 0);
    EXPECT_EQ(object->getAILastHit(), ObjectRef::Invalid);
    EXPECT_EQ(object->getAILastDirection(), Facing(0));
    EXPECT_EQ(object->getAILastDamageType(), DamageType::DAMAGE_DIRECT);
    EXPECT_EQ(object->getAILastItemUsed(), ObjectRef::Invalid);
    EXPECT_EQ(aiState.order_value, 0u);
    EXPECT_EQ(aiState.order_counter, 0);
    EXPECT_FALSE(aiState.wp_valid);
    EXPECT_TRUE(waypoint_list_t::empty(aiState.wp_lst));
    EXPECT_EQ(aiState.astar_timer, 0u);

    object->spawnAIState(6);

    EXPECT_EQ(aiState.getSelf(), object->getObjRef());
    EXPECT_EQ(aiState.getTarget(), object->getObjRef());
    EXPECT_EQ(aiState.getOldTarget(), object->getObjRef());
    EXPECT_EQ(aiState.getBumped(), object->getObjRef());
    EXPECT_EQ(aiState.alert, ALERTIF_SPAWNED);
    EXPECT_EQ(aiState.state, object->getProfile()->getStateOverride());
    EXPECT_EQ(aiState.content, object->getProfile()->getContentOverride());
    EXPECT_EQ(aiState.passage, 0);
    EXPECT_EQ(aiState.owner, object->getObjRef());
    EXPECT_EQ(aiState.child, object->getObjRef());
    EXPECT_FLOAT_EQ(aiState.maxSpeed, 1.0f);
    EXPECT_EQ(aiState.hitlast, object->getObjRef());
    EXPECT_EQ(aiState.order_value, 0u);
    EXPECT_EQ(aiState.order_counter, 6);
    EXPECT_FALSE(aiState.wp_valid);
    EXPECT_FALSE(waypoint_list_t::empty(aiState.wp_lst));
    EXPECT_TRUE(ai_state_t::get_wp(aiState));
    EXPECT_TRUE(aiState.wp_valid);
    EXPECT_FLOAT_EQ(aiState.wp[kX], object->getSpawnPosition().x());
    EXPECT_FLOAT_EQ(aiState.wp[kY], object->getSpawnPosition().y());
}

TEST_F(ObjectAccessorFixture, MovementAndCollisionMaskAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(306);
    ASSERT_NE(object, nullptr);

    EXPECT_EQ(object->getStoppedByMask(), 0);
    EXPECT_EQ(object->getBumpListNext(), ObjectRef::Invalid);
    EXPECT_EQ(object->getTurnMode(), TURNMODE_VELOCITY);

    object->setStoppedByMask(23);
    object->setBumpListNext(ObjectRef(24));
    object->setTurnMode(TURNMODE_SPIN);

    EXPECT_EQ(object->getStoppedByMask(), 23);
    EXPECT_EQ(object->getBumpListNext(), ObjectRef(24));
    EXPECT_EQ(object->getTurnMode(), TURNMODE_SPIN);
}

TEST_F(ObjectAccessorFixture, OrientationAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(307);
    ASSERT_NE(object, nullptr);

    object->setFacingZ(Facing(1111));
    object->setMapTwistFacingX(Facing(2222));
    object->setMapTwistFacingY(Facing(3333));
    object->setPreviousFacingZ(Facing(4444));

    EXPECT_EQ(object->getFacingZ(), Facing(1111));
    EXPECT_EQ(object->getMapTwistFacingX(), Facing(2222));
    EXPECT_EQ(object->getMapTwistFacingY(), Facing(3333));
    EXPECT_EQ(object->getPreviousFacingZ(), Facing(4444));
}

TEST_F(ObjectAccessorFixture, CollisionShapeAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(308);
    ASSERT_NE(object, nullptr);

    bumper_t initialBump;
    initialBump.size = 11.0f;
    initialBump.size_big = 13.0f;
    initialBump.height = 17.0f;

    bumper_t currentBump;
    currentBump.size = 19.0f;
    currentBump.size_big = 23.0f;
    currentBump.height = 29.0f;

    bumper_t looseBump;
    looseBump.size = 31.0f;
    looseBump.size_big = 37.0f;
    looseBump.height = 41.0f;

    oct_bb_t minCollisionVolume(oct_vec_v2_t(-1.0f, -2.0f, -3.0f, -4.0f, -5.0f));
    oct_bb_t maxCollisionVolume(oct_vec_v2_t(6.0f, 7.0f, 8.0f, 9.0f, 10.0f));
    std::array<oct_bb_t, SLOT_COUNT> slotCollisionVolumes;
    slotCollisionVolumes.fill(oct_bb_t());
    slotCollisionVolumes[SLOT_LEFT] = oct_bb_t(oct_vec_v2_t(11.0f, 12.0f, 13.0f, 14.0f, 15.0f));
    slotCollisionVolumes[SLOT_RIGHT] = oct_bb_t(oct_vec_v2_t(16.0f, 17.0f, 18.0f, 19.0f, 20.0f));

    object->initializeBaseBump(initialBump);
    object->setCurrentBump(currentBump);
    object->setLooseBump(looseBump);
    object->setCollisionVolumes(minCollisionVolume, maxCollisionVolume, slotCollisionVolumes);

    EXPECT_FLOAT_EQ(object->getInitialBump().size, 11.0f);
    EXPECT_FLOAT_EQ(object->getInitialBump().size_big, 13.0f);
    EXPECT_FLOAT_EQ(object->getInitialBump().height, 17.0f);
    EXPECT_FLOAT_EQ(object->getSavedBump().size, 11.0f);
    EXPECT_FLOAT_EQ(object->getSavedBump().size_big, 13.0f);
    EXPECT_FLOAT_EQ(object->getSavedBump().height, 17.0f);
    EXPECT_FLOAT_EQ(object->getCurrentBump().size, 19.0f);
    EXPECT_FLOAT_EQ(object->getCurrentBump().size_big, 23.0f);
    EXPECT_FLOAT_EQ(object->getCurrentBump().height, 29.0f);
    EXPECT_FLOAT_EQ(object->getLooseBump().size, 31.0f);
    EXPECT_FLOAT_EQ(object->getLooseBump().size_big, 37.0f);
    EXPECT_FLOAT_EQ(object->getLooseBump().height, 41.0f);
    EXPECT_FLOAT_EQ(object->getMinCollisionVolume()._mins[OCT_X], -1.0f);
    EXPECT_FLOAT_EQ(object->getMinCollisionVolume()._maxs[OCT_X], -1.0f);
    EXPECT_FLOAT_EQ(object->getMaxCollisionVolume()._mins[OCT_X], 6.0f);
    EXPECT_FLOAT_EQ(object->getMaxCollisionVolume()._maxs[OCT_X], 6.0f);
    EXPECT_FLOAT_EQ(object->getSlotCollisionVolume(SLOT_LEFT)._mins[OCT_X], 11.0f);
    EXPECT_FLOAT_EQ(object->getSlotCollisionVolume(SLOT_RIGHT)._mins[OCT_X], 16.0f);
}

TEST_F(ObjectAccessorFixture, AppearanceAndProfileAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(309);
    ASSERT_NE(object, nullptr);

    const SKIN_T validSkin = object->getProfile()->isValidSkin(1) ? 1 : 0;
    ASSERT_TRUE(object->getProfile()->isValidSkin(validSkin));

    EXPECT_FALSE(object->isOverlay());

    object->setBaseSkin(3);
    object->setBaseModelRef(ObjectProfileRef(44));
    object->setOverlay(true);
    object->setBaseShadowSize(9.5f);
    object->setSavedShadowSize(12);
    object->setShadowSize(15);

    EXPECT_EQ(object->getBaseSkin(), 3);
    EXPECT_EQ(object->getBaseModelRef(), ObjectProfileRef(44));
    EXPECT_TRUE(object->isOverlay());
    EXPECT_FLOAT_EQ(object->getBaseShadowSize(), 9.5f);
    EXPECT_EQ(object->getSavedShadowSize(), 12u);
    EXPECT_EQ(object->getShadowSize(), 15u);

    EXPECT_TRUE(object->setSkin(validSkin));
    EXPECT_EQ(object->getSkin(), validSkin);
}

TEST_F(ObjectAccessorFixture, AppearanceProfileRoleSurfaceExposesSkinPricingDressinessAndSpellQueries)
{
    auto dressySpellObject = makeObject(_objectHandler, "mp_data/globalobjects/players/healer.obj", 3091);
    auto plainObject = makeObject(_objectHandler, "mp_data/globalobjects/players/rogue.obj", 3092);
    ASSERT_NE(dressySpellObject, nullptr);
    ASSERT_NE(plainObject, nullptr);

    IAppearanceProfile& appearance = *dressySpellObject;
    const IAppearanceProfile& constAppearance = *dressySpellObject;
    const IAppearanceProfile& constPlainAppearance = *plainObject;

    ASSERT_TRUE(appearance.setSkin(0));
    EXPECT_EQ(constAppearance.getSkin(), dressySpellObject->getSkin());
    EXPECT_EQ(constAppearance.getSkinCost(0), dressySpellObject->getProfile()->getSkinInfo(0).cost);
    EXPECT_TRUE(constAppearance.isCurrentSkinDressy());
    EXPECT_TRUE(constAppearance.hasIntellectDamageParticle());
    EXPECT_FALSE(appearance.setSkin(9999));

    ASSERT_TRUE(plainObject->setSkin(2));
    plainObject->getProfile()->_skinInfo[2].dressy = false;
    EXPECT_FALSE(constPlainAppearance.isCurrentSkinDressy());
    EXPECT_FALSE(constPlainAppearance.hasIntellectDamageParticle());
}

TEST_F(ObjectAccessorFixture, MorphControlRoleSurfaceSupportsMorphAndResizeState)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/players/rogue.obj", 3093);
    auto target = makeObject(objectHandler, "mp_data/globalobjects/players/healer.obj", 3094);
    ASSERT_NE(object, nullptr);
    ASSERT_NE(target, nullptr);

    IMorphControl& morphControl = *object;
    const IMorphControl& targetMorphControl = *target;

    morphControl.setTargetFat(2.25f);
    morphControl.setResizeTimeRemaining(33);
    EXPECT_FLOAT_EQ(morphControl.getTargetFat(), 2.25f);
    EXPECT_EQ(morphControl.getResizeTimeRemaining(), 33);

    const ObjectProfileRef objectBaseModelBefore = object->getBaseModelRef();
    const ObjectProfileRef targetBaseModel = targetMorphControl.getBaseModelRef();
    const SKIN_T targetSkin = targetMorphControl.getSkin();
    const float targetFat = targetMorphControl.getFat();
    const ObjectProfileRef publishedBaseModel = ObjectProfileRef(55);

    morphControl.setBaseModelRef(publishedBaseModel);
    EXPECT_EQ(object->getBaseModelRef(), publishedBaseModel);

    morphControl.polymorphObject(targetBaseModel, targetSkin);
    morphControl.setTargetFat(targetFat);
    morphControl.setResizeTimeRemaining(Object::SIZETIME);

    EXPECT_NE(objectBaseModelBefore, publishedBaseModel);
    EXPECT_EQ(object->getBaseModelRef(), publishedBaseModel);
    EXPECT_EQ(object->getProfileID(), targetBaseModel);
    EXPECT_EQ(object->getSkin(), targetSkin);
    EXPECT_FLOAT_EQ(object->getTargetFat(), targetFat);
    EXPECT_EQ(object->getResizeTimeRemaining(), Object::SIZETIME);
}

TEST_F(ObjectAccessorFixture, RenderStateAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(310);
    ASSERT_NE(object, nullptr);

    object->setAlpha(123);
    object->setLight(87);
    object->setSheen(45);
    object->setColorShift(colorshift_t(1, 2, 3));
    object->setUOffset(321);
    object->setVOffset(654);

    EXPECT_EQ(object->getAlpha(), 123);
    EXPECT_EQ(object->getLight(), 87);
    EXPECT_EQ(object->getSheen(), 45);
    EXPECT_EQ(object->getColorShift().red, 1);
    EXPECT_EQ(object->getColorShift().green, 2);
    EXPECT_EQ(object->getColorShift().blue, 3);
    EXPECT_EQ(object->getUOffset(), 321);
    EXPECT_EQ(object->getVOffset(), 654);

    GameSessionContext::get().publishLocalPlayerPerception(LocalPlayerPerceptionState{});

    EXPECT_TRUE(object->hasModelDescriptor());

    GLXvector4f tint;
    object->getTint(tint, false, CHR_ALPHA);

    constexpr float colourScale = 87.0f / 255.0f;
    constexpr float alphaScale = 123.0f / 255.0f;
    EXPECT_NEAR(tint[RR], colourScale / 2.0f, 0.0001f);
    EXPECT_NEAR(tint[GG], colourScale / 4.0f, 0.0001f);
    EXPECT_NEAR(tint[BB], colourScale / 8.0f, 0.0001f);
    EXPECT_NEAR(tint[AA], alphaScale, 0.0001f);

    object->setAlpha(0);
    EXPECT_EQ(object->getReflectionAlpha(), 0);
}

TEST_F(ObjectAccessorFixture, RenderRoleSurfaceExposesRenderPolicyAndGeometryQueries)
{
    auto object = makeFollower(350);
    ASSERT_NE(object, nullptr);

    object->setAlpha(123);
    object->setLight(87);
    object->setSheen(45);
    object->setColorShift(colorshift_t(1, 2, 3));
    object->setMatrix(idlib::identity<Ego::Matrix4f4f>());

    const IRenderable& renderable = *object;

    EXPECT_EQ(renderable.getObjRef(), object->getObjRef());
    EXPECT_EQ(renderable.isPhongMapped(), object->getProfile()->isPhongMapped());
    EXPECT_EQ(renderable.hasReflection(), object->getProfile()->hasReflection());
    EXPECT_EQ(renderable.isDontCullBackfaces(), object->getProfile()->isDontCullBackfaces());
    EXPECT_TRUE(renderable.hasModelDescriptor());
    EXPECT_NE(renderable.getModelDescriptor(), nullptr);
    EXPECT_EQ(renderable.getAlpha(), 123);
    EXPECT_EQ(renderable.getLight(), 87);
    EXPECT_EQ(renderable.getSheen(), 45);
    EXPECT_EQ(renderable.getVertexCount(), object->getVertexCount());
    EXPECT_EQ(&renderable.getMatrix(), &object->getMatrix());
    EXPECT_EQ(&renderable.getReflectionMatrix(), &object->getReflectionMatrix());
    EXPECT_EQ(renderable.getAmbientColour(), object->getAmbientColour());

    GLXvector4f tint;
    renderable.getTint(tint, false, CHR_ALPHA);

    constexpr float colourScale = 87.0f / 255.0f;
    constexpr float alphaScale = 123.0f / 255.0f;
    EXPECT_NEAR(tint[RR], colourScale / 2.0f, 0.0001f);
    EXPECT_NEAR(tint[GG], colourScale / 4.0f, 0.0001f);
    EXPECT_NEAR(tint[BB], colourScale / 8.0f, 0.0001f);
    EXPECT_NEAR(tint[AA], alphaScale, 0.0001f);
}

TEST_F(ObjectAccessorFixture, RenderStateAccessorsApplyReflectionPolicy)
{
    auto object = makeFollower(311);
    ASSERT_NE(object, nullptr);

    object->setAlpha(200);
    object->setLight(60);
    object->setSheen(10);
    object->setColorShift(colorshift_t(1, 2, 3));

    const float altitudeAboveGround = std::max(0.0f, object->getPosZ() - object->getFloorElevation());
    float alphaFade = (255.0f - altitudeAboveGround) * 0.5f;
    alphaFade = Ego::Math::constrain(alphaFade, 0.0f, 255.0f);

    const uint8_t reflectedAlpha = 200 * alphaFade * idlib::fraction<float, 1, 255>();
    EXPECT_EQ(object->getReflectionAlpha(), reflectedAlpha);

    GLXvector4f tint;
    object->getTint(tint, true, CHR_ALPHA);

    const int reflectedLight = 60 * reflectedAlpha * idlib::fraction<float, 1, 255>();
    const float reflectedAlphaScale = reflectedAlpha * idlib::fraction<float, 1, 255>();
    const float reflectedLightScale = reflectedLight * idlib::fraction<float, 1, 255>();
    EXPECT_NEAR(tint[RR], reflectedLightScale / 4.0f, 0.0001f);
    EXPECT_NEAR(tint[GG], reflectedLightScale / 8.0f, 0.0001f);
    EXPECT_NEAR(tint[BB], reflectedLightScale / 16.0f, 0.0001f);
    EXPECT_NEAR(tint[AA], reflectedAlphaScale, 0.0001f);
}

TEST_F(ObjectAccessorFixture, RenderStateAccessorsApplyLocalPlayerPerceptionOverrides)
{
    auto object = makeFollower(312);
    ASSERT_NE(object, nullptr);

    object->setAlpha(30);
    object->setLight(40);
    object->setColorShift(colorshift_t(0, 0, 0));

    LocalPlayerPerceptionState perception;
    perception.seeInvisibleLevel = 1.0f;
    perception.seeDarkMagnitude = 2.0f;
    GameSessionContext::get().publishLocalPlayerPerception(perception);

    GLXvector4f tint;
    object->getTint(tint, false, CHR_ALPHA);

    constexpr float perceivedAlphaScale = SEEINVISIBLE / 255.0f;
    constexpr float perceivedLightScale = 80.0f / 255.0f;
    EXPECT_NEAR(tint[RR], perceivedLightScale, 0.0001f);
    EXPECT_NEAR(tint[GG], perceivedLightScale, 0.0001f);
    EXPECT_NEAR(tint[BB], perceivedLightScale, 0.0001f);
    EXPECT_NEAR(tint[AA], perceivedAlphaScale, 0.0001f);
}

TEST_F(ObjectAccessorFixture, MatrixCacheAccessorsRoundTripAndInvalidate)
{
    auto object = makeFollower(313);
    ASSERT_NE(object, nullptr);

    matrix_cache_t cache;
    cache.valid = true;
    cache.matrix_valid = false;
    cache.type_bits = MAT_WEAPON;
    cache.rotate[kX] = Facing(101);
    cache.rotate[kY] = Facing(202);
    cache.rotate[kZ] = Facing(303);
    cache.pos = Ego::Vector3f(4.0f, 5.0f, 6.0f);
    cache.grip_chr = ObjectRef(77);
    cache.grip_slot = SLOT_RIGHT;
    cache.grip_verts[0] = 8;
    cache.self_scale = Ego::Vector3f(1.5f, 2.5f, 3.5f);

    object->setMatrixCache(cache);

    const matrix_cache_t roundTrip = object->getMatrixCache();
    EXPECT_TRUE(object->hasValidMatrixCache());
    EXPECT_FALSE(object->hasValidMatrixValue());
    EXPECT_FALSE(roundTrip.isValid());
    EXPECT_EQ(roundTrip.type_bits, MAT_WEAPON);
    EXPECT_EQ(roundTrip.rotate[kX], Facing(101));
    EXPECT_EQ(roundTrip.rotate[kY], Facing(202));
    EXPECT_EQ(roundTrip.rotate[kZ], Facing(303));
    EXPECT_FLOAT_EQ(roundTrip.pos[kX], 4.0f);
    EXPECT_FLOAT_EQ(roundTrip.pos[kY], 5.0f);
    EXPECT_FLOAT_EQ(roundTrip.pos[kZ], 6.0f);
    EXPECT_EQ(roundTrip.grip_chr, ObjectRef(77));
    EXPECT_EQ(roundTrip.grip_slot, SLOT_RIGHT);
    EXPECT_EQ(roundTrip.grip_verts[0], 8);
    EXPECT_FLOAT_EQ(roundTrip.self_scale[kX], 1.5f);
    EXPECT_FLOAT_EQ(roundTrip.self_scale[kY], 2.5f);
    EXPECT_FLOAT_EQ(roundTrip.self_scale[kZ], 3.5f);

    object->setMatrixValueValid(true);

    EXPECT_TRUE(object->hasValidMatrixCache());
    EXPECT_TRUE(object->hasValidMatrixValue());
    EXPECT_TRUE(object->getMatrixCache().isValid());

    object->invalidateMatrixCache();

    EXPECT_FALSE(object->hasValidMatrixCache());
    EXPECT_FALSE(object->hasValidMatrixValue());
    EXPECT_FALSE(object->getMatrixCache().isValid());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsProfileResetRestoresRenderDefaultsAndInvalidatesMatrixCache)
{
    auto object = makeFollower(314);
    ASSERT_NE(object, nullptr);

    object->setAlpha(12);
    object->setLight(34);
    object->setSheen(56);
    object->setUOffset(321);
    object->setVOffset(654);

    matrix_cache_t staleCache;
    staleCache.valid = true;
    staleCache.matrix_valid = true;
    staleCache.type_bits = MAT_WEAPON;
    staleCache.grip_chr = ObjectRef(77);
    staleCache.grip_slot = SLOT_RIGHT;
    object->setMatrixCache(staleCache);

    ASSERT_TRUE(object->hasValidMatrixCache());
    ASSERT_TRUE(object->hasValidMatrixValue());

    object->inst.setObjectProfile(object->getProfile());

    EXPECT_EQ(object->getAlpha(), object->getProfile()->getAlpha());
    EXPECT_EQ(object->getLight(), object->getProfile()->getLight());
    EXPECT_EQ(object->getSheen(), object->getProfile()->getSheen());
    EXPECT_EQ(object->getUOffset(), 0);
    EXPECT_EQ(object->getVOffset(), 0);
    EXPECT_TRUE(object->hasModelDescriptor());
    EXPECT_FALSE(object->hasValidMatrixCache());
    EXPECT_FALSE(object->hasValidMatrixValue());
    EXPECT_EQ(object->getMatrixCache().type_bits, MAT_UNKNOWN);
    EXPECT_EQ(object->getMatrixCache().grip_chr, ObjectRef::Invalid);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsProfileResetRestoresLiveIdleAnimationPolicy)
{
    auto object = makeFollower(315);
    ASSERT_NE(object, nullptr);

    object->inst._currentAnimation = ACTION_KA;
    object->inst._nextAnimation = ACTION_KA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = true;
    object->inst._animationRate = 3.0f;

    object->inst.setObjectProfile(object->getProfile());

    EXPECT_EQ(object->getCurrentAnimation(), ACTION_DA);
    EXPECT_FALSE(object->canBeInterrupted());
    EXPECT_FLOAT_EQ(object->getAnimationSpeed(), 1.0f);
    EXPECT_FALSE(object->inst._freezeAtLastFrame);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsProfileResetRestoresDeadDeathAnimationPolicy)
{
    auto object = makeFollower(316);
    ASSERT_NE(object, nullptr);

    object->_isAlive = false;
    object->inst._currentAnimation = ACTION_DA;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;
    object->inst._animationRate = 2.0f;

    object->inst.setObjectProfile(object->getProfile());

    EXPECT_TRUE(ACTION_IS_TYPE(object->getCurrentAnimation(), K));
    EXPECT_FALSE(object->canBeInterrupted());
    EXPECT_FLOAT_EQ(object->getAnimationSpeed(), 1.0f);
    EXPECT_TRUE(object->hasModelDescriptor());
    EXPECT_TRUE(object->inst._freezeAtLastFrame);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsSetActionMutatesAnimationStateWithoutTouchingFrameBookkeeping)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 334);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const ModelAction nextAction = findValidAction(object, {ACTION_WA, ACTION_WB, ACTION_WC, ACTION_DA, ACTION_DB, ACTION_DC}, currentAction);
    ASSERT_NE(nextAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int sourceFrame = model->getFirstFrame(currentAction);
    const int targetFrame = model->getLastFrame(currentAction);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WB;
    object->inst._canBeInterrupted = false;
    object->inst._sourceFrameIndex = sourceFrame;
    object->inst._targetFrameIndex = targetFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;

    EXPECT_TRUE(object->inst.setAction(nextAction, true, true));

    EXPECT_EQ(object->getCurrentAnimation(), nextAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_DA);
    EXPECT_TRUE(object->canBeInterrupted());
    EXPECT_EQ(object->inst._sourceFrameIndex, sourceFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, targetFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 3);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.75f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsSetFrameMutatesBookkeepingWithoutChangingActionState)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 335);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(currentAction);
    const int lastFrame = model->getLastFrame(currentAction);
    ASSERT_NE(firstFrame, lastFrame);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WA;
    object->inst._canBeInterrupted = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = lastFrame;
    object->inst._animationProgressInteger = 2;
    object->inst._animationProgress = 0.5f;

    EXPECT_TRUE(object->inst.setFrame(firstFrame));

    EXPECT_EQ(object->getCurrentAnimation(), currentAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WA);
    EXPECT_FALSE(object->canBeInterrupted());
    EXPECT_EQ(object->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, firstFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsStartAnimationRestartsAtFirstFrameAndUsesPriorTargetAsSource)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 336);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const ModelAction nextAction = findValidAction(object, {ACTION_WA, ACTION_WB, ACTION_WC, ACTION_DA, ACTION_DB, ACTION_DC}, currentAction);
    ASSERT_NE(nextAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(currentAction);
    const int lastFrame = model->getLastFrame(currentAction);
    const int nextFirstFrame = model->getFirstFrame(nextAction);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WB;
    object->inst._canBeInterrupted = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = lastFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;

    EXPECT_TRUE(object->inst.startAnimation(nextAction, true, true));

    EXPECT_EQ(object->getCurrentAnimation(), nextAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_DA);
    EXPECT_TRUE(object->canBeInterrupted());
    EXPECT_EQ(object->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, nextFirstFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsSetFrameFullHealsCurrentActionAndPreservesSourceFrame)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 337);
    ASSERT_NE(object, nullptr);

    ModelAction invalidAction = ACTION_COUNT;
    ModelAction healedAction = ACTION_COUNT;
    ASSERT_TRUE(findHealableInvalidAction(object, invalidAction, healedAction));

    const auto& model = object->inst.getModelDescriptor();
    const int preservedSourceFrame = model->getFirstFrame(healedAction);
    const int firstFrame = model->getFirstFrame(healedAction);
    const int lastFrame = model->getLastFrame(healedAction);
    const int frameCount = 1 + (lastFrame - firstFrame);
    const int frameAlong = (frameCount > 1) ? 1 : 0;

    object->inst._currentAnimation = invalidAction;
    object->inst._sourceFrameIndex = preservedSourceFrame;
    object->inst._targetFrameIndex = firstFrame;
    object->inst._animationProgressInteger = 0;
    object->inst._animationProgress = 0.0f;

    EXPECT_TRUE(object->inst.setFrameFull(frameAlong, 2));

    EXPECT_FALSE(model->isActionValid(invalidAction));
    EXPECT_EQ(object->getCurrentAnimation(), healedAction);
    EXPECT_EQ(object->inst._sourceFrameIndex, preservedSourceFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, std::min(firstFrame + frameAlong, lastFrame));
    EXPECT_EQ(object->inst._animationProgressInteger, 2);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.5f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsRemoveInterpolationSnapsToTargetWithoutChangingActionState)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 339);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(currentAction);
    const int lastFrame = model->getLastFrame(currentAction);
    ASSERT_NE(firstFrame, lastFrame);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WA;
    object->inst._canBeInterrupted = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = lastFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;

    object->inst.removeInterpolation();

    EXPECT_EQ(object->getCurrentAnimation(), currentAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WA);
    EXPECT_FALSE(object->canBeInterrupted());
    EXPECT_EQ(object->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsRemoveInterpolationIsNoOpWhenAlreadyCollapsed)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 340);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findValidAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int frame = model->getFirstFrame(currentAction);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = ACTION_WB;
    object->inst._canBeInterrupted = true;
    object->inst._sourceFrameIndex = frame;
    object->inst._targetFrameIndex = frame;
    object->inst._animationProgressInteger = 0;
    object->inst._animationProgress = 0.0f;

    object->inst.removeInterpolation();

    EXPECT_EQ(object->getCurrentAnimation(), currentAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WB);
    EXPECT_TRUE(object->canBeInterrupted());
    EXPECT_EQ(object->inst._sourceFrameIndex, frame);
    EXPECT_EQ(object->inst._targetFrameIndex, frame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsMovementPolicyKeepsMappedWalkFrameAsInterpolationSource)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 338);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(object->inst.getModelDescriptor()->isActionValid(ACTION_WA));

    const auto& model = object->inst.getModelDescriptor();
    const ModelAction initialAction = findValidAction(object, {ACTION_DA, ACTION_DB, ACTION_DC, ACTION_WC}, ACTION_WA);
    ASSERT_NE(initialAction, ACTION_COUNT);

    const int initialFirstFrame = model->getFirstFrame(initialAction);
    const int initialTargetFrame = model->getLastFrame(initialAction);
    const int expectedSourceFrame = model->getFrameLipToWalkFrame(LIPWA, model->getMD2()->getFrames()[initialTargetFrame].framelip);
    const int expectedTargetFrame = model->getFirstFrame(ACTION_WA);

    object->_stealth = true;
    object->inst._currentAnimation = initialAction;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;
    object->inst._sourceFrameIndex = initialFirstFrame;
    object->inst._targetFrameIndex = initialTargetFrame;
    object->inst._animationProgressInteger = 2;
    object->inst._animationProgress = 0.5f;
    object->_objectPhysics._groundElevation = object->getPosZ();
    object->setVelocity(Ego::Vector3f(10.0f, 0.0f, 0.0f));
    movementControl(*object).setDesiredVelocity(Ego::Vector2f(1.0f, 0.0f));

    object->inst.updateAnimationRate();

    EXPECT_EQ(object->getCurrentAnimation(), ACTION_WA);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WA);
    EXPECT_EQ(object->inst._sourceFrameIndex, expectedSourceFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, expectedTargetFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsMountedSceneryAnimationPolicyStopsAnimationRate)
{
    auto& objectHandler = beginActiveTestModule();
    auto holder = makeFollower(objectHandler, 317);
    auto rider = makeFollower(objectHandler, 318);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(rider, nullptr);

    holder->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_NULL));
    holder->setBaseAttribute(Ego::Attribute::ACCELERATION, 0.0f);
    rider->setHolderRef(holder->getObjRef());
    rider->inst._currentAnimation = ACTION_MI;
    rider->inst._canBeInterrupted = true;
    rider->inst._freezeAtLastFrame = false;
    rider->inst._animationRate = 1.0f;

    rider->inst.updateAnimationRate();

    EXPECT_TRUE(holder->isScenery());
    EXPECT_FLOAT_EQ(rider->getAnimationSpeed(), 0.1f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsMountedAnimationPolicyCopiesHolderAnimationRate)
{
    auto& objectHandler = beginActiveTestModule();
    auto holder = makeFollower(objectHandler, 319);
    auto rider = makeFollower(objectHandler, 320);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(rider, nullptr);

    holder->setTeamRef(static_cast<TEAM_REF>(Team::TEAM_GOOD));
    holder->setBaseAttribute(Ego::Attribute::ACCELERATION, 1.0f);
    holder->setAnimationSpeed(2.5f);
    rider->setHolderRef(holder->getObjRef());
    rider->inst._currentAnimation = ACTION_MH;
    rider->inst._canBeInterrupted = true;
    rider->inst._freezeAtLastFrame = false;
    rider->inst._animationRate = 1.0f;

    rider->inst.updateAnimationRate();

    EXPECT_FALSE(holder->isScenery());
    EXPECT_FLOAT_EQ(rider->getAnimationSpeed(), holder->getAnimationSpeed());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsIdlePolicyRaisesBoredAlertAndResetsTimer)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 321);
    ASSERT_NE(object, nullptr);

    object->setAIAlertBits(0);
    object->setBoredTimer(0);
    object->_stealth = false;
    object->inst._currentAnimation = ACTION_DA;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;
    object->_objectPhysics._groundElevation = object->getPosZ();
    movementControl(*object).setDesiredVelocity(idlib::zero<Ego::Vector2f>());

    object->inst.updateAnimationRate();

    EXPECT_TRUE(object->hasAnyAIAlertBits(ALERTIF_BORED));
    EXPECT_GT(object->getBoredTimer(), 0);
    EXPECT_TRUE(ACTION_IS_TYPE(object->getCurrentAnimation(), D));
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsIdlePolicyReturnsWalkingAnimationToIdle)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeFollower(objectHandler, 322);
    ASSERT_NE(object, nullptr);

    object->setBoredTimer(12);
    object->inst._currentAnimation = ACTION_WC;
    object->inst._nextAnimation = ACTION_WC;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;
    object->_objectPhysics._groundElevation = object->getPosZ();
    movementControl(*object).setDesiredVelocity(idlib::zero<Ego::Vector2f>());

    object->inst.updateAnimationRate();

    EXPECT_EQ(object->getCurrentAnimation(), ACTION_DA);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_DA);
    EXPECT_FLOAT_EQ(object->getAnimationSpeed(), 1.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsMovementPolicySelectsStealthWalkAnimation)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 323);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(object->inst.getModelDescriptor()->isActionValid(ACTION_WA));

    object->_stealth = true;
    object->inst._currentAnimation = ACTION_DA;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;
    object->_objectPhysics._groundElevation = object->getPosZ();
    object->setVelocity(Ego::Vector3f(10.0f, 0.0f, 0.0f));
    movementControl(*object).setDesiredVelocity(Ego::Vector2f(1.0f, 0.0f));

    object->inst.updateAnimationRate();

    EXPECT_EQ(object->getCurrentAnimation(), ACTION_WA);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WA);
    EXPECT_FLOAT_EQ(object->getAnimationSpeed(), 1.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsMovementPolicyRemapsFlyingIdleToFlapAnimation)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 324);
    ASSERT_NE(object, nullptr);
    ASSERT_TRUE(object->inst.getModelDescriptor()->isActionValid(ACTION_WC));

    object->setBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT, 1.0f);
    object->setVelocity(idlib::zero<Ego::Vector3f>());
    object->inst._currentAnimation = ACTION_DA;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = true;
    object->inst._freezeAtLastFrame = false;

    object->inst.updateAnimationRate();

    EXPECT_EQ(object->getCurrentAnimation(), ACTION_WC);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_WC);
    EXPECT_FLOAT_EQ(object->getAnimationSpeed(), 1.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsAnimationEndFreezeKeepsLastFrameAndMakesActionInterruptible)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 325);
    ASSERT_NE(object, nullptr);

    const ModelAction action = findValidAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(action, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int lastFrame = model->getLastFrame(action);

    object->inst._currentAnimation = action;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = true;
    object->inst._loopAnimation = false;
    object->inst._sourceFrameIndex = model->getFirstFrame(action);
    object->inst._targetFrameIndex = lastFrame;

    object->inst.incrementFrame();

    EXPECT_EQ(object->getCurrentAnimation(), action);
    EXPECT_EQ(object->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, lastFrame);
    EXPECT_TRUE(object->canBeInterrupted());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsUpdateAnimationAdvancesQuarterStepWithoutChangingFrames)
{
    auto object = makeObject(_objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 341);
    ASSERT_NE(object, nullptr);

    const ModelAction action = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(action, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(action);
    const int secondFrame = firstFrame + 1;
    ASSERT_LE(secondFrame, model->getLastFrame(action));

    object->inst._currentAnimation = action;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = false;
    object->inst._loopAnimation = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = secondFrame;
    object->inst._animationProgressInteger = 0;
    object->inst._animationProgress = 0.0f;
    object->inst._animationRate = 1.0f;

    object->inst.updateAnimation();

    EXPECT_EQ(object->inst._sourceFrameIndex, firstFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, secondFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 1);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.25f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsUpdateAnimationAdvancesFrameWhenCrossingBoundary)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 342);
    ASSERT_NE(object, nullptr);

    const ModelAction action = findActionWithMinimumFrameCount(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC}, 3);
    ASSERT_NE(action, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(action);
    const int oldTargetFrame = firstFrame + 1;
    const int expectedNextFrame = firstFrame + 2;

    object->inst._currentAnimation = action;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = false;
    object->inst._loopAnimation = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = oldTargetFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;
    object->inst._animationRate = 1.0f;

    object->inst.updateAnimation();

    EXPECT_EQ(object->inst._sourceFrameIndex, oldTargetFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, expectedNextFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.0f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsUpdateAnimationKeepsResidualProgressAfterFrameAdvance)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 343);
    ASSERT_NE(object, nullptr);

    const ModelAction action = findActionWithMinimumFrameCount(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC}, 3);
    ASSERT_NE(action, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(action);
    const int oldTargetFrame = firstFrame + 1;
    const int expectedNextFrame = firstFrame + 2;

    object->inst._currentAnimation = action;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = false;
    object->inst._loopAnimation = false;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = oldTargetFrame;
    object->inst._animationProgressInteger = 3;
    object->inst._animationProgress = 0.75f;
    object->inst._animationRate = 1.5f;

    object->inst.updateAnimation();

    EXPECT_EQ(object->inst._sourceFrameIndex, oldTargetFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, expectedNextFrame);
    EXPECT_EQ(object->inst._animationProgressInteger, 0);
    EXPECT_FLOAT_EQ(object->inst._animationProgress, 0.125f);
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsAnimationEndLoopWrapsToFirstFrameAndMakesActionInterruptible)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 326);
    ASSERT_NE(object, nullptr);

    const ModelAction action = findLoopingAction(object, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC});
    ASSERT_NE(action, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int firstFrame = model->getFirstFrame(action);
    const int lastFrame = model->getLastFrame(action);

    object->inst._currentAnimation = action;
    object->inst._nextAnimation = ACTION_DA;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = false;
    object->inst._loopAnimation = true;
    object->inst._sourceFrameIndex = firstFrame;
    object->inst._targetFrameIndex = lastFrame;

    object->inst.incrementFrame();

    EXPECT_EQ(object->getCurrentAnimation(), action);
    EXPECT_EQ(object->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, firstFrame);
    EXPECT_TRUE(object->canBeInterrupted());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsAnimationEndMountedLoopWithHeldItemSubstitutesSitAnimation)
{
    auto& objectHandler = beginActiveTestModule();
    auto holder = makeFollower(objectHandler, 327);
    auto rider = makeFollower(objectHandler, 328);
    auto heldItem = makeFollower(objectHandler, 329);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(rider, nullptr);
    ASSERT_NE(heldItem, nullptr);

    const auto& model = rider->inst.getModelDescriptor();
    const ModelAction mountedAction = model->getAction(ACTION_MH);
    ASSERT_NE(mountedAction, ACTION_COUNT);

    const ModelAction initialAction = findValidAction(rider, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC}, mountedAction);
    ASSERT_NE(initialAction, ACTION_COUNT);
    const int lastFrame = model->getLastFrame(initialAction);
    const int firstMountFrame = model->getFirstFrame(mountedAction);

    rider->setHolderRef(holder->getObjRef());
    rider->setHeldObject(SLOT_LEFT, heldItem->getObjRef());
    rider->inst._currentAnimation = initialAction;
    rider->inst._nextAnimation = ACTION_DA;
    rider->inst._canBeInterrupted = false;
    rider->inst._freezeAtLastFrame = false;
    rider->inst._loopAnimation = true;
    rider->inst._sourceFrameIndex = model->getFirstFrame(initialAction);
    rider->inst._targetFrameIndex = lastFrame;

    rider->inst.incrementFrame();

    EXPECT_EQ(rider->getCurrentAnimation(), mountedAction);
    EXPECT_EQ(rider->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(rider->inst._targetFrameIndex, firstMountFrame);
    EXPECT_TRUE(rider->canBeInterrupted());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsAnimationEndMountedLoopWithEmptyHandsSubstitutesRideAnimation)
{
    auto& objectHandler = beginActiveTestModule();
    auto holder = makeFollower(objectHandler, 330);
    auto rider = makeFollower(objectHandler, 331);
    ASSERT_NE(holder, nullptr);
    ASSERT_NE(rider, nullptr);

    const auto& model = rider->inst.getModelDescriptor();
    const ModelAction mountedAction = model->getAction(ACTION_MI);
    ASSERT_NE(mountedAction, ACTION_COUNT);

    const ModelAction initialAction = findValidAction(rider, {ACTION_WC, ACTION_WA, ACTION_DA, ACTION_DB, ACTION_DC}, mountedAction);
    ASSERT_NE(initialAction, ACTION_COUNT);
    const int lastFrame = model->getLastFrame(initialAction);
    const int firstMountFrame = model->getFirstFrame(mountedAction);

    rider->setHolderRef(holder->getObjRef());
    rider->setHeldObject(SLOT_LEFT, ObjectRef::Invalid);
    rider->setHeldObject(SLOT_RIGHT, ObjectRef::Invalid);
    rider->inst._currentAnimation = initialAction;
    rider->inst._nextAnimation = ACTION_DA;
    rider->inst._canBeInterrupted = false;
    rider->inst._freezeAtLastFrame = false;
    rider->inst._loopAnimation = true;
    rider->inst._sourceFrameIndex = model->getFirstFrame(initialAction);
    rider->inst._targetFrameIndex = lastFrame;

    rider->inst.incrementFrame();

    EXPECT_EQ(rider->getCurrentAnimation(), mountedAction);
    EXPECT_EQ(rider->inst._sourceFrameIndex, lastFrame);
    EXPECT_EQ(rider->inst._targetFrameIndex, firstMountFrame);
    EXPECT_TRUE(rider->canBeInterrupted());
}

TEST_F(ObjectAccessorFixture, ObjectGraphicsAnimationEndTransitionsToQueuedNextAction)
{
    auto& objectHandler = beginActiveTestModule();
    auto object = makeObject(objectHandler, "mp_data/globalobjects/monsters/zombi.obj", 332);
    ASSERT_NE(object, nullptr);

    const ModelAction currentAction = findLoopingAction(object, {ACTION_DA, ACTION_DB, ACTION_DC, ACTION_WC, ACTION_WA});
    ASSERT_NE(currentAction, ACTION_COUNT);

    const ModelAction nextAction = findValidAction(object, {ACTION_WC, ACTION_WA, ACTION_WB, ACTION_DA, ACTION_DB, ACTION_DC}, currentAction);
    ASSERT_NE(nextAction, ACTION_COUNT);

    const auto& model = object->inst.getModelDescriptor();
    const int currentLastFrame = model->getLastFrame(currentAction);
    const int nextFirstFrame = model->getFirstFrame(nextAction);

    object->inst._currentAnimation = currentAction;
    object->inst._nextAnimation = nextAction;
    object->inst._canBeInterrupted = false;
    object->inst._freezeAtLastFrame = false;
    object->inst._loopAnimation = false;
    object->inst._sourceFrameIndex = model->getFirstFrame(currentAction);
    object->inst._targetFrameIndex = currentLastFrame;

    object->inst.incrementFrame();

    EXPECT_EQ(object->getCurrentAnimation(), nextAction);
    EXPECT_EQ(object->inst._nextAnimation, ACTION_DA);
    EXPECT_EQ(object->inst._sourceFrameIndex, currentLastFrame);
    EXPECT_EQ(object->inst._targetFrameIndex, nextFirstFrame);
    EXPECT_TRUE(object->canBeInterrupted());
}

TEST_F(ObjectAccessorFixture, StatsAmmoGenderAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(333);
    ASSERT_NE(object, nullptr);

    object->setGender(Gender::Neuter);
    object->setExperience(345u);
    object->setExperienceLevelIndex(4);
    object->setAmmoMax(27);
    object->setAmmo(19);

    EXPECT_EQ(object->getGender(), Gender::Neuter);
    EXPECT_EQ(object->getExperience(), 345u);
    EXPECT_EQ(object->getExperienceLevelIndex(), 4);
    EXPECT_EQ(object->getExperienceLevel(), 5);
    EXPECT_EQ(object->getAmmoMax(), 27);
    EXPECT_EQ(object->getAmmo(), 19);
}

} // namespace
