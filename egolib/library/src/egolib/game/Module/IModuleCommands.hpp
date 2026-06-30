#pragma once

/// @file egolib/game/Module/IModuleCommands.hpp
/// @brief Active module command/effect seam.

#include "egolib/Mesh/Info.hpp"
#include "egolib/typedef.h"
#include "egolib/_math.h"
#include "egolib/integrations/math.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

class Passage;
enum XPType : uint8_t;
namespace Ego { class Player; }

/// @brief The active module command surface for callers that need module
///        gameplay effects without depending on the concrete GameModule.
///
/// Lifecycle and ownership stay on GameSessionContext/GameModule. This seam is
/// intentionally limited to effects and queries that are currently needed by
/// gameplay/script/presentation helpers during an active module.
class IModuleCommands
{
public:
    virtual ~IModuleCommands() = default;

    virtual void update() = 0;

    virtual ObjectRef spawnObjectRef(const Ego::Vector3f& pos,
                                     ObjectProfileRef profile,
                                     TEAM_REF team,
                                     int skin,
                                     const Facing& facing,
                                     const std::string& name,
                                     ObjectRef overrideRef) = 0;

    virtual void setImportPlayers(const std::list<std::string>& players) = 0;
    virtual void setRespawnValid(bool valid) = 0;
    virtual void beatModule() = 0;
    virtual void setExportValid(bool valid) = 0;
    virtual const std::string& getPath() const = 0;

    virtual void enablePitsKill() = 0;
    virtual void enablePitsTeleport(const Ego::Vector3f& location) = 0;
    virtual bool isInsidePitBounds(float x, float y) const = 0;

    virtual bool setTileType(Index1D tileIndex, uint16_t tileType) = 0;
    virtual bool tryGetTileTypeAtPosition(const Ego::Vector2f& position, uint16_t& tileType) const = 0;
    virtual bool setTileTypeAtPosition(const Ego::Vector2f& position, uint16_t tileType) = 0;

    virtual std::shared_ptr<Passage> getPassageByID(int id) = 0;
    virtual ObjectRef getShopOwner(float x, float y) = 0;
    virtual void removeShopOwner(ObjectRef owner) = 0;

    virtual ObjectRef getTeamLeaderRef(TEAM_REF teamRef) const = 0;
    virtual ObjectRef getTeamCallerForHelpRef(TEAM_REF teamRef) const = 0;
    virtual uint16_t getTeamMorale(TEAM_REF teamRef) const = 0;
    virtual void giveTeamExperience(TEAM_REF teamRef, int amount, XPType type) const = 0;

    virtual std::shared_ptr<Ego::Player> tryGetPlayer(size_t index) = 0;
};

/// @brief Install @a commands as the active module command surface. Passing
///        nullptr is equivalent to clearModuleCommands().
void installModuleCommands(IModuleCommands* commands);

/// @brief Clear the installed active module command surface.
void clearModuleCommands();

/// @brief The installed module command surface, or nullptr if none is installed.
IModuleCommands* tryActiveModuleCommands();

/// @brief The installed active module command surface.
/// @throw std::logic_error if no module commands are installed.
IModuleCommands& activeModuleCommands();
