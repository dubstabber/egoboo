#pragma once

#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/game.h"

#include <functional>

class Object;

namespace module_spawn_realization
{

using ProfileLoadedPredicate = std::function<bool(ObjectProfileRef)>;

struct PlayerBindingRequest
{
    size_t deviceIndex = 0;
    bool identifySpawnOnSuccess = false;
};

struct SpawnRealizationState
{
    bool importValid = false;
    size_t importAmount = 0;
    size_t playerAmount = 0;
    const import_list_t* importList = nullptr;
    const pro_import_t* importData = nullptr;
    ProfileLoadedPredicate isProfileLoaded;
};

struct SpawnRealizationOps
{
    std::function<ObjectRef(const spawn_file_info_t&)> spawnObject;
    std::function<Object*(ObjectRef)> resolveObject;
    std::function<void(ObjectRef)> makeCharacterMatrix;
    std::function<void(ObjectRef, ObjectRef)> attachInventoryItem;
    std::function<bool(ObjectRef)> isObjectTerminated;
    std::function<bool(ObjectRef, ObjectRef, grip_offset_t)> attachToGrip;
    std::function<size_t()> currentPlayerCount;
    std::function<size_t()> currentLocalPlayerCount;
    std::function<bool(ObjectRef, const PlayerBindingRequest&)> addPlayer;
};

ObjectRef realizeSpawnEntry(const spawn_file_info_t& spawnInfo,
                            ObjectRef parentRef,
                            const SpawnRealizationState& state,
                            const SpawnRealizationOps& ops);

} // namespace module_spawn_realization
