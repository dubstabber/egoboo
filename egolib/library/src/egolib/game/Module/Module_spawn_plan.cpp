//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/Module/Module_spawn_plan.cpp
/// @brief Internal planning helpers for spawn.txt realization.

#include "egolib/game/Module/Module_spawn_plan.hpp"

#include "egolib/Log/_Include.hpp"
#include "egolib/game/Module/module_spawn.h"

#include <algorithm>
#include <unordered_set>

namespace module_spawn_plan
{
namespace
{

constexpr int FIRST_DYNAMIC_PROFILE_SLOT = 1 + MAX_IMPORT_PER_PLAYER * MAX_PLAYER;

ObjectProfileRef reserveDynamicSlot(const std::string& spawnName,
                                    std::unordered_map<int, std::string>& reservedSlots,
                                    const SlotLoadedPredicate& isSlotLoaded)
{
    for (ObjectProfileRef profileSlot(FIRST_DYNAMIC_PROFILE_SLOT);
         profileSlot < ObjectProfileRef::Invalid;
         ++profileSlot)
    {
        if (isSlotLoaded && isSlotLoaded(profileSlot))
        {
            continue;
        }

        auto existing = reservedSlots.find(profileSlot.get());
        if (existing != reservedSlots.end())
        {
            if (existing->second == spawnName)
            {
                return profileSlot;
            }
            continue;
        }

        reservedSlots[profileSlot.get()] = spawnName;
        return profileSlot;
    }

    return ObjectProfileRef::Invalid;
}

void resolveDynamicEntries(SpawnPlan& plan)
{
    for (auto& entry : plan.entries)
    {
        if (entry.slot > -1)
        {
            continue;
        }

        for (const auto& reserved : plan.reservedSlots)
        {
            if (reserved.second == entry.spawn_comment)
            {
                entry.slot = reserved.first;
                break;
            }
        }
    }
}

} // namespace

SpawnPlan buildSpawnPlan(std::vector<spawn_file_info_t> entries,
                         const Ego::TreasureTables& treasureTables,
                         const SlotLoadedPredicate& isSlotLoaded)
{
    SpawnPlan plan;
    plan.entries.reserve(std::min(entries.size(), static_cast<size_t>(OBJECTS_MAX)));

    std::unordered_set<std::string> dynamicObjectNames;

    for (auto& entry : entries)
    {
        if (plan.entries.size() >= OBJECTS_MAX)
        {
            Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                             "too many objects in file ", "`", "mp_data/spawn,txt", "`",
                                             ". Maximum number of objects is ", OBJECTS_MAX, Log::EndOfEntry);
            break;
        }

        if (entry.slot >= INVALID_PRO_REF)
        {
            Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                             "invalid slot ", entry.slot, " for ", "`", entry.spawn_comment, "`",
                                             " in file ", "`", "mp_data/spawn,txt", "`", Log::EndOfEntry);
            continue;
        }

        convert_spawn_file_load_name(entry, treasureTables);

        if (entry.slot <= -1)
        {
            dynamicObjectNames.insert(entry.spawn_comment);
        }
        else if (plan.reservedSlots[entry.slot].empty())
        {
            plan.reservedSlots[entry.slot] = entry.spawn_comment;
        }

        plan.entries.push_back(entry);
    }

    for (const auto& spawnName : dynamicObjectNames)
    {
        const ObjectProfileRef profileSlot = reserveDynamicSlot(spawnName, plan.reservedSlots, isSlotLoaded);
        if (profileSlot == ObjectProfileRef::Invalid)
        {
            Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                             "unable to acquire free dynamic slot for object ",
                                             spawnName, ". All slots in use?", Log::EndOfEntry);
        }
    }

    resolveDynamicEntries(plan);
    return plan;
}

} // namespace module_spawn_plan
