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

/// @file egolib/game/Module/Module_update.cpp
/// @brief Per-update simulation steps with explicit inputs, and the GameModule update orchestrator.

#include "egolib/game/Module/Module_internal.h"
#include "egolib/game/Module/Module_update.hpp"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/Graphics/ICameraSystem.hpp"

namespace
{
Object* tryObservedPlayerObject(const std::shared_ptr<Ego::Player>& player)
{
    if (!player)
    {
        return nullptr;
    }

    Object* object = player->tryObject();
    return object != nullptr && !object->isTerminated() ? object : nullptr;
}

ObjectAttribution terrainDamageAttribution(const ObjectHandler& objectHandler, ObjectRef attackerRef)
{
    const Object* attacker = objectHandler.get(attackerRef);
    return attacker != nullptr
        ? attacker->attribution(static_cast<TEAM_REF>(Team::TEAM_DAMAGE))
        : ObjectAttribution(static_cast<TEAM_REF>(Team::TEAM_DAMAGE));
}

int applyTerrainDamage(IDamageable& target, const IPair& damage, DamageType damageType,
                       ObjectAttribution attacker, bool setDamageTime)
{
    return target.damage(ATK_BEHIND, damage, damageType,
                         attacker, true, setDamageTime, false);
}
}

namespace module_update
{

void checkPassageMusic(const std::vector<std::shared_ptr<Ego::Player>>& players,
                       const std::vector<std::shared_ptr<Passage>>& passages,
                       IAudioSystem& audioSystem)
{
    // Look at each player
    for (const std::shared_ptr<Ego::Player> &player : players)
    {
        Object* pchr = tryObservedPlayerObject(player);
        if (!pchr) continue;

        if (!pchr->isAlive()) continue;

        // Don't do items in hands or inventory.
        if (pchr->isBeingHeld()) continue;

        //Loop through every passage
        for (const std::shared_ptr<Passage>& passage : passages)
        {
            if (passage->checkPassageMusic(*pchr, audioSystem))
            {
                return;
            }
        }
    }
}

void updateAllObjects(ObjectHandler& objectHandler, uint32_t currentUpdateFrame,
                      uint32_t& characterStatClock)
{
    for (const ObjectRef& objectRef : objectHandler.objectRefIterator())
    {
        Object* object = objectHandler.get(objectRef);
        if (object == nullptr)
        {
            continue;
        }

        //Skip terminated objects
        if (object->isTerminated()) {
            continue;
        }

        //Update object logic
        object->update();

        //Update model animation
        object->getGraphics().updateAnimation();

        //Check if this object should be poofed (destroyed)
        bool timeOut = (object->getAIPoofTime() > 0) && (object->getAIPoofTime() <= static_cast<int32_t>(currentUpdateFrame));
        if (timeOut) {
            object->requestTerminate();
        }
    }

    // Reset the clock
    if (characterStatClock >= ONESECOND) {
        characterStatClock -= ONESECOND;
    }
}

void updateDamageTiles(ObjectHandler& objectHandler, const ego_mesh_t& mesh,
                       const damagetile_instance_t& damageTile,
                       IParticleHandler& particleHandler, uint32_t currentUpdateFrame)
{
    // do the damage tile stuff
    for (const ObjectRef& objectRef : objectHandler.objectRefIterator()) {
        Object* pchr = objectHandler.get(objectRef);
        if (pchr == nullptr)
        {
            continue;
        }

        IDamageable& damageable = *pchr;
        const IDamageable& constDamageable = *pchr;

        // if the object is not really in the game, do nothing
        if (pchr->isHidden() || !constDamageable.isAlive()) continue;

        // if you are being held by something, you are protected
        if (pchr->isInsideInventory()) continue;

        // are we on a damage tile?
        if (!mesh.grid_is_valid(pchr->getTile())) continue;
        if (0 == mesh.test_fx(pchr->getTile(), MAPFX_DAMAGE)) continue;

        // are we low enough?
        if (pchr->getPosZ() > pchr->getFloorElevation() + DAMAGERAISE) continue;

        // allow reaffirming damage to things like torches, even if they are being held,
        // but make the tolerance closer so that books won't burn so easily
        if (!pchr->isBeingHeld() || pchr->getPosZ() < pchr->getFloorElevation() + DAMAGERAISE)
        {
            if (constDamageable.getReaffirmDamageType() == damageTile.damagetype)
            {
                if (0 == (currentUpdateFrame & TILE_REAFFIRM_AND))
                {
                    reaffirm_attached_particles(pchr->getObjRef());
                }
            }
        }

        // do not do direct damage to items that are being held
        if (pchr->isBeingHeld()) continue;

        // don't do direct damage to invulnerable objects
        if (constDamageable.isInvincible()) continue;

        if (0 == constDamageable.getDamageTimer())
        {
            int actual_damage = applyTerrainDamage(damageable, damageTile.amount,
                                                   static_cast<DamageType>(damageTile.damagetype),
                                                   ObjectAttribution(static_cast<TEAM_REF>(Team::TEAM_DAMAGE)), false);

            damageable.setDamageTimer(DAMAGETILETIME);

            if ((actual_damage > 0) && (LocalParticleProfileRef::Invalid != damageTile.part_gpip) && 0 == (currentUpdateFrame & damageTile.partand)) {
                particleHandler.spawnGlobalParticle(pchr->getPosition(), ATK_FRONT, damageTile.part_gpip, 0);
            }
        }
    }
}

} // namespace module_update

void PitsState::enableTeleport(const Ego::Vector3f& location)
{
    teleportPos = location;
    teleport = true;
    kill = false;
}

void PitsState::enableKill()
{
    teleport = false;
    kill = true;
}

void PitsState::update(ObjectHandler& objectHandler, IParticleHandler& particleHandler,
                       IAudioSystem& audioSystem, const damagetile_instance_t& damageTile)
{
    //Are pits enabled?
    if (!kill && !teleport) {
        return;
    }

    //Decrease the timer
    if (clock > 0) {
        clock--;
    }

    if (0 == clock)
    {
        //Reset timer
        clock = CLOCK_RATE;

        // Kill any particles that fell in a pit, if they die in water...
        for (const std::shared_ptr<Ego::Particle> &particle : particleHandler.iterator()) {
            if (particle->getPosZ() < DEPTH && particle->getProfile()->end_water)
            {
                particle->requestTerminate();
            }
        }

        // Kill or teleport any characters that fell in a pit...
        for (const ObjectRef& objectRef : objectHandler.objectRefIterator()) {
            Object* pchr = objectHandler.get(objectRef);
            if (pchr == nullptr)
            {
                continue;
            }

            IDamageable& damageable = *pchr;

            // Is it a valid character?
            if (damageable.isInvincible() || !damageable.isAlive()) continue;
            if (pchr->isBeingHeld()) continue;

            // Do we kill it?
            if (kill && pchr->getPosZ() < DEPTH)
            {
                // Got one!
                damageable.kill(ObjectAttribution(), false);
                pchr->setVelocity({0.0f, 0.0f, pchr->getVelocity().z()});
            }

            // Do we teleport it?
            else if (teleport && pchr->getPosZ() < DEPTH * 4)
            {
                // Teleport them back to a "safe" spot
                if (!pchr->teleport(teleportPos, pchr->getFacingZ())) {
                    // Kill it instead
                    damageable.kill(ObjectAttribution(), false);
                    pchr->setVelocity({0.0f, 0.0f, pchr->getVelocity().z()});
                }
                else {
                    // Stop movement
                    pchr->setVelocity(idlib::zero<Ego::Vector3f>());

                    // Play sound effect
                    if (pchr->isPlayer()) {
                        audioSystem.playSoundFull(audioSystem.getGlobalSound(GSND_PITFALL));
                    }
                    else {
                        audioSystem.playSound(pchr->getPosition(), audioSystem.getGlobalSound(GSND_PITFALL));
                    }

                    // Do some damage (same as damage tile)
                    applyTerrainDamage(damageable, damageTile.amount, static_cast<DamageType>(damageTile.damagetype),
                                       terrainDamageAttribution(objectHandler, pchr->getAIBumped()), false);
                }
            }
        }
    }
}

void GameModule::enablePitsTeleport(const Ego::Vector3f &location)
{
    _pits.enableTeleport(location);
}

void GameModule::enablePitsKill()
{
    _pits.enableKill();
}

void GameModule::update()
{
    updateModuleServices();
    updateModuleSimulation();
    finalizeModuleUpdate();
}

void GameModule::updateModuleServices()
{
    //status text for player stats
    MainLoop::check_stats();

    //Check abilities of all local players
    MainLoop::updateLocalStats();

    // keep the mpdfx lists up-to-date. No calculation is done unless one
    // of the mpdfx values was changed during the last update
    _mesh->_fxlists.synch(_mesh->_tmem, false);

    //Rebuild the quadtree for fast object lookup
    _gameObjects.updateQuadTree(0.0f, 0.0f, _mesh->_info.getTileCountX() * Info<float>::Grid::Size(),
                                            _mesh->_info.getTileCountY() * Info<float>::Grid::Size());

    _runtime.audioSystem().update();
    _runtime.billboardSystem().update();
    _animatedTilesState.update();
    getWater().update();
    module_update::updateDamageTiles(_gameObjects, *_mesh, _damageTile,
                                     _runtime.particleHandler(), _runtime.worldUpdateCount());
    _pits.update(_gameObjects, _runtime.particleHandler(), _runtime.audioSystem(), _damageTile);
    _weatherState.update(_playerList, _runtime.particleHandler(), *this);
    module_update::checkPassageMusic(_playerList, _passages, _runtime.audioSystem());
}

void GameModule::updateModuleSimulation()
{
    // Run AI after the first update frame, matching the legacy gate.
    if (_runtime.worldUpdateCount() > 0)
    {
        MainLoop::let_all_characters_think();
        MainLoop::readPlayerInput();
    }

    //Reset some profiling counters
    chr_stoppedby_tests = 0;
    chr_pressure_tests  = 0;

    module_update::updateAllObjects(_gameObjects, _runtime.worldUpdateCount(),
                                    _runtime.characterStatClock());
    _runtime.particleHandler().updateAllParticles();
    MainLoop::move_all_objects();
    Ego::Physics::CollisionSystem::get().update();
}

void GameModule::finalizeModuleUpdate()
{
    //Camera movement
    _runtime.cameraSystem().updateAll(_mesh.get());

    //Increment update frame counter
    _runtime.worldUpdateCount()++;
}
