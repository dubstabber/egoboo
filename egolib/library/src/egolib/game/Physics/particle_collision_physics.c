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

/// @file egolib/game/Physics/particle_collision_physics.c
/// @brief Particle-side physics math + platform-attachment geometry.
/// @details The game-state-light slice of the particle collision module: pure mass / recoil
///          math (CHR_INFINITE_WEIGHT-aware) plus the platform-attachment detection +
///          attach_prt_to_platform helper. The chr-prt damage / deflect / knockback response
///          pipeline lives in particle_collision_response.c. Both TUs stay in egolib-library
///          (game layer) — egolib-physics (5 TUs) is a different, lower archive.

#include "egolib/game/Physics/particle_collision.h"
#include "egolib/Physics/physics.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"

namespace
{
/// The entity world the collision step queries (object container), reached through the
/// lower-layer Ego::Entities::IObjectWorld seam rather than game/ (GameModule).
Ego::Entities::IObjectWorld& objectWorld()
{
    return Ego::Entities::activeObjectWorld();
}

/// The active world's update tick, reached through the lower-layer activeWorldUpdateCount() seam.
uint32_t worldUpdateCount()
{
    return Ego::Entities::activeWorldUpdateCount();
}

const IPhysical& physical(const Object& object)
{
    return object;
}
}

static bool attach_prt_to_platform( Ego::Particle * pprt, Object * pplat );

//--------------------------------------------------------------------------------------------
bool get_prt_mass( Ego::Particle * pprt, const IPhysical * pchr, float * wt )
{
    /// @author BB
    /// @details calculate a "mass" for each object, taking into account possible infinite masses.

    float loc_wprt;

    if ( NULL == pprt || NULL == pchr ) return false;

    if ( NULL == wt ) wt = &loc_wprt;

    // determine an approximate mass for the particle
    if ( 0.0f == pprt->phys.bumpdampen )
    {
        *wt = -( float )Ego::Physics::CHR_INFINITE_WEIGHT;
    }
    else if ( pprt->isAttached() )
    {
        if ( Ego::Physics::CHR_INFINITE_WEIGHT == pprt->phys.weight || 0.0f == pprt->phys.bumpdampen )
        {
            *wt = -( float )Ego::Physics::CHR_INFINITE_WEIGHT;
        }
        else
        {
            *wt = pprt->phys.weight / pprt->phys.bumpdampen;
        }
    }
    else
    {
        float max_damage = std::abs(pprt->damage.base) + std::abs(pprt->damage.rand);

        *wt = 1.0f;

        if ( 0 == max_damage )
        {
            // this is a particle like the wind particles in the whirlwind
            // make the particle have some kind of predictable constant effect
            // relative to any character;
            *wt = pchr->getPhysicsWeight() / 10.0f;
        }
        else
        {
            // determine an "effective mass" for the particle, based on it's max damage
            // and velocity

            float prt_vel2;
            float prt_ke;
            Ego::Vector3f vdiff;

            vdiff = pprt->getVelocity() - pchr->getVelocity();

            // the damage is basically like the kinetic energy of the particle
            prt_vel2 = Ego::dot(vdiff, vdiff);

            // It can happen that a damage particle can hit something
            // at almost zero velocity, which would make for a huge "effective mass".
            // by making a reasonable "minimum velocity", we limit the maximum mass to
            // something reasonable
            prt_vel2 = std::max( 100.0f, prt_vel2 );

            // get the "kinetic energy" from the damage
            prt_ke = 3.0f * max_damage;

            // the faster the particle is going, the smaller the "mass" it
            // needs to do the damage
            *wt = prt_ke / ( 0.5f * prt_vel2 );
        }

        *wt /= pprt->phys.bumpdampen;
    }

    return true;
}

//--------------------------------------------------------------------------------------------
void get_recoil_factors( float wta, float wtb, float * recoil_a, float * recoil_b )
{
    float loc_recoil_a, loc_recoil_b;

    if ( NULL == recoil_a ) recoil_a = &loc_recoil_a;
    if ( NULL == recoil_b ) recoil_b = &loc_recoil_b;

    if ( wta >= ( float )Ego::Physics::CHR_INFINITE_WEIGHT ) wta = -( float )Ego::Physics::CHR_INFINITE_WEIGHT;
    if ( wtb >= ( float )Ego::Physics::CHR_INFINITE_WEIGHT ) wtb = -( float )Ego::Physics::CHR_INFINITE_WEIGHT;

    if ( wta < 0.0f && wtb < 0.0f )
    {
        *recoil_a = 0.5f;
        *recoil_b = 0.5f;
    }
    else if ( wta == wtb )
    {
        *recoil_a = 0.5f;
        *recoil_b = 0.5f;
    }
    else if ( wta < 0.0f || 0.0f == wtb )
    {
        *recoil_a = 0.0f;
        *recoil_b = 1.0f;
    }
    else if ( wtb < 0.0f || 0.0f == wta )
    {
        *recoil_a = 1.0f;
        *recoil_b = 0.0f;
    }
    else
    {
        *recoil_a = wtb / ( wta + wtb );
        *recoil_b = wta / ( wta + wtb );
    }
}

//--------------------------------------------------------------------------------------------
bool do_prt_platform_detection( const ObjectRef ichr_a, const ParticleRef iprt_b )
{
    Object * pchr_a;

    bool platform_a;

    oct_vec_v2_t odepth;
    bool collide_x  = false;
    bool collide_y  = false;
    bool collide_xy = false;
    bool collide_yx = false;
    bool collide_z  = false;

    // make sure that A is valid
    if ( !objectWorld().getObjectHandler().exists( ichr_a ) ) return false;
    pchr_a = objectWorld().getObjectHandler().get( ichr_a );

    // make sure that B is valid
    const std::shared_ptr<Ego::Particle> &pprt_b = activeParticleHandler()[iprt_b];
    if ( !pprt_b || pprt_b->isTerminated() ) return false;

    //Already attached to a platform?
    if(!objectWorld().getObjectHandler().exists(pprt_b->onwhichplatform_ref)) {
        return false;
    }

    // if you are mounted, only your mount is affected by platforms
    if ( objectWorld().getObjectHandler().exists( pchr_a->getHolderRef() ) || pprt_b->isAttached() ) return false;

    // only check possible object-platform interactions
    platform_a = /* pprt_b->canuseplatforms && */ pchr_a->isPlatform();
    if ( !platform_a ) return false;
    const IPhysical& physicalCharacter = physical(*pchr_a);
    const oct_bb_t& chrMinCollision = physicalCharacter.getMinCollisionVolume();

    odepth[OCT_Z]  = std::min( pprt_b->prt_max_cv._maxs[OCT_Z] + pprt_b->getPosZ(), chrMinCollision._maxs[OCT_Z] + pchr_a->getPosZ() ) -
                     std::max( pprt_b->prt_max_cv._mins[OCT_Z] + pprt_b->getPosZ(), chrMinCollision._mins[OCT_Z] + pchr_a->getPosZ() );

    collide_z = (odepth[OCT_Z] > -PLATTOLERANCE) && (odepth[OCT_Z] < PLATTOLERANCE);

    if ( !collide_z ) return false;

    // determine how the characters can be attached
    odepth[OCT_Z] = ( pchr_a->getPosZ() + chrMinCollision._maxs[OCT_Z] ) - ( pprt_b->getPosZ() + pprt_b->prt_max_cv._mins[OCT_Z] );

    // size of b doesn't matter

    odepth[OCT_X] = std::min((chrMinCollision._maxs[OCT_X] + pchr_a->getPosX()) - pprt_b->getPosX(),
                              pprt_b->getPosX() - ( chrMinCollision._mins[OCT_X] + pchr_a->getPosX() ) );

    odepth[OCT_Y]  = std::min(( chrMinCollision._maxs[OCT_Y] + pchr_a->getPosY() ) -  pprt_b->getPosY(),
                                pprt_b->getPosY() - ( chrMinCollision._mins[OCT_Y] + pchr_a->getPosY() ) );

    odepth[OCT_XY] = std::min(( chrMinCollision._maxs[OCT_XY] + ( pchr_a->getPosX() + pchr_a->getPosY() ) ) - ( pprt_b->getPosX() + pprt_b->getPosY() ),
                              ( pprt_b->getPosX() + pprt_b->getPosY() ) - ( chrMinCollision._mins[OCT_XY] + ( pchr_a->getPosX() + pchr_a->getPosY() ) ) );

    odepth[OCT_YX] = std::min(( chrMinCollision._maxs[OCT_YX] + ( -pchr_a->getPosX() + pchr_a->getPosY() ) ) - ( -pprt_b->getPosX() + pprt_b->getPosY() ),
                              ( -pprt_b->getPosX() + pprt_b->getPosY() ) - ( chrMinCollision._mins[OCT_YX] + ( -pchr_a->getPosX() + pchr_a->getPosY() ) ) );

    collide_x  = odepth[OCT_X]  > 0.0f;
    collide_y  = odepth[OCT_Y]  > 0.0f;
    collide_xy = odepth[OCT_XY] > 0.0f;
    collide_yx = odepth[OCT_YX] > 0.0f;
    collide_z  = odepth[OCT_Z] > -PLATTOLERANCE && odepth[OCT_Z] < PLATTOLERANCE;

    if ( collide_x && collide_y && collide_xy && collide_yx && collide_z )
    {
        // check for the best possible attachment
        if ( pchr_a->getPosZ() + chrMinCollision._maxs[OCT_Z] > pprt_b->targetplatform_level )
        {
            pprt_b->targetplatform_level = pchr_a->getPosZ() + chrMinCollision._maxs[OCT_Z];
            pprt_b->targetplatform_ref   = ichr_a;

            attach_prt_to_platform(pprt_b.get(), pchr_a);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------
static bool attach_prt_to_platform( Ego::Particle * pprt, Object * pplat )
{
    /// @author BB
    /// @details attach a particle to a platform

    // verify that we do not have two dud pointers
    if (!pprt || pprt->isTerminated() ) return false;
    if (!pplat || pplat->isTerminated()) return false;

    // check if they can be connected
    if ( !pplat->isPlatform() ) return false;

    // do the attachment
    pprt->onwhichplatform_ref    = pplat->getObjRef();
    pprt->onwhichplatform_update = worldUpdateCount();
    pprt->targetplatform_ref     = ObjectRef::Invalid;

    // update the character's relationship to the ground
    const IPhysical& platformPhysical = physical(*pplat);
    pprt->setElevation( std::max( pprt->enviro.level, pplat->getPosZ() + platformPhysical.getMinCollisionVolume()._maxs[OCT_Z] ) );

    return true;
}
