#pragma once

#include "egolib/Logic/TreasureTables.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Ego::SpawnFile
{

using TreasureResolver = std::function<std::string(const std::string&)>;

struct ReconciliationCandidate
{
    std::string objectName;
    std::string resolvedVirtualPath;
    std::string sourceKind;
    std::string originPath;
};

struct ReconciliationSuggestion
{
    std::string objectName;
    std::string resolvedVirtualPath;
    std::string sourceKind;
    std::string originPath;
    std::string matchReason;
    int score = 0;
};

std::string normalizeSpawnLoadName(const std::string& loadName);

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const TreasureResolver& resolveTreasure);

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const Ego::TreasureTables& treasureTables);

std::string buildReconciliationKey(const std::string& loadName);

bool isPlaceholderLikeSpawnName(const std::string& loadName);

std::vector<ReconciliationSuggestion> suggestSpawnReconciliationMatches(
    const std::string& loadName,
    const std::vector<ReconciliationCandidate>& candidates,
    size_t limit = 5);

} // namespace Ego::SpawnFile
