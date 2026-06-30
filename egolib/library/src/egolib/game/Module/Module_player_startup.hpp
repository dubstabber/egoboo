#pragma once

#include "egolib/Entities/_Include.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Logic/PlayerQuestLog.hpp"
#include "egolib/game/game.h"

#include <functional>
#include <memory>
#include <vector>

namespace module_player_startup
{

inline std::shared_ptr<Ego::Player> registerPlayerBinding(std::vector<std::shared_ptr<Ego::Player>>& playerList,
                                                          ObjectHandler& objectHandler,
                                                          Object& object,
                                                          const Ego::Input::InputDevice& device)
{
    std::shared_ptr<Ego::Player> player = Ego::Player::createForObject(objectHandler, object.getObjRef(), device);
    if (!player)
    {
        return nullptr;
    }

    playerList.push_back(player);

    // Set the reference before any startup side effects can observe the player.
    object.setPlayerNumber(playerList.size() - 1);
    return player;
}

inline void applySuccessfulLocalPlayerBookkeeping(Object& object,
                                                  size_t registeredPlayerCount,
                                                  const std::function<void(size_t)>& publishLocalPlayerCount,
                                                  bool identifySpawnOnSuccess)
{
    object.setLocalPlayer(true);
    publishLocalPlayerCount(registeredPlayerCount);

    if (identifySpawnOnSuccess)
    {
        object.setNameKnown(true);
    }
}

inline void finalizeLocalPlayerStartup(Object& object,
                                       const std::shared_ptr<Ego::Player>& player,
                                       size_t registeredPlayerCount,
                                       const std::function<void(size_t)>& publishLocalPlayerCount,
                                       bool identifySpawnOnSuccess)
{
    Ego::loadPlayerQuestLog(player->getQuestLog(), object.getProfile()->getPathname());
    applySuccessfulLocalPlayerBookkeeping(object,
                                          registeredPlayerCount,
                                          publishLocalPlayerCount,
                                          identifySpawnOnSuccess);
}

inline bool addPlayer(std::vector<std::shared_ptr<Ego::Player>>& playerList,
                      ObjectHandler& objectHandler,
                      ObjectRef objectRef,
                      const Ego::Input::InputDevice& device,
                      const std::function<void(size_t)>& publishLocalPlayerCount,
                      bool identifySpawnOnSuccess)
{
    Object* object = objectHandler.get(objectRef);
    if (object == nullptr || object->isTerminated())
    {
        return false;
    }

    const std::shared_ptr<Ego::Player> player = registerPlayerBinding(playerList, objectHandler, *object, device);
    if (!player)
    {
        return false;
    }

    finalizeLocalPlayerStartup(*object,
                               player,
                               playerList.size(),
                               publishLocalPlayerCount,
                               identifySpawnOnSuccess);
    return true;
}

} // namespace module_player_startup
