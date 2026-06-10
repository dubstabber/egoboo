#pragma once

#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/Logic/TreasureTables.hpp"
#include "egolib/typedef.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace module_spawn_plan
{

using SlotLoadedPredicate = std::function<bool(ObjectProfileRef)>;

struct SpawnPlan
{
    std::vector<spawn_file_info_t> entries;
    std::unordered_map<int, std::string> reservedSlots;
};

SpawnPlan buildSpawnPlan(std::vector<spawn_file_info_t> entries,
                         const Ego::TreasureTables& treasureTables,
                         const SlotLoadedPredicate& isSlotLoaded,
                         int firstDynamicSlot);

} // namespace module_spawn_plan
