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

/// @file egolib/game/Physics/particle_collision_response_bump.c
/// @brief Character-particle collision response: bump effects and bump-particle spawn.
/// @details The bump-consequence cluster dispatched by do_chr_prt_collision: applying the
///          bump outcome (handle_bump -- fire-spawn, money pickup, particle termination) and
///          the bump-particle spawning fallout (spawn_bump_particles). Shares the
///          chr_prt_collision_data_t block and helpers through
///          particle_collision_response_internal.h. Stays in egolib-library (game layer).

#include "egolib/game/Physics/particle_collision_response_internal.h"

//Private functions
static int spawn_bump_particles(ObjectRef objectRef, const ParticleRef particle);

//--------------------------------------------------------------------------------------------
bool do_chr_prt_collision_handle_bump( chr_prt_collision_data_t& pdata )
{
    if ( !pdata.prt_bumps_chr ) return false;

    if ( !pdata.prt_bumps_chr ) return false;

    // Catch on fire
    spawn_bump_particles( pdata.pchr->getObjRef(), pdata.pprt->getParticleID() );

    // handle some special particle interactions
    if ( pdata.pprt->getProfile()->end_bump )
    {
        if (pdata.pprt->getProfile()->bump_money)
        {
            Object *pcollector = pdata.pchr;

            // Let mounts collect money for their riders
            if (pdata.pchr->isMount())
            {
                // if the mount's rider can't get money, the mount gets to keep the money!
                const std::shared_ptr<Object> &rider = heldItem(*pdata.pchr, SLOT_LEFT);
                if (rider != nullptr && rider->getProfile()->canGrabMoney()) {
                    pcollector = rider.get();
                }
            }

            const IDamageable& damageableCollector = damageable(*pcollector);
            if ( pcollector->getProfile()->canGrabMoney() && pcollector->isAlive() && 0 == damageableCollector.getDamageTimer() && pcollector->getMoney() < Object::MAXMONEY)
            {
                pcollector->giveMoney(pdata.pprt->getProfile()->bump_money);

                // the coin disappears when you pick it up
                pdata.terminate_particle = true;
            }
        }
        else
        {
            // Only hit one character, not several
            pdata.terminate_particle = true;
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------
int spawn_bump_particles(ObjectRef character, const ParticleRef particle)
{
    /// @author ZZ
    /// @details This function is for catching characters on fire and such

    const std::shared_ptr<Ego::Particle> &pprt = activeParticleHandler()[particle];
    if(!pprt || pprt->isTerminated()) {
        return 0;
    }

    const std::shared_ptr<ParticleProfile> &ppip = pprt->getProfile();

    // no point in going on, is there?
    if (0 == ppip->bumpspawn._amount && !ppip->spawnenchant) return 0;
    int amount = ppip->bumpspawn._amount;

    if (!objectWorld().getObjectHandler().exists(character)) return 0;
    Object *pchr = objectWorld().getObjectHandler().get(character);

    int bs_count = 0;

    // Only damage if hitting from proper direction
    Facing direction = vec_to_facing(pprt->getVelocity().x(), pprt->getVelocity().y());
    const IPhysical& physicalCharacter = physical(*pchr);
    direction = ATK_BEHIND + physicalCharacter.getFacingZ() - direction;

    // Check that direction
    if (ppip->hasBit(DAMFX_NBLOC) || !pchr->isInvictusDirection(direction))
    {
        // Spawn new enchantments
        if (ppip->spawnenchant)
        {
            const std::shared_ptr<ObjectProfile> &spawnerProfile = activeProfileSystem().getProfile(pprt->getSpawnerProfile());
            pchr->addEnchant(spawnerProfile->getEnchantRef(), pprt->getSpawnerProfile().get(), objectWorld().getObjectHandler()[pprt->owner_ref], Object::INVALID_OBJECT);
        }

        // Spawn particles - this has been modded to maximize the visual effect
        // on a given target. It is not the most optimal solution for lots of particles
        // spawning. Thst would probably be to make the distance calculations and then
        // to quicksort the list and choose the n closest points.
        //
        // however, it seems that the bump particles in game rarely attach more than
        // one bump particle

        //check if we resisted the attack, we could resist some of the particles or none
        for (int cnt = 0; cnt < amount; cnt++)
        {
            if (Random::nextFloat() <= pchr->getDamageReduction(pprt->damagetype)) amount--;
        }

        if (amount > 0 && !pchr->getProfile()->hasResistBumpSpawn() && !pchr->isInvincible())
        {
            int slot_count = 0;

            if (pchr->getProfile()->isSlotValid(SLOT_LEFT)) slot_count++;
            if (pchr->getProfile()->isSlotValid(SLOT_RIGHT)) slot_count++;

            // Compute number of grip vertices.
            // Ensure that the number of grip vertices is at least one.
            int grip_verts = 0 == slot_count ? 1 : GRIP_VERTS * slot_count;
            // Compute the number of vertices.
            // Ensure that the number of vertices is non-negative.
            int vertices = (int)pchr->getVertexCount() - (int)grip_verts;
            vertices = std::max(0, vertices);

            if (vertices != 0)
            {
                auto vertex_occupied = std::make_unique<ParticleRef[]>(vertices);
                auto vertex_distance = std::make_unique<float[]>(vertices);

                // this could be done more easily with a quicksort....
                // but I guess it doesn't happen all the time
                float dist = idlib::manhattan_norm(pprt->getPosition() - pchr->getPosition());

                // clear the occupied list
                float z = pprt->getPosZ() - pchr->getPosZ();
                Facing facing = idlib::canonicalize(pprt->facing - physicalCharacter.getFacingZ());
                Facing turn = facing;
                float fsin = std::sin(turn);
                float fcos = std::cos(turn);
                float x = dist * fcos;
                float y = dist * fsin;

                // prepare the array values
                for (int cnt = 0; cnt < vertices; cnt++)
                {
                    dist = std::abs(x - pchr->getVertex(vertices - cnt - 1).pos[XX])
                         + std::abs(y - pchr->getVertex(vertices - cnt - 1).pos[YY])
                         + std::abs(z - pchr->getVertex(vertices - cnt - 1).pos[ZZ]);

                    vertex_distance[cnt] = dist;
                    vertex_occupied[cnt] = ParticleRef::Invalid;
                }

                // determine if some of the vertex sites are already occupied
                for(const std::shared_ptr<Ego::Particle> &particle : activeParticleHandler().iterator())
                {
                    if(particle->isTerminated()) continue;

                    if (pchr != particle->getAttachedObject().get()) continue;

                    if (particle->attachedto_vrt_off < vertices)
                    {
                        vertex_occupied[particle->attachedto_vrt_off] = particle->getParticleID();
                    }
                }

                    // Find best vertices to attach the particles to
                    for (int cnt = 0; cnt < amount; cnt++)
                    {
                        int bestvertex = 0;
                        uint32_t bestdistance = std::numeric_limits<uint32_t>::max(); //Really high number

                        for (int i = 0; i < vertices; i++)
                        {
                            if (ParticleRef::Invalid != vertex_occupied[i])
                                continue;

                            if (vertex_distance[i] < bestdistance)
                            {
                                bestdistance = vertex_distance[i];
                                bestvertex = i;
                            }
                        }

                        std::shared_ptr<Ego::Particle> bs_part =
                            activeParticleHandler().spawnLocalParticle(pchr->getPosition(), idlib::canonicalize(physicalCharacter.getFacingZ()), ObjectProfileRef(pprt->getSpawnerProfile()), ppip->bumpspawn._lpip,
                                                                      character, bestvertex + 1, pprt->team, pprt->owner_ref, particle, cnt, character);

                        if (bs_part)
                        {
                            vertex_occupied[bestvertex] = bs_part->getParticleID();
                            bs_part->is_bumpspawn = true;
                            bs_count++;
                        }
                    }
                //}
                //else
                //{
                //    // Multiple particles are attached to character
                //    for ( cnt = 0; cnt < amount; cnt++ )
                //    {
                //        int irand = Random::next(std::numeric_limits<uint16_t>::max());

                //        bs_part = spawn_one_particle( pchr->pos, pchr->ori.facing_z, pprt->profile_ref, ppip->bumpspawn_lpip.get(),
                //                                      character, irand % vertices, pprt->team, pprt->owner_ref, particle, cnt, character );

                //        if( DEFINED_PRT(bs_part) )
                //        {
                //            PrtList.lst[bs_part].is_bumpspawn = true;
                //            bs_count++;
                //        }
                //    }
                //}
            }
        }
    }

    return bs_count;
}
