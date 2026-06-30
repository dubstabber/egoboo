#pragma once

#include "egolib/FileFormats/wawalite_file.h"          // wawalite_weather_t
#include "egolib/Profiles/LocalParticleProfileRef.hpp"  // LocalParticleProfileRef
#include "egolib/typedef.h"                             // PLA_REF

#include <memory>
#include <vector>

class IParticleHandler;
namespace Ego { class Player; }
namespace Ego { namespace Physics { class ICollisionWorld; } }

/// The state of the weather.
struct WeatherState
{
    int timer_reset;                    ///< How long between each spawn?
    bool  over_water;                   ///< Only spawn over water?
    LocalParticleProfileRef part_gpip;  ///< Which particle to spawn?

    PLA_REF iplayer;
    int     time;                       ///< 0 is no weather

    void upload(const wawalite_weather_t& source);
    /// @brief Iterate the state of the weather.
    /// @remarks Drops snowflakes or rain or whatever.
    void update(const std::vector<std::shared_ptr<Ego::Player>>& players,
                IParticleHandler& particleHandler,
                const Ego::Physics::ICollisionWorld& collisionWorld);
};
