#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

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
    }

    static void TearDownTestSuite()
    {
        AudioSystem::uninitialize();
        s_runtime.reset();
    }

    void SetUp() override
    {
        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
        setup_init_module_vfs_paths("mp_modules/test.mod");
    }

    void TearDown() override
    {
        setup_clear_module_vfs_paths();
    }

    std::shared_ptr<Object> makeFollower(int slot)
    {
        const ObjectProfileRef profile = ProfileSystem::get().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return _objectHandler.insert(profile);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ObjectAccessorFixture::s_runtime;

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
    EXPECT_EQ(object->getAttachmentSlot(), SLOT_LEFT);
    EXPECT_EQ(object->getInventoryHolderRef(), ObjectRef::Invalid);
    EXPECT_FALSE(object->isPlatform());
    EXPECT_FALSE(object->canUsePlatforms());
    EXPECT_EQ(object->getHoldingWeight(), 0);

    object->setHolderRef(ObjectRef(41));
    object->setAttachmentSlot(SLOT_RIGHT);
    object->setInventoryHolderRef(ObjectRef(42));
    object->setPlatform(true);
    object->setCanUsePlatforms(true);
    object->setHoldingWeight(7);
    object->adjustHoldingWeight(5);

    EXPECT_EQ(object->getHolderRef(), ObjectRef(41));
    EXPECT_EQ(object->getAttachmentSlot(), SLOT_RIGHT);
    EXPECT_EQ(object->getInventoryHolderRef(), ObjectRef(42));
    EXPECT_TRUE(object->isPlatform());
    EXPECT_TRUE(object->canUsePlatforms());
    EXPECT_EQ(object->getHoldingWeight(), 12);
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

TEST_F(ObjectAccessorFixture, AppearanceAndProfileAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(307);
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

TEST_F(ObjectAccessorFixture, StatsAmmoGenderAccessorsRoundTripSelectedState)
{
    auto object = makeFollower(308);
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
