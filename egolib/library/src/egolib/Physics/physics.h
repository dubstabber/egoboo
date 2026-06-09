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

/// @file egolib/Physics/physics.h
/// @brief Oct-bb (oct_bb_t) collision-geometry free functions (phys_expand_*/phys_estimate_*/
///        phys_intersect_*). Relocated from egolib/game/physics.h into the lower-layer Physics
///        nucleus: it includes only lower-layer headers (bbox.h, PhysicsData.h, PhysicalConstants)
///        and forward-declares the entity types it touches by pointer.

#pragma once

#include "egolib/bbox.h"
#include "egolib/PhysicsData.h"  // orientation_t, apos_t, phys_data_t, PLATTOLERANCE, PHYS_PLATFORM_*, PLATFORM_STICKINESS
#include "egolib/Physics/PhysicalConstants.hpp"


//--------------------------------------------------------------------------------------------

class Object;
class IPhysical;
namespace Ego { class Particle; }

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
/// @details use the velocity of an object and its oct_bb_t to determine the
///               amount of territory that an object will cover in the range [tmin,tmax].
///               One update equals [tmin,tmax] == [0,1].
bool phys_expand_oct_bb(const oct_bb_t& src, const Ego::Vector3f& vel, const float tmin, const float tmax, oct_bb_t& dst);

/// @details use the object velocity to figure out where the volume that the character will
///               occupy during this update. Use the loser chr_max_cv and include extra height if
///               it is a platform.
bool phys_expand_chr_bb(const IPhysical *pchr, float tmin, float tmax, oct_bb_t& dst);
bool phys_expand_prt_bb(Ego::Particle *pprt, float tmin, float tmax, oct_bb_t& dst);

bool phys_estimate_collision_normal(const oct_bb_t& obb_a, const oct_bb_t& pobb_b, const float exponent, oct_vec_v2_t& odepth, Ego::Vector3f& nrm, float& depth);
bool phys_estimate_pressure_normal(const oct_bb_t& obb_a, const oct_bb_t& pobb_b, const float exponent, oct_vec_v2_t& odepth, Ego::Vector3f& nrm, float& depth);

bool phys_intersect_oct_bb(const oct_bb_t& src1, const Ego::Vector3f& pos1, const Ego::Vector3f& vel1, const oct_bb_t& src2, const Ego::Vector3f& pos2, const Ego::Vector3f& vel2, int test_platform, oct_bb_t& dst, float *tmin, float *tmax);
