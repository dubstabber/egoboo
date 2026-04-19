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

/// @file egolib/game/game_targeting.c
/// @brief AI targeting — target finding and validation for characters and particles

#include "egolib/game/game_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
ObjectRef prt_find_target( const Ego::Vector3f& pos, Facing facing,
                           const PIP_REF particletype, const TEAM_REF team,
	                       ObjectRef donttarget, ObjectRef oldtarget, Facing *targetAngle )
{
    /// @author ZF
    /// @details This is the new improved targeting system for particles. Also includes distance in the Z direction.
    GameModule& module = activeModule();

    const float max_dist2 = WIDE * WIDE;

    std::shared_ptr<ParticleProfile> ppip;

    ObjectRef besttarget = ObjectRef::Invalid;
    float  longdist2 = max_dist2;

    facing = idlib::canonicalize(facing);

    if ( !EngineContext::get().profileSystem().isParticleProfileLoaded( particletype ) ) return ObjectRef::Invalid;
    ppip = EngineContext::get().profileSystem().getParticleProfile( particletype );

    for(const std::shared_ptr<Object> &pchr : module.getObjectHandler().iterator())
    {
        if ( !pchr->isAlive() || pchr->isItem() || module.getObjectHandler().exists( pchr->getInventoryHolderRef() ) ) continue;

        // prefer targeting riders over the mount itself
        if ( pchr->isMount() && ( module.getObjectHandler().exists( pchr->getHeldObject(SLOT_LEFT) ) || module.getObjectHandler().exists( pchr->getHeldObject(SLOT_RIGHT) ) ) ) continue;

        // ignore invictus
        if ( pchr->isInvincible() ) continue;

        // we are going to give the player a break and not target things that
        // can't be damaged, unless the particle is homing. If it homes in,
        // the he damage_timer could drop off en route.
        if ( !ppip->homing && ( 0 != pchr->getDamageTimer() ) ) continue;

        // Don't retarget someone we already had or not supposed to target
        if ( pchr->getObjRef() == oldtarget || pchr->getObjRef() == donttarget ) continue;

        Team &particleTeam = module.getTeamList()[team];

        bool target_friend = ppip->onlydamagefriendly && particleTeam == pchr->getTeam();
        bool target_enemy  = !ppip->onlydamagefriendly && particleTeam.hatesTeam(pchr->getTeam() );

        if ( target_friend || target_enemy )
        {
            Facing angle = Facing(FACING_T(-facing + vec_to_facing( pchr->getPosX() - pos[kX] , pchr->getPosY() - pos[kY] )));

            // Only proceed if we are facing the target
            if ( angle < Facing(ppip->targetangle) || angle > Facing( 0xFFFF - ppip->targetangle ))
            {
                float dist2 = idlib::squared_euclidean_norm(pchr->getPosition() - pos);

                if ( dist2 < longdist2 && dist2 <= max_dist2 )
                {
                    (*targetAngle) = angle;
                    besttarget = pchr->getObjRef();
                    longdist2 = dist2;
                }
            }
        }
    }

    // All done
    return besttarget;
}

//--------------------------------------------------------------------------------------------
bool chr_check_target( Object * psrc, const std::shared_ptr<Object>& ptst, const IDSZ2 &idsz, const BIT_FIELD targeting_bits )
{
    GameModule& module = activeModule();
    bool retval = false;

    // Skip non-existing objects
    if (!psrc || psrc->isTerminated()) return false;

    // Skip hidden characters
    if ( ptst->isHidden() ) return false;

    // Players only?
    if (( HAS_SOME_BITS( targeting_bits, TARGET_PLAYERS ) || HAS_SOME_BITS( targeting_bits, TARGET_QUEST ) ) && !ptst->isPlayer() ) return false;

    // Skip held objects
    if ( ptst->isBeingHeld() ) return false;

    // Allow to target ourselves?
    if ( psrc == ptst.get() && HAS_NO_BITS( targeting_bits, TARGET_SELF ) ) return false;

    // Don't target our holder if we are an item and being held
    if ( psrc->isItem() && psrc->getHolderRef() == ptst->getObjRef() ) return false;

    // Allow to target dead stuff?
    if ( ptst->isAlive() == HAS_SOME_BITS( targeting_bits, TARGET_DEAD ) ) return false;

    // Don't target invisible stuff, unless we can actually see them
    if ( !psrc->canSeeObject(ptst) ) return false;

    //Need specific skill? ([NONE] always passes)
    if ( HAS_SOME_BITS( targeting_bits, TARGET_SKILL ) && !ptst->hasSkillIDSZ(idsz) ) return false;

    // Require player to have specific quest?
    if ( HAS_SOME_BITS( targeting_bits, TARGET_QUEST ) )
    {
        if(!ptst->isPlayer()) {
            return false;
        }

        std::shared_ptr<Ego::Player>& player = module.getPlayer(ptst->getPlayerNumber());

        // find only active quests?
        // this makes it backward-compatible with zefz's version
        if (!player->getQuestLog().hasActiveQuest(idsz)) {
            return false;
        }
    }

    bool is_hated = psrc->getTeam().hatesTeam(ptst->getTeam());

    // Target neutral items? (still target evil items, could be pets)
    if (( ptst->isItem() || ptst->isInvincible() ) && !HAS_SOME_BITS( targeting_bits, TARGET_ITEMS ) ) return false;

    // Only target those of proper team. Skip this part if it's a item
    if ( !ptst->isItem() )
    {
        if (( HAS_NO_BITS( targeting_bits, TARGET_ENEMIES ) && is_hated ) ) return false;
        if (( HAS_NO_BITS( targeting_bits, TARGET_FRIENDS ) && !is_hated ) ) return false;
    }

    //This is the last and final step! Check for specific IDSZ too? (not needed if we are looking for a quest)
    if ( IDSZ2::None == idsz || HAS_SOME_BITS( targeting_bits, TARGET_QUEST ) )
    {
        retval = true;
    }
    else
    {
        bool match_idsz = ( idsz == ptst->getProfile()->getIDSZ(IDSZ_PARENT) ) ||
                            ( idsz == ptst->getProfile()->getIDSZ(IDSZ_TYPE) );

        if ( match_idsz )
        {
            if ( !HAS_SOME_BITS( targeting_bits, TARGET_INVERTID ) ) retval = true;
        }
        else
        {
            if ( HAS_SOME_BITS( targeting_bits, TARGET_INVERTID ) ) retval = true;
        }
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
ObjectRef chr_find_target( Object * psrc, float max_dist, const IDSZ2& idsz, const BIT_FIELD targeting_bits )
{
    /// @author ZF
    /// @details This is the new improved AI targeting algorithm. Also includes distance in the Z direction.
    ///     If max_dist is 0 then it searches without a max limit.

    GameModule& module = activeModule();
    line_of_sight_info_t los_info;

    if (!psrc || psrc->isTerminated()) return ObjectRef::Invalid;

    std::vector<std::shared_ptr<Object>> searchList;

    //Only loop through the players
    if ( HAS_SOME_BITS( targeting_bits, TARGET_PLAYERS ) || HAS_SOME_BITS( targeting_bits, TARGET_QUEST ) )
    {
        for(const std::shared_ptr<Ego::Player> &player : module.getPlayerList())
        {
            const std::shared_ptr<Object> &object = player->getObject();
            if(player) {

                //Within range?
                float distance = idlib::euclidean_norm(object->getPosition() - psrc->getPosition());
                if(max_dist == NEAREST || distance < max_dist) {
                    searchList.push_back(object);
                }

            }
        }
    }

    //All objects in level
    else if(max_dist == NEAREST)
    {
        searchList = module.getObjectHandler().getAllObjects();
    }

    //All objects within range
    else
    {
        searchList = module.getObjectHandler().findObjects(psrc->getPosX(), psrc->getPosY(), max_dist, true);
    }


    // set the line-of-sight source
    los_info.x0         = psrc->getPosX();
    los_info.y0         = psrc->getPosY();
    los_info.z0         = psrc->getPosZ() + psrc->getCurrentBump().height;
    los_info.stopped_by = psrc->getStoppedByMask();

    ObjectRef best_target = ObjectRef::Invalid;
    float best_dist2  = (max_dist == NEAREST) ? std::numeric_limits<float>::max() : max_dist*max_dist + 1.0f;
    for(const std::shared_ptr<Object> &ptst : searchList)
    {
        if(ptst->isTerminated()) continue;

        //Skip held items
        if(ptst->isBeingHeld()) continue;

        if (!chr_check_target(psrc, ptst, idsz, targeting_bits)) continue;

		float dist2 = idlib::squared_euclidean_norm(psrc->getPosition() - ptst->getPosition());
        if (dist2 < best_dist2)
        {
            //Invictus chars do not need a line of sight
            if ( !psrc->isInvincible() )
            {
                // set the line-of-sight source
                los_info.x1 = ptst->getPosition()[kX];
                los_info.y1 = ptst->getPosition()[kY];
                los_info.z1 = ptst->getPosition()[kZ] + std::max( 1.0f, ptst->getCurrentBump().height );

                if ( line_of_sight_info_t::blocked( los_info, module.getMeshPointer() ) ) continue;
            }

            //Set the new best target found
            best_target = ptst->getObjRef();
            best_dist2  = dist2;
        }
    }

    return best_target;
}
