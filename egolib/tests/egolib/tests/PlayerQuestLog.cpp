#include "gtest/gtest.h"

#include "TestEnvironment.hpp"
#include "egolib/game/Core/ContentRuntimeBootstrap.hpp"
#include "egolib/game/Logic/PlayerQuestLog.hpp"
#include "egolib/game/Logic/QuestLog.hpp"
#include "egolib/vfs.h"

#include <cstdlib>
#include <memory>

namespace
{

constexpr char kQuestTestRoot[] = "quest-tests";

class PlayerQuestLogFixture : public ::testing::Test
{
protected:
    static std::unique_ptr<ContentRuntimeBootstrap> s_runtime;

    static void SetUpTestSuite()
    {
        Ego::Test::configureDataDirectory();

        ContentRuntimeBootstrap::Options opts;
        opts.initializeVirtualFileSystem = true;
        opts.initializeBaseVfsPaths = true;
        opts.initializeLogging = false;
        opts.initializePerkHandler = false;
        opts.initializeProfileSystem = false;
        opts.clearModuleVfsPathsOnShutdown = true;
        opts.clearBaseVfsPathsOnShutdown = true;
        opts.binaryPath = "";

        s_runtime = std::make_unique<ContentRuntimeBootstrap>(opts);
    }

    static void TearDownTestSuite()
    {
        s_runtime.reset();
    }

    void SetUp() override
    {
        vfs_removeDirectoryAndContents(kQuestTestRoot);
    }

    void TearDown() override
    {
        vfs_removeDirectoryAndContents(kQuestTestRoot);
    }

    void writeQuestFile(const std::string& profilePath, const std::string& idsz, int progress) const
    {
        ASSERT_TRUE(vfs_mkdir(profilePath));

        vfs_FILE* questFile = vfs_openWrite(profilePath + "/quest.txt");
        ASSERT_NE(questFile, nullptr);
        vfs_printf(questFile, ":[%4s] %d\n", idsz.c_str(), progress);
        vfs_close(questFile);
    }
};

std::unique_ptr<ContentRuntimeBootstrap> PlayerQuestLogFixture::s_runtime;

TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogReturnsFalseForMissingQuestFileAndClearsState)
{
    Ego::QuestLog questLog;
    questLog.setQuestProgress(IDSZ2('T', 'E', 'S', 'T'), 7);

    ASSERT_TRUE(vfs_mkdir(std::string(kQuestTestRoot) + "/missing"));

    EXPECT_FALSE(Ego::loadPlayerQuestLog(questLog, std::string(kQuestTestRoot) + "/missing"));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogLoadsQuestProgressFromQuestFile)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/present";
    Ego::QuestLog questLog;

    writeQuestFile(profilePath, "TEST", 5);

    ASSERT_TRUE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], 5);
}

} // namespace
