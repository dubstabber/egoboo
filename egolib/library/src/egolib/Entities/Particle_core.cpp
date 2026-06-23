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

/// @file egolib/Entities/Particle_core.cpp
/// @brief Core Particle helpers, queries, and physics wrappers.

#include "egolib/Entities/Particle_internal.h"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"

namespace Ego
{

namespace
{

Ego::Physics::ICollisionWorld& collisionWorld()
{
    return Ego::Physics::activeCollisionWorld();
}

IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

IParticleHandler& particleHandler()
{
    return activeParticleHandler();
}
}

prt_environment_t::prt_environment_t() :
    twist(0),
    floor_level(0.0f),
    level(0.0f),
    zlerp(0.0f),
    //
    adj_level(0.0f),
    adj_floor(0.0f),
    //
    is_slipping(false),
    is_slippy(false),
    is_watery(false),
    air_friction(0.0f),
    fluid_friction_hrz(0.0f), fluid_friction_vrt(0.0f),
    friction_hrz(0.0f),
    traction(0.0f),
    //
    inwater(false),
    acc(idlib::zero<Vector3f>())
{}

void prt_environment_t::reset()
{
    *this = prt_environment_t();
}

const std::shared_ptr<Particle> Particle::INVALID_PARTICLE = nullptr;

Particle::Particle() :
    _particleID(),
    _particlePhysics(*this),
    _collidedObjects(),
    _attachedTo(),
    _particleProfileID(INVALID_PIP_REF),
    _particleProfile(nullptr),
    _isTerminated(true),
    _target(),
    _spawnerProfile(),
    _isHoming(false)
{
    reset(ParticleRef::Invalid);
}

bool Particle::isAttached() const
{
    return activeModule().getObjectHandler().exists(_attachedTo);
}

BIT_FIELD Particle::hit_wall(const Vector3f& pos, Vector2f& nrm, float *pressure)
{
    BIT_FIELD stoppedby = MAPFX_IMPASS;
    if (0 != getProfile()->bump_money) SET_BIT(stoppedby, MAPFX_WALL);

    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;
    return activeModule().getMeshPointer()->hit_wall(pos, 0.0f, stoppedby, nrm, pressure);
}

BIT_FIELD Particle::hit_wall(const Vector3f& pos, Vector2f& nrm, float *pressure, mesh_wall_data_t& data)
{
    BIT_FIELD stoppedby = MAPFX_IMPASS;
    if (0 != getProfile()->bump_money) SET_BIT(stoppedby, MAPFX_WALL);

    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;
    return activeModule().getMeshPointer()->hit_wall(pos, 0.0f, stoppedby, nrm, pressure, data);
}

BIT_FIELD Particle::test_wall(const Vector3f& pos)
{
    BIT_FIELD stoppedby = MAPFX_IMPASS;
    if (0 != getProfile()->bump_money) SET_BIT(stoppedby, MAPFX_WALL);

    // Do the wall test.
    g_meshStats.mpdfxTests = 0;
    g_meshStats.boundTests = 0;
    g_meshStats.pressureTests = 0;
    return activeModule().getMeshPointer()->test_wall(pos, 0.0f, stoppedby);
}

const std::shared_ptr<ParticleProfile>& Particle::getProfile() const
{
    return _particleProfile;
}

float Particle::getScale() const
{
    float scale = 0.25f;

    // set some particle dependent properties
    switch (type)
    {
        case SPRITE_SOLID: scale *= 0.9384f; break;
        case SPRITE_ALPHA: scale *= 0.9353f; break;
        case SPRITE_LIGHT: scale *= 1.5912f; break;
    }

    return scale;
}

void Particle::setSize(int setSize)
{
    // set the graphical size
    this->size = Ego::Math::constrain(setSize, 0, 0xFFFF);

    // set the bumper size, if available
    if (0 == bump_size_stt)
    {
        // make the particle non-interacting if the initial bumper size was 0
        bump_real.size = 0;
        bump_padded.size = 0;
    }
    else
    {
        const float realSize = FP8_TO_FLOAT(size) * getScale();

        if (0.0f == bump_real.size || 0.0f == size)
        {
            // just set the size, assuming a spherical particle
            bump_real.size = realSize;
            bump_real.size_big = realSize * idlib::sqrt_two<float>();
            bump_real.height = realSize;
        }
        else
        {
            float mag = realSize / bump_real.size;

            // resize all dimensions equally
            bump_real.size *= mag;
            bump_real.size_big *= mag;
            bump_real.height *= mag;
        }

        // make sure that the virtual bumper size is at least as big as what is in the pip file
        bump_padded.size     = std::max<float>(bump_real.size, getProfile()->bump_size);
        bump_padded.size_big = std::max<float>(bump_real.size_big, getProfile()->bump_size * idlib::sqrt_two<float>());
        bump_padded.height   = std::max<float>(bump_real.height, getProfile()->bump_height);
    }

    // set the real size of the particle
    prt_min_cv.assign(bump_real);

    // use the padded bumper to figure out the chr_max_cv
    prt_max_cv.assign(bump_padded);
}

void Particle::requestTerminate()
{
    _isTerminated = true;
}

void Particle::setElevation(const float level)
{
    enviro.level = level;

    float loc_height = bump_real.height;

    enviro.adj_level = enviro.level;
    enviro.adj_floor = enviro.floor_level;

    enviro.adj_level += loc_height;
    enviro.adj_floor += loc_height;

    // set the zlerp after we have done everything to the particle's level we care to
    enviro.zlerp = (getPosZ() - enviro.adj_level) / PLATTOLERANCE;
    enviro.zlerp = Ego::Math::constrain(enviro.zlerp, 0.0f, 1.0f);
}

bool Particle::isHidden() const
{
    const std::shared_ptr<Object>& attachedToObject = activeModule().getObjectHandler()[_attachedTo];

    if(!attachedToObject) {
        return false;
    }

    return attachedToObject->isHidden();
}

bool Particle::hasValidTarget() const
{
    return activeModule().getObjectHandler().get(_target) != nullptr;
}

PIP_REF Particle::getProfileID() const
{
    return _particleProfileID;
}

void Particle::playSound(int8_t sound)
{
    //Invalid sound?
    if(sound < 0) {
        return;
    }

    //If we were spawned by an Object, then use that Object's sound pool
    const std::shared_ptr<ObjectProfile> &profile = activeProfileSystem().getProfile(_spawnerProfile);
    if (profile) {
        audioSystem().playSound(getPosition(), profile->getSoundID(sound));
    }

    //Else we are a global particle and use global particle sounds
    else if (sound >= 0 && sound < GSND_COUNT)
    {
        GlobalSound globalSound = static_cast<GlobalSound>(sound);
        audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(globalSound));
    }
}

bool Particle::isOverWater() const
{
    return (0 != collisionWorld().testFX(getTile(), MAPFX_WATER));
}

void Particle::setTarget(const ObjectRef target)
{
    _target = target;
}

ParticleRef Particle::getParticleID() const
{
    return _particleID;
}

void Particle::setHoming(bool homing)
{
    _isHoming = homing;
}

bool Particle::hasCollided(ObjectRef objectRef) const
{
    for(const ObjectRef ref : _collidedObjects)
    {
        if(ref == objectRef)
        {
            return true;
        }
    }
    return false;
}

void Particle::addCollision(ObjectRef objectRef)
{
    if(isTerminated()) return;
    if(objectRef == ObjectRef::Invalid) return;
    _collidedObjects.push_front(objectRef);
}

bool Particle::isEternal() const
{
    return is_eternal;
}

bool Particle::canCollide() const
{
    if(isTerminated()) {
        return false;
    }

    if(isHidden()) {
        return false;
    }

    //Particle is destroyed on any collision?
    if(getProfile()->end_bump || getProfile()->end_ground) {
        return true;
    }

    //Has collision size?
    /// @todo this is a stopgap solution, figure out if this is the correct place or
    ///       we need to fix the loop in fill_interaction_list instead
    if(getProfile()->bump_height <= 0 && getProfile()->bump_size <= 0) {
        return false;
    }

    // Each one of these tests allows one MORE reason to include the particle, not one less.
    // Removed bump particles. We have another loop that can detect these, and there
    // is no reason to fill up the BSP with particles like coins.

    // Make this optional? Is there any reason to fail if the particle has no profile reference?
    if (getProfile()->spawnenchant)
    {
        if(activeProfileSystem().isEnchantProfileLoaded(activeProfileSystem().getProfile(getSpawnerProfile())->getEnchantRef())) {
            return true;
        }
    }

    // any possible damage?
    if((std::abs(damage.base) + std::abs(damage.rand)) > 0) {
        return true;
    }

    // the other possible status effects
    // do not require damage
    if((0 != getProfile()->grogTime) || (0 != getProfile()->dazeTime) || ( 0 != getProfile()->lifeDrain ) || (0 != getProfile()->manaDrain)) {
        return true;
    }

    //Causes special effect? these are not implemented yet
    //if(getProfile()->cause_pancake || getProfile()->cause_roll) return true;

    //Can push?
    if(getProfile()->allowpush) {
        return true;
    }

    //No valid interactions
    return false;
}

Ego::Physics::ParticlePhysics& Particle::getParticlePhysics()
{
    return _particlePhysics;
}

ObjectRef Particle::getOwner(int depth)
{
    // be careful because this can be recursive
    if (depth > static_cast<int>(particleHandler().getCount()) - static_cast<int>(particleHandler().getFreeCount()))
    {
        return ObjectRef::Invalid;
    }

    if(isTerminated()) {
        return ObjectRef::Invalid;
    }

    ObjectRef iowner = ObjectRef::Invalid;
    if (activeModule().getObjectHandler().exists(owner_ref))
    {
        iowner = owner_ref;
    }
    else
    {
        // make a check for a stupid looping structure...
        // cannot be sure you could never get a loop, though

        if (!particleHandler()[parent_ref])
        {
            // make sure that a non valid parent_ref is marked as non-valid
            parent_ref = ParticleRef::Invalid;
        }
        else
        {
            // if a particle has been poofed, and another particle lives at that address,
            // it is possible that the pprt->parent_ref points to a valid particle that is
            // not the parent. Depending on how scrambled the list gets, there could actually
            // be looping structures. I have actually seen this, so don't laugh :)
            if (_particleID != parent_ref)
            {
                iowner = particleHandler()[parent_ref]->getOwner(depth + 1);
            }
        }
    }

    return iowner;
}

} // namespace Ego
