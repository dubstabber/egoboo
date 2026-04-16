#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/Audio/AudioSystem.hpp"
#define private public
#include "egolib/Entities/_Include.hpp"
#undef private
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
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
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }
        game_reset_players();
        vfs_removeDirectoryAndContents(kQuestTestRoot);
        ProfileSystem::get().reset();
        ProfileSystem::get().loadModuleProfiles();
    }

    void TearDown() override
    {
        auto& session = GameSessionContext::get();
        if (session.hasActiveModule())
        {
            session.quitModule();
        }
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
    auto& session = GameSessionContext::get();
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    const std::shared_ptr<Object> object;

    EXPECT_FALSE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[0], false));
    EXPECT_TRUE(playerList.empty());
    EXPECT_EQ(session.localPlayerCount(), 0u);
    EXPECT_FALSE(session.hasLocalPlayers());
    EXPECT_FALSE(session.allLocalPlayersDead());
}

TEST_F(ModulePlayerStartupFixture, AddPlayerRegistersLocalPlayerAndKeepsMissingQuestLoadSilent)
{
    auto& session = GameSessionContext::get();
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
    EXPECT_EQ(session.localPlayerCount(), 1u);
    EXPECT_TRUE(session.hasLocalPlayers());
    EXPECT_FALSE(session.allLocalPlayersDead());
    EXPECT_EQ(player->getQuestLog()[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

TEST_F(ModulePlayerStartupFixture, AddPlayerPreservesRegistrationOrderInPlayerIndices)
{
    auto& session = GameSessionContext::get();
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;

    auto firstObject = makeFollower(objectHandler, 122);
    auto secondObject = makeFollower(objectHandler, 123);
    ASSERT_NE(firstObject, nullptr);
    ASSERT_NE(secondObject, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, firstObject, Ego::Input::InputDevice::DeviceList[0], false));
    ASSERT_TRUE(module_player_startup::addPlayer(playerList, secondObject, Ego::Input::InputDevice::DeviceList[1], false));
    ASSERT_EQ(playerList.size(), 2u);

    EXPECT_EQ(firstObject->is_which_player, 0);
    EXPECT_EQ(secondObject->is_which_player, 1);
    EXPECT_EQ(playerList[0]->getObject(), firstObject);
    EXPECT_EQ(playerList[1]->getObject(), secondObject);
    EXPECT_EQ(session.localPlayerCount(), 2u);
    EXPECT_TRUE(session.hasLocalPlayers());
}

TEST_F(ModulePlayerStartupFixture, AddPlayerCanIdentifySpawnOnSuccessfulBinding)
{
    auto& session = GameSessionContext::get();
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;

    auto object = makeFollower(objectHandler, 124);
    ASSERT_NE(object, nullptr);
    object->nameknown = false;

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[1], true));
    ASSERT_EQ(playerList.size(), 1u);

    EXPECT_TRUE(object->isPlayer());
    EXPECT_TRUE(object->nameknown);
    EXPECT_EQ(session.localPlayerCount(), 1u);
    EXPECT_TRUE(session.hasLocalPlayers());
}

TEST_F(ModulePlayerStartupFixture, AddPlayerHydratesQuestLogFromProfilePath)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    const std::string profilePath = std::string(kQuestTestRoot) + "/module-player-startup-present.obj";

    const ObjectProfileRef profile = makeIsolatedFollowerProfile(profilePath, 125);
    ASSERT_NE(profile, ObjectProfileRef::Invalid);

    writeQuestFile(profilePath, IDSZ2('T', 'E', 'S', 'T'), 4);

    auto object = objectHandler.insert(profile);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[1], false));
    ASSERT_EQ(playerList.size(), 1u);
    ASSERT_NE(playerList.front(), nullptr);
    EXPECT_EQ(playerList.front()->getQuestLog()[IDSZ2('T', 'E', 'S', 'T')], 4);
}

TEST_F(ModulePlayerStartupFixture, LegacyLocalPlayerCountMirrorTracksPreModuleFallback)
{
    auto& session = GameSessionContext::get();
    ASSERT_FALSE(session.hasActiveModule());

    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    auto object = makeFollower(objectHandler, 126);
    ASSERT_NE(object, nullptr);

    EXPECT_EQ(session.localPlayerCount(), 0u);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[0], false));
    ASSERT_EQ(playerList.size(), 1u);

    EXPECT_EQ(session.localPlayerCount(), static_cast<size_t>(local_stats.player_count));
    EXPECT_EQ(session.localPlayerCount(), 1u);
    EXPECT_TRUE(session.hasLocalPlayers());
    EXPECT_FALSE(session.allLocalPlayersDead());
    EXPECT_FALSE(local_stats.noplayers);
}

TEST_F(ModulePlayerStartupFixture, LegacyLocalPlayerMirrorsResetWithSessionState)
{
    auto& session = GameSessionContext::get();

    session.publishLocalPlayerCount(2);
    session.publishLocalPlayerStatus(LocalPlayerStatus{2, 0, 2});
    ASSERT_EQ(session.localPlayerCount(), 2u);
    ASSERT_TRUE(session.allLocalPlayersDead());

    game_reset_players();

    EXPECT_EQ(session.localPlayerCount(), 0u);
    EXPECT_FALSE(session.hasLocalPlayers());
    EXPECT_FALSE(session.allLocalPlayersDead());
    EXPECT_EQ(local_stats.player_count, 0);
    EXPECT_TRUE(local_stats.noplayers);
    EXPECT_FALSE(local_stats.allpladead);
}

TEST_F(ModulePlayerStartupFixture, LegacyAllPlayersDeadMirrorTracksPublishedSessionStatus)
{
    auto& session = GameSessionContext::get();

    session.publishLocalPlayerCount(2);
    session.publishLocalPlayerStatus(LocalPlayerStatus{2, 1, 1});

    EXPECT_EQ(session.localPlayerStatus().registeredCount, 2u);
    EXPECT_EQ(session.localPlayerStatus().aliveCount, 1u);
    EXPECT_EQ(session.localPlayerStatus().deadCount, 1u);
    EXPECT_TRUE(session.hasLocalPlayers());
    EXPECT_FALSE(session.allLocalPlayersDead());
    EXPECT_FALSE(local_stats.allpladead);

    session.publishLocalPlayerStatus(LocalPlayerStatus{2, 0, 2});

    EXPECT_TRUE(session.allLocalPlayersDead());
    EXPECT_TRUE(local_stats.allpladead);
}

TEST_F(ModulePlayerStartupFixture, LocalPlayerStatusTreatsEmptyRegistrationAsAllPlayersDead)
{
    const std::vector<std::shared_ptr<Ego::Player>> playerList;

    const LocalPlayerStatus status = collectLocalPlayerStatus(playerList);

    EXPECT_EQ(status.registeredCount, 0u);
    EXPECT_EQ(status.aliveCount, 0u);
    EXPECT_EQ(status.deadCount, 0u);
    EXPECT_TRUE(status.allPlayersDead());
}

TEST_F(ModulePlayerStartupFixture, LocalPlayerStatusCountsAliveRegisteredPlayers)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    auto object = makeFollower(objectHandler, 127);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[0], false));

    const LocalPlayerStatus status = collectLocalPlayerStatus(playerList);

    EXPECT_EQ(status.registeredCount, 1u);
    EXPECT_EQ(status.aliveCount, 1u);
    EXPECT_EQ(status.deadCount, 0u);
    EXPECT_FALSE(status.allPlayersDead());
}

TEST_F(ModulePlayerStartupFixture, LocalPlayerStatusCountsDeadRegisteredPlayers)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    auto object = makeFollower(objectHandler, 128);
    ASSERT_NE(object, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, object, Ego::Input::InputDevice::DeviceList[0], false));
    object->_isAlive = false;

    const LocalPlayerStatus status = collectLocalPlayerStatus(playerList);

    EXPECT_EQ(status.registeredCount, 1u);
    EXPECT_EQ(status.aliveCount, 0u);
    EXPECT_EQ(status.deadCount, 1u);
    EXPECT_TRUE(status.allPlayersDead());
}

TEST_F(ModulePlayerStartupFixture, LocalPlayerStatusDistinguishesMixedAliveAndDeadPlayers)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    auto firstObject = makeFollower(objectHandler, 129);
    auto secondObject = makeFollower(objectHandler, 130);
    ASSERT_NE(firstObject, nullptr);
    ASSERT_NE(secondObject, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, firstObject, Ego::Input::InputDevice::DeviceList[0], false));
    ASSERT_TRUE(module_player_startup::addPlayer(playerList, secondObject, Ego::Input::InputDevice::DeviceList[1], false));
    firstObject->_isAlive = false;

    const LocalPlayerStatus status = collectLocalPlayerStatus(playerList);

    EXPECT_EQ(status.registeredCount, 2u);
    EXPECT_EQ(status.aliveCount, 1u);
    EXPECT_EQ(status.deadCount, 1u);
    EXPECT_FALSE(status.allPlayersDead());
}

TEST_F(ModulePlayerStartupFixture, LocalPlayerStatusSkipsNullAndTerminatedObjectsButKeepsRegistrationCount)
{
    std::vector<std::shared_ptr<Ego::Player>> playerList;
    ObjectHandler objectHandler;
    auto aliveObject = makeFollower(objectHandler, 131);
    auto terminatedObject = makeFollower(objectHandler, 132);
    ASSERT_NE(aliveObject, nullptr);
    ASSERT_NE(terminatedObject, nullptr);

    ASSERT_TRUE(module_player_startup::addPlayer(playerList, aliveObject, Ego::Input::InputDevice::DeviceList[0], false));
    ASSERT_TRUE(module_player_startup::addPlayer(playerList, terminatedObject, Ego::Input::InputDevice::DeviceList[1], false));
    terminatedObject->_terminateRequested = true;
    playerList.push_back(std::make_shared<Ego::Player>(std::shared_ptr<Object>(), Ego::Input::InputDevice::DeviceList[2]));

    const LocalPlayerStatus status = collectLocalPlayerStatus(playerList);

    EXPECT_EQ(status.registeredCount, 3u);
    EXPECT_EQ(status.aliveCount, 1u);
    EXPECT_EQ(status.deadCount, 0u);
    EXPECT_FALSE(status.allPlayersDead());
}

} // namespace
