#include "egolib/FileFormats/SpawnFile/SpawnName.hpp"

#include "gtest/gtest.h"

TEST(SpawnName, NormalizeTrimsAddsSuffixAndLowercases)
{
    EXPECT_EQ("shutter.obj", Ego::SpawnFile::normalizeSpawnLoadName("  ShUtTeR  "));
}

TEST(SpawnName, NormalizePreservesExistingSuffix)
{
    EXPECT_EQ("g'nome.obj", Ego::SpawnFile::normalizeSpawnLoadName("g'nome.obj"));
}

TEST(SpawnName, ResolveUsesTreasureResolverBeforeNormalization)
{
    const std::string resolved = Ego::SpawnFile::resolveSpawnLoadName(
        "%BossLoot",
        [](const std::string& tableName)
        {
            EXPECT_EQ("%BossLoot", tableName);
            return "Dark Glower";
        });

    EXPECT_EQ("dark glower.obj", resolved);
}

TEST(SpawnName, ReconciliationKeyDropsFormattingCharacters)
{
    EXPECT_EQ("gnomewarriorname",
              Ego::SpawnFile::buildReconciliationKey("G'nome- warrior_name.obj"));
}
