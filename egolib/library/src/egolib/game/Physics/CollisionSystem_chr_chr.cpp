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
#include "CollisionSystem_internal.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Physics/physics.h"  // phys_expand_*/phys_estimate_* + PhysicalConstants (CHR_INFINITE_WEIGHT)

namespace Ego
{
namespace Physics
{

// get_recoil_factors stays a file-scope static: it is called only from do_chr_chr_collision below.
static void get_recoil_factors( float wta, float wtb, float * recoil_a, float * recoil_b );

bool do_chr_chr_collision(Object& objectA, Object& objectB, const float tmin, const float tmax)
{
    const ObjectRef ichr_a = objectA.getObjRef();
    const ObjectRef ichr_b = objectB.getObjRef();
    const oct_bb_t& objectAMinCollision = objectA.getMinCollisionVolume();
    const oct_bb_t& objectBMinCollision = objectB.getMinCollisionVolume();

    // platform interaction. if the onwhichplatform_ref is set, then
    // all collision tests have been met
    if ( ichr_a == objectB.onwhichplatform_ref )
    {
        // this is handled in ObjectPhysics.cpp
        return true;
    }

    // platform interaction. if the onwhichplatform_ref is set, then
    // all collision tests have been met
    if ( ichr_b == objectA.onwhichplatform_ref )
    {
        // this is handled in ObjectPhysics.cpp
        return true;
    }

    // items can interact with platforms but not with other characters/objects
    if ( (objectA.isItem() && !objectA.isPlatform()) || (objectB.isItem() && !objectB.isPlatform()) ) {
        return false;
    }

    // don't interact with your mount, or your held items
    if (ichr_a == objectB.getHolderRef() || ichr_b == objectA.getHolderRef()) {
        return false;
    }

    // don't do anything if there is no interaction strength
    if (0.0f == objectA.getInitialBump().size || 0.0f == objectB.getInitialBump().size) {
        return false;
    }

    float interaction_strength = 0.1f + (0.9f-objectA.phys.bumpdampen) * (0.9f-objectB.phys.bumpdampen);
    
    //ZF> This was supposed to make ghosts more insubstantial, but it also affects invisible characters
    //interaction_strength *= objectA.inst.alpha * idlib::fraction<float,1,255>();
    //interaction_strength *= objectB.inst.alpha * idlib::fraction<float,1,255>();

    // reduce your interaction strength if you have just detached from an object
    if ( objectA.getDismountObject() == ichr_b )
    {
        float dismount_lerp = ( float )objectA.getDismountTimer() / static_cast<float>(Object::PHYS_DISMOUNT_TIME);
        dismount_lerp = Ego::Math::constrain( dismount_lerp, 0.0f, 1.0f );

        interaction_strength *= dismount_lerp;
    }

    if ( objectB.getDismountObject() == ichr_a )
    {
        float dismount_lerp = ( float )objectB.getDismountTimer() / static_cast<float>(Object::PHYS_DISMOUNT_TIME);
        dismount_lerp = Ego::Math::constrain( dismount_lerp, 0.0f, 1.0f );

        interaction_strength *= dismount_lerp;
    }

    // seriously reduce the interaction_strength with mounts
    // this thould allow characters to mount certain mounts a lot easier
    if (( objectA.isMount() && ObjectRef::Invalid == objectA.getHeldObject(SLOT_LEFT) && !objectB.isMount() ) ||
        ( objectB.isMount() && ObjectRef::Invalid == objectB.getHeldObject(SLOT_LEFT) && !objectA.isMount() ) )
    {
        interaction_strength *= 0.75f;
    }

    // reduce the interaction strength with platforms
    // that are overlapping with the platform you are actually on
    if ( objectB.canUsePlatforms() && objectA.isPlatform() && ObjectRef::Invalid != objectB.onwhichplatform_ref && ichr_a != objectB.onwhichplatform_ref )
    {
        float lerp_z = ( objectB.getPosZ() - ( objectA.getPosZ() + objectAMinCollision._maxs[OCT_Z] ) ) / PLATTOLERANCE;
        lerp_z = Ego::Math::constrain(lerp_z, -1.0f, 1.0f);

        if ( lerp_z >= 0.0f )
        {
            interaction_strength = 0.0f;
        }
        else
        {
            interaction_strength *= -lerp_z;
        }
    }

    if ( objectA.canUsePlatforms() && objectB.isPlatform() && ObjectRef::Invalid != objectA.onwhichplatform_ref && ichr_b != objectA.onwhichplatform_ref )
    {
        float lerp_z = ( objectA.getPosZ() - ( objectB.getPosZ() + objectBMinCollision._maxs[OCT_Z] ) ) / PLATTOLERANCE;
        lerp_z = Ego::Math::constrain( lerp_z, -1.0f, +1.0f );

        if ( lerp_z >= 0.0f )
        {
            interaction_strength = 0.0f;
        }
        else
        {
            interaction_strength *= -lerp_z;
        }
    }

	// object bounding boxes shifted so that they are in the correct place on the map
	oct_bb_t map_bb_a, map_bb_b;

    // shift the character bounding boxes to be centered on their positions
    map_bb_a = idlib::translate(objectAMinCollision, objectA.getPosition());
    map_bb_b = idlib::translate(objectBMinCollision, objectB.getPosition());

    // make the object more like a table if there is a platform-like interaction
    float exponent = 1.0f;
    if ( objectA.canUsePlatforms() && objectB.isPlatform() ) exponent += 2;
    if ( objectB.canUsePlatforms() && objectA.isPlatform() ) exponent += 2;

	float recoil_a, recoil_b;



	Vector3f nrm;
	oct_vec_v2_t odepth;
	bool bump = false;

    // use the info from the collision volume to determine whether the objects are colliding
    bool collision = tmin > 0.0f;

    // estimate the collision normal at the point of contact
    bool valid_normal = false;
    float depth_min    = 0.0f;
    if ( collision )
    {
        // find the collision volumes at 10% overlap
        oct_bb_t exp1, exp2;

        float tmp_min = tmin;
        float tmp_max = tmin + ( tmax - tmin ) * 0.1f;

        // determine the expanded collision volumes for both objects
        phys_expand_oct_bb(map_bb_a, objectA.getVelocity(), tmp_min, tmp_max, exp1);
        phys_expand_oct_bb(map_bb_b, objectB.getVelocity(), tmp_min, tmp_max, exp2);

        valid_normal = phys_estimate_collision_normal(exp1, exp2, exponent, odepth, nrm, depth_min);
    }

    if ( !collision || depth_min <= 0.0f )
    {
        valid_normal = phys_estimate_pressure_normal(map_bb_a, map_bb_b, exponent, odepth, nrm, depth_min);
    }

    if ( depth_min <= 0.0f )
        return false;

    // if we can't obtain a valid collision normal, we fail
    if ( !valid_normal ) return false;

    //------------------
    // do character-character interactions

    // calculate a "mass" for each object, taking into account possible infinite masses
    float wta = objectA.getMass();
    float wtb = objectB.getMass();

    // make a special exception for interaction between "Mario platforms"
    if (( wta < 0.0f && objectA.isPlatform() ) && ( wtb < 0.0f && objectA.isPlatform() ) )
    {
        return false;
    }

    // make a special exception for immovable scenery objects
    // they can collide, but cannot push each other apart... that might mess up the scenery ;)
    if ( !collision && objectA.isScenery() && objectB.isScenery() )
    {
        return false;
    }

    // determine the relative effect of impulses, given the known weights
    get_recoil_factors( wta, wtb, &recoil_a, &recoil_b );

    //---- calculate the character-character interactions
    {
        const float max_pressure_strength = 0.25f;//1.0f - std::min(objectA.phys.dampen, objectB.phys.dampen);
        float pressure_strength     = max_pressure_strength * interaction_strength;

        Vector3f pdiff_a;

        bool need_displacement = false;
        bool need_velocity = false;

        Vector3f vdiff_a;

        if ( depth_min <= 0.0f || collision )
        {
            need_displacement = false;
            pdiff_a = idlib::zero<Vector3f>();
        }
        else
        {
            // add a small amount to the pressure difference so that
            // the function will actually separate the objects in a finite number
            // of iterations
            need_displacement = (recoil_a > 0.0f) || (recoil_b > 0.0f);
            pdiff_a = nrm * (depth_min + 1.0f);
        }

        // find the relative velocity
        vdiff_a = objectB.getVelocity() - objectA.getVelocity();

        need_velocity = false;
        if (idlib::manhattan_norm(vdiff_a) > 1e-6)
        {
            need_velocity = (recoil_a > 0.0f) || (recoil_b > 0.0f);
        }

        //---- handle the relative velocity
        if ( need_velocity )
        {

            // what type of interaction is this? (collision or pressure)
            if ( collision )
            {
                // !!!! COLLISION !!!!

                // an actual bump, use impulse to make the objects bounce appart

                Vector3f vdiff_para_a, vdiff_perp_a;

                // generic coefficient of restitution.
                float cr = objectA.phys.dampen * objectB.phys.dampen;

                // decompose this relative to the collision normal
                fvec3_decompose(vdiff_a, nrm, vdiff_perp_a, vdiff_para_a);

                if (recoil_a > 0.0f)
                {
                    Vector3f vimp_a = vdiff_perp_a * +(recoil_a * (1.0f + cr) * interaction_strength);
                    objectA.phys.sum_avel(vimp_a);
                }

                if (recoil_b > 0.0f)
                {
                    Vector3f vimp_b = vdiff_perp_a * -(recoil_b * (1.0f + cr) * interaction_strength);
                    objectB.phys.sum_avel(vimp_b);
                }

                // this was definitely a bump
                bump = true;
            }
            else
            {
                // !!!! PRESSURE !!!!

                // not a bump at all. two objects are rubbing against one another
                // and continually overlapping.
                //
                // reduce the relative velocity if the objects are moving towards each other,
                // but ignore it if they are moving away.

                // use pressure to push them appart. reduce their relative velocities.

                float distance = idlib::euclidean_norm(objectA.getPosition() - objectB.getPosition());
                distance /= std::max(objectA.getCurrentBump().size, objectB.getCurrentBump().size);
                if(distance > 0.0f)
                {
                    objectA.phys.sum_avel(nrm * distance * recoil_a * interaction_strength);
                    objectB.phys.sum_avel(-nrm * distance * recoil_b * interaction_strength);

                    // This is genuine contact (depth_min > 0, guaranteed by the early-return above)
                    // with genuine relative motion (need_velocity, guaranteed by the enclosing "if"):
                    // that is enough to call it a "bump" for alerting purposes, without also requiring
                    // a velocity sign-flip across the contact normal. A player walking into a
                    // stationary object never produces a sign-flip (their velocity is regenerated
                    // toward the same setpoint every tick), so the old sign-flip-only predicate left
                    // scripts like a chest's IfBumped silent on ordinary walk-in contact.
                    //
                    // This is a deliberate, narrower deviation from the reference engine (2.6.8
                    // char.c), which alerts on every overlapping tick not resolved as platform
                    // stacking (gated only by nonzero bump height): a permanently-resting overlapped
                    // pair has zero relative velocity, so need_velocity is false, this block never
                    // runs, and the pair stays silent instead of alerting forever.
                    bump = true;
                }
            }

        }

        //---- fix the displacement regardless of what kind of interaction
        if ( need_displacement )
        {
            if ( recoil_a > 0.0f )
            {
                Vector3f pimp_a = pdiff_a * +(recoil_a * pressure_strength);
                objectA.phys.sum_acoll(pimp_a);
            }

            if ( recoil_b > 0.0f )
            {
                Vector3f pimp_b = pdiff_a * -(recoil_b * pressure_strength);
                objectB.phys.sum_acoll(pimp_b);
            }
        }
    }

    if ( bump )
    {
        objectA.recordAIBump(ichr_b);
        objectB.recordAIBump(ichr_a);

        //Destroy stealth for both objects if they are not friendly
        //
        // NOTE: this block used to only run on a velocity sign-flip (a "real" bump), so it was
        // effectively unreachable for ordinary pressure contact. The widened `bump` predicate
        // above now also drives this block on every pressure-contact tick with relative motion
        // (not just swept collisions), so a stealthed character in sustained contact with a
        // hostile now gets revealed on ordinary walk-in/rub contact where it previously did not.
        // This is a real gameplay-frequency change beyond alert delivery, not just "alert-only".
        if(!objectA.isScenery() && !objectB.isScenery() && objectA.getTeam().hatesTeam(objectB.getTeam())) {
            if(!objectA.hasPerk(Ego::Perks::SHADE)) objectA.deactivateStealth();
            if(!objectB.hasPerk(Ego::Perks::SHADE)) objectB.deactivateStealth();
        }
    }

    return true;
}

static void get_recoil_factors( float wta, float wtb, float * recoil_a, float * recoil_b )
{
    float loc_recoil_a, loc_recoil_b;

    if ( NULL == recoil_a ) recoil_a = &loc_recoil_a;
    if ( NULL == recoil_b ) recoil_b = &loc_recoil_b;

    if ( wta >= Ego::Physics::CHR_INFINITE_WEIGHT ) wta = -static_cast<float>(Ego::Physics::CHR_INFINITE_WEIGHT);
    if ( wtb >= Ego::Physics::CHR_INFINITE_WEIGHT ) wtb = -static_cast<float>(Ego::Physics::CHR_INFINITE_WEIGHT);

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

} //namespace Physics
} //namespace Ego
