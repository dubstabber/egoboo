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

/// @file egolib/game/Physics/particle_collision_response.c
/// @brief Character-particle collision response: interaction detection and orchestration.
/// @details The do_chr_prt_collision entry-point orchestrator plus the interaction-detection
///          steps it drives directly: get_details (collision-volume classification), init
///          (data-block setup), and bump (friend-foe / damage-validity predicate). The
///          damage/deflect/knockback steps live in particle_collision_response_damage.c and
///          the bump-spawn fallout in particle_collision_response_bump.c, both dispatched
///          through particle_collision_response_internal.h. Stays in egolib-library (game layer).

#include "egolib/game/Physics/particle_collision_response_internal.h"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

chr_prt_collision_data_t::chr_prt_collision_data_t() :
    ichr(),
    pchr(nullptr),

    iprt(),
    pprt(nullptr),
    ppip(nullptr),

    int_min(false),
    depth_min(0.0f),
    int_max(false),
    depth_max(0.0f),

    is_impact(false),
    is_pressure(false),
    is_collision(false),
    dot(0.0f),
    nrm(0.0f, 0.0f, 1.0f),

    mana_paid(false),
    max_damage(0),
    actual_damage(0),
    vdiff(),
    vdiff_para(),
    vdiff_perp(),
    block_factor(0.0f),

    terminate_particle(false),
    prt_bumps_chr(false),
    prt_damages_chr(false)
{
    //ctor
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static bool do_chr_prt_collision_init( const ObjectRef ichr, const ParticleRef iprt, chr_prt_collision_data_t * pdata );
static bool do_chr_prt_collision_get_details( chr_prt_collision_data_t& pdata, const float tmin, const float tmax );
static bool do_chr_prt_collision_bump( chr_prt_collision_data_t& pdata );

//--------------------------------------------------------------------------------------------

bool do_chr_prt_collision_get_details(chr_prt_collision_data_t& pdata, const float tmin, const float tmax)
{
    // Get details about the character-particle interaction
    //
    // We already know that the largest particle cv intersects with the a
    // character cv sometime this frame. We need more details to know
    // how to handle the collision.

    oct_bb_t cv_chr, cv_prt_max, cv_prt_min;
    oct_vec_v2_t odepth;

    // make the object more like a table if there is a platform-like interaction
    float exponent = 1;
    if ( SPRITE_SOLID == pdata.pprt->type && pdata.pchr->isPlatform() ) exponent += 2;

    // assume the simplest interaction normal
    pdata.nrm = Ego::Vector3f(0.0f, 0.0f, 1.0f);

    // no valid interactions, yet
    bool handled = false;

    // shift the source bounding boxes to be centered on the given positions
    const IPhysical& physicalCharacter = physical(*pdata.pchr);
    cv_chr = idlib::translate(physicalCharacter.getMinCollisionVolume(), pdata.pchr->getPosition());

    // the smallest particle collision volume
    cv_prt_min = idlib::translate(pdata.pprt->prt_min_cv, pdata.pprt->getPosition());

    // the largest particle collision volume (the hit-box)
    cv_prt_max = idlib::translate(pdata.pprt->prt_max_cv, pdata.pprt->getPosition());

    if ( tmin <= 0.0f || std::abs( tmin ) > 1e6 || std::abs( tmax ) > 1e6 )
    {
        // use "pressure" to determine the normal and overlap
        phys_estimate_pressure_normal(cv_prt_min, cv_chr, exponent, odepth, pdata.nrm, pdata.depth_min);

        handled = true;
        if ( tmin <= 0.0f )
        {
            handled = pdata.depth_min > 0.0f;
        }

        // tag the type of interaction
        pdata.int_min = handled;
        pdata.is_pressure = handled;
    }
    else
    {
        // find the collision volumes at 10% overlap
        oct_bb_t exp1, exp2;

        float tmp_min, tmp_max;

        tmp_min = tmin;
        tmp_max = tmin + ( tmax - tmin ) * 0.1f;

        // determine the expanded collision volumes for both objects
        phys_expand_oct_bb(cv_prt_min, pdata.pprt->getVelocity(), tmp_min, tmp_max, exp1);
        phys_expand_oct_bb(cv_chr,     pdata.pchr->getVelocity(), tmp_min, tmp_max, exp2);

        // use "collision" to determine the normal and overlap
        handled = phys_estimate_collision_normal(exp1, exp2, exponent, odepth, pdata.nrm, pdata.depth_min);

        // tag the type of interaction
        pdata.int_min      = handled;
        pdata.is_collision = handled;
    }

    if ( !handled )
    {
        if ( tmin <= 0.0f || std::abs( tmin ) > 1e6 || std::abs( tmax ) > 1e6 )
        {
            // use "pressure" to determine the normal and overlap
            phys_estimate_pressure_normal(cv_prt_max, cv_chr, exponent, odepth, pdata.nrm, pdata.depth_max);

            handled = true;
            if ( tmin <= 0.0f )
            {
                handled = pdata.depth_max > 0.0f;
            }

            // tag the type of interaction
            pdata.int_max     = handled;
            pdata.is_pressure = handled;
        }
        else
        {
            // find the collision volumes at 10% overlap
            oct_bb_t exp1, exp2;

            float tmp_min, tmp_max;

            tmp_min = tmin;
            tmp_max = tmin + ( tmax - tmin ) * 0.1f;

            // determine the expanded collision volumes for both objects
            phys_expand_oct_bb(cv_prt_max, pdata.pprt->getVelocity(), tmp_min, tmp_max, exp1);
            phys_expand_oct_bb(cv_chr,     pdata.pchr->getVelocity(), tmp_min, tmp_max, exp2);

            // use "collision" to determine the normal and overlap
            handled = phys_estimate_collision_normal(exp1, exp2, exponent, odepth, pdata.nrm, pdata.depth_max);

            // tag the type of interaction
            pdata.int_max      = handled;
            pdata.is_collision = handled;
        }
    }

    return handled;
}

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision_bump( chr_prt_collision_data_t& pdata )
{
    const float maxDamage = std::abs(pdata.pprt->damage.base) + std::abs(pdata.pprt->damage.rand);
    const IDamageable& constDamageableCharacter = damageable(*pdata.pchr);

    // always allow valid reaffirmation
    if ((constDamageableCharacter.getReaffirmDamageType() < DAMAGE_COUNT) &&
        ( pdata.pprt->damagetype < DAMAGE_COUNT ) &&
        (constDamageableCharacter.getReaffirmDamageType() == pdata.pprt->damagetype) &&
        ( maxDamage > 0) )
    {
        return true;
    }

    //Only allow one collision per particle unless that particle is eternal
    if(!pdata.pprt->isEternal() && pdata.pprt->hasCollided(objectWorld().getObjectHandler()[pdata.pchr->getObjRef()])) {
        return false;
    }

    bool prt_belongs_to_chr = (pdata.pchr->getObjRef() == pdata.pprt->owner_ref);

    if ( !prt_belongs_to_chr )
    {
        // no simple owner relationship. Check for something deeper.
		ObjectRef prt_owner = pdata.pprt->getOwner();
        if ( objectWorld().getObjectHandler().exists( prt_owner ) )
        {
            ObjectRef chr_wielder = chr_get_lowest_attachment( pdata.pchr->getObjRef(), true );
			ObjectRef prt_wielder = chr_get_lowest_attachment( prt_owner, true );

            if ( !objectWorld().getObjectHandler().exists( chr_wielder ) ) chr_wielder = pdata.pchr->getObjRef();
            if ( !objectWorld().getObjectHandler().exists( prt_wielder ) ) prt_wielder = prt_owner;

            prt_belongs_to_chr = (chr_wielder == prt_wielder);
        }
    }

    // does the particle team hate the character's team
    bool prt_hates_chr = team_hates_team( pdata.pprt->team, pdata.pchr->getTeamRef() );

    // Only bump into hated characters?
    bool valid_onlydamagehate = prt_hates_chr && pdata.pprt->getProfile()->hateonly;

    // allow neutral particles to attack anything
    bool prt_attacks_chr = false;
    if(prt_hates_chr || ((Team::TEAM_NULL != pdata.pchr->getTeamRef()) && (Team::TEAM_NULL == pdata.pprt->team)) ) {
        prt_attacks_chr = (maxDamage > 0);
    }

    // this is the onlydamagefriendly condition from the particle search code
    bool valid_onlydamagefriendly = (pdata.ppip->onlydamagefriendly && pdata.pprt->team == pdata.pchr->getTeamRef())
		                         || (!pdata.ppip->onlydamagefriendly && prt_attacks_chr);

    // I guess "friendly fire" does not mean "self fire", which is a bit unfortunate.
    bool valid_friendlyfire = (pdata.ppip->friendlyfire && !prt_hates_chr && !prt_belongs_to_chr)
		                   || (!pdata.ppip->friendlyfire && prt_attacks_chr);

    pdata.prt_bumps_chr = valid_friendlyfire || valid_onlydamagefriendly || valid_onlydamagehate;

    return pdata.prt_bumps_chr;
}

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision_init( const ObjectRef ichr, const ParticleRef iprt, chr_prt_collision_data_t * pdata )
{
    if ( NULL == pdata ) return false;

    *pdata = chr_prt_collision_data_t();

    if ( !activeParticleHandler()[iprt] ) return false;
    pdata->iprt = iprt;
    pdata->pprt = activeParticleHandler()[iprt];

    // make sure that it is on
    if ( !objectWorld().getObjectHandler().exists( ichr ) ) return false;
    pdata->ichr = ichr;
    pdata->pchr = objectWorld().getObjectHandler().get( ichr );

    pdata->ppip = pdata->pprt->getProfile();

    // estimate the maximum possible "damage" from this particle
    // other effects can magnify this number, like vulnerabilities
    // or DAMFX_* bits
    pdata->max_damage = std::abs( pdata->pprt->damage.base ) + std::abs( pdata->pprt->damage.rand );

    return true;
}

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision(const ObjectRef object, const ParticleRef particle, const float tmin, const float tmax)
{
    /// @author BB
    /// @details this funciton goes through all of the steps to handle character-particle
    ///               interactions. A basic interaction has been detected. This needs to be refined
    ///               and then handled. The function returns false if the basic interaction was wrong
    ///               or if the interaction had no effect.
    ///
    /// @note This function is a little more complicated than the character-character case because
    ///       of the friend-foe logic as well as the damage and other special effects that particles can do.

    bool retval = false;

    chr_prt_collision_data_t cn_data;

    bool intialized = do_chr_prt_collision_init(object, particle, &cn_data );
    if ( !intialized ) return false;

    // ignore dead characters
    if ( !cn_data.pchr->isAlive() ) return false;

    // skip objects that are inside inventories
    if ( cn_data.pchr->isInsideInventory() ) return false;

    // if the particle is attached to this character, ignore a "collision"
    if ( cn_data.pprt->getAttachedObjectID() == cn_data.ichr )
    {
        return false;
    }

    // is there any collision at all?
    if ( !do_chr_prt_collision_get_details(cn_data, tmin, tmax) )
    {
        return false;
    }
    else
    {
        // help classify impacts

        if ( cn_data.is_pressure )
        {
            // on the odd chance that we want to use the pressure
            // algorithm for an obvious collision....
            if ( tmin > 0.0f ) cn_data.is_impact = true;

        }

        if ( cn_data.is_collision )
        {
            cn_data.is_impact = true;
        }
    }

    // if there is no collision, no point in going farther
    if (!cn_data.int_min && !cn_data.int_max) return false;

    // if the particle is not actually hitting the object, then limit the
    // interaction to 2d
    if (cn_data.int_max && !cn_data.int_min)
    {
        // do not re-normalize this vector
        cn_data.nrm[kZ] = 0.0f;
    }

    // find the relative velocity
    cn_data.vdiff = cn_data.pchr->getVelocity() - cn_data.pprt->getVelocity();

    // decompose the relative velocity parallel and perpendicular to the surface normal
    cn_data.dot = fvec3_decompose(cn_data.vdiff, cn_data.nrm, cn_data.vdiff_perp, cn_data.vdiff_para);

    // refine the logic for a particle to hit a character
    bool prt_can_hit_chr = do_chr_prt_collision_bump(cn_data);

    // determine whether the particle is deflected by the character
    const bool prt_deflected = prt_can_hit_chr && do_chr_prt_collision_deflect(cn_data);
    if (prt_deflected) {
        retval = true;
        prt_can_hit_chr = false;
    }

    // Torches and such are marked as invulnerable, so the particle is always deflected.
    // make a special case for reaffirmation
    const IDamageable& damageableCharacter = damageable(*cn_data.pchr);
    if (0 == damageableCharacter.getDamageTimer() )
    {
        // Check reaffirmation of particles
        if ( damageableCharacter.getReaffirmDamageType() == cn_data.pprt->damagetype )
        {
            // This prevents items in shops from being burned
            if ( !cn_data.pchr->isShopItem() )
            {
                if ( 0 != reaffirm_attached_particles( cn_data.ichr ) )
                {
                    retval = true;
                }
            }
        }
    }

    //Do they hit each other?
    if(prt_can_hit_chr && 0 == damageableCharacter.getDamageTimer())
    {
        bool dodged = false;

        //Does the character have a dodge ability?
        if(cn_data.pchr->hasPerk(Ego::Perks::DODGE)) {
            float dodgeChance = cn_data.pchr->getAttribute(Ego::Attribute::AGILITY);

            //Masterful Dodge Perk gives flat +10% dodge chance
            if(cn_data.pchr->hasPerk(Ego::Perks::MASTERFUL_DODGE)) {
                dodgeChance += 10.0f;
            }

            //1% dodge chance per Agility
            if(Random::getPercent() <= dodgeChance)
            {
                dodged = true;
            }
        }

        if(!dodged) {
            // do "damage" to the character
            if (!prt_deflected)
            {
                // we can't even get to this point if the character is completely invulnerable (invictus)
                // or can't be damaged this round
                cn_data.prt_damages_chr = do_chr_prt_collision_damage( cn_data );
                if ( cn_data.prt_damages_chr )
                {
                    //Remember the collision so that this doesn't happen again
                    cn_data.pprt->addCollision(objectWorld().getObjectHandler()[cn_data.pchr->getObjRef()]);
                    retval = true;
                }
            }

            //Cause knockback (Hold the Line perk makes Objects immune to knockback)
            if(!cn_data.pchr->hasPerk(Ego::Perks::HOLD_THE_LINE)) {
                do_chr_prt_collision_knockback(cn_data);
            }
        }

        //Attack was dodged!
        else {
            //Cannot collide again
            cn_data.pprt->addCollision(objectWorld().getObjectHandler()[cn_data.pchr->getObjRef()]);

            //Play sound effect
            audioSystem().playSound(cn_data.pchr->getPosition(), audioSystem().getGlobalSound(GSND_DODGE));

            // Initialize for the billboard
            Ego::Graphics::activeBillboardSystem().makeBillboard( cn_data.pchr->getObjRef(), "Dodged!", Ego::Colour4f::white(), Ego::Colour4f(1.0f, 0.6f, 0.0f, 1.0f), 3, Ego::Graphics::Billboard::Flags::All);
        }


        // handle a couple of special cases (grabbing money)
        if (cn_data.prt_bumps_chr)
        {
            if ( do_chr_prt_collision_handle_bump(cn_data) )
            {
                retval = true;
            }
        }
    }

    // terminate the particle if needed
    if ( cn_data.terminate_particle )
    {
        cn_data.pprt->requestTerminate();
        retval = true;
    }

    return retval;
}

bool do_chr_prt_collision(const std::shared_ptr<Object> &object, const std::shared_ptr<Ego::Particle> &particle, const float tmin, const float tmax)
{
    if (!object || !particle) return false;
    return do_chr_prt_collision(object->getObjRef(), particle->getParticleID(), tmin, tmax);
}
