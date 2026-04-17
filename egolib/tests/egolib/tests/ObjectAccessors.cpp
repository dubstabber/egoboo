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

} // namespace
