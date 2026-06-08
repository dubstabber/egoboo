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

/// @file egolib/PhysicsData.h
/// @brief Lower-layer physics data primitives (orientation_t, apos_t, phys_data_t)
///        and platform constants. Extracted from game/physics.h so the Entities layer
///        (Object.hpp, Common.hpp) can hold these as by-value members without pulling
///        the game-aware physics free functions. Pure idlib math (Vector3f / Facing);
///        no game-layer dependency. The game-aware phys_expand_*/phys_estimate_* function
///        declarations remain in game/physics.h, which includes this header.

#pragma once

#include "egolib/bbox.h"   // Ego::Vector3f
#include "egolib/_math.h"  // Facing

//--------------------------------------------------------------------------------------------

#define PLATTOLERANCE       50                     ///< Platform tolerance...

enum
{
    PHYS_PLATFORM_NONE = 0,
    PHYS_PLATFORM_OBJ1 = ( 1 << 0 ),
    PHYS_PLATFORM_OBJ2 = ( 1 << 1 )
};

//--------------------------------------------------------------------------------------------
struct orientation_t
{
    static const Facing MAP_TURN_OFFSET;

    Facing facing_z;            ///< Character's z-rotation 0 to 0xFFFF
    Facing map_twist_facing_y;  ///< Character's y-rotation 0 to 0xFFFF
    Facing map_twist_facing_x;  ///< Character's x-rotation 0 to 0xFFFF
};

//--------------------------------------------------------------------------------------------
/**
 * @brief
 *  Tracks the extrema and the sum of translations.
 * @remark
 *  Given a set of translation vectors \f$t_0,t_1,t_2,\ldots,t_n\f$
 *  the minimum of the translations is defined as
 *  \f[
 *  \left(
 *  min(t_{0_x},\ldots,t_{n_x}),
 *  min(t_{0_y},\ldots,t_{n_y}),
 *  min(t_{0_z},\ldots,t_{n_z})
 *  \right)
 *  \f]
 *  and the maximum as
 *  \f[
 *  \left(
 *  max(t_{0_x},\ldots,t_{n_x}),
 *  max(t_{0_y},\ldots,t_{n_y}),
 *  max(t_{0_z},\ldots,t_{n_z})
 *  \right)
 *  \f]
 *  The sum of the translations is
 *  \[
 *  \sum_{i=0}^n t_i
 *  \]
 *  If the set of translations is empty, the minimum, the maximum and the sum are all 0.
 */
struct apos_t
{

    /**
     * @brief
     *  The minimum of the translations.
     * @default
     *  <tt>(0,0,0)</tt>
     */
	Ego::Vector3f mins;

    /**
     * @brief
     *  The maximum of the translations.
     * @default
     *  <tt>(0,0,0)</tt>
     */
	Ego::Vector3f maxs;

    /**
     * @brief
     *  The translation induced by the translation sequence.
     * @default
     *  <tt>(0,0,0)</tt>
     */
	Ego::Vector3f sum;

	apos_t() :
		mins(),
		maxs(),
		sum()
	{
		//ctor
	}

    apos_t(const apos_t& other) :
        mins(other.mins),
        maxs(other.maxs),
        sum(other.sum)
    {
    }

    apos_t& operator=(const apos_t& other)
    {
        mins = other.mins;
        maxs = other.maxs;
        sum  = other.sum;
        return *this;
    }

	/// Update the displacement extrema.
    void join(const apos_t& other);
	/// Update this displacement extrema.
    void join(const Ego::Vector3f& other);
    /**
     * @brief
     *  Update this displacement extrema at a given axis.
     * @param t
     *  the translation along the axis
     * @param i
     *  the index of the axis
     */
    void join(const float displacement, const size_t index);
    static void evaluate(const apos_t& self, Ego::Vector3f& dst);
};

//--------------------------------------------------------------------------------------------

/// Data for doing the physics in bump_all_objects()
/// @details should prevent you from being bumped into a wall
struct phys_data_t
{
    apos_t         aplat, acoll;
	Ego::Vector3f  avel;

    float          bumpdampen;                    ///< "Mass" = weight / bumpdampen
    uint32_t       weight;                        ///< Weight
    float          dampen;                        ///< Bounciness

	phys_data_t();
	void sum_acoll(const Ego::Vector3f& v);
	void sum_avel(const Ego::Vector3f& v);
	void sum_aplat(const float v, const size_t index);
	void sum_avel(const float v, const size_t index);
	void clear();
};

//--------------------------------------------------------------------------------------------
// the global physics/friction values

static constexpr float PLATFORM_STICKINESS = 0.1f;     ///< Friction between characters and platforms
