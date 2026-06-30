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

/// @file egolib/game/Physics/ObjectPhysics.cpp
/// @brief Code for handling object physics (movement integration, hill/Z-velocity,
///        facing, max-speed, trivial accessors).  Platform, mesh-collision, grab, and
///        attachment are split into ObjectPhysics_terrain.cpp and
///        ObjectPhysics_attachment.cpp respectively.
/// @author Johan Jansen aka Zefz
#include "egolib/game/Physics/ObjectPhysics_internal.h"
#include "egolib/game/CharacterMatrix.h"
#include "egolib/FileFormats/map_fx.hpp"          // MAPFX_SLIPPY (was via Module.hpp -> mesh.h)

namespace Ego
{
namespace Physics
{

ObjectPhysics::ObjectPhysics(Object &object) :
    _object(object),
    _platformOffset(0.0f, 0.0f),
    _desiredVelocity(0.0f, 0.0f),
    _traction(1.0f),
    _groundElevation(0.0f),
    _aabb2D()
{
    //ctor
}

void ObjectPhysics::keepItemsWithHolder()
{
    const Object* holder = objectByRef(_object.getHolderRef());
    if (holder)
    {
        // Keep in hand weapons with iattached
        if ( chr_matrix_valid(&_object) )
        {
            _object.setPosition(mat_getTranslate(_object.getMatrix()));
        }
        else
        {
            _object.setPosition(holder->getPosition());
        }

        _object.setFacingZ(holder->getFacingZ());

        // Copy this stuff ONLY if it's a weapon, not for mounts
        if (holder->getProfile()->transferBlending() && _object.isItem())
        {

            // Items become partially invisible in hands of players
            if (holder->isPlayer() && 255 != holder->getAlpha())
            {
                _object.setAlpha(SEEINVISIBLE);
            }
            else
            {
                // Only if not naturally transparent
                if (255 == _object.getProfile()->getAlpha())
                {
                    _object.setAlpha(holder->getAlpha());
                }
                else
                {
                    _object.setAlpha(_object.getProfile()->getAlpha());
                }
            }

            // Do light too
            if (holder->isPlayer() && 255 != holder->getLight())
            {
                _object.setLight(SEEINVISIBLE);
            }
            else
            {
                // Only if not naturally transparent
                if (255 == _object.getProfile()->getLight())
                {
                    _object.setLight(holder->getLight());
                }
                else
                {
                    _object.setLight(_object.getProfile()->getLight());
                }
            }
        }
    }
    else
    {
        _object.setHolderRef(ObjectRef::Invalid);
    }
}

void ObjectPhysics::setDesiredVelocity(const Vector2f &velocity)
{
    _desiredVelocity = velocity;

    //Constrain desired velocity between -1.0f and 1.0f
    if (idlib::euclidean_norm(_desiredVelocity) > 1.0f) {
        _desiredVelocity *= (1.0f / idlib::euclidean_norm(_desiredVelocity));
    }
}

void ObjectPhysics::updateMovement()
{
    //Can it move?
    if (_object.isAlive() && _object.getAttribute(Ego::Attribute::ACCELERATION) > 0.0f)  {
 
        // Reverse movements for daze
        if (_object.getDazeTimer() > 0) {
            _desiredVelocity.x() = -_desiredVelocity.x();
            _desiredVelocity.y() = -_desiredVelocity.y();
        }

        // Switch x and y for grog
        if (_object.getGrogTimer() > 0) {
            std::swap(_desiredVelocity.x(), _desiredVelocity.y());
        }

        //Update which way we are looking
        updateFacing();
    }
    else {
        //Immobile object
        _desiredVelocity = idlib::zero<Vector2f>();
    }

    //Is there any movement going on?
    Vector2f velocitySetpoint;
    if(idlib::manhattan_norm(_desiredVelocity) > 0.05f) {
        const float maxSpeed = getMaxSpeed();

        //Scale [-1 , 1] to velocity of the object
        velocitySetpoint = _desiredVelocity * maxSpeed;

        //Limit to max velocity
        if(idlib::euclidean_norm(velocitySetpoint) > maxSpeed) {
            velocitySetpoint *= maxSpeed / idlib::euclidean_norm(velocitySetpoint);
        }
    }
    else {
        //Try to stand still
		velocitySetpoint = idlib::zero<Vector2f>();
    }

    //Determine acceleration/deceleration
    Vector2f acceleration;
    acceleration.x() = (velocitySetpoint.x() - _object.getVelocity().x()) * (4.0f / ONESECOND);
    acceleration.y() = (velocitySetpoint.y() - _object.getVelocity().y()) * (4.0f / ONESECOND);

    //How good grip do we have to add additional momentum?
    acceleration *= _traction;

    //Finally apply acceleration to velocity
    _object.setVelocity(_object.getVelocity() +
                        Vector3f(acceleration.x(), acceleration.y(), 0.0f));
}

void ObjectPhysics::updateHillslide()
{
    const uint8_t floorTwist = collisionWorld().getTwist(_object.getTile());

    //This makes it hard for characters to jump uphill
    if(_object.getVelocity().z() > 0.0f && floorIsSlippy() && !g_meshLookupTables.twist_flat[floorTwist]) {
        _object.setVelocity({_object.getVelocity().x(),
                             _object.getVelocity().y(),
                             _object.getVelocity().z() * 0.8f});
    }

    //Only slide if we are touching the floor
    if(isTouchingGround()) {

        //Can the character slide on this floor?
        if (floorIsSlippy() && !objectByRef(_object.getAttachedPlatformRef()))
        {
            //Make characters slide down hills
            if(!g_meshLookupTables.twist_flat[floorTwist]) {
                const float hillslide = Ego::Physics::g_environment.hillslide * (1.0f - getLerpZ()) * (1.0f - _traction);
                _object.setVelocity(_object.getVelocity() +
                                    Vector3f(g_meshLookupTables.twist_nrm[floorTwist].x() * hillslide,
                                             g_meshLookupTables.twist_nrm[floorTwist].y() * hillslide,
                                             0.0f));

                //Reduce traction while we are sliding downhill
                _traction *= 0.8f;
            }
            else {
                //Flat icy floor -> reduced traction
                _traction = 1.0f - Ego::Physics::g_environment.icefriction;
            }
        }
        else {
            //Reset traction
            _traction = 1.0f;
        }
    }
}

float ObjectPhysics::getLerpZ() const
{
    return Ego::Math::constrain((_object.getPosZ() - _groundElevation) / PLATTOLERANCE, 0.0f, 1.0f);
}

void ObjectPhysics::updateVelocityZ()
{
    //Flying?
    if(_object.isFlying()) {
        float flyLevel = std::max(0.0f, _groundElevation);
        _object.setVelocity(_object.getVelocity() +
                            Vector3f(0.0f, 0.0f, 
                                     (flyLevel + _object.getAttribute(Ego::Attribute::FLY_TO_HEIGHT) - _object.getPosZ()) * FLYDAMPEN));
        _object.setJumpReady(false);
        return;
    }

    //Apply gravity
    _object.setVelocity(_object.getVelocity() +
                        Vector3f(0.0f, 0.0f,
                                getLerpZ() * Ego::Physics::g_environment.gravity));

    // Do ground hits
    if(isTouchingGround()) {
        _object.setJumpReady(true);

        if (_object.getVelocity().z() < -Ego::Physics::STOP_BOUNCING && _object.isHitReady()) {
            scriptable(_object).addAIAlertBits(ALERTIF_HITGROUND);
            _object.setHitReady(false);
        }

        if (0 == _object.getJumpTimer()) {
            // Reset jumping on flat areas of slippiness
            if(!floorIsSlippy() || g_meshLookupTables.twist_flat[collisionWorld().getTwist(_object.getTile())]) {
                _object.setJumpNumber(_object.getAttribute(Ego::Attribute::NUMBER_OF_JUMPS));
            }
        }
    }
    else {
        _object.setJumpReady(false);
    }
}

void ObjectPhysics::updatePhysics()
{
    // Keep inventory items with the carrier
    if(_object.isInsideInventory()) {
        Object* inventoryHolder = objectWorld().getObjectHandler().get(_object.getInventoryHolderRef());
        _object.setPosition(inventoryHolder->getPosition());
        return;
    }

    // Character's old location
    _object.setOldVelocity(_object.getVelocity());
    _object.setPreviousFacingZ(_object.getFacingZ());

    //Is this character being held by another character?
    if(_object.isBeingHeld()) {
        keepItemsWithHolder();
        return;
    }

    //Generate velocity from sliding on hills
    updateHillslide();

    //Generate velocity from user input (or AI script)
    updateMovement();

    //Keep us on the platform we are standing on
    updatePlatformPhysics();

    //Generate Z velocity (jumping, gravity, flight, etc.)
    updateVelocityZ();

    //Handle collision with the floor and walls
    updateMeshCollision();

    //Cutoff for low velocities to make them truly stop
    if(idlib::manhattan_norm(_object.getVelocity()) < 0.05f) {
        _object.setVelocity(idlib::zero<Vector3f>());
    }

    //Recalculate the altitude of the ground beneath our feet
    //Apperantly this function is quite expensive so cache the result every update
    _groundElevation = recalculateGroundElevation();
}

float ObjectPhysics::recalculateGroundElevation()
{
    //Standing on a platform?
    const Object* platform = objectByRef(_object.getAttachedPlatformRef());
    if (platform) {
        return platform->getPosZ() + platform->getMinCollisionVolume()._maxs[OCT_Z];
    }

    //Walking on water?
    if(_object.isOnWaterTile() && _object.getAttribute(Ego::Attribute::WALK_ON_WATER) > 0) {
        return collisionWorld().getElevation(Vector2f(_object.getPosX(), _object.getPosY()), true);
    }

    //Standing on regular ground
    return collisionWorld().getElevation(Vector2f(_object.getPosX(), _object.getPosY()), false);
}

float ObjectPhysics::getMaxSpeed() const
{
    // this is the maximum speed that a character could go under the v2.22 system
    float maxspeed = _object.getAttribute(Ego::Attribute::ACCELERATION) * Ego::Physics::g_environment.airfriction / (1.0f - Ego::Physics::g_environment.airfriction);
    float speedBonus = 1.0f;

    //Sprint perk gives +10% movement speed if above 75% life remaining
    if(_object.hasPerk(Ego::Perks::SPRINT) && _object.getLife() >= _object.getAttribute(Ego::Attribute::MAX_LIFE)*0.75f) {
        speedBonus += 0.1f;

        //Uninjured? (Dash perk can give another 10% extra speed)
        if(_object.hasPerk(Ego::Perks::DASH) && _object.getAttribute(Ego::Attribute::MAX_LIFE)-_object.getLife() < 1.0f) {
            speedBonus += 0.1f;
        }
    }

    //Rally Bonus? (+10%)
    if(_object.hasPerk(Ego::Perks::RALLY) && worldUpdateCount() < _object.getRallyDuration()) {
        speedBonus += 0.1f;
    }    

    //Increase movement by 1% per Agility above 10 (below 10 agility reduces movement speed!)
    speedBonus += (_object.getAttribute(Ego::Attribute::AGILITY)-10.0f) * 0.01f;

    //Now apply speed modifiers
    maxspeed *= speedBonus;

    //Are we in water?
    if(_object.isSubmerged() && collisionWorld().isWater()) {
        if(_object.hasPerk(Ego::Perks::ATHLETICS)) {
            maxspeed *= 0.25f; //With athletics perk we can have three-quarters speed
        }
        else {
            maxspeed *= 0.5f; //Half speed in water
        }
    }

    //Check animation frame freeze movement
    if ( _object.getGraphics().getFrameFX() & MADFX_STOP )
    {
        //Allow 50% movement while using Shield and have the Mobile Defence perk
        if(_object.hasPerk(Ego::Perks::MOBILE_DEFENCE) && ACTION_IS_TYPE(_object.getCurrentAnimation(), P))
        {
            maxspeed *= 0.5f;
        }
        //Allow 50% movement with Mobility perk and attacking with a weapon
        else if(_object.hasPerk(Ego::Perks::MOBILITY) && _object.isAttacking())
        {
            maxspeed *= 0.5f;
        }
        else
        {
            //No movement allowed
            maxspeed = 0.0f;
        }
    }

    //Check if AI has limited movement rate
    else if(!_object.isPlayer())
    {
        maxspeed *= scriptable(_object).getAIMaxSpeed();
    }

    //Reduce speed while stealthed
    if(_object.isStealthed()) {
        if(_object.hasPerk(Ego::Perks::SHADE)) {
            maxspeed *= 0.75f;  //Shade allows 75% movement speed while stealthed
        }
        else if(_object.hasPerk(Ego::Perks::STALKER)) {
            maxspeed *= 0.50f;  //Stalker allows 50% movement speed while stealthed
        }
        else {
            maxspeed *= 0.33f;  //Can only move at 33% speed while stealthed
        }
    }

    return maxspeed;    
}

void ObjectPhysics::updateFacing()
{
    //Figure out how to turn around
    switch ( _object.getTurnMode() )
    {
        // Get direction from ACTUAL change in velocity
        default:
        case TURNMODE_VELOCITY:
            {
                if (idlib::manhattan_norm(_desiredVelocity) > TURNSPD)
                {
                    //Every Agility increases turn speed by 2%
                    const float turnSpeed = std::max(2.0f, 8.0f * (1.0f - _object.getAttribute(Ego::Attribute::AGILITY) / 50.0f)); //turn delay is 8.0f -2% per Agility
                    _object.setFacingZ(idlib::canonicalize(rotate(_object.getFacingZ(), vec_to_facing(_desiredVelocity.x(), _desiredVelocity.y()), turnSpeed)));
                }
            }
            break;

        // Get direction from the DESIRED change in velocity
        case TURNMODE_WATCH:
            {
                if (idlib::manhattan_norm(_desiredVelocity) > WATCHMIN )
                {
                    _object.setFacingZ(idlib::canonicalize(rotate(_object.getFacingZ(), vec_to_facing(_desiredVelocity.x(), _desiredVelocity.y()), 8.0f)));
                }
            }
            break;

        // Face the target
        case TURNMODE_WATCHTARGET:
            {
                //Only proceed if we have a valid AI target that is not ourselves
                const IScriptable& scriptableObject = scriptable(_object);
                Object* aiTarget = objectWorld().getObjectHandler().get(scriptableObject.getAITarget());
                if (aiTarget != nullptr && aiTarget->getObjRef() != _object.getObjRef())
                {
                    _object.setFacingZ(idlib::canonicalize(rotate(_object.getFacingZ(), vec_to_facing(aiTarget->getPosX() - _object.getPosX(), aiTarget->getPosY() - _object.getPosY()), 8.0f)));
                }
            }
            break;

        // Otherwise make it spin
        case TURNMODE_SPIN:
            {
                _object.setFacingZ(_object.getFacingZ() + Facing(SPINRATE));
            }
            break;
    }
}

const Vector2f& ObjectPhysics::getDesiredVelocity() const
{
    return _desiredVelocity;
}

float ObjectPhysics::getMass() const
{
    // Weight 255 (CAP_INFINITE_WEIGHT, promoted to CHR_INFINITE_WEIGHT at spawn — see
    // Object_attributes.cpp) marks immovable scenery: it must yield an infinite collision mass
    // regardless of bump dampening, so a character bumping into it cannot shove it. Most immovable
    // content ALSO sets bumpdampen 0.0 (handled by the second branch), but some (e.g. the tent)
    // relies solely on weight 255 with a non-zero bumpdampen, so this first branch is required.
    if ( Ego::Physics::CHR_INFINITE_WEIGHT == _object.phys.weight )
    {
        return -static_cast<float>(Ego::Physics::CHR_INFINITE_WEIGHT);
    }
    else if ( 0.0f == _object.phys.bumpdampen )
    {
        return -static_cast<float>(Ego::Physics::CHR_INFINITE_WEIGHT);
    }
    else
    {
        return _object.phys.weight / _object.phys.bumpdampen;
    }
}

bool ObjectPhysics::floorIsSlippy() const
{
    //Water tiles are never slippy
    if(_object.isInWater() && collisionWorld().isWater()) return false;

    //Check tile slippy bit
    return 0 != collisionWorld().testFX(_object.getTile(), MAPFX_SLIPPY);
}

bool ObjectPhysics::isTouchingGround() const
{
    //Never touching the ground while levitating
    if(_object.isFlying()) {
        return false;
    }

    return std::abs(_object.getPosZ() - _groundElevation) <= FLOOR_TOLERANCE;
}

float ObjectPhysics::getGroundElevation() const
{
    return _groundElevation;
}

const AxisAlignedBox2f& ObjectPhysics::getAxisAlignedBox2D() const
{
    return _aabb2D;
}

} //Physics
} //Ego
