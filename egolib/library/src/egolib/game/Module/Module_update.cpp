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
/// @brief GameModule per-frame services, simulation, and terrain update helpers.

#include "egolib/game/Module/Module_internal.h"

void GameModule::checkPassageMusic()
{
    // Look at each player
    for (const std::shared_ptr<Ego::Player> &player : _playerList)
    {
        const std::shared_ptr<Object> &pchr = player->getObject();
        if (!pchr || pchr->isTerminated()) continue;

        if (!pchr->isAlive()) continue;

        // Don't do items in hands or inventory.
        if (pchr->isBeingHeld()) continue;

        //Loop through every passage
        for (const std::shared_ptr<Passage>& passage : _passages)
        {
            if (passage->checkPassageMusic(pchr))
            {
                return;
            }
        }
    }
}

void GameModule::updateAllObjects()
{
    const uint32_t currentUpdateFrame = worldUpdateCount();

    for (const std::shared_ptr<Object> &object : getObjectHandler().iterator())
    {
        //Skip terminated objects
        if (object->isTerminated()) {
            continue;
        }

        //Update object logic
        object->update();

        //Update model animation
        object->inst.updateAnimation();

        //Check if this object should be poofed (destroyed)
        bool timeOut = (object->ai.poof_time > 0) && (object->ai.poof_time <= static_cast<int32_t>(currentUpdateFrame));
        if (timeOut) {
            object->requestTerminate();
        }
    }

    // Reset the clock
    if (characterStatClock() >= ONESECOND) {
        characterStatClock() -= ONESECOND;
    }
}

void GameModule::updatePits()
{
    //Are pits enabled?
    if (!_pitsKill && !_pitsTeleport) {
        return;
    }

    //Decrease the timer
    if (_pitsClock > 0) {
        _pitsClock--;
    }

    if (0 == _pitsClock)
    {
        //Reset timer
        _pitsClock = PIT_CLOCK_RATE;

        // Kill any particles that fell in a pit, if they die in water...
        for (const std::shared_ptr<Ego::Particle> &particle : ParticleHandler::get().iterator()) {
            if (particle->getPosZ() < PITDEPTH && particle->getProfile()->end_water)
            {
                particle->requestTerminate();
            }
        }

        // Kill or teleport any characters that fell in a pit...
        for (const std::shared_ptr<Object> &pchr : _gameObjects.iterator()) {
            // Is it a valid character?
            if (pchr->isInvincible() || !pchr->isAlive()) continue;
            if (pchr->isBeingHeld()) continue;

            // Do we kill it?
            if (_pitsKill && pchr->getPosZ() < PITDEPTH)
            {
                // Got one!
                pchr->kill(Object::INVALID_OBJECT, false);
                pchr->setVelocity({0.0f, 0.0f, pchr->getVelocity().z()});
            }

            // Do we teleport it?
            else if (_pitsTeleport && pchr->getPosZ() < PITDEPTH * 4)
            {
                // Teleport them back to a "safe" spot
                if (!pchr->teleport(_pitsTeleportPos, Facing(pchr->ori.facing_z))) {
                    // Kill it instead
                    pchr->kill(Object::INVALID_OBJECT, false);
                    pchr->setVelocity({0.0f, 0.0f, pchr->getVelocity().z()});
                }
                else {
                    // Stop movement
                    pchr->setVelocity(idlib::zero<Ego::Vector3f>());

                    // Play sound effect
                    if (pchr->isPlayer()) {
                        AudioSystem::get().playSoundFull(AudioSystem::get().getGlobalSound(GSND_PITFALL));
                    }
                    else {
                        AudioSystem::get().playSound(pchr->getPosition(), AudioSystem::get().getGlobalSound(GSND_PITFALL));
                    }

                    // Do some damage (same as damage tile)
                    pchr->damage(ATK_BEHIND, _damageTile.amount, static_cast<DamageType>(_damageTile.damagetype), Team::TEAM_DAMAGE,
                                 _gameObjects[pchr->ai.getBumped()], true, false, false);
                }
            }
        }
    }
}

void GameModule::enablePitsTeleport(const Ego::Vector3f &location)
{
    _pitsTeleportPos = location;
    _pitsTeleport = true;
    _pitsKill = false;
}

void GameModule::enablePitsKill()
{
    _pitsTeleport = false;
    _pitsKill = true;
}

void GameModule::updateDamageTiles()
{
    const uint32_t currentUpdateFrame = worldUpdateCount();

    // do the damage tile stuff
    for (const std::shared_ptr<Object> &pchr : _gameObjects.iterator()) {
        // if the object is not really in the game, do nothing
        if (pchr->isHidden() || !pchr->isAlive()) continue;

        // if you are being held by something, you are protected
        if (pchr->isInsideInventory()) continue;

        // are we on a damage tile?
        if (!_mesh->grid_is_valid(pchr->getTile())) continue;
        if (0 == _mesh->test_fx(pchr->getTile(), MAPFX_DAMAGE)) continue;

        // are we low enough?
        if (pchr->getPosZ() > pchr->getObjectPhysics().getGroundElevation() + DAMAGERAISE) continue;

        // allow reaffirming damage to things like torches, even if they are being held,
        // but make the tolerance closer so that books won't burn so easily
        if (!pchr->isBeingHeld() || pchr->getPosZ() < pchr->getObjectPhysics().getGroundElevation() + DAMAGERAISE)
        {
            if (pchr->getReaffirmDamageType() == _damageTile.damagetype)
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
        if (pchr->isInvincible()) continue;

        if (0 == pchr->getDamageTimer())
        {
            int actual_damage = pchr->damage(ATK_BEHIND, _damageTile.amount,
                                             static_cast<DamageType>(_damageTile.damagetype),
                                             Team::TEAM_DAMAGE, nullptr, true, false, false);

            pchr->setDamageTimer(DAMAGETILETIME);

            if ((actual_damage > 0) && (LocalParticleProfileRef::Invalid != _damageTile.part_gpip) && 0 == (currentUpdateFrame & _damageTile.partand)) {
                ParticleHandler::get().spawnGlobalParticle(pchr->getPosition(), ATK_FRONT, _damageTile.part_gpip, 0);
            }
        }
    }
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

    AudioSystem::get().update();
    GFX::get().getBillboardSystem().update();
    _animatedTilesState.update();
    getWater().update();
    updateDamageTiles();
    updatePits();
    _weatherState.update();
    checkPassageMusic();
}

void GameModule::updateModuleSimulation()
{
    // Run AI after the first update frame, matching the legacy gate.
    if (worldUpdateCount() > 0)
    {
        MainLoop::let_all_characters_think();
        MainLoop::readPlayerInput();
    }

    //Reset some profiling counters
    chr_stoppedby_tests = 0;
    chr_pressure_tests  = 0;

    updateAllObjects();
    ParticleHandler::get().updateAllParticles();
    MainLoop::move_all_objects();
    Ego::Physics::CollisionSystem::get().update();
}

void GameModule::finalizeModuleUpdate()
{
    //Camera movement
    CameraSystem::get().updateAll(_mesh.get());

    //Increment update frame counter
    worldUpdateCount()++;
}
