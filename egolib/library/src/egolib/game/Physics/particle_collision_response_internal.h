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

/// @file egolib/game/Physics/particle_collision_response_internal.h
/// @brief Private shared declarations for the character-particle collision response pipeline.
/// @details Shared across the three response TUs (particle_collision_response.c /
///          particle_collision_response_damage.c / particle_collision_response_bump.c):
///          the chr_prt_collision_data_t communication block, the cross-cluster facade
///          helpers, and the promoted forward declarations of the response steps the
///          do_chr_prt_collision orchestrator dispatches.

#pragma once

#include "egolib/game/Physics/particle_collision.h"
#include "egolib/Graphics/IBillboardSystem.hpp"  // Ego::Graphics::activeBillboardSystem
#include "egolib/game/CharacterParticleOps.h"  // chr_get_lowest_attachment, reaffirm_attached_particles (was game.h)
#include "egolib/Audio/IAudioSystem.hpp"  // GSND_* + activeAudioSystem()/playSound/getGlobalSound (was reached via game/graphic.h)
#include "egolib/Physics/physics.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() object access (the entity-world seam)
#include "egolib/Profiles/_Include.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"

/// Cross-cluster facade helpers shared by the response steps. Inline (vague linkage) so the
/// three response TUs each see one definition; the uniquely-named namespace keeps these
/// distinct from the like-named helpers in other egolib-library physics TUs.
namespace particle_collision_response_detail
{
inline Ego::Entities::IObjectWorld& objectWorld()
{
    return Ego::Entities::activeObjectWorld();
}

inline IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

inline IDamageable& damageable(Object& object)
{
    return object;
}

inline const IPhysical& physical(const Object& object)
{
    return object;
}

inline Object* heldItem(const IInventoryHolder& object, slot_t slot)
{
    return objectWorld().getObjectHandler().get(object.getHeldObject(slot));
}
}
using namespace particle_collision_response_detail;

/// data block used to communicate between the different "modules" governing the character-particle collision
struct chr_prt_collision_data_t
{
public:
    chr_prt_collision_data_t();

public:
    // object parameters
    ObjectRef ichr;
    Object *pchr;

    ParticleRef iprt;
    std::shared_ptr<Ego::Particle> pprt;
    std::shared_ptr<ParticleProfile> ppip;

    //---- collision parameters

    // true collisions
    bool int_min;
    float depth_min;

    // hit-box collisions
    bool int_max;
    float depth_max;

    bool is_impact;
    bool is_pressure;
    bool is_collision;
    float dot;
    Ego::Vector3f nrm;

    // collision modifications
    bool mana_paid;
    int max_damage, actual_damage;
    Ego::Vector3f vdiff, vdiff_para, vdiff_perp;
    float block_factor;

    // collision reaction
    bool terminate_particle;
    bool prt_bumps_chr;
    bool prt_damages_chr;
};

// Response steps dispatched by the do_chr_prt_collision orchestrator; defined in the
// particle_collision_response_damage.c / particle_collision_response_bump.c siblings.
bool do_chr_prt_collision_deflect(chr_prt_collision_data_t& pdata);
bool do_chr_prt_collision_damage(chr_prt_collision_data_t& pdata);
void do_chr_prt_collision_knockback(chr_prt_collision_data_t& pdata);
bool do_chr_prt_collision_handle_bump(chr_prt_collision_data_t& pdata);
