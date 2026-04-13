#include "egolib/FileFormats/SpawnFile/SpawnName.hpp"

#include "egolib/Core/StringUtilities.hpp"

#include <algorithm>
#include <cctype>

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

} // namespace Ego::SpawnFile
