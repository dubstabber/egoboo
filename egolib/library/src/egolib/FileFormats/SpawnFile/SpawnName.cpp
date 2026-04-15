#include "egolib/FileFormats/SpawnFile/SpawnName.hpp"

#include "egolib/Core/StringUtilities.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <vector>

namespace Ego::SpawnFile
{

namespace
{

bool endsWithObjectSuffix(const std::string& value)
{
    static constexpr const char* suffix = ".obj";
    const size_t suffixLength = 4;
    return value.size() >= suffixLength
        && 0 == value.compare(value.size() - suffixLength, suffixLength, suffix);
}

std::string basenameOf(const std::string& value)
{
    const size_t slash = value.find_last_of("/\\");
    if (slash == std::string::npos)
    {
        return value;
    }
    return value.substr(slash + 1);
}

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size()
        && 0 == value.compare(0, prefix.size(), prefix);
}

bool endsWith(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size()
        && 0 == value.compare(value.size() - suffix.size(), suffix.size(), suffix);
}

bool isModuleLocalSource(const std::string& sourceKind)
{
    return startsWith(sourceKind, "module_");
}

bool isWorkInProgressSource(const std::string& sourceKind)
{
    return sourceKind.find("work_in_progress") != std::string::npos;
}

int sourceKindBonus(const std::string& sourceKind)
{
    if ("module_data" == sourceKind)
    {
        return 50;
    }

    if ("module_user" == sourceKind)
    {
        return 45;
    }

    if (isWorkInProgressSource(sourceKind))
    {
        return 0;
    }

    return 20;
}

size_t boundedEditDistance(const std::string& left,
                           const std::string& right,
                           const size_t maxDistance)
{
    const size_t leftSize = left.size();
    const size_t rightSize = right.size();

    if (left == right)
    {
        return 0;
    }

    if (leftSize > rightSize + maxDistance || rightSize > leftSize + maxDistance)
    {
        return maxDistance + 1;
    }

    std::vector<size_t> previous(rightSize + 1);
    std::vector<size_t> current(rightSize + 1);

    for (size_t column = 0; column <= rightSize; ++column)
    {
        previous[column] = column;
    }

    for (size_t row = 1; row <= leftSize; ++row)
    {
        current[0] = row;
        size_t rowMinimum = current[0];

        for (size_t column = 1; column <= rightSize; ++column)
        {
            const size_t substitutionCost = left[row - 1] == right[column - 1] ? 0 : 1;
            current[column] = std::min({previous[column] + 1,
                                        current[column - 1] + 1,
                                        previous[column - 1] + substitutionCost});
            rowMinimum = std::min(rowMinimum, current[column]);
        }

        if (rowMinimum > maxDistance)
        {
            return maxDistance + 1;
        }

        previous.swap(current);
    }

    return previous[rightSize];
}

struct CandidateScore
{
    ReconciliationSuggestion suggestion;
    bool moduleLocal = false;
    bool workInProgress = false;
};

} // namespace

std::string normalizeSpawnLoadName(const std::string& loadName)
{
    std::string normalized = Ego::trim_ws(loadName);
    if (!endsWithObjectSuffix(normalized))
    {
        normalized += ".obj";
    }

    std::transform(normalized.begin(),
                   normalized.end(),
                   normalized.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });

    return normalized;
}

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const TreasureResolver& resolveTreasure)
{
    std::string resolved = Ego::trim_ws(loadName);
    if (!resolved.empty() && '%' == resolved[0])
    {
        resolved = resolveTreasure(resolved);
    }
    return normalizeSpawnLoadName(resolved);
}

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const Ego::TreasureTables& treasureTables)
{
    return resolveSpawnLoadName(
        loadName,
        [&treasureTables](const std::string& treasureTableName)
        {
            return treasureTables.getRandomTreasure(treasureTableName);
        });
}

std::string buildReconciliationKey(const std::string& loadName)
{
    std::string key = basenameOf(loadName);
    key = normalizeSpawnLoadName(key);
    if (endsWithObjectSuffix(key))
    {
        key.resize(key.size() - 4);
    }

    std::string canonical;
    canonical.reserve(key.size());
    for (unsigned char character : key)
    {
        switch (character)
        {
            case ' ':
            case '\'':
            case '-':
            case '_':
                break;
            default:
                canonical.push_back(static_cast<char>(std::tolower(character)));
                break;
        }
    }

    return canonical;
}

bool isPlaceholderLikeSpawnName(const std::string& loadName)
{
    const std::string normalized = normalizeSpawnLoadName(loadName);
    const std::string key = buildReconciliationKey(normalized);
    if (normalized == ".obj" || key.empty())
    {
        return true;
    }

    static const std::array<const char*, 8> placeholderKeys = {
        "unknown",
        "object",
        "placeholder",
        "dummy",
        "none",
        "todo",
        "temp",
        "unused",
    };

    return std::find(placeholderKeys.begin(), placeholderKeys.end(), key) != placeholderKeys.end();
}

std::vector<ReconciliationSuggestion> suggestSpawnReconciliationMatches(
    const std::string& loadName,
    const std::vector<ReconciliationCandidate>& candidates,
    const size_t limit)
{
    const std::string normalized = normalizeSpawnLoadName(loadName);
    const std::string key = buildReconciliationKey(normalized);

    std::vector<CandidateScore> ranked;
    ranked.reserve(candidates.size());

    for (const auto& candidate : candidates)
    {
        const std::string candidateKey = buildReconciliationKey(candidate.objectName);
        if (candidateKey.empty())
        {
            continue;
        }

        int baseScore = 0;
        std::string matchReason;

        if (candidateKey == key)
        {
            baseScore = 1000;
            matchReason = "exact_canonical_key";
        }
        else if (!key.empty() && startsWith(candidateKey, key))
        {
            baseScore = 800;
            matchReason = "prefix";
        }
        else if (!key.empty() && endsWith(candidateKey, key))
        {
            baseScore = 780;
            matchReason = "suffix";
        }
        else if (!key.empty() &&
                 (candidateKey.size() <= key.size() + 3 && key.size() <= candidateKey.size() + 3) &&
                 (candidateKey.find(key) != std::string::npos || key.find(candidateKey) != std::string::npos))
        {
            baseScore = 760;
            matchReason = "contains";
        }
        else
        {
            const size_t distance = boundedEditDistance(key, candidateKey, 3);
            if (distance > 3)
            {
                continue;
            }

            baseScore = 600 - static_cast<int>(distance) * 40;
            matchReason = "edit_distance_" + std::to_string(distance);
        }

        CandidateScore scored;
        scored.moduleLocal = isModuleLocalSource(candidate.sourceKind);
        scored.workInProgress = isWorkInProgressSource(candidate.sourceKind);
        scored.suggestion.objectName = candidate.objectName;
        scored.suggestion.resolvedVirtualPath = candidate.resolvedVirtualPath;
        scored.suggestion.sourceKind = candidate.sourceKind;
        scored.suggestion.originPath = candidate.originPath;
        scored.suggestion.matchReason = matchReason;
        scored.suggestion.score = baseScore + sourceKindBonus(candidate.sourceKind);
        ranked.push_back(std::move(scored));
    }

    std::sort(ranked.begin(),
              ranked.end(),
              [](const CandidateScore& left, const CandidateScore& right)
              {
                  if (left.suggestion.score != right.suggestion.score)
                  {
                      return left.suggestion.score > right.suggestion.score;
                  }
                  if (left.moduleLocal != right.moduleLocal)
                  {
                      return left.moduleLocal;
                  }
                  if (left.workInProgress != right.workInProgress)
                  {
                      return !left.workInProgress;
                  }
                  if (left.suggestion.matchReason != right.suggestion.matchReason)
                  {
                      return left.suggestion.matchReason < right.suggestion.matchReason;
                  }
                  if (left.suggestion.objectName != right.suggestion.objectName)
                  {
                      return left.suggestion.objectName < right.suggestion.objectName;
                  }
                  return left.suggestion.originPath < right.suggestion.originPath;
              });

    std::vector<ReconciliationSuggestion> suggestions;
    suggestions.reserve(std::min(limit, ranked.size()));
    for (const auto& scored : ranked)
    {
        if (suggestions.size() >= limit)
        {
            break;
        }
        suggestions.push_back(scored.suggestion);
    }

    return suggestions;
}

} // namespace Ego::SpawnFile
