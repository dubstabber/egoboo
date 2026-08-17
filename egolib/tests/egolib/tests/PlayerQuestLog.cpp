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

    // Writes arbitrary (possibly malformed) bytes as quest.txt, for exercising the
    // ReadContext/IDSZ parse-failure paths rather than the happy path above.
    void writeRawQuestFile(const std::string& profilePath, const std::string& rawContent) const
    {
        ASSERT_TRUE(vfs_mkdir(profilePath));

        vfs_FILE* questFile = vfs_openWrite(profilePath + "/quest.txt");
        ASSERT_NE(questFile, nullptr);
        vfs_printf(questFile, "%s", rawContent.c_str());
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

// Verified against ReadContext.cpp/ReadContext_literals.cpp: a colon with nothing after it
// makes skipToColon(true) succeed (it returns as soon as it consumes the ':') and then
// readIDSZ() hits END_OF_INPUT before finding '[', throwing idlib::hll::compilation_error
// ("premature end of input while scanning IDSZ", ReadContext.cpp:96-99).
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogReturnsFalseForTruncatedAfterColon)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/truncated-after-colon";
    Ego::QuestLog questLog;
    questLog.setQuestProgress(IDSZ2('T', 'E', 'S', 'T'), 7);

    writeRawQuestFile(profilePath, ":");

    EXPECT_FALSE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

// Verified: the byte right after the colon is not '[', so readIDSZ() falls into its
// "unexpected character while scanning IDSZ" arm (ReadContext.cpp:101-105) rather than the
// end-of-input arm above.
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogReturnsFalseForGarbageWhereIdszExpected)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/garbage-idsz";
    Ego::QuestLog questLog;
    questLog.setQuestProgress(IDSZ2('T', 'E', 'S', 'T'), 7);

    writeRawQuestFile(profilePath, ":GARBAGE 1\n");

    EXPECT_FALSE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

// Verified: readIDSZ() parses "[TEST]" successfully, then readIntegerLiteral() sees 'B' (not
// a digit, '+' or '-') and throws idlib::hll::compilation_error from
// ReadContext::parseIntegerLiteral's "unexpected character while scanning integer literal"
// arm (ReadContext_literals.cpp:162-186).
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogReturnsFalseForBadLevelLiteral)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/bad-level-literal";
    Ego::QuestLog questLog;
    questLog.setQuestProgress(IDSZ2('T', 'E', 'S', 'T'), 7);

    writeRawQuestFile(profilePath, ":[TEST] BAD\n");

    EXPECT_FALSE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

// A quest.txt that entirely parses (one good entry) followed by a truncated second entry
// must not leave the first entry behind: loadFromFile's miss contract is "all or nothing",
// not "everything parsed before the failure". This is the case that would NOT be caught by
// QuestLog::loadFromFile's pre-existing unconditional _questLog.clear() at the top of the
// function alone, because the first entry is added to _questLog *after* that clear and
// *before* the second entry's parse failure.
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogClearsPartiallyParsedEntriesOnMalformedFile)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/partial-then-malformed";
    Ego::QuestLog questLog;

    writeRawQuestFile(profilePath, ":[TEST] 5\n:");

    EXPECT_FALSE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

// Verified: an empty quest.txt is not actually a failure case at all. Scanner's constructor
// (Script/Scanner.hpp) succeeds reading 0 bytes, and skipToColon(true) with an immediately-
// empty buffer sees END_OF_INPUT and returns false without throwing (ReadContext.cpp:165-170,
// optional branch), so the parse loop in loadFromFile never executes and the function returns
// true with an empty (already-cleared) log. Documented here as a control so this behavior
// isn't confused with the malformed-content cases above.
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogTreatsEmptyFileAsEmptyLog)
{
    const std::string profilePath = std::string(kQuestTestRoot) + "/empty";
    Ego::QuestLog questLog;
    questLog.setQuestProgress(IDSZ2('T', 'E', 'S', 'T'), 7);

    writeRawQuestFile(profilePath, "");

    EXPECT_TRUE(Ego::loadPlayerQuestLog(questLog, profilePath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], Ego::QuestLog::QUEST_NONE);
}

// A failed load must not leave the QuestLog (or anything ReadContext-adjacent) in a state
// that breaks a later, unrelated, well-formed load.
TEST_F(PlayerQuestLogFixture, LoadPlayerQuestLogRecoversAfterMalformedLoadWithSubsequentGoodLoad)
{
    const std::string malformedPath = std::string(kQuestTestRoot) + "/recover-malformed";
    const std::string goodPath = std::string(kQuestTestRoot) + "/recover-good";
    Ego::QuestLog questLog;

    writeRawQuestFile(malformedPath, ":");
    ASSERT_FALSE(Ego::loadPlayerQuestLog(questLog, malformedPath));

    writeQuestFile(goodPath, "TEST", 3);
    ASSERT_TRUE(Ego::loadPlayerQuestLog(questLog, goodPath));
    EXPECT_EQ(questLog[IDSZ2('T', 'E', 'S', 'T')], 3);
}

} // namespace
