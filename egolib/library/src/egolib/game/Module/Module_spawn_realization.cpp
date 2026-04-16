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

/// @file egolib/game/Module/Module_spawn_realization.cpp
/// @brief Internal live spawn realization helpers for spawn.txt entries.

#include "egolib/game/Module/Module_spawn_realization.hpp"

#include "egolib/Entities/_Include.hpp"
#include "egolib/Log/_Include.hpp"

namespace module_spawn_realization
{
namespace
{

enum class PlayerBindingPolicy
{
    None,
    LocalDeviceSlot,
    ImportedLocalPlayer,
};

struct PlayerBindingDecision
{
    PlayerBindingPolicy policy = PlayerBindingPolicy::None;
    size_t deviceIndex = 0;
    bool identifySpawnOnSuccess = false;
};

size_t currentPlayerCount(const SpawnRealizationOps& ops)
{
    return ops.currentPlayerCount ? ops.currentPlayerCount() : 0u;
}

size_t currentLocalPlayerCount(const SpawnRealizationOps& ops)
{
    return ops.currentLocalPlayerCount ? ops.currentLocalPlayerCount() : 0u;
}

bool isObjectTerminated(const std::shared_ptr<Object>& object, const SpawnRealizationOps& ops)
{
    if (ops.isObjectTerminated)
    {
        return ops.isObjectTerminated(object);
    }

    return object ? object->isTerminated() : true;
}

int findImportMatchIndex(const ObjectProfileRef profileID, const SpawnRealizationState& state)
{
    if (!state.importList || !state.importData || !state.isProfileLoaded)
    {
        return -1;
    }

    if (profileID.get() > state.importData->max_slot || !state.isProfileLoaded(profileID))
    {
        return -1;
    }

    const int importedSlot = state.importData->slot_lst[profileID.get()];
    for (size_t index = 0; index < state.importList->count; ++index)
    {
        if (importedSlot == state.importList->lst[index].slot)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

PlayerBindingDecision decidePlayerBinding(const spawn_file_info_t& spawnInfo,
                                          const ObjectProfileRef profileID,
                                          const SpawnRealizationState& state,
                                          const SpawnRealizationOps& ops)
{
    if (!spawnInfo.stat || !ops.addPlayer)
    {
        return {};
    }

    const size_t playerCount = currentPlayerCount(ops);
    if (0 == state.importAmount && playerCount < state.playerAmount)
    {
        return { PlayerBindingPolicy::LocalDeviceSlot, currentLocalPlayerCount(ops), true };
    }

    if (playerCount >= state.importAmount || playerCount >= state.playerAmount || !state.importList || playerCount >= state.importList->count)
    {
        return {};
    }

    const int localIndex = findImportMatchIndex(profileID, state);
    if (-1 != localIndex)
    {
        return {
            PlayerBindingPolicy::ImportedLocalPlayer,
            static_cast<size_t>(state.importList->lst[localIndex].local_player_num),
            false
        };
    }

    // The old remote-input branch was already a no-op here. Keep it that way
    // until player-binding responsibilities are redesigned more broadly.
    return {};
}

void bindSpawnedPlayer(const spawn_file_info_t& spawnInfo,
                       const std::shared_ptr<Object>& object,
                       const SpawnRealizationState& state,
                       const SpawnRealizationOps& ops)
{
    if (!object)
    {
        return;
    }

    const PlayerBindingDecision decision = decidePlayerBinding(spawnInfo, object->getProfileID(), state, ops);
    switch (decision.policy)
    {
        case PlayerBindingPolicy::None:
        break;

        case PlayerBindingPolicy::LocalDeviceSlot:
        {
            const bool playerAdded = ops.addPlayer(object, decision.deviceIndex);
            if (decision.identifySpawnOnSuccess && playerAdded)
            {
                object->nameknown = true;
            }
        }
        break;

        case PlayerBindingPolicy::ImportedLocalPlayer:
            ops.addPlayer(object, decision.deviceIndex);
        break;
    }
}

} // namespace

std::shared_ptr<Object> realizeSpawnEntry(const spawn_file_info_t& spawnInfo,
                                          const std::shared_ptr<Object>& parent,
                                          const SpawnRealizationState& state,
                                          const SpawnRealizationOps& ops)
{
    if (!spawnInfo.do_spawn || spawnInfo.slot < 0)
    {
        return nullptr;
    }

    if (spawnInfo.attach != ATTACH_NONE && !parent)
    {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                         "failed to spawn ", "`", spawnInfo.spawn_name, "`",
                                         " due to missing parent", Log::EndOfEntry);
        return nullptr;
    }

    if (!ops.spawnObject)
    {
        return nullptr;
    }

    std::shared_ptr<Object> object = ops.spawnObject(spawnInfo);
    if (!object)
    {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                         "unable to spawn ", "`", spawnInfo.spawn_name, "`",
                                         Log::EndOfEntry);
        return nullptr;
    }

    object->giveMoney(spawnInfo.money);
    object->ai.content = spawnInfo.content;
    object->ai.passage = spawnInfo.passage;

    switch (spawnInfo.attach)
    {
        case ATTACH_NONE:
            if (ops.makeCharacterMatrix)
            {
                ops.makeCharacterMatrix(object);
            }
        break;

        case ATTACH_INVENTORY:
            if (ops.attachInventoryItem)
            {
                ops.attachInventoryItem(parent, object);
            }

            if (isObjectTerminated(object, ops))
            {
                return nullptr;
            }

            SET_BIT(object->ai.alert, ALERTIF_GRABBED);
        break;

        case ATTACH_LEFT:
        case ATTACH_RIGHT:
            if (ops.attachToGrip)
            {
                const grip_offset_t grip = (ATTACH_LEFT == spawnInfo.attach) ? GRIP_LEFT : GRIP_RIGHT;
                ops.attachToGrip(parent, object, grip);
            }
        break;
    }

    if (spawnInfo.level > 0 && object->experiencelevel < spawnInfo.level)
    {
        object->experience = object->getProfile()->getXPNeededForLevel(spawnInfo.level);
    }

    if (!state.importValid && nullptr != parent && parent->isPlayer())
    {
        object->nameknown = true;
        object->iskursed = false;
    }

    bindSpawnedPlayer(spawnInfo, object, state, ops);
    return object;
}

} // namespace module_spawn_realization
