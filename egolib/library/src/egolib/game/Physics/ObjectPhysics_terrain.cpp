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

/// @file egolib/game/Physics/ObjectPhysics_terrain.cpp
/// @brief Platform attach/detach and mesh-collision sub-pipeline for ObjectPhysics.
/// @details Implements: detachFromPlatform, attachToPlatform, updatePlatformPhysics,
///          updateMeshCollision.  Split from ObjectPhysics.cpp to reduce TU size.
/// @author Johan Jansen aka Zefz
#include "egolib/game/Physics/ObjectPhysics_internal.h"

namespace Ego
{
namespace Physics
{

void ObjectPhysics::detachFromPlatform()
{
    // adjust the platform weight, if necessary
    Object* platform = objectByRef(_object.getAttachedPlatformRef());
    if (platform) {
        platform->adjustHoldingWeight(-static_cast<int>(_object.phys.weight));
    }

    // undo the attachment
    _object.onwhichplatform_ref    = ObjectRef::Invalid;
    _object.onwhichplatform_update = 0;
    _object.targetplatform_ref     = ObjectRef::Invalid;
    _object.targetplatform_level   = -1e32;
    _platformOffset = idlib::zero<Vector2f>();
}

bool ObjectPhysics::attachToPlatform(ObjectRef platformRef)
{
    Object* platform = objectByRef(platformRef);
    if (!platform) {
        return false;
    }

    // check if they can be connected
    if(!_object.canUsePlatforms() || _object.isFlying() || !platform->isPlatform() || platform == &_object) {
        return false;
    }

    // do the attachment
    _object.onwhichplatform_ref    = platform->getObjRef();
    _object.onwhichplatform_update = worldUpdateCount();
    _object.targetplatform_ref     = ObjectRef::Invalid;

    _platformOffset.x() = _object.getPosX() - platform->getPosX();
    _platformOffset.y() = _object.getPosY() - platform->getPosY();

    //Make sure the object is now on top of the platform
    const float platformElevation = platform->getPosZ() + platform->getMinCollisionVolume()._maxs[OCT_Z];
    if(_object.getPosZ() < platformElevation) {
        _object.setPosition(_object.getPosX(), _object.getPosY(), platformElevation);
    }

    // add the weight to the platform
    platform->adjustHoldingWeight(static_cast<int>(_object.phys.weight));

    // update the character jumping
    _object.setJumpReady(true);
    _object.setJumpNumber(_object.getAttribute(Ego::Attribute::NUMBER_OF_JUMPS));

    // tell the platform that we bumped into it
    // this is necessary for key buttons to work properly, for instance
    scriptable(*platform).recordAIBump(_object.getObjRef());

    return true;
}

void ObjectPhysics::updatePlatformPhysics()
{
    const Object* platform = objectByRef(_object.getAttachedPlatformRef());
    if(!platform) {
        return;
    }

    // grab the pre-computed zlerp value, and map it to our needs
    float lerp_z = 1.0f - getLerpZ();

    // if your velocity is going up much faster than the
    // platform, there is no need to suck you to the level of the platform
    // this was one of the things preventing you from jumping from platforms
    // properly
    if(std::abs(_object.getVelocity().z() - platform->getVelocity().z()) / 5.0f <= PLATFORM_STICKINESS) {
        _object.setVelocity(_object.getVelocity() +
                            Vector3f(0.0f, 0.0f, (platform->getVelocity().z()  - _object.getVelocity().z()) * lerp_z));
    }

    // determine the rotation rates
    int16_t rot_b = FACING_T(_object.getFacingZ()) - FACING_T(_object.getPreviousFacingZ());
    int16_t rot_a = FACING_T(platform->getFacingZ()) - FACING_T(platform->getPreviousFacingZ());
    _object.setFacingZ(_object.getFacingZ() + Facing(int16_t((rot_a - rot_b) * PLATFORM_STICKINESS)));

    //Allows movement on the platform
    _platformOffset.x() += _object.getVelocity().x();
    _platformOffset.y() += _object.getVelocity().y();

    //Inherit position of platform with given offsets
    float zCorrection = (_groundElevation - _object.getPosZ()) * 0.125f * lerp_z;
    _object.setPosition(platform->getPosX() + _platformOffset.x(), platform->getPosY() + _platformOffset.y(), _object.getPosZ() + zCorrection);
}

void ObjectPhysics::updateMeshCollision()
{
    Vector3f tmp_pos = _object.getPosition();;

    // interaction with the mesh
    //if ( std::abs( _object.vel.z() ) > 0.0f )
    {
        //Make everything "float" a little above the mesh
        const float floorElevation = _groundElevation + 10.0f;

        tmp_pos.z() += _object.getVelocity().z();

        //Hit the floor?
        if (tmp_pos.z() <= floorElevation)
        {
            tmp_pos.z() = floorElevation;

            //Stop bouncing when below threshold
            if (std::abs(_object.getVelocity().z()) < Ego::Physics::STOP_BOUNCING)
            {
                _object.setVelocity({_object.getVelocity().x(),
                                     _object.getVelocity().y(),
                                     0.0f});
            }

            //Make it bounce!
            else if (_object.getVelocity().z() < 0.0f)
            {
                _object.setVelocity({_object.getVelocity().x(), _object.getVelocity().y(),
                                     -_object.getVelocity().z() * _object.getProfile()->getBounciness()});
            }
        }
    }

    // fixes to the z-position
    if (_object.isFlying())
    {
        // Don't fall in pits...
        if (tmp_pos.z() < 0.0f) {
            tmp_pos.z() = 0.0f;
        }
    }


    //if (std::abs(_object.getVelocity().x()) + std::abs(_object.getVelocity().y()) > 0.0f)
    {
        float old_x = tmp_pos.x();
        float old_y = tmp_pos.y();

        float new_x = old_x + _object.getVelocity().x();
        float new_y = old_y + _object.getVelocity().y();

        tmp_pos.x() = new_x;
        tmp_pos.y() = new_y;

        //Wall collision?
        if ( EMPTY_BIT_FIELD != _object.test_wall( tmp_pos ) )
        {
            Vector2f nrm;
            float   pressure;

            _object.hit_wall(tmp_pos, nrm, &pressure );

            // how is the character hitting the wall?
            if (pressure > 0.0f)
            {
                tmp_pos.x() -= _object.getVelocity().x();
                tmp_pos.y() -= _object.getVelocity().y();

                const float bumpdampen = std::max(0.1f, 1.0f-_object.phys.bumpdampen);

                //Bounce velocity of normal
                Vector2f velocity = Vector2f(_object.getVelocity().x(), _object.getVelocity().y());
                velocity.x() -= 2.0f * (dot(nrm, velocity) * nrm.x());
                velocity.y() -= 2.0f * (dot(nrm, velocity) * nrm.y());

                _object.setVelocity({_object.getVelocity().x() * bumpdampen + velocity.x()*(1.0f - bumpdampen),
                                     _object.getVelocity().y() * bumpdampen + velocity.y()*(1.0f - bumpdampen),
                                     _object.getVelocity().z()});

                //Add additional pressure perpendicular from wall depending on how far inside wall we are
                float displacement = idlib::euclidean_norm(xy(_object.getSafePosition()) - xy(tmp_pos));
                if(displacement > MAX_DISPLACEMENT_XY) {
                    displacement = MAX_DISPLACEMENT_XY;
                }
                _object.setVelocity(_object.getVelocity() +
                                    Vector3f(displacement * bumpdampen * pressure * nrm.x(),
                                             displacement * bumpdampen * pressure * nrm.y(),
                                             0.0f));

                //Apply correction
                tmp_pos.x() += _object.getVelocity().x();
                tmp_pos.y() += _object.getVelocity().y();
            }
        }
    }

    _object.setPosition(tmp_pos);

    // Characters with sticky butts lie on the surface of the mesh
    if(_object.getProfile()->hasStickyButt() || !_object.isAlive()) {
        float fkeep = (7.0f + getLerpZ()) / 8.0f;
        float fnew  = (1.0f - getLerpZ()) / 8.0f;

        if (fnew > 0) {
            const uint8_t floorTwist = collisionWorld().getTwist(_object.getTile());
            _object.setMapTwistFacingX(idlib::canonicalize(_object.getMapTwistFacingX() * fkeep + g_meshLookupTables.twist_facing_x[floorTwist] * fnew));
            _object.setMapTwistFacingY(idlib::canonicalize(_object.getMapTwistFacingY() * fkeep + g_meshLookupTables.twist_facing_y[floorTwist] * fnew));
        }
    }
}

} //Physics
} //Ego
