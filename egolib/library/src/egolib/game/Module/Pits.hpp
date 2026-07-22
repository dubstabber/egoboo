#pragma once

#include "egolib/integrations/math.hpp"  // Ego::Vector3f

#include <cstdint>

class IAudioSystem;
class IParticleHandler;
class ObjectHandler;
struct damagetile_instance_t;

/// The in-game state of the kill/teleport pits (method bodies in Module_update.cpp).
struct PitsState
{
    static constexpr float DEPTH = -60;         ///< Depth to kill character
    static constexpr uint32_t CLOCK_RATE = 20;  ///< How many game ticks between each pit check

    uint32_t clock = CLOCK_RATE;  ///< Countdown until the next pit check
    bool kill = false;            ///< Do they kill?
    bool teleport = false;        ///< Do they teleport?
    Ego::Vector3f teleportPos{};  ///< If they teleport, then where to?

    /// @brief Make falling into a pit instantly kill characters.
    ///        Mutually exclusive with enableTeleport().
    void enableKill();

    /// @brief Make falling into a pit teleport characters back to @a location.
    ///        Mutually exclusive with enableKill().
    void enableTeleport(const Ego::Vector3f& location);

    /// @brief Kill or teleport any particle or character that fell into a pit.
    void update(ObjectHandler& objectHandler, IParticleHandler& particleHandler,
                IAudioSystem& audioSystem, const damagetile_instance_t& damageTile);
};
