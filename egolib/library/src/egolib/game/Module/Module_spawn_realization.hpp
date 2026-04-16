#pragma once

#include "egolib/FileFormats/SpawnFile/spawn_file.h"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/game.h"

#include <functional>
#include <memory>

class Object;

namespace module_spawn_realization
{

using ProfileLoadedPredicate = std::function<bool(ObjectProfileRef)>;

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
    std::function<std::shared_ptr<Object>(const spawn_file_info_t&)> spawnObject;
    std::function<void(const std::shared_ptr<Object>&)> makeCharacterMatrix;
    std::function<void(const std::shared_ptr<Object>&, const std::shared_ptr<Object>&)> attachInventoryItem;
    std::function<bool(const std::shared_ptr<Object>&)> isObjectTerminated;
    std::function<bool(const std::shared_ptr<Object>&, const std::shared_ptr<Object>&, grip_offset_t)> attachToGrip;
    std::function<size_t()> currentPlayerCount;
    std::function<size_t()> currentLocalPlayerCount;
    std::function<bool(const std::shared_ptr<Object>&, size_t)> addPlayer;
};

std::shared_ptr<Object> realizeSpawnEntry(const spawn_file_info_t& spawnInfo,
                                          const std::shared_ptr<Object>& parent,
                                          const SpawnRealizationState& state,
                                          const SpawnRealizationOps& ops);

} // namespace module_spawn_realization
