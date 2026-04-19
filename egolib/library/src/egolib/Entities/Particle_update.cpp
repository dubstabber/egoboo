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

/// @file egolib/Entities/Particle_update.cpp
/// @brief Particle update-loop and animation/environment helpers.

#include "egolib/Entities/Particle_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace Ego
{

void Particle::update()
{
    //Should never happen
    if(isTerminated()) {
        return;
    }

    // If the particle is hidden, there is nothing else to do.
    if(isHidden()) {
        return;
    }

    //Clear invalid attachements incase Object have been removed from the game
    if(_attachedTo != ObjectRef::Invalid) {
        if(isAttached()) {
            //keep particles with whomever they are attached to
            placeAtVertex(getAttachedObject(), attachedto_vrt_off);
        }
        else {
            _attachedTo = ObjectRef::Invalid;
            requestTerminate();
            return;
        }
    }

    // Determine if a "homing" particle still has something to "home":
    // If its homing (according to its profile), is not attached to an object (yet),
    // and a target exists, then the particle will "home" that target.
    if(_isHoming) {
        _isHoming = !isAttached() && hasValidTarget();
    }

    updateDynamicLighting();

    // Update the particle animation.
    updateAnimation();
    if(isTerminated()) {
        return; //destroyed by end of animation
    }

    // Update the particle interaction with water
    updateWater();
    if(isTerminated()) {
        return; //destroyed by water
    }

    //Spawn other particles
    updateContinuousSpawning();

    //Damage whomever we are attached to
    updateAttachedDamage();

    // down the remaining lifetime of the particle
    if (!is_eternal)
    {
        if (lifetime_remaining > 0) {
            lifetime_remaining--;
        }
        else {
            //end of life
            requestTerminate();
        }
    }
}

void Particle::updateWater()
{
    bool inwater = (getPosZ() < activeModule().getWater()._surface_level) && isOverWater();

    if (inwater && activeModule().getWater()._is_water && getProfile()->end_water)
    {
        // Check for disaffirming character
        if (isAttached() && owner_ref == _attachedTo)
        {
            // Disaffirm the whole character
            disaffirm_attached_particles(_attachedTo);
        }
        else
        {
            // destroy the particle
            requestTerminate();
            return;
        }
    }
    else if (inwater)
    {
        bool  spawn_valid = false;
        LocalParticleProfileRef global_pip_index;
        Vector3f vtmp = Vector3f(getPosX(), getPosY(), activeModule().getWater()._surface_level);

        if (ObjectRef::Invalid == owner_ref && (PIP_SPLASH == getProfileID() || PIP_RIPPLE == getProfileID()))
        {
            /* do not spawn anything for a splash or a ripple */
            spawn_valid = false;
        }
        else
        {
            if (!enviro.inwater)
            {
                if (SPRITE_SOLID == type)
                {
                    global_pip_index = LocalParticleProfileRef(PIP_SPLASH);
                }
                else
                {
                    global_pip_index = LocalParticleProfileRef(PIP_RIPPLE);
                }
                spawn_valid = true;
            }
            else
            {
                if (SPRITE_SOLID == type && !isAttached())
                {
                    // only spawn ripples if you are touching the water surface!
                    if (getPosZ() + bump_real.height > activeModule().getWater()._surface_level && getPosZ() - bump_real.height < activeModule().getWater()._surface_level)
                    {
                        static constexpr int RIPPLEAND = 15;          ///< How often ripples spawn
                        if (0 == ((worldUpdateCount() + _particleID.get()) & (RIPPLEAND << 1)))
                        {
                            spawn_valid = true;
                            global_pip_index = LocalParticleProfileRef(PIP_RIPPLE);
                        }
                    }
                }
            }
        }

        if (spawn_valid)
        {
            // Splash for particles is just a ripple
            ParticleHandler::get().spawnGlobalParticle(vtmp, Facing(0), global_pip_index, 0);
        }

        enviro.inwater = true;
    }
    else
    {
        enviro.inwater = false;
    }
}

void Particle::updateAnimation()
{
    /// animate the particle

    bool image_overflow = false;
    long image_overflow_amount = 0;
    if (_image._offset >= _image._count)
    {
        // how did the image get here?
        image_overflow = true;

        // cast the integers to larger type to make sure there are no overflows
        image_overflow_amount = (long)_image._offset + (long)_image._add - (long)_image._count;
    }
    else
    {
        // the image is in the correct range
        if ((_image._count - _image._offset) > _image._add)
        {
            // the image will not overflow this update
            _image._offset = _image._offset + _image._add;
        }
        else
        {
            image_overflow = true;
            // cast the integers to larger type to make sure there are no overflows
            image_overflow_amount = (long)_image._offset + (long)_image._add - (long)_image._count;
        }
    }

    // what do you do about an image overflow?
    if (image_overflow)
    {
        //if (getProfile()->end_lastframe /*&& getProfile()->end_time > 0*/) //ZF> I don't think the second statement is needed
        //{
        //    // freeze it at the last frame
        //    _image._offset = std::max(0, _image._count - 1);
        //}
        //else
        {
            // the animation is looped. set the value to image_overflow_amount
            // so that we get the exact number of image updates called for
            _image._offset = image_overflow_amount;
        }
    }

    // rotate the particle
    rotate += Facing(rotate_add);

    // update the particle size
    if (0 != size_add)
    {
        // resize the paricle
        setSize(static_cast<int>(size) + size_add);
    }

    // spin the particle
    facing += Facing(getProfile()->facingadd);

    // frames_remaining refers to the number of animation updates, not the
    // number of frames displayed
    if (frames_remaining > 0)
    {
        frames_remaining--;
    }

    // the animation has terminated
    if (getProfile()->end_lastframe && 0 == frames_remaining)
    {
        //End of life
        requestTerminate();
    }
}

void Particle::updateDynamicLighting()
{
    // Change dyna light values
    if (dynalight.level > 0)
    {
        dynalight.level += getProfile()->dynalight.level_add;
        if (dynalight.level < 0) dynalight.level = 0;
    }
    else if (dynalight.level < 0)
    {
        // try to guess what should happen for negative lighting
        dynalight.level += getProfile()->dynalight.level_add;
        if (dynalight.level > 0) dynalight.level = 0;
    }
    else
    {
        dynalight.level += getProfile()->dynalight.level_add;
    }

    dynalight.falloff += getProfile()->dynalight.falloff_add;
}

size_t Particle::updateContinuousSpawning()
{
    size_t spawn_count = 0;

    if (getProfile()->contspawn._amount <= 0 || LocalParticleProfileRef::Invalid == getProfile()->contspawn._lpip)
    {
        return spawn_count;
    }

    //Are we ready to spawn yet?
    if (contspawn_timer > 0) {
        contspawn_timer--;
        return spawn_count;
    }

    //Optimization: Only spawn cosmetic sub-particles if we ourselves were rendered
    //This prevents a lot of cosmetic particles from spawning outside visible range
    const std::shared_ptr<ParticleProfile>& childProfile = EngineContext::get().profileSystem().getParticleProfile(getProfile()->contspawn._lpip.get());
    if(!childProfile->force && !inst.indolist) {

        //Is is something that spawns often? (often = at least once every 2 seconds)
        if(contspawn_timer < GameEngine::GAME_TARGET_UPS * 2) {

            //Don't spawn this particle
            return spawn_count;
        }
    }

    // reset the spawn timer
    contspawn_timer = getProfile()->contspawn._delay;

    Facing facingAdd = this->facing;
    for (size_t tnc = 0; tnc < getProfile()->contspawn._amount; tnc++)
    {
        std::shared_ptr<Ego::Particle> prt_child;
        if(_spawnerProfile == ObjectProfileRef::Invalid) {
            prt_child = ParticleHandler::get().spawnGlobalParticle(getPosition(), facingAdd, getProfile()->contspawn._lpip, tnc);
        }
        else {
            prt_child = ParticleHandler::get().spawnLocalParticle(getPosition(), facingAdd, ObjectProfileRef(_spawnerProfile), getProfile()->contspawn._lpip,
                                                                  ObjectRef::Invalid, GRIP_LAST, team, owner_ref, _particleID, tnc, _target);
        }

        if (prt_child)
        {
            //Keep count of how many were actually spawned
            spawn_count++;
        }

        facingAdd += Facing(getProfile()->contspawn._facingAdd);
    }

    return spawn_count;
}

} // namespace Ego
