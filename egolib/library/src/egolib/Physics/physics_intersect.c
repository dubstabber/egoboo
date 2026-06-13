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

/// @file egolib/Physics/physics_intersect.c
/// @brief Swept-AABB octagonal-bounding-box (oct_bb_t) intersection pipeline — the
///        phys_intersect_oct_bb family, split out of physics.c (2026-06-13).

#include "egolib/Physics/physics.h"
#include "egolib/Float.hpp"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

// Internal helpers (definitions below); phys_intersect_oct_bb itself is declared in physics.h.
static bool phys_intersect_oct_bb_index(int index, const oct_bb_t& src1, const oct_vec_v2_t& ovel1, const oct_bb_t& src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax);
static bool phys_intersect_oct_bb_close_index(int index, const oct_bb_t& src1, const oct_vec_v2_t& ovel1, const oct_bb_t& src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax);

bool phys_intersect_oct_bb_index(int index, const oct_bb_t& src1, const oct_vec_v2_t& ovel1, const oct_bb_t& src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax)
{
    if (!tmin)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "tmin");
    }
    if (!tmax)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "tmax");
    }
    if (index < 0)
    {
        throw std::invalid_argument("index < 0");
    }
    if (index >= OCT_COUNT)
    {
        throw std::invalid_argument("index >= OCT_COUNT");
    }

    float vdiff = ovel2[index] - ovel1[index];
    if ( 0.0f == vdiff ) return false;

    float src1_min = src1._mins[index];
    float src1_max = src1._maxs[index];
    float src2_min = src2._mins[index];
    float src2_max = src2._maxs[index];

    if (OCT_Z != index)
    {
        // Is there any possibility of the 2 objects acting as a platform pair.
        bool close_test_1 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ1);
        bool close_test_2 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ2);

        // Only do a close test if the object's feet are above the platform.
        close_test_1 = close_test_1 && (src1._mins[OCT_Z] > src2._maxs[OCT_Z]);
        close_test_2 = close_test_2 && (src2._mins[OCT_Z] > src1._maxs[OCT_Z]);

        if (!close_test_1 && !close_test_2)
        {
            // NEITHER is a platform.
            float time[4];

            time[0] = (src1_min - src2_min) / vdiff;
            time[1] = (src1_min - src2_max) / vdiff;
            time[2] = (src1_max - src2_min) / vdiff;
            time[3] = (src1_max - src2_max) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else
        {
            return phys_intersect_oct_bb_close_index(index, src1, ovel1, src2, ovel2, test_platform, tmin, tmax);
        }
    }
    else /* OCT_Z == index */
    {
        float plat_min, plat_max;

        // Add in a tolerance into the vertical direction for platforms
        float tolerance_1 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ1) ? PLATTOLERANCE : 0.0f;
        float tolerance_2 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ2) ? PLATTOLERANCE : 0.0f;

        if ( 0.0f == tolerance_1 && 0.0f == tolerance_2 )
        {
            // NEITHER is a platform.
            float time[4];

            time[0] = (src1_min - src2_min) / vdiff;
            time[1] = (src1_min - src2_max) / vdiff;
            time[2] = (src1_max - src2_min) / vdiff;
            time[3] = (src1_max - src2_max) / vdiff;

            *tmin = std::min( std::min( time[0], time[1] ), std::min( time[2], time[3] ) );
            *tmax = std::max( std::max( time[0], time[1] ), std::max( time[2], time[3] ) );
        }
        else if (0.0f == tolerance_1)
        {
            float time[4];

            // 2nd object is a platform.
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = (src1_min - plat_min) / vdiff;
            time[1] = (src1_min - plat_max) / vdiff;
            time[2] = (src1_max - plat_min) / vdiff;
            time[3] = (src1_max - plat_max) / vdiff;

            *tmin = std::min(std::min(time[0], time[1]), std::min(time[2], time[3]));
            *tmax = std::max(std::max(time[0], time[1]), std::max(time[2], time[3]));
        }
        else if (0.0f == tolerance_2)
        {
            float time[4];

            // 1st object is a platform.
            plat_min = src1_min;
            plat_max = src1_max + tolerance_2;

            time[0] = (plat_min - src2_min) / vdiff;
            time[1] = (plat_min - src2_max) / vdiff;
            time[2] = (plat_max - src2_min) / vdiff;
            time[3] = (plat_max - src2_max) / vdiff;

            *tmin = std::min(std::min(time[0], time[1]), std::min(time[2], time[3]));
            *tmax = std::max(std::max(time[0], time[1]), std::max(time[2], time[3]));
        }
        else if ( tolerance_1 > 0.0f && tolerance_2 > 0.0f )
        {
            // BOTH are platforms.
            // They cannot both act as plaforms at the same time,
            // so do 8 tests.

            float time[8];

            // Assume: 2nd object is platform.
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = (src1_min - plat_min) / vdiff;
            time[1] = (src1_min - plat_max) / vdiff;
            time[2] = (src1_max - plat_min) / vdiff;
            time[3] = (src1_max - plat_max) / vdiff;
            float tmp_min1 = std::min({time[0], time[1], time[2], time[3]});
            float tmp_max1 = std::max({time[0], time[1], time[2], time[3]});

            // Assume: 1st object is platform.
            plat_min = src1_min;
            plat_max = src1_max + tolerance_2;

            time[4] = (plat_min - src2_min) / vdiff;
            time[5] = (plat_min - src2_max) / vdiff;
            time[6] = (plat_max - src2_min) / vdiff;
            time[7] = (plat_max - src2_max) / vdiff;
            float tmp_min2 = std::min({time[4], time[5], time[6], time[7]});
            float tmp_max2 = std::max({time[4], time[5], time[6], time[7]});

            *tmin = std::min(tmp_min1, tmp_min2);
            *tmax = std::max(tmp_max1, tmp_max2);
        }
    }

    // Normalize the results for the diagonal directions.
    if (OCT_XY == index || OCT_YX == index)
    {
        *tmin *= idlib::inv_sqrt_two<float>();
        *tmax *= idlib::inv_sqrt_two<float>();
    }

    if (*tmax <= *tmin) return false;

    return true;
}

//--------------------------------------------------------------------------------------------
bool phys_intersect_oct_bb(const oct_bb_t& src1_orig, const Ego::Vector3f& pos1, const Ego::Vector3f& vel1, const oct_bb_t& src2_orig, const Ego::Vector3f& pos2, const Ego::Vector3f& vel2, int test_platform, oct_bb_t& dst, float *tmin, float *tmax)
{
    /// @author BB
	/// @details A test to determine whether two "fast moving" objects are interacting within a frame.
	///               Designed to determine whether a bullet particle will interact with character.

    float  local_tmin, local_tmax;

    // handle optional parameters
    if ( NULL == tmin ) tmin = &local_tmin;
    if ( NULL == tmax ) tmax = &local_tmax;

    // convert the position and velocity vectors to octagonal format
    oct_vec_v2_t opos1(pos1), opos2(pos2),
                 ovel1(vel1), ovel2(vel2);

    // shift the bounding boxes to their starting positions
    auto src1 = idlib::translate(src1_orig, opos1),
		 src2 = idlib::translate(src2_orig, opos2);

    bool found = false;
    *tmin = +1.0e6;
    *tmax = -1.0e6;

    int failure_count = 0;
    if (idlib::manhattan_norm(vel1-vel2) < 1.0e-6)
    {
        // No relative motion, so avoid the loop to save time.
        failure_count = OCT_COUNT;
    }
    else
    {
        // Cycle through the coordinates to see when the two volumes might coincide.
        for (size_t index = 0; index < OCT_COUNT; ++index)
        {
            if (std::abs(ovel1[index] - ovel2[index]) < 1.0e-6)
            {
                failure_count++;
            }
            else
            {
                float tmp_min = 0.0f, tmp_max = 0.0f;
                bool intersectsOnAxis = phys_intersect_oct_bb_index(index, src1, ovel1, src2, ovel2, test_platform, &tmp_min, &tmp_max);

                // Treat invalid interval math as a non-intersection on this axis.
                if (idlib::is_bad(tmp_min) || idlib::is_bad(tmp_max))
                {
                    intersectsOnAxis = false;
                }

                if (!intersectsOnAxis)
                {
                    // There is no usable overlap interval on this axis for this frame.
                    failure_count++;
                }
                else
                {
                    if (!found)
                    {
                        *tmin = tmp_min;
                        *tmax = tmp_max;
                        found = true;
                    }
                    else
                    {
                        *tmin = std::max(*tmin, tmp_min);
                        *tmax = std::min(*tmax, tmp_max);
                    }

                    // Check the values vs. reasonable bounds.
                    if (*tmax <= *tmin) return false;
                    if (*tmin > 1.0f || *tmax < 0.0f) return false;
                }
            }
        }
    }

    if (OCT_COUNT == failure_count)
    {
        // No relative motion on any axis.
        // Just say that they are interacting for the whole frame.

        *tmin = 0.0f;
        *tmax = 1.0f;

        // Determine the intersection of these two expanded volumes (for this frame).
        dst = oct_bb_t::intersection(src1, src2);
    }
    else
    {
        float tmp_min, tmp_max;

        // Check to see if there the intersection times make any sense.
        if (*tmax <= *tmin) return false;

        // Check whether there is any overlap this frame.
        if (*tmin >= 1.0f || *tmax <= 0.0f) return false;

        // Clip the interaction time to just one frame.
        tmp_min = Ego::Math::constrain(*tmin, 0.0f, 1.0f);
        tmp_max = Ego::Math::constrain(*tmax, 0.0f, 1.0f);

        // determine the expanded collision volumes for both objects (for this frame)
        oct_bb_t exp1, exp2;
        phys_expand_oct_bb(src1, vel1, tmp_min, tmp_max, exp1);
        phys_expand_oct_bb(src2, vel2, tmp_min, tmp_max, exp2);

        // determine the intersection of these two expanded volumes (for this frame)
        dst = oct_bb_t::intersection(exp1, exp2);
    }

    if (0 != test_platform)
    {
        dst._maxs[OCT_Z] += PLATTOLERANCE;
        dst._empty = oct_bb_t::empty_raw(dst);
    }

    if (dst._empty) return false;

    return true;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool phys_intersect_oct_bb_close_index(int index, const oct_bb_t& src1, const oct_vec_v2_t& ovel1, const oct_bb_t& src2, const oct_vec_v2_t& ovel2, int test_platform, float *tmin, float *tmax)
{
    if (!tmin)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "tmin");
    }
    if (!tmax)
    {
        throw idlib::argument_null_error(__FILE__, __LINE__, "tmax");
    }
    if (index < 0)
    {
        throw std::invalid_argument("index < 0");
    }
    if (index >= OCT_COUNT)
    {
        throw std::invalid_argument("index >= OCT_COUNT");
    }
    float vdiff = ovel2[index] - ovel1[index];
    if (0.0f == vdiff) return false;

    /// @todo Use src1.getMin(index), src2.getMax(index) and src1.getMid(index).
    float src1_min = src1._mins[index];
    float src1_max = src1._maxs[index];
    float opos1 = (src1_min + src1_max) * 0.5f;

    /// @todo Use src2.getMin(index), src2.getMax(index) and src2.getMid(index).
    float src2_min = src2._mins[index];
    float src2_max = src2._maxs[index];
    float opos2 = (src2_min + src2_max) * 0.5f;

    if (OCT_Z != index)
    {
        bool platform_1 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ1);
        bool platform_2 = HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ2);

        if (!platform_1 && !platform_2)
        {
            // NEITHER is a platform.
            // Use the eqn. from phys_intersect_oct_bb_index().

            float time[4];

            time[0] = (src1_min - src2_min) / vdiff;
            time[1] = (src1_min - src2_max) / vdiff;
            time[2] = (src1_max - src2_min) / vdiff;
            time[3] = (src1_max - src2_max) / vdiff;

			*tmin = std::min({ time[0], time[1], time[2], time[3] });
			*tmax = std::max({ time[0], time[1], time[2], time[3] });
        }
        else if ( platform_1 && !platform_2 )
        {
            float time[2];

            // 1st object is the platform.
            time[0] = (src1_min - opos2) / vdiff;
            time[1] = (src1_max - opos2) / vdiff;

            *tmin = std::min(time[0], time[1]);
            *tmax = std::max(time[0], time[1]);
        }
        else if (!platform_1 && platform_2)
        {
            float time[2];

            // 2nd object is the platform.
            time[0] = (opos1 - src2_min) / vdiff;
            time[1] = (opos1 - src2_max) / vdiff;

            *tmin = std::min(time[0], time[1]);
            *tmax = std::max(time[0], time[1]);
        }
        else
        {
            // BOTH are platforms. must check all possibilities.
            float time[4];

            // 1st object is the platform.
            time[0] = (src1_min - opos2) / vdiff;
            time[1] = (src1_max - opos2) / vdiff;

            // 2nd object 2 is the platform.
            time[2] = (opos1 - src2_min) / vdiff;
            time[3] = (opos1 - src2_max) / vdiff;

            *tmin = std::min({time[0], time[1], time[2], time[3]});
            *tmax = std::max({time[0], time[1], time[2], time[3]});
        }
    }
    else /* OCT_Z == index */
    {
        float plat_min, plat_max;
        float obj_pos;

        float tolerance_1 =  HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ1)
                          ? PLATTOLERANCE : 0.0f;
        float tolerance_2 =  HAS_SOME_BITS(test_platform, PHYS_PLATFORM_OBJ2)
                          ? PLATTOLERANCE : 0.0f;

        if (0.0f == tolerance_1 && 0.0f == tolerance_2)
        {
            // NEITHER is a platform.
            // Use the eqn. from phys_intersect_oct_bb_index().

            float time[4];

            time[0] = (src1_min - src2_min) / vdiff;
            time[1] = (src1_min - src2_max) / vdiff;
            time[2] = (src1_max - src2_min) / vdiff;
            time[3] = (src1_max - src2_max) / vdiff;

            *tmin = std::min({ time[0], time[1], time[2], time[3] });
            *tmax = std::max({ time[0], time[1], time[2], time[3] });
        }
        else if (0.0f != tolerance_1 && 0.0f == tolerance_2)
        {
            float time[2];

            // 1st object is the platform.
            obj_pos  = src2_min;
            plat_min = src1_min;
            plat_max = src1_max + tolerance_1;

            time[0] = (plat_min - obj_pos) / vdiff;
            time[1] = (plat_max - obj_pos) / vdiff;

            *tmin = std::min(time[0], time[1]);
            *tmax = std::max(time[0], time[1]);
        }
        else if (0.0f == tolerance_1 && 0.0f != tolerance_2)
        {
            float time[2];

            // 2nd object is the platform.
            obj_pos  = src1_min;
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = (obj_pos - plat_min) / vdiff;
            time[1] = (obj_pos - plat_max) / vdiff;

            *tmin = std::min(time[0], time[1]);
            *tmax = std::max(time[0], time[1]);
        }
        else
        {
            // BOTH are platforms.
            float time[4];

            // 2nd object is a platform.
            obj_pos  = src1_min;
            plat_min = src2_min;
            plat_max = src2_max + tolerance_2;

            time[0] = (obj_pos - plat_min) / vdiff;
            time[1] = (obj_pos - plat_max) / vdiff;

            // 1st object is a platform.
            obj_pos  = src2_min;
            plat_min = src1_min;
            plat_max = src1_max + tolerance_1;

            time[2] = (plat_min - obj_pos) / vdiff;
            time[3] = (plat_max - obj_pos) / vdiff;

            *tmin = std::min({ time[0], time[1], time[2], time[3] });
            *tmax = std::max({ time[0], time[1], time[2], time[3] });
        }
    }

    // Normalize the results for the diagonal directions.
    if (OCT_XY == index || OCT_YX == index)
    {
        *tmin *= idlib::inv_sqrt_two<float>();
        *tmax *= idlib::inv_sqrt_two<float>();
    }

    if (*tmax < *tmin) return false;

    return true;
}
