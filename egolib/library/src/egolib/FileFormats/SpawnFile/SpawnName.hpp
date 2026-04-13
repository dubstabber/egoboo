#pragma once

#include "egolib/Logic/TreasureTables.hpp"

#include <functional>
#include <string>

namespace Ego::SpawnFile
{

using TreasureResolver = std::function<std::string(const std::string&)>;

std::string normalizeSpawnLoadName(const std::string& loadName);

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const TreasureResolver& resolveTreasure);

std::string resolveSpawnLoadName(const std::string& loadName,
                                 const Ego::TreasureTables& treasureTables);

std::string buildReconciliationKey(const std::string& loadName);

} // namespace Ego::SpawnFile
