#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Logic/QuestLog.hpp"
#include "egolib/game/Module/Module_player_startup.hpp"
#include "egolib/IDSZ.hpp"
#include "egolib/game/game.h"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

constexpr char kQuestTestRoot[] = "quest-tests";

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
        vfs_removeDirectoryAndContents(kQuestTestRoot);
        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
    }

    void TearDown() override
    {
        game_reset_players();
        vfs_removeDirectoryAndContents(kQuestTestRoot);
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

    ObjectProfileRef makeIsolatedFollowerProfile(const std::string& profilePath, int slot) const
    {
        setup_init_module_vfs_paths("mp_modules/test.mod");

        vfs_removeDirectoryAndContents(profilePath.c_str());
        EXPECT_TRUE(vfs_copyDirectory("mp_objects/follower.obj", profilePath.c_str()));
        EXPECT_TRUE(vfs_exists((profilePath + "/data.txt").c_str()));
        EXPECT_TRUE(vfs_exists((profilePath + "/naming.txt").c_str()));
        EXPECT_TRUE(vfs_exists((profilePath + "/tris.md2").c_str()));

        return ProfileSystem::get().loadOneProfile(profilePath, slot);
    }

    void writeQuestFile(const std::string& profilePath, const IDSZ2& idsz, int progress) const
    {
        vfs_FILE* questFile = vfs_openWrite(profilePath + "/quest.txt");
        ASSERT_NE(questFile, nullptr);
        vfs_printf(questFile, ":[%4s] %d\n", idsz.toString().c_str(), progress);
        vfs_close(questFile);
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

TEST_F(ModulePlayerStartupFixture, AddPlayerHydratesQuestLogFromProfilePath)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    const std::string profilePath = std::string(kQuestTestRoot) + "/module-player-startup-present.obj";

    const ObjectProfileRef profile = makeIsolatedFollowerProfile(profilePath, 122);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    writeQuestFile(profilePath, IDSZ2('T', 'E', 'S', 'T'), 4);

    auto object = objectHandler.insert(profile);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[1], false));
    ASSERT_EQ(playerList.size(), 1u);
    ASSERT_NE(playerList.front(), nullptr);
    EXPECT_EQ(playerList.front()->getQuestLog()[IDSZ2('T', 'E', 'S', 'T')], 4);
}

} // namespace
