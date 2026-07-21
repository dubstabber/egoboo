#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Logic/Team.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

namespace
{

class GameplayAlertPublicationFixture : public ::testing::Test
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
        opts.randomSeed = 43;
        opts.binaryPath = "";
        opts.logPath = "/debug/gameplay-alert-publication-tests.log";
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

        const ObjectRef objectRef = module.spawnObjectRef(position, profile, static_cast<TEAM_REF>(Team::TEAM_NULL), 0, Facing(0), "", ObjectRef::Invalid);
        return module.getObjectHandler().getHandle(objectRef);
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
        const bool began = session.beginModule(module, 43);
        EXPECT_TRUE(began);
        return session.activeModule();
    }

    void flushObjectHandler(GameModule& module) const
    {
        auto refs = module.getObjectHandler().objectRefIterator();
        (void)refs;
    }

    std::shared_ptr<Object> makeAliveItem(GameModule& module, int slotBase) const
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

        ADD_FAILURE() << "unable to load an alive item fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeStackableWeapon(GameModule& module, int slotBase) const
    {
        static const std::vector<std::string> candidates = {
            "mp_data/globalobjects/weapons/knife.obj",
            "mp_data/globalobjects/weapons/cknife.obj",
            "mp_data/globalobjects/items/gem.obj"
        };

        for (size_t i = 0; i < candidates.size(); ++i)
        {
            auto item = makeObject(module, candidates[i], slotBase + static_cast<int>(i));
            if (item && item->isItem() && item->getProfile()->isStackable())
            {
                return item;
            }
        }

        ADD_FAILURE() << "unable to load a stackable weapon fixture";
        return nullptr;
    }

    std::shared_ptr<Object> makeLongbowWeapon(GameModule& module, int slot) const
    {
        auto weapon = makeObject(module, "mp_data/globalobjects/weapons/lbow.obj", slot);
        if (!weapon)
        {
            ADD_FAILURE() << "unable to load a longbow fixture";
        }
        return weapon;
    }

    std::shared_ptr<Object> makeCrushableOccupant(GameModule& module, int slotBase) const
    {
        auto occupant = makeObject(module, "mp_objects/follower.obj", slotBase);
        if (!occupant)
        {
            ADD_FAILURE() << "unable to load follower fixture for passage test";
            return nullptr;
        }

        occupant->setCanBeCrushed(true);
        if (occupant->getProfile()->canOpenStuff())
        {
            occupant->kill(ObjectAttribution(), true);
        }

        return occupant;
    }
};

std::unique_ptr<ContentRuntimeBootstrap> GameplayAlertPublicationFixture::s_runtime;

TEST_F(GameplayAlertPublicationFixture, UnarmedLatchAttackPublishesUsedAlertAndLastItemUsed)
{
    auto& module = beginActiveTestModule();
    auto actor = makeObject(module, "mp_objects/follower.obj", 5401);

    ASSERT_NE(actor, nullptr);
    ASSERT_TRUE(actor->isAlive());

    EXPECT_TRUE(chr_do_latch_attack(actor.get(), SLOT_LEFT));
    EXPECT_EQ(actor->getAILastItemUsed(), actor->getObjRef());
    EXPECT_TRUE(actor->hasAnyAIAlertBits(ALERTIF_USED));
}

TEST_F(GameplayAlertPublicationFixture, CallForHelpPublishesOnlyToFriendlyListeners)
{
    auto& module = beginActiveTestModule();
    auto caller = makeObject(module, "mp_objects/follower.obj", 5411);
    auto friendlyListener = makeObject(module, "mp_objects/follower.obj", 5412);
    auto enemyListener = makeObject(module, "mp_objects/follower.obj", 5413);

    ASSERT_NE(caller, nullptr);
    ASSERT_NE(friendlyListener, nullptr);
    ASSERT_NE(enemyListener, nullptr);

    caller->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD), true);
    friendlyListener->setTeam(static_cast<TEAM_REF>(Team::TEAM_GOOD), true);
    enemyListener->setTeam(static_cast<TEAM_REF>(Team::TEAM_EVIL), true);
    module.getTeamList()[Team::TEAM_GOOD].clearLeader();
    flushObjectHandler(module);

    caller->callTeamForHelp();

    EXPECT_EQ(module.getTeamList()[Team::TEAM_GOOD].getCallerForHelpRef(), caller->getObjRef());
    EXPECT_TRUE(friendlyListener->hasAnyAIAlertBits(ALERTIF_CALLEDFORHELP));
    EXPECT_FALSE(enemyListener->hasAnyAIAlertBits(ALERTIF_CALLEDFORHELP));
}

TEST_F(GameplayAlertPublicationFixture, ClosingPassagePublishesCrushedAlert)
{
    auto& module = beginActiveTestModule();
    auto occupant = makeCrushableOccupant(module, 5421);

    ASSERT_NE(occupant, nullptr);
    occupant->setPosition(64.0f, 64.0f, 0.0f);
    flushObjectHandler(module);

    Passage passage(module, 0, 0, 1, 1, MAPFX_IMPASS);
    EXPECT_TRUE(passage.close());
    EXPECT_TRUE(occupant->hasAnyAIAlertBits(ALERTIF_CRUSHED));
}

TEST_F(GameplayAlertPublicationFixture, AttachToObjectPublishesGrabbedAlertForItem)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 5431);
    auto item = makeAliveItem(module, 5432);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(item, nullptr);

    EXPECT_TRUE(item->attachToObject(holder->getObjRef(), GRIP_LEFT));
    EXPECT_TRUE(item->hasAnyAIAlertBits(ALERTIF_GRABBED));
}

TEST_F(GameplayAlertPublicationFixture, CharacterSwipePublishesThrownAlertOnSpawnedStackedWeapon)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 5436);
    auto thrownWeapon = makeStackableWeapon(module, 5437);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(thrownWeapon, nullptr);
    ASSERT_TRUE(thrownWeapon->attachToObject(holder->getObjRef(), GRIP_LEFT));

    thrownWeapon->setAmmo(2);
    flushObjectHandler(module);

    character_swipe(holder->getObjRef(), SLOT_LEFT);
    flushObjectHandler(module);

    bool foundThrownCopy = false;
    ObjectHandler& objectHandler = module.getObjectHandler();
    for (const ObjectRef& objectRef : objectHandler.objectRefIterator())
    {
        Object* object = objectHandler.get(objectRef);
        if (object == nullptr || objectRef == holder->getObjRef() || objectRef == thrownWeapon->getObjRef())
        {
            continue;
        }

        if (object->getProfileID() == thrownWeapon->getProfileID() &&
            object->hasAnyAIAlertBits(ALERTIF_THROWN) &&
            object->getAmmo() == 1)
        {
            foundThrownCopy = true;
            break;
        }
    }

    EXPECT_TRUE(foundThrownCopy);
}

TEST_F(GameplayAlertPublicationFixture, LongbowLatchAttackPreservesKursedHeldItemGate)
{
    auto& module = beginActiveTestModule();
    auto actorAllowed = makeObject(module, "mp_data/globalobjects/players/archadventurer.obj", 5439);
    auto actorBlocked = makeObject(module, "mp_data/globalobjects/players/archadventurer.obj", 5440);
    auto allowedWeapon = makeLongbowWeapon(module, 5441);
    auto blockedWeapon = makeLongbowWeapon(module, 5442);

    ASSERT_NE(actorAllowed, nullptr);
    ASSERT_NE(actorBlocked, nullptr);
    ASSERT_NE(allowedWeapon, nullptr);
    ASSERT_NE(blockedWeapon, nullptr);
    ASSERT_TRUE(allowedWeapon->attachToObject(actorAllowed->getObjRef(), GRIP_LEFT));
    ASSERT_TRUE(blockedWeapon->attachToObject(actorBlocked->getObjRef(), GRIP_LEFT));

    EXPECT_TRUE(chr_do_latch_attack(actorAllowed.get(), SLOT_LEFT));

    blockedWeapon->setKursed(true);
    EXPECT_FALSE(chr_do_latch_attack(actorBlocked.get(), SLOT_LEFT));
}

TEST_F(GameplayAlertPublicationFixture, KursedPutawayPublishesNotPutAwayAlert)
{
    auto& module = beginActiveTestModule();
    auto holder = makeObject(module, "mp_objects/follower.obj", 5441);
    auto item = makeAliveItem(module, 5442);

    ASSERT_NE(holder, nullptr);
    ASSERT_NE(item, nullptr);
    ASSERT_TRUE(item->attachToObject(holder->getObjRef(), GRIP_LEFT));

    item->setKursed(true);

    EXPECT_FALSE(Inventory::add_item(*holder, item->getObjRef(), 0, false));
    EXPECT_TRUE(item->hasAnyAIAlertBits(ALERTIF_NOTPUTAWAY));
}

} // namespace
