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

TEST(SpawnReconciliation, PlaceholderDetectionFlagsKnownSentinels)
{
    EXPECT_TRUE(Ego::SpawnFile::isPlaceholderLikeSpawnName("unknown"));
    EXPECT_TRUE(Ego::SpawnFile::isPlaceholderLikeSpawnName(".obj"));
    EXPECT_TRUE(Ego::SpawnFile::isPlaceholderLikeSpawnName("Object"));
    EXPECT_FALSE(Ego::SpawnFile::isPlaceholderLikeSpawnName("shutter"));
}

TEST(SpawnReconciliation, ExactCanonicalMatchOutranksPrefixMatches)
{
    const std::vector<Ego::SpawnFile::ReconciliationCandidate> candidates = {
        {"blacktower.obj", "mp_objects/blacktower.obj", "module_data", "/module/blacktower.obj"},
        {"tower.obj", "mp_objects/tower.obj", "global_misc", "/global/tower.obj"},
    };

    const auto suggestions = Ego::SpawnFile::suggestSpawnReconciliationMatches("tower", candidates);
    ASSERT_GE(suggestions.size(), 2u);
    EXPECT_EQ("tower.obj", suggestions[0].objectName);
    EXPECT_EQ("exact_canonical_key", suggestions[0].matchReason);
    EXPECT_EQ("blacktower.obj", suggestions[1].objectName);
    EXPECT_EQ("suffix", suggestions[1].matchReason);
}

TEST(SpawnReconciliation, ModuleLocalExactMatchWinsWithinSameKind)
{
    const std::vector<Ego::SpawnFile::ReconciliationCandidate> candidates = {
        {"chime.obj", "mp_objects/chime.obj", "global_work_in_progress", "/global/chime.obj"},
        {"chime.obj", "mp_objects/chime.obj", "module_data", "/module/chime.obj"},
    };

    const auto suggestions = Ego::SpawnFile::suggestSpawnReconciliationMatches("chime", candidates);
    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ("module_data", suggestions[0].sourceKind);
    EXPECT_EQ("exact_canonical_key", suggestions[0].matchReason);
}

TEST(SpawnReconciliation, TruncationFindsPrefixSuggestion)
{
    const std::vector<Ego::SpawnFile::ReconciliationCandidate> candidates = {
        {"chime.obj", "mp_objects/chime.obj", "module_data", "/module/chime.obj"},
        {"chimepuzzle.obj", "mp_objects/chimepuzzle.obj", "module_data", "/module/chimepuzzle.obj"},
        {"chimekeeper.obj", "mp_objects/chimekeeper.obj", "global_misc", "/global/chimekeeper.obj"},
    };

    const auto suggestions = Ego::SpawnFile::suggestSpawnReconciliationMatches("chimepuzl", candidates);
    ASSERT_FALSE(suggestions.empty());
    EXPECT_EQ("chimepuzzle.obj", suggestions[0].objectName);
    EXPECT_EQ("edit_distance_2", suggestions[0].matchReason);
}
