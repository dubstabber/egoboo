#pragma once

/// @file egolib/game/Core/ISessionState.hpp
/// @brief Active read-only session-state seam.

#include "egolib/IDSZ.hpp"
#include "egolib/typedef.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Ego { class Player; }

struct LocalPlayerStatus
{
    size_t registeredCount = 0;
    size_t aliveCount = 0;
    size_t deadCount = 0;

    bool allPlayersDead() const
    {
        return deadCount >= registeredCount;
    }
};

struct LocalPlayerPerceptionState
{
    float grogLevel = 0.0f;
    float dazeLevel = 0.0f;
    float seeInvisibleLevel = 0.0f;
    float seeInvisibleMagnitude = 1.0f;
    float seeDarkLevel = 0.0f;
    float seeDarkMagnitude = 1.0f;
    float seeKurseLevel = 0.0f;
};

struct EnemySenseState
{
    EnemySenseState();
    EnemySenseState(TEAM_REF team, const IDSZ2& idsz);

    TEAM_REF team;
    IDSZ2 idsz;
};

/// @brief The active read-only session state for runtime callers that need
///        player, perception, enemy-sense, respawn, or clock state without
///        depending on GameSessionContext.
class ISessionState
{
public:
    virtual ~ISessionState() = default;

    virtual const std::vector<std::shared_ptr<Ego::Player>>& playerList() const = 0;
    virtual size_t localPlayerCount() const = 0;
    virtual const LocalPlayerStatus& localPlayerStatus() const = 0;
    virtual const LocalPlayerPerceptionState& localPlayerPerception() const = 0;
    virtual const EnemySenseState& enemySense() const = 0;
    virtual int respawnCooldown() const = 0;
    virtual bool hasLocalPlayers() const = 0;
    virtual bool allLocalPlayersDead() const = 0;
    virtual uint32_t worldUpdateCount() const = 0;
    virtual uint32_t characterStatClock() const = 0;
    virtual uint32_t enchantStatClock() const = 0;
};

/// @brief Install @a state as the active session-state surface. Passing nullptr
///        is equivalent to clearSessionState().
void installSessionState(ISessionState* state);

/// @brief Clear the installed active session-state surface.
void clearSessionState();

/// @brief The installed session-state surface, or nullptr if none is installed.
ISessionState* tryActiveSessionState();

/// @brief The installed active session-state surface.
/// @throw std::logic_error if no session state is installed.
ISessionState& activeSessionState();
