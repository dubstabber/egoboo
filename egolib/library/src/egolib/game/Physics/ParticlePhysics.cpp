#include "egolib/game/Physics/ParticlePhysics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() object/team access (the entity-world seam)
#include "egolib/Physics/PhysicalConstants.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"    // activeCollisionWorld() terrain queries (the mesh-query seam)
#include "egolib/Physics/MeshLookupTables.hpp"   // g_meshLookupTables
#include "egolib/FileFormats/map_fx.hpp"          // TWIST_FLAT, MAPFX_SLIPPY (was via Module.hpp -> mesh.h)
#include "egolib/game/CharacterMatrix.h"
#include "egolib/game/Physics/ParticlePhysics_internal.h"

namespace Ego
{
namespace Physics
{

ParticlePhysics::ParticlePhysics(Ego::Particle &particle) :
	_particle(particle)
{
	//ctor
}

void ParticlePhysics::updatePhysics()
{
    // if the particle is hidden it is frozen in time. do nothing.
    if (_particle.isTerminated() || _particle.isHidden()) {
    	return;
    }

    // save the acceleration from the last time-step
    _particle.enviro.acc = _particle.getVelocity() - _particle.getOldVelocity();

    // determine the actual velocity for attached particles
    if (_particle.isAttached()) {
        _particle.setVelocity(_particle.getPosition() - _particle.getOldPosition());
    }

    // Store particle's old location
    _particle.setOldPosition(_particle.getPosition());
    _particle.setOldVelocity(_particle.getVelocity());

    // what is the local environment like?
    updateEnviroment();

    // do friction with the floor before voluntary motion
    updateFloorFriction();

    updateHoming();

    //Update gravitational pull of particle (if any)
    updateGravity();

    //Different physics depending if we are attached to an Object or not
    if (_particle.isAttached()) {
        return updateAttached();
    }
    else {
	    updateMovement();
    }
}

void ParticlePhysics::updateHoming()
{
    // is the particle a homing type?
    if (!_particle.getProfile()->homing) return;

    // the particle update function is supposed to turn homing off if the particle looses its target
    if (!_particle.isHoming()) return;

    // the _particle.isHoming() variable is supposed to track the following, but it could have lost synch by this point
    if (_particle.isAttached() || !_particle.hasValidTarget()) return;

    // grab a pointer to the target
    Object* ptarget = objectWorld().getObjectHandler().get(_particle.getTargetID());
    if (ptarget == nullptr) return;

    const IPhysical& targetPhysical = physical(*ptarget);

    Vector3f vdiff = ptarget->getPosition() - _particle.getPosition();
    vdiff.z() += targetPhysical.getCurrentBump().height * 0.5f;

    float min_length = 2 * 5 * 256 * (FLOAT_TO_FP8(objectWorld().getObjectHandler().get(_particle.owner_ref)->getAttribute(Ego::Attribute::INTELLECT)) / (float)PERFECTBIG);

    // make a little incertainty about the target
    float uncertainty = 256.0f * (1.0f - FLOAT_TO_FP8(objectWorld().getObjectHandler().get(_particle.owner_ref)->getAttribute(Ego::Attribute::INTELLECT)) / (float)PERFECTBIG);

    Vector3f vdither;
    int ival;

    ival = Random::next(std::numeric_limits<uint16_t>::max());
    vdither.x() = (((float)ival / 0x8000) - 1.0f)  * uncertainty;

    ival = Random::next(std::numeric_limits<uint16_t>::max());
    vdither.y() = (((float)ival / 0x8000) - 1.0f)  * uncertainty;

    ival = Random::next(std::numeric_limits<uint16_t>::max());
    vdither.z() = (((float)ival / 0x8000) - 1.0f)  * uncertainty;

    // take away any dithering along the direction of motion of the particle
    float vlen = idlib::squared_euclidean_norm(_particle.getVelocity());
    if (vlen > 0.0f)
    {
        float vdot = dot(vdither, _particle.getVelocity()) / vlen;

        vdither -= vdiff * (vdot/vlen);
    }

    // add in the dithering
    vdiff += vdither;

    // Make sure that vdiff doesn't ever get too small.
    // That just makes the particle slooooowww down when it approaches the target.
    // Do a real kludge here. this should be a lot faster than a square root, but ...
    vlen = idlib::manhattan_norm(vdiff);
    if (vlen > FLT_EPSILON)
    {
        float factor = min_length / vlen;

        vdiff *= factor;
    }

    _particle.setVelocity((_particle.getVelocity() + vdiff * _particle.getProfile()->homingaccel) * _particle.getProfile()->homingfriction);
}

void ParticlePhysics::updateFloorFriction()
{
    Vector3f vup;

    Ego::prt_environment_t *penviro = &(_particle.enviro);

    // if the particle is homing in on something, ignore friction
    if (_particle.isHoming()) return;

    // limit floor friction effects to solid objects
    if (SPRITE_SOLID != _particle.type) return;

    // figure out the acceleration due to the current "floor"
    Vector3f floor_acc = idlib::zero<Vector3f>();
    float temp_friction_xy = 1.0f;

    const std::shared_ptr<Object> &platform = objectWorld().getObjectHandler()[_particle.onwhichplatform_ref];
    if (platform)
    {
        temp_friction_xy = 1.0f - PLATFORM_STICKINESS;

        floor_acc = platform->getVelocity() - platform->getOldVelocity();

        chr_getMatUp(platform.get(), vup);
    }
    else
    {
        //Is the floor slippery?
        if (collisionWorld().gridIsValid(_particle.getTile()) && penviro->is_slippy)
        {
            // It's slippy all right...
            temp_friction_xy = 1.0f - Ego::Physics::g_environment.slippyfriction;
        }
        else 
        {
            temp_friction_xy = 1.0f - Ego::Physics::g_environment.noslipfriction;
        }


        floor_acc = -_particle.getVelocity();

        //Is floor flat or sloped?
        if (TWIST_FLAT == penviro->twist)
        {
            vup = Vector3f(0.0f, 0.0f, 1.0f);
        }
        else
        {
            vup = g_meshLookupTables.twist_nrm[penviro->twist];
        }
    }

    // the first guess about the floor friction
    Vector3f fric_floor = floor_acc * (1.0f - penviro->zlerp) * temp_friction_xy * penviro->traction;

    // the total "friction" due to the floor
    Vector3f fric = fric_floor + penviro->acc;

    //---- limit the friction to whatever is horizontal to the mesh
    if (std::abs(vup.z()) > 0.9999f)
    {
        floor_acc.z() = 0.0f;
        fric.z() = 0.0f;
    }
    else
    {
        float ftmp = dot(floor_acc, vup);
        floor_acc -= vup * ftmp;

        ftmp = dot(fric, vup);
        fric -= vup * ftmp;
    }

    // test to see if the particle has any more friction left?
    penviro->is_slipping = idlib::manhattan_norm(fric) > penviro->friction_hrz;
    if (penviro->is_slipping)
    {
        penviro->traction *= 0.5f;
        temp_friction_xy = std::sqrt(temp_friction_xy);

        fric_floor = floor_acc * ((1.0f - penviro->zlerp) * (1.0f - temp_friction_xy) * penviro->traction);
        float ftmp = dot(fric_floor, vup);
        fric_floor -= vup * ftmp;
    }

    // Apply the floor friction
    _particle.setVelocity(_particle.getVelocity() + fric_floor * 0.25f);
}

void ParticlePhysics::updateGravity()
{
    //Only do world gravity for solid particles
    if (!_particle.no_gravity && _particle.type == SPRITE_SOLID && !_particle.isHoming() && !_particle.isAttached()) {
        _particle.setVelocity({_particle.getVelocity().x(), _particle.getVelocity().y(),
                               _particle.getVelocity().z() + Ego::Physics::g_environment.gravity *
                               Ego::Physics::g_environment.airfriction});
    }

    //Some particles can have a special gravity field that pulls or pushes
    if(_particle.getProfile()->getGravityPull() != 0.0f) {
        float pullDistance = _particle.getProfile()->bump_size * 3.0f;
        const auto &particleTeam = objectWorld().getTeamList()[_particle.team];

        //Pull all nearby objects
        std::vector<ObjectRef> affectedObjectRefs;
        objectWorld().getObjectHandler().findObjectRefs(_particle.getPosX(), _particle.getPosY(), pullDistance, affectedObjectRefs, false);
        for (const ObjectRef& objectRef : affectedObjectRefs)
        {
            Object* object = objectWorld().getObjectHandler().get(objectRef);
            if (object == nullptr || object->isTerminated()) {
                continue;
            }

            //Do not affect the object we are attached to
            if(_particle.getAttachedObjectID() == objectRef) continue;

            //Allow friendly fire?
            if(!_particle.getProfile()->hateonly && !particleTeam.hatesTeam(object->getTeam())) continue;

            //Skip objects that cannot collide
            if(!object->canCollide()) continue;

            const Vector3f pull = _particle.getPosition() - object->getPosition();
            const float distance = idlib::squared_euclidean_norm(pull);
            if(distance > 10.0f) {
                object->setVelocity(object->getVelocity() + (pull * _particle.getProfile()->getGravityPull()) * (1.0f/distance));
            }
        }

        //Pull all nearby particles
        for(const std::shared_ptr<Ego::Particle> &particle : activeParticleHandler().iterator())
        {
            //Don't to terminated particles
            if(particle->isTerminated()) continue;

            //Skip attached particles
            if(particle->isAttached()) continue;

            //Do not affect ourselves!
            if(particle.get() == &_particle) continue;

            //Skip those that are not affected by gravity
            if(particle->no_gravity) continue;

            //Skip particles that cannot collide with anything
            if(!particle->canCollide()) continue;

            const Vector3f pull = _particle.getPosition() - particle->getPosition();
            const float distance = idlib::squared_euclidean_norm(pull);
            if(distance > 10.0f) {
                particle->setVelocity(particle->getVelocity() + (pull * _particle.getProfile()->getGravityPull()) * (1.0f/distance));
            }
        }
    }
}

void ParticlePhysics::updateEnviroment()
{
    float loc_level = 0.0f;

    Ego::prt_environment_t *penviro = &(_particle.enviro);

    const std::shared_ptr<Object>& platform = objectWorld().getObjectHandler()[_particle.onwhichplatform_ref];

    //---- character "floor" level
    penviro->floor_level = collisionWorld().getElevation(Vector2f(_particle.getPosX(), _particle.getPosY()));
    penviro->level = penviro->floor_level;

    //---- The actual level of the characer.
    //     Estimate platform attachment from whatever is in the onwhichplatform_ref variable from the
    //     last loop
    loc_level = penviro->floor_level;
    if (platform)
    {
        const IPhysical& platformPhysical = physical(*platform);
        loc_level = std::max(penviro->floor_level, platform->getPosZ() + platformPhysical.getMinCollisionVolume()._maxs[OCT_Z]);
    }
    _particle.setElevation(loc_level);

    //---- the "twist" of the floor
    penviro->twist = TWIST_FLAT;
    Index1D itile = Index1D::Invalid;
    if (platform)
    {
        // this only works for 1 level of attachment
        itile = platform->getTile();
    }
    else
    {
        itile = _particle.getTile();
    }

    penviro->twist = collisionWorld().getTwist(itile);

    // the "watery-ness" of whatever water might be here
    penviro->is_watery = collisionWorld().isWater() && penviro->inwater;
    penviro->is_slippy = !penviro->is_watery && (0 != collisionWorld().testFX(_particle.getTile(), MAPFX_SLIPPY));

    //---- traction
    penviro->traction = 1.0f;
    if (_particle.isHoming())
    {
        // any traction factor here
        /* traction = ??; */
    }
    else if (platform)
    {
        // in case the platform is tilted
        // unfortunately platforms are attached in the collision section
        // which occurs after the movement section.

        Vector3f platform_up;

        chr_getMatUp(platform.get(), platform_up);
        platform_up = normalize(platform_up).get_vector();

        penviro->traction = std::abs(platform_up.z()) * (1.0f - penviro->zlerp) + 0.25f * penviro->zlerp;

        if (penviro->is_slippy)
        {
            penviro->traction /= Ego::Physics::g_environment.hillslide * (1.0f - penviro->zlerp) + 1.0f * penviro->zlerp;
        }
    }
    else if (collisionWorld().gridIsValid(_particle.getTile()))
    {
        penviro->traction = std::abs(g_meshLookupTables.twist_nrm[penviro->twist].z()) * (1.0f - penviro->zlerp) + 0.25f * penviro->zlerp;

        if (penviro->is_slippy)
        {
            penviro->traction /= Ego::Physics::g_environment.hillslide * (1.0f - penviro->zlerp) + 1.0f * penviro->zlerp;
        }
    }

    //---- the friction of the fluid we are in
    if (penviro->is_watery)
    {
        penviro->fluid_friction_vrt = Ego::Physics::g_environment.waterfriction;
        penviro->fluid_friction_hrz = Ego::Physics::g_environment.waterfriction;
    }
    else
    {
        penviro->fluid_friction_hrz = Ego::Physics::g_environment.airfriction;       // like real-life air friction
        penviro->fluid_friction_vrt = Ego::Physics::g_environment.airfriction;
    }

    //---- friction
    penviro->friction_hrz = 1.0f;
    if (!_particle.isHoming())
    {
        // Make the characters slide
        float temp_friction_xy = Ego::Physics::g_environment.noslipfriction;
        if (collisionWorld().gridIsValid(_particle.getTile()) && penviro->is_slippy)
        {
            // It's slippy all right...
            temp_friction_xy = Ego::Physics::g_environment.slippyfriction;
        }

        penviro->friction_hrz = penviro->zlerp * 1.0f + (1.0f - penviro->zlerp) * temp_friction_xy;
    }
}

void ParticlePhysics::detachFromPlatform()
{
    // undo the attachment
    _particle.onwhichplatform_ref    = ObjectRef::Invalid;
    _particle.onwhichplatform_update = 0;
    _particle.targetplatform_ref     = ObjectRef::Invalid;
    _particle.targetplatform_level   = -1e32;

    // get the correct particle environment
    updateEnviroment();
}

} //Physics
} //Ego
