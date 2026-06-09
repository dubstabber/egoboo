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

/// @file egolib/game/CharacterParticleOps.h
/// @brief Thin declaration header for the character/particle gameplay operations that the
///        lower-layer Entities/ code (Object_*.cpp, Particle_*.cpp) calls back into game/.
///        These were split out of the heavy game.h grab-bag so the Entities translation
///        units can reach these few entry points WITHOUT dragging game.h's transitive
///        conduit (EngineContext.hpp / mesh.h / Inventory.hpp / Shop.hpp / Module-state
///        headers). Every signature here uses only lower-layer types, so this header has no
///        game/ include. game.h re-includes it, so existing game-layer callers are unaffected.

#pragma once

#include "egolib/typedef.h"              // ObjectRef, TEAM_REF, PIP_REF, GCC_PRINTF_FUNC
#include "egolib/Logic/ObjectSlot.hpp"   // slot_t
#include "egolib/_math.h"                // Facing
#include "egolib/integrations/math.hpp"  // Ego::Vector3f

#include <string>

class Object;

// counters for debugging wall collisions
extern int chr_stoppedby_tests;
extern int chr_pressure_tests;

/// Particles
/// @brief Get the number of particles attached to an object.
int number_of_attached_particles(ObjectRef objectRef);
/// @brief Make sure an object has no particles attached
void disaffirm_attached_particles(ObjectRef objectRef);
/// @brief Make sure an object has all particles attached
/// @return the number of particles added
int reaffirm_attached_particles(ObjectRef objectRef);

// Latches
bool chr_do_latch_attack(Object *pchr, slot_t which_slot);
void character_swipe(ObjectRef cnt, slot_t slot);

/// @brief AI targeting for a particle.
ObjectRef prt_find_target(const Ego::Vector3f& pos, Facing facing, const PIP_REF ipip, const TEAM_REF team, ObjectRef dontTarget, ObjectRef oldTarget, Facing *targetAngle);

ObjectRef chr_get_lowest_attachment(ObjectRef object_ref, bool non_item);

// Message printing functions (TODO: Rewrite to c++)
int DisplayMsg_printf(const char *format, ...) GCC_PRINTF_FUNC(1);
void DisplayMsg_print(const std::string &text);
