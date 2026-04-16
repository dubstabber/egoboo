#pragma once

#include "egolib/Entities/_Include.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/game.h"

#include <memory>
#include <vector>

namespace module_player_startup
{

inline std::shared_ptr<Ego::Player> registerPlayerBinding(std::vector<std::shared_ptr<Ego::Player>>& playerList,
                                                          const std::shared_ptr<Object>& object,
                                                          const Ego::Input::InputDevice& device)
{
    std::shared_ptr<Ego::Player> player = std::make_shared<Ego::Player>(object, device);
    playerList.push_back(player);

    // Set the reference before any startup side effects can observe the player.
    object->is_which_player = playerList.size() - 1;
    return player;
}

inline void finalizeLocalPlayerStartup(const std::shared_ptr<Object>& object,
                                       const std::shared_ptr<Ego::Player>& player,
                                       bool identifySpawnOnSuccess)
{
    // Preserve the current best-effort quest hydration behavior.
    player->getQuestLog().loadFromFile(object->getProfile()->getPathname());

    local_stats.noplayers = false;
    object->islocalplayer = true;
    local_stats.player_count++;

    if (identifySpawnOnSuccess)
    {
        object->nameknown = true;
    }
}

inline bool addPlayer(std::vector<std::shared_ptr<Ego::Player>>& playerList,
                      const std::shared_ptr<Object>& object,
                      const Ego::Input::InputDevice& device,
                      bool identifySpawnOnSuccess)
{
    if (!object || object->isTerminated())
    {
        return false;
    }

    const std::shared_ptr<Ego::Player> player = registerPlayerBinding(playerList, object, device);
    finalizeLocalPlayerStartup(object, player, identifySpawnOnSuccess);
    return true;
}

} // namespace module_player_startup
