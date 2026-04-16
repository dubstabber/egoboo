#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Logic/QuestLog.hpp"
#include "egolib/game/Module/Module_player_startup.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

class ModulePlayerStartupFixture : public ::testing::Test
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
        opts.randomSeed = 11;
        opts.binaryPath = "";
        opts.logPath = "/debug/module-player-startup-tests.log";
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
        game_reset_players();
        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
    }

    void TearDown() override
    {
        game_reset_players();
        setup_clear_module_vfs_paths();
    }

    std::shared_ptr<Object> makeFollower(ObjectHandler& objectHandler, int slot) const
    {
        setup_init_module_vfs_paths("mp_modules/test.mod");
        const ObjectProfileRef profile = ProfileSystem::get().loadOneProfile("mp_objects/follower.obj", slot);
        EXPECT_NE(profile, ObjectProfileRef::Invalid);
        if (profile == ObjectProfileRef::Invalid)
        {
            return nullptr;
        }

        return objectHandler.insert(profile);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> ModulePlayerStartupFixture::s_runtime;

TEST_F(ModulePlayerStartupFixture, AddPlayerRejectsNullObjectsWithoutChangingModuleState)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    const std::shared_ptr<Object> object;

    EXPECT_FALSE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[0], false));
    EXPECT_TRUE(playerList.empty());
    EXPECT_EQ(local_stats.player_count, 0);
    EXPECT_TRUE(local_stats.noplayers);
}

TEST_F(ModulePlayerStartupFixture, AddPlayerRegistersLocalPlayerAndKeepsMissingQuestLoadSilent)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;

    auto object = makeFollower(objectHandler, 121);
    ASSERT_NE(object, nullptr);
    object->nameknown = false;

    ASSERT_NE(object->getProfile(), nullptr);
    EXPECT_FALSE(vfs_exists((object->getProfile()->getPathname() + "/quest.txt").c_str()));

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[1], false));
    ASSERT_EQ(playerList.size(), 1u);

    const auto& player = playerList.front();
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->getObject(), object);
    EXPECT_EQ(object->is_which_player, 0);
    EXPECT_TRUE(object->isPlayer());
    EXPECT_FALSE(object->nameknown);
    EXPECT_EQ(local_stats.player_count, 1);
    EXPECT_FALSE(local_stats.noplayers);
    EXPECT_EQ(player->getQuestLog()[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

} // namespace
