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
#include "CollisionSystem.hpp"
#include "CollisionSystem_internal.h"            // do_chr_chr_collision() (moved to CollisionSystem_chr_chr.cpp)
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() object/team/update-tick access (the entity-world seam)
#include "egolib/Physics/physics.h"  // phys_expand_*/phys_estimate_*/phys_intersect_* + PhysicalConstants
#include "egolib/FileFormats/map_file.h"          // Info<float>::Grid::Size (was via Module.hpp -> mesh.h)

#include "particle_collision.h"

namespace Ego
{
namespace Physics
{

namespace
{
/// The entity world the collision step queries (object container), reached through the
/// lower-layer Ego::Entities::IObjectWorld seam rather than game/ (GameModule).
Ego::Entities::IObjectWorld& objectWorld()
{
    return Ego::Entities::activeObjectWorld();
}

/// The active world's update tick, reached through the lower-layer activeWorldUpdateCount() seam
/// (sibling of objectWorld()) rather than GameSessionContext.
uint32_t worldUpdateCount()
{
    return Ego::Entities::activeWorldUpdateCount();
}
}

CollisionSystem::CollisionSystem()
{

}


CollisionSystem::~CollisionSystem()
{

}

void CollisionSystem::update()
{
    ObjectHandler& objectHandler = objectWorld().getObjectHandler();

    // blank the accumulators
    for(const ObjectRef& objectRef : objectHandler.objectRefIterator())
    {
        Object* object = objectHandler.get(objectRef);
        if (object != nullptr) {
            object->phys.clear();
        }
    }
    for(const std::shared_ptr<Ego::Particle> &particle : activeParticleHandler().iterator())
    {
        particle->phys.clear();
    }

    updateObjectCollisions();
    updateParticleCollisions();

    // accumulate the accumulators
    for(const ObjectRef& objectRef : objectHandler.objectRefIterator())
    {
        Object* pchr = objectHandler.get(objectRef);
        if(pchr == nullptr || pchr->isTerminated()) {
            continue;
        }
        
        float tmpx, tmpy;
        bool position_updated = false;
        Vector3f max_apos;

		Vector3f tmp_pos = pchr->getPosition();

        // do the "integration" of the accumulated accelerations
        pchr->setVelocity(pchr->getVelocity() + pchr->phys.avel);

        // get a net displacement vector from aplat and acoll
        {
            // create a temporary apos_t
            apos_t  apos_tmp;

            // copy 1/2 of the data over
            apos_tmp = pchr->phys.aplat;

            // get the resultant apos_t
            apos_tmp.join(pchr->phys.acoll);

            // turn this into a vector
            apos_t::evaluate(apos_tmp, max_apos);
        }

        // limit the size of the displacement
        max_apos[kX] = Ego::Math::constrain( max_apos[kX], -Info<float>::Grid::Size(), Info<float>::Grid::Size());
        max_apos[kY] = Ego::Math::constrain( max_apos[kY], -Info<float>::Grid::Size(), Info<float>::Grid::Size());
        max_apos[kZ] = Ego::Math::constrain( max_apos[kZ], -Info<float>::Grid::Size(), Info<float>::Grid::Size());

        // do the "integration" on the position
        if (std::abs(max_apos[kX]) > 0.0f)
        {
            tmpx = tmp_pos[kX];
            tmp_pos[kX] += max_apos[kX];
            if ( EMPTY_BIT_FIELD != pchr->test_wall( tmp_pos ) )
            {
                // restore the old values
                tmp_pos[kX] = tmpx;
            }
            else
            {
                //pchr->vel[kX] += pchr->phys.apos_coll[kX] * bump_str;
                position_updated = true;
            }
        }

        if (std::abs(max_apos[kY]) > 0.0f)
        {
            tmpy = tmp_pos[kY];
            tmp_pos[kY] += max_apos[kY];
            if ( EMPTY_BIT_FIELD != pchr->test_wall( tmp_pos ) )
            {
                // restore the old values
                tmp_pos[kY] = tmpy;
            }
            else
            {
                //pchr->vel[kY] += pchr->phys.apos_coll[kY] * bump_str;
                position_updated = true;
            }
        }

        if (std::abs(max_apos[kZ]) > 0.0f)
        {
            tmp_pos[kZ] += max_apos[kZ];
            if ( tmp_pos[kZ] < pchr->getFloorElevation() )
            {
                // restore the old values
                tmp_pos[kZ] = pchr->getFloorElevation();
                if ( pchr->getVelocity().z() < 0 )
                {
                    pchr->setVelocity(pchr->getVelocity() +
                                      Vector3f(0.0f, 0.0f,
                                               -(1.0f + pchr->getProfile()->getBounciness()) * pchr->getVelocity().z()));
                }
                position_updated = true;
            }
            else
            {
                //pchr->vel[kZ] += pchr->phys.apos_coll[kZ] * bump_str;
                position_updated = true;
            }
        }

        if ( position_updated )
        {
            pchr->setPosition(tmp_pos);
        }
    }

    // accumulate the accumulators
    for(const std::shared_ptr<Ego::Particle> &particle : activeParticleHandler().iterator())
    {
        float tmpx, tmpy;
        bool position_updated = false;
        Vector3f max_apos;

        if(particle->isTerminated()) {
            continue;
        }

        Vector3f tmp_pos = particle->getPosition();

        // do the "integration" of the accumulated accelerations
        particle->setVelocity(particle->getVelocity() + particle->phys.avel);

        position_updated = false;

        // get a net displacement vector from aplat and acoll
        {
            // create a temporary apos_t
            apos_t  apos_tmp;

            // copy 1/2 of the data over
            apos_tmp = particle->phys.aplat;

            // get the resultant apos_t
            apos_tmp.join(particle->phys.acoll);

            // turn this into a vector
            apos_t::evaluate(apos_tmp, max_apos);
        }

        max_apos[kX] = Ego::Math::constrain( max_apos[kX], -Info<float>::Grid::Size(), Info<float>::Grid::Size());
        max_apos[kY] = Ego::Math::constrain( max_apos[kY], -Info<float>::Grid::Size(), Info<float>::Grid::Size());
        max_apos[kZ] = Ego::Math::constrain( max_apos[kZ], -Info<float>::Grid::Size(), Info<float>::Grid::Size());

        // do the "integration" on the position
        if (std::abs(max_apos[kX]) > 0.0f)
        {
            tmpx = tmp_pos[kX];
            tmp_pos[kX] += max_apos[kX];
            if ( EMPTY_BIT_FIELD != particle->test_wall( tmp_pos ) )
            {
                // restore the old values
                tmp_pos[kX] = tmpx;
            }
            else
            {
                //bdl.prt_ptr->vel[kX] += bdl.prt_ptr->phys.apos_coll[kX] * bump_str;
                position_updated = true;
            }
        }

        if (std::abs(max_apos[kY]) > 0.0f)
        {
            tmpy = tmp_pos[kY];
            tmp_pos[kY] += max_apos[kY];
            if ( EMPTY_BIT_FIELD != particle->test_wall( tmp_pos ) )
            {
                // restore the old values
                tmp_pos[kY] = tmpy;
            }
            else
            {
                //bdl.prt_ptr->vel[kY] += bdl.prt_ptr->phys.apos_coll[kY] * bump_str;
                position_updated = true;
            }
        }

        if (std::abs(max_apos[kZ]) > 0.0f)
        {
            tmp_pos[kZ] += max_apos[kZ];
            if ( tmp_pos[kZ] < particle->enviro.floor_level )
            {
                // restore the old values
                tmp_pos[kZ] = particle->enviro.floor_level;
                if ( particle->getVelocity().z() < 0 )
                {
                    particle->setVelocity(particle->getVelocity() +
                                          Vector3f(0.0f, 0.0f,
                                                   -(1.0f + particle->getProfile()->dampen) * particle->getVelocity().z()));;
                }
                position_updated = true;
            }
            else
            {
                //bdl.prt_ptr->vel[kZ] += bdl.prt_ptr->phys.apos_coll[kZ] * bump_str;
                position_updated = true;
            }
        }

        // Change the direction of the particle
        if ( particle->getProfile()->rotatetoface )
        {
            // Turn to face new direction
            particle->facing = Facing(vec_to_facing( particle->getVelocity().x() , particle->getVelocity().y() ));
        }

        if ( position_updated )
        {
            particle->setPosition(tmp_pos);
        }
    }
}

void CollisionSystem::updateObjectCollisions()
{
    std::unordered_set<ObjectRef> handledObjects;
    ObjectHandler& handler = objectWorld().getObjectHandler();

    //Detect character -> character collisions
    for(const ObjectRef& objectRef : handler.objectRefIterator()) {
        Object* objectPtr = handler.get(objectRef);
        if (objectPtr == nullptr) {
            continue;
        }
        Object& object = *objectPtr;

        //Can we collide?
        if (!object.canCollide()) {
            continue;
        }
        handledObjects.insert(object.getObjRef());

        //First check if this object is still attached to it's Platform
        Object* platform = handler.get(object.onwhichplatform_ref);
        if (platform != nullptr && !platform->isTerminated())
        {
            //If we are no longer colliding in the horizontal plane, then we are disconnected
            if(!idlib::is_intersecting(object.getAxisAlignedBox2D(), platform->getAxisAlignedBox2D()))
            {
                object.detachFromPlatform();
            }
        }

        //TODO: Remove this messy block and replace it with something better
        // use the object velocity to figure out where the volume that the object will occupy during this update
        // convert the oct_bb_t to a correct BSP_aabb_t
        oct_bb_t tmp_oct;
        phys_expand_chr_bb(&object, 0.0f, 1.0f, tmp_oct);
        const AxisAlignedBox2f aabb2d = AxisAlignedBox2f(Point2f(tmp_oct._mins[OCT_X], tmp_oct._mins[OCT_Y]), Point2f(tmp_oct._maxs[OCT_X], tmp_oct._maxs[OCT_Y]));

        //Do not collide scenery with other scenery objects - unless they can use platforms,
        //for example boxes stacked on top of other boxes
        bool canCollideWithScenery = !object.isScenery() || object.canUsePlatforms();

        // Check collisions to nearby Objects
        std::vector<ObjectRef> possibleCollisionRefs;
        handler.findObjectRefs(object.getAxisAlignedBox2D(), possibleCollisionRefs, canCollideWithScenery);
        for (const ObjectRef& otherRef : possibleCollisionRefs)
        {
            Object* other = handler.get(otherRef);
            if (other == nullptr || other->isTerminated()) {
                continue;
            }

            //Skip possible interactions that have already been handled earlier this iteration
            if(handledObjects.find(otherRef) != handledObjects.end()) {
                continue;
            }

            //Can it collide?
            if(!other->canCollide()) {
                continue;
            }

            //Detect any collisions and handle it if needed
            float tmin, tmax;
            if(detectCollision(object, *other, &tmin, &tmax)) {
                handleCollision(object, *other, tmin, tmax);
            }
        }
    }
}

void CollisionSystem::updateParticleCollisions()
{
    //Check collisions with particles
    for(const std::shared_ptr<Ego::Particle> &particleHandle : activeParticleHandler().iterator())
    {
        if (!particleHandle) {
            continue;
        }
        Ego::Particle& particle = *particleHandle;

        if(!particle.canCollide()) {
            continue;
        }

        //First check if this Particle is still attached to a platform
        if (particle.onwhichplatform_update < worldUpdateCount() && objectWorld().getObjectHandler().exists(particle.onwhichplatform_ref)) {
            particle.getParticlePhysics().detachFromPlatform();
        }

        // use the object velocity to figure out where the volume that the object will occupy during this update
        // convert the oct_bb_t to a correct AABB2f
        oct_bb_t   tmp_oct;
        phys_expand_prt_bb(&particle, 0.0f, 1.0f, tmp_oct);
        const AxisAlignedBox2f aabb2d = AxisAlignedBox2f(Point2f(tmp_oct._mins[OCT_X], tmp_oct._mins[OCT_Y]), Point2f(tmp_oct._maxs[OCT_X], tmp_oct._maxs[OCT_Y]));

        //Detect collisions with nearby Objects
        std::vector<ObjectRef> possibleCollisionRefs;
         objectWorld().getObjectHandler().findObjectRefs(aabb2d, possibleCollisionRefs, true);
        for (const ObjectRef& objectRef : possibleCollisionRefs)
        {
            Object* object = objectWorld().getObjectHandler().get(objectRef);
            if (object == nullptr || object->isTerminated()) {
                continue;
            }

            //Is it a valid collision?
            if(!object->canCollide()) {
                continue;
            }

            //Detect any collisions and handle it if needed
            float tmin, tmax;
            if(detectCollision(particle, *object, &tmin, &tmax)) {
                do_prt_platform_detection(object->getObjRef(), particle.getParticleID());
                do_chr_prt_collision(object->getObjRef(), particle.getParticleID(), tmin, tmax);
            }
        }
    }    
}

bool CollisionSystem::detectCollision(const Ego::Particle& particle, const Object& object, float *tmin, float *tmax) const
{
    // particles don't "collide" with anything they are attached to.
    // that only happes through doing bump particle damage
    if (particle.getAttachedObjectID() == object.getObjRef())
    {
        return false;
    }

    //Detect collisions with platforms?
    BIT_FIELD testPlatform = EMPTY_BIT_FIELD;
    if ( object.isPlatform() /*&& ( SPRITE_SOLID == particle.type )*/ ) {
        SET_BIT(testPlatform, PHYS_PLATFORM_OBJ1);
    }

    // Some information about the estimated collision.
    //TODO: ZF> hmmm unused?
    oct_bb_t cv;

    // detect a when the possible collision occurred
    return phys_intersect_oct_bb(object.getMinCollisionVolume(), object.getPosition(), object.getVelocity(), particle.prt_max_cv, particle.getPosition(), particle.getVelocity(), testPlatform, cv, tmin, tmax);
}

bool CollisionSystem::detectCollision(const Object& objectA, const Object& objectB, float *tmin, float *tmax) const
{
    // "non-interacting" objects interact with platforms
    if ((0 == objectA.getCurrentBump().size && !objectB.isPlatform() ) ||
        (0 == objectB.getCurrentBump().size && !objectA.isPlatform() )) {
        return false;
    }

    // handle the dismount exception
    if (objectA.getDismountTimer() > 0 && objectA.getDismountObject() == objectB.getObjRef()) {
        return false;
    }
    if (objectB.getDismountTimer() > 0 && objectB.getDismountObject() == objectA.getObjRef()) {
        return false;
    }

    //Is it a platform collision?
    BIT_FIELD testPlatform = EMPTY_BIT_FIELD;
    if (objectA.isPlatform() && objectB.canUsePlatforms()) {
        SET_BIT(testPlatform, PHYS_PLATFORM_OBJ1);
    }
    if (objectB.isPlatform() && objectA.canUsePlatforms()) {
        SET_BIT(testPlatform, PHYS_PLATFORM_OBJ2);
    }

    // Some information about the estimated collision.
    //TODO: ZF> hmmm unused?
    oct_bb_t cv;

    // detect a when the possible collision occurred
    return phys_intersect_oct_bb(objectA.getMaxCollisionVolume(), objectA.getPosition(), objectA.getVelocity(), objectB.getMaxCollisionVolume(), objectB.getPosition(), objectB.getVelocity(), testPlatform, cv, tmin, tmax);
}

void CollisionSystem::handleCollision(Object& objectA, Object& objectB, const float tmin, const float tmax)
{
    //Try to mount A with B
    if(objectA.canMount(objectB.getObjRef())) {
        if(handleMountingCollision(objectA, objectB)) {
            return;
        }
    }

    //Try to mount B with A
    if(objectB.canMount(objectA.getObjRef())) {
        if(handleMountingCollision(objectB, objectA)) {
            return;
        }
    }

    //Try to resolve any platform collision
    //This attaches objects to other platform objects
    handlePlatformCollision(objectA, objectB);

    //TODO: inline?
    do_chr_chr_collision(objectA, objectB, tmin, tmax);
}

bool CollisionSystem::handleMountingCollision(Object& character, Object& mount)
{
    //Do some collision checks
	bool collideXY = idlib::euclidean_norm(xy(character.getPosition()) - xy(mount.getPosition())) < MOUNTTOLERANCE;

	bool collideZ = (mount.getPosZ() + mount.getMinCollisionVolume()._maxs[OCT_Z]) < character.getPosZ();

    //If we are falling on top of the mount, then we are trying to mount
	bool characterWantsToMount = collideXY && collideZ;

    //If we are facing the mount and jumping towards it, then we are trying to mount
    //else if(collideXY) {
    //    if(character->jump_timer > 0 && character->isFacingLocation(mount->getPosX(), mount->getPosY())) {
    //        characterWantsToMount = true;
    //    }
    //}

    //Attempt to mount?
    if(characterWantsToMount) {
        return character.attachToObject(mount.getObjRef(), GRIP_ONLY);
    }

    return false;
}

bool CollisionSystem::handlePlatformCollision(Object& objectA, Object& objectB)
{
    oct_vec_v2_t odepth;
    const oct_bb_t& objectAMinCollision = objectA.getMinCollisionVolume();
    const oct_bb_t& objectBMinCollision = objectB.getMinCollisionVolume();

    const auto ichr_a = objectA.getObjRef();
    const auto ichr_b = objectB.getObjRef();

    // only check possible object-platform interactions
    bool platform_a = objectB.canUsePlatforms() && !objectWorld().getObjectHandler().exists(objectB.onwhichplatform_ref) && objectA.isPlatform();
    bool platform_b = objectA.canUsePlatforms() && !objectWorld().getObjectHandler().exists(objectA.onwhichplatform_ref) && objectB.isPlatform();

    //Only allow scenery objects on top of other scenery objects
    if(objectA.isScenery() != objectB.isScenery()) {
        platform_a &= objectA.isScenery();
        platform_b &= objectB.isScenery();
    }

    if ( !platform_a && !platform_b ) return false;

    odepth[OCT_Z] = std::min(objectBMinCollision._maxs[OCT_Z] + objectB.getPosZ(), objectAMinCollision._maxs[OCT_Z] + objectA.getPosZ()) -
                    std::max(objectBMinCollision._mins[OCT_Z] + objectB.getPosZ(), objectAMinCollision._mins[OCT_Z] + objectA.getPosZ() );

    bool collide_z  = odepth[OCT_Z] > -PLATTOLERANCE && odepth[OCT_Z] < PLATTOLERANCE;

    if ( !collide_z ) return false;

    // determine how the characters can be attached
    bool chara_on_top = true;
    odepth[OCT_Z] = 2 * PLATTOLERANCE;
    if ( platform_a && platform_b )
    {
        float depth_a, depth_b;

        depth_a = ( objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z] ) - ( objectA.getPosZ() + objectAMinCollision._mins[OCT_Z] );
        depth_b = ( objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z] ) - ( objectB.getPosZ() + objectBMinCollision._mins[OCT_Z] );

        odepth[OCT_Z] = std::min( objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z], objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z] ) -
                        std::max( objectB.getPosZ() + objectBMinCollision._mins[OCT_Z], objectA.getPosZ() + objectAMinCollision._mins[OCT_Z] );

        chara_on_top = std::abs(odepth[OCT_Z] - depth_a) < std::abs(odepth[OCT_Z] - depth_b);

        // the collision is determined by the platform size
        if ( chara_on_top )
        {
            // size of a doesn't matter
            odepth[OCT_X]  = std::min(( objectBMinCollision._maxs[OCT_X] + objectB.getPosX() ) - objectA.getPosX(),
                                        objectA.getPosX() - ( objectBMinCollision._mins[OCT_X] + objectB.getPosX() ) );

            odepth[OCT_Y]  = std::min(( objectBMinCollision._maxs[OCT_Y] + objectB.getPosY() ) -  objectA.getPosY(),
                                        objectA.getPosY() - ( objectBMinCollision._mins[OCT_Y] + objectB.getPosY() ) );

            odepth[OCT_XY] = std::min(( objectBMinCollision._maxs[OCT_XY] + ( objectB.getPosX() + objectB.getPosY() ) ) - ( objectA.getPosX() + objectA.getPosY() ),
                                      ( objectA.getPosX() + objectA.getPosY() ) - ( objectBMinCollision._mins[OCT_XY] + ( objectB.getPosX() + objectB.getPosY() ) ) );

            odepth[OCT_YX] = std::min(( objectBMinCollision._maxs[OCT_YX] + ( -objectB.getPosX() + objectB.getPosY() ) ) - ( -objectA.getPosX() + objectA.getPosY() ),
                                      ( -objectA.getPosX() + objectA.getPosY() ) - ( objectBMinCollision._mins[OCT_YX] + ( -objectB.getPosX() + objectB.getPosY() ) ) );
        }
        else
        {
            // size of b doesn't matter

            odepth[OCT_X]  = std::min(( objectAMinCollision._maxs[OCT_X] + objectA.getPosX() ) - objectB.getPosX(),
                                        objectB.getPosX() - ( objectAMinCollision._mins[OCT_X] + objectA.getPosX() ) );

            odepth[OCT_Y]  = std::min(( objectAMinCollision._maxs[OCT_Y] + objectA.getPosY() ) -  objectB.getPosY(),
                                        objectB.getPosY() - ( objectAMinCollision._mins[OCT_Y] + objectA.getPosY() ) );

            odepth[OCT_XY] = std::min(( objectAMinCollision._maxs[OCT_XY] + ( objectA.getPosX() + objectA.getPosY() ) ) - ( objectB.getPosX() + objectB.getPosY() ),
                                      ( objectB.getPosX() + objectB.getPosY() ) - ( objectAMinCollision._mins[OCT_XY] + ( objectA.getPosX() + objectA.getPosY() ) ) );

            odepth[OCT_YX] = std::min(( objectAMinCollision._maxs[OCT_YX] + ( -objectA.getPosX() + objectA.getPosY() ) ) - ( -objectB.getPosX() + objectB.getPosY() ),
                                      ( -objectB.getPosX() + objectB.getPosY() ) - ( objectAMinCollision._mins[OCT_YX] + ( -objectA.getPosX() + objectA.getPosY() ) ) );
        }
    }
    else if ( platform_a )
    {
        chara_on_top = false;
        odepth[OCT_Z] = ( objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z] ) - ( objectB.getPosZ() + objectBMinCollision._mins[OCT_Z] );

        // size of b doesn't matter

        odepth[OCT_X] = std::min((objectAMinCollision._maxs[OCT_X] + objectA.getPosX() ) - objectB.getPosX(),
                                  objectB.getPosX() - ( objectAMinCollision._mins[OCT_X] + objectA.getPosX() ) );

        odepth[OCT_Y] = std::min((objectAMinCollision._maxs[OCT_Y] + objectA.getPosY()) - objectB.getPosY(),
                                  objectB.getPosY() - ( objectAMinCollision._mins[OCT_Y] + objectA.getPosY() ) );

        odepth[OCT_XY] = std::min((objectAMinCollision._maxs[OCT_XY] + (objectA.getPosX() + objectA.getPosY())) - (objectB.getPosX() + objectB.getPosY()),
                                  ( objectB.getPosX() + objectB.getPosY() ) - ( objectAMinCollision._mins[OCT_XY] + ( objectA.getPosX() + objectA.getPosY() ) ) );

        odepth[OCT_YX] = std::min((objectAMinCollision._maxs[OCT_YX] + (-objectA.getPosX() + objectA.getPosY())) - (-objectB.getPosX() + objectB.getPosY()),
                                  ( -objectB.getPosX() + objectB.getPosY() ) - ( objectAMinCollision._mins[OCT_YX] + ( -objectA.getPosX() + objectA.getPosY() ) ) );
    }
    else if ( platform_b )
    {
        chara_on_top = true;
        odepth[OCT_Z] = ( objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z] ) - ( objectA.getPosZ() + objectAMinCollision._mins[OCT_Z] );

        // size of a doesn't matter
        odepth[OCT_X] = std::min((objectBMinCollision._maxs[OCT_X] + objectB.getPosX()) - objectA.getPosX(),
                                  objectA.getPosX() - ( objectBMinCollision._mins[OCT_X] + objectB.getPosX() ) );

        odepth[OCT_Y] = std::min(objectBMinCollision._maxs[OCT_Y] + (objectB.getPosY() - objectA.getPosY()),
                                 ( objectA.getPosY() - objectBMinCollision._mins[OCT_Y] ) + objectB.getPosY() );

        odepth[OCT_XY] = std::min((objectBMinCollision._maxs[OCT_XY] + (objectB.getPosX() + objectB.getPosY())) - (objectA.getPosX() + objectA.getPosY()),
                                  ( objectA.getPosX() + objectA.getPosY() ) - ( objectBMinCollision._mins[OCT_XY] + ( objectB.getPosX() + objectB.getPosY() ) ) );

        odepth[OCT_YX] = std::min(( objectBMinCollision._maxs[OCT_YX] + ( -objectB.getPosX() + objectB.getPosY() ) ) - ( -objectA.getPosX() + objectA.getPosY() ),
                                  ( -objectA.getPosX() + objectA.getPosY() ) - ( objectBMinCollision._mins[OCT_YX] + ( -objectB.getPosX() + objectB.getPosY() ) ) );

    }

    bool collide_x  = odepth[OCT_X]  > 0.0f;
    bool collide_y  = odepth[OCT_Y]  > 0.0f;
    bool collide_xy = odepth[OCT_XY] > 0.0f;
    bool collide_yx = odepth[OCT_YX] > 0.0f;
    collide_z  = odepth[OCT_Z] > -PLATTOLERANCE && odepth[OCT_Z] < PLATTOLERANCE;

    if ( collide_x && collide_y && collide_xy && collide_yx && collide_z )
    {
        // check for the best possible attachment
        if ( chara_on_top )
        {
            if ( objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z] > objectA.targetplatform_level )
            {
                objectA.targetplatform_level = objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z];
                objectA.targetplatform_ref   = ichr_b;

                return objectA.attachToPlatform(objectB.getObjRef());
            }
        }
        else
        {
            if ( objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z] > objectB.targetplatform_level )
            {
                objectB.targetplatform_level = objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z];
                objectB.targetplatform_ref   = ichr_a;

                return objectB.attachToPlatform(objectA.getObjRef());
            }
        }
    }

    return false;
}

} //namespace Physics
} //namespace Ego
