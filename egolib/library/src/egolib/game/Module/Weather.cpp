#include "egolib/game/Module/Weather.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"

void WeatherState::update(const std::vector<std::shared_ptr<Ego::Player>>& players,
                          IParticleHandler& particleHandler,
                          const Ego::Physics::ICollisionWorld& collisionWorld)
{
    //Does this module have valid weather?
    if (time < 0 || part_gpip == LocalParticleProfileRef::Invalid) {
        return;
    }

    time--;
    if (0 == time)
    {
        time = timer_reset;

        // Find a valid player
        std::shared_ptr<Ego::Player> player = nullptr;
        if (!players.empty()) {
            iplayer = (iplayer + 1) % players.size();
            player = players[iplayer];
        }

        // Did we find one?
        if (player)
        {
            Object* pchr = player->tryObject();
            if (pchr)
            {
                // Yes, so spawn nearby that character
                std::shared_ptr<Ego::Particle> particle = particleHandler.spawnGlobalParticle(pchr->getPosition(), ATK_FRONT, part_gpip, 0, over_water);
                if (particle)
                {
                    // Weather particles spawned at the edge of the map look ugly, so don't spawn them there
                    if (particle->getPosX() < EDGE || particle->getPosX() > collisionWorld.getEdgeX() - EDGE)
                    {
                        particle->requestTerminate();
                    }
                    else if (particle->getPosY() < EDGE || particle->getPosY() > collisionWorld.getEdgeY() - EDGE)
                    {
                        particle->requestTerminate();
                    }
                }
            }
        }
    }
}

void WeatherState::upload(const wawalite_weather_t& source)
{
    this->iplayer = 0;

    this->timer_reset = source.timer_reset;
    this->over_water = source.over_water;
    this->part_gpip = source.part_gpip;

    // Ensure an update.
    this->time = this->timer_reset;
}
