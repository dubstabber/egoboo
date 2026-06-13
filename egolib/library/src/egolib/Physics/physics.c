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

/// @file egolib/Physics/physics.c

#include "egolib/Physics/physics.h"
#include "egolib/Entities/_Include.hpp"  // IPhysical / Ego::Particle full defs (deref'd by phys_expand_chr_bb/_prt_bb)
#include "egolib/Float.hpp"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

// g_environment is defined in egolib/Physics/PhysicalConstants.cpp (lower layer).
// The phys_intersect_oct_bb swept-AABB family lives in the sibling physics_intersect.c.

/// @brief A test to determine whether two "fast moving" objects are interacting within a frame.
///        Designed to determine whether a bullet particle will interact with character.
//static bool phys_intersect_oct_bb_close(const oct_bb_t& src1_orig, const Vector3f& pos1, const Vector3f& vel1, const oct_bb_t& src2_orig, const Vector3f& pos2, const Vector3f& vel2, int test_platform, oct_bb_t& dst, float *tmin, float *tmax);
static bool phys_estimate_depth(const oct_vec_v2_t& odepth, const float exponent, Ego::Vector3f& nrm, float& depth);
//static float phys_get_depth(const oct_vec_v2_t& odepth, const Vector3f& nrm);
static bool phys_warp_normal(const float exponent, Ego::Vector3f& nrm);
static bool phys_get_pressure_depth(const oct_bb_t& bb_a, const oct_bb_t& bb_b, oct_vec_v2_t& odepth);
static bool phys_get_collision_depth(const oct_bb_t& bb_a, const oct_bb_t& bb_b, oct_vec_v2_t& odepth);

const Facing orientation_t::MAP_TURN_OFFSET(0x8000);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool phys_get_collision_depth(const oct_bb_t& bb_a, const oct_bb_t& bb_b, oct_vec_v2_t& odepth)
{
    odepth = oct_vec_v2_t();

    // are the initial volumes any good?
    if (bb_a._empty || bb_b._empty) return false;

    // is there any overlap?
    oct_bb_t otmp = oct_bb_t::intersection(bb_a, bb_b);
    if (otmp.isEmpty()) {
        return false;
    }

    // Estimate the "cm position" of the objects by the bounding volumes.
    oct_vec_v2_t opos_a = bb_a.getMid();
    oct_vec_v2_t opos_b = bb_b.getMid();

    // find the (signed) depth in each dimension
    bool retval = true;
    for (size_t i = 0; i < OCT_COUNT; ++i)
    {
        float fdiff = opos_b[i] - opos_a[i];
        float fdepth = otmp._maxs[i] - otmp._mins[i];

        // if the measured depth is less than zero, or the difference in positions
        // is ambiguous, this algorithm fails
        if (fdepth <= 0.0f || 0.0f == fdiff) retval = false;

        odepth[i] = (fdiff < 0.0f) ? -fdepth : fdepth;
    }
    odepth[OCT_XY] *= idlib::inv_sqrt_two<float>();
    odepth[OCT_YX] *= idlib::inv_sqrt_two<float>();

    return retval;
}

//--------------------------------------------------------------------------------------------
bool phys_get_pressure_depth(const oct_bb_t& bb_a, const oct_bb_t& bb_b, oct_vec_v2_t& odepth)
{
    odepth = oct_vec_v2_t();

    // assume the best
    bool result = true;

    // scan through the dimensions of the oct_bbs
    for (size_t i = 0; i < OCT_COUNT; ++i)
    {
        float diff1 = bb_a._maxs[i] - bb_b._mins[i];
        float diff2 = bb_b._maxs[i] - bb_a._mins[i];

        if (diff1 < 0.0f || diff2 < 0.0f)
        {
            // this case will only happen if there is no overlap in one of the dimensions,
            // meaning there was a bad collision detection... it should NEVER happen.
            // In any case this math still generates the proper direction for
            // the normal pointing away from b.
            if (std::abs(diff1) < std::abs(diff2))
            {
                odepth[i] = diff1;
            }
            else
            {
                odepth[i] = -diff2;
            }

            result = false;
        }
        else if (diff1 < diff2)
        {
            odepth[i] = -diff1;
        }
        else
        {
            odepth[i] = diff2;
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------
bool phys_warp_normal(const float exponent, Ego::Vector3f& nrm)
{
    // use the exponent to warp the normal into a cylinder-like shape, if needed

    if (1.0f == exponent) return true;

    if (0.0f == idlib::manhattan_norm(nrm)) return false;

    float length_hrz_2 = idlib::squared_euclidean_norm(Ego::Vector2f(nrm[kX],nrm[kY]));
    float length_vrt_2 = idlib::squared_euclidean_norm(nrm) - length_hrz_2;

    nrm[kX] = nrm[kX] * std::pow( length_hrz_2, 0.5f * ( exponent - 1.0f ) );
    nrm[kY] = nrm[kY] * std::pow( length_hrz_2, 0.5f * ( exponent - 1.0f ) );
    nrm[kZ] = nrm[kZ] * std::pow( length_vrt_2, 0.5f * ( exponent - 1.0f ) );

    // normalize the normal
	nrm = Ego::normalize(nrm).get_vector();
    return idlib::euclidean_norm(nrm) >= 0.0f;
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_depth(const oct_vec_v2_t& odepth, const float exponent, Ego::Vector3f& nrm, float& depth)
{
    // use the given (signed) podepth info to make a normal vector, and measure
    // the shortest distance to the border

	Ego::Vector3f nrm_aa;

    if (0.0f != odepth[OCT_X]) nrm_aa[kX] = 1.0f / odepth[OCT_X];
    if (0.0f != odepth[OCT_Y]) nrm_aa[kY] = 1.0f / odepth[OCT_Y];
    if (0.0f != odepth[OCT_Z]) nrm_aa[kZ] = 1.0f / odepth[OCT_Z];

    if ( 1.0f == exponent )
    {
        nrm_aa = Ego::normalize(nrm_aa).get_vector();
    }
    else
    {
        phys_warp_normal(exponent, nrm_aa);
    }

    // find a minimum distance
    float tmin_aa = 1e6;

    if (nrm_aa[kX] != 0.0f)
    {
        float ftmp = odepth[OCT_X] / nrm_aa[kX];
        ftmp = std::max(0.0f, ftmp);
        tmin_aa = std::min(tmin_aa, ftmp);
    }

    if (nrm_aa[kY] != 0.0f)
    {
        float ftmp = odepth[OCT_Y] / nrm_aa[kY];
        ftmp = std::max(0.0f, ftmp);
        tmin_aa = std::min(tmin_aa, ftmp);
    }

    if (nrm_aa[kZ] != 0.0f)
    {
        float ftmp = odepth[OCT_Z] / nrm_aa[kZ];
        ftmp = std::max(0.0f, ftmp);
        tmin_aa = std::min(tmin_aa, ftmp);
    }

    if (tmin_aa <= 0.0f || tmin_aa >= 1e6) return false;

    // Next do the diagonal axes.
	Ego::Vector3f nrm_diag = idlib::zero<Ego::Vector3f>();

    if (0.0f != odepth[OCT_XY]) nrm_diag[kX] = 1.0f / (odepth[OCT_XY] * idlib::inv_sqrt_two<float>());
    if (0.0f != odepth[OCT_YX]) nrm_diag[kY] = 1.0f / (odepth[OCT_YX] * idlib::inv_sqrt_two<float>());
    if (0.0f != odepth[OCT_Z ]) nrm_diag[kZ] = 1.0f / odepth[OCT_Z];

    if (1.0f == exponent)
    {
        nrm_diag = Ego::normalize(nrm_diag).get_vector();
    }
    else
    {
        phys_warp_normal(exponent, nrm_diag);
    }

    // find a minimum distance
    float tmin_diag = 1e6;

    if (nrm_diag[kX] != 0.0f)
    {
        float ftmp = idlib::inv_sqrt_two<float>() * odepth[OCT_XY] / nrm_diag[kX];
        ftmp = std::max(0.0f, ftmp);
        tmin_diag = std::min(tmin_diag, ftmp);
    }

    if (nrm_diag[kY] != 0.0f)
    {
        float ftmp = idlib::inv_sqrt_two<float>() * odepth[OCT_YX] / nrm_diag[kY];
        ftmp = std::max(0.0f, ftmp);
        tmin_diag = std::min(tmin_diag, ftmp);
    }

    if (nrm_diag[kZ] != 0.0f)
    {
        float ftmp = odepth[OCT_Z] / nrm_diag[kZ];
        ftmp = std::max(0.0f, ftmp);
        tmin_diag = std::min(tmin_diag, ftmp);
    }

    if (tmin_diag <= 0.0f || tmin_diag >= 1e6) return false;

    float tmin;
    if (tmin_aa < tmin_diag)
    {
        tmin = tmin_aa;
		nrm = nrm_aa;
    }
    else
    {
        tmin = tmin_diag;

        // !!!! rotate the diagonal axes onto the axis aligned ones !!!!!
        nrm[kX] = (nrm_diag[kX] - nrm_diag[kY]) * idlib::inv_sqrt_two<float>();
        nrm[kY] = (nrm_diag[kX] + nrm_diag[kY]) * idlib::inv_sqrt_two<float>();
        nrm[kZ] = nrm_diag[kZ];
    }

    // normalize this normal
	nrm = Ego::normalize(nrm).get_vector();
    bool result = idlib::euclidean_norm(nrm) > 0.0f;

    // find the depth in the direction of the normal, if possible
    if (result)
    {
        depth = tmin;
    }

    return result;
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_collision_normal(const oct_bb_t& obb_a, const oct_bb_t& obb_b, const float exponent, oct_vec_v2_t& odepth, Ego::Vector3f& nrm, float& depth)
{
    // estimate the normal for collision volumes that are partially overlapping

    // Do we need to use the more expensive algorithm?
    bool use_pressure = false;
    if (oct_bb_t::contains(obb_a, obb_b))
    {
        use_pressure = true;
    }
    else if (oct_bb_t::contains(obb_b, obb_a))
    {
        use_pressure = true;
    }

    if (!use_pressure)
    {
        // Try to get the collision depth.
        if (!phys_get_collision_depth(obb_a, obb_b, odepth))
        {
            use_pressure = true;
        }
    }

    if (use_pressure)
    {
        return phys_estimate_pressure_normal(obb_a, obb_b, exponent, odepth, nrm, depth);
    }

    return phys_estimate_depth(odepth, exponent, nrm, depth);
}

//--------------------------------------------------------------------------------------------
bool phys_estimate_pressure_normal(const oct_bb_t& obb_a, const oct_bb_t& obb_b, const float exponent, oct_vec_v2_t& odepth, Ego::Vector3f& nrm, float& depth)
{
    // use a more robust algorithm to get the normal no matter how the 2 volumes are
    // related

    // calculate the direction of the nearest way out for each octagonal axis
    if (!phys_get_pressure_depth(obb_a, obb_b, odepth))
    {
        return false;
    }

    return phys_estimate_depth(odepth, exponent, nrm, depth);
}

//--------------------------------------------------------------------------------------------
bool phys_expand_oct_bb(const oct_bb_t& src, const Ego::Vector3f& vel, const float tmin, const float tmax, oct_bb_t& dst)
{
    if (0.0f == idlib::manhattan_norm(vel))
    {
        dst = src;
        return true;
    }

    oct_bb_t tmp_min, tmp_max;
    // Determine the bounding volume at t == tmin.
    if (0.0f == tmin)
    {
        tmp_min = src;
    }
    else
    {
		Ego::Vector3f tmp_diff = vel * tmin;
        // Adjust the bounding box to take in the position at the next step.
        tmp_min = idlib::translate(src, tmp_diff);
    }

    // Determine the bounding volume at t == tmax.
    if ( tmax == 0.0f )
    {
        tmp_max = src;
    }
    else
    {
		Ego::Vector3f tmp_diff = vel * tmax;
        // Adjust the bounding box to take in the position at the next step.
		tmp_max = idlib::translate(src, tmp_diff);
	}

    // Determine bounding box for the range of times.
	oct_bb_t::join(tmp_min, tmp_max, dst);

    return true;
}

//--------------------------------------------------------------------------------------------
bool phys_expand_chr_bb(const IPhysical *pchr, float tmin, float tmax, oct_bb_t& dst)
{
    // add in the current position to the bounding volume
    auto tmp_oct2 = idlib::translate(pchr->getMaxCollisionVolume(), pchr->getPosition());

    // streach the bounding volume to cover the path of the object
    return phys_expand_oct_bb(tmp_oct2, pchr->getVelocity(), tmin, tmax, dst);
}

//--------------------------------------------------------------------------------------------
bool phys_expand_prt_bb(Ego::Particle *pprt, float tmin, float tmax, oct_bb_t& dst)
{
    /// @author BB
    /// @details use the object velocity to figure out where the volume that the particle will
    ///               occupy during this update
    if(!pprt || pprt->isTerminated()) return false;

    // copy the volume
    auto tmp_oct1 = pprt->prt_max_cv;

    // add in the current position to the bounding volume
    auto tmp_oct2 = idlib::translate(tmp_oct1, pprt->getPosition());

    // streach the bounging volume to cover the path of the object
    return phys_expand_oct_bb(tmp_oct2, pprt->getVelocity(), tmin, tmax, dst);
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
#if 0
/**
 * @brief
 *  Snap a world coordinate point to grid.
 *  
 *  The point is moved along the x- and y-axis such that it is centered on the tile it is on.
 * @param p
 *  the point
 * @return
 *  the snapped world coordinate point
 */
static Vector3f snap(const Vector3f& p)
{
    return Vector3f((std::floor(p[kX] / GRID_FSIZE) + 0.5f) * GRID_FSIZE,
                    (std::floor(p[kY] / GRID_FSIZE) + 0.5f) * GRID_FSIZE,
                    p[kZ]);
}
#endif

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void phys_data_t::clear()
{
	aplat = apos_t();
	acoll = apos_t();
	avel = idlib::zero<Ego::Vector3f>();
    /// @todo Seems like dynamic and loaded data are mixed here;
    /// We may not blank bumpdampen, weight or dampen for now.
#if 0
    bumpdampen = 1.0f;
    weight = 1.0f;
    dampen = 0.5f;
#endif
}

phys_data_t::phys_data_t()
	: aplat(), acoll(), avel(),
	  bumpdampen(1.0f), weight(1.0f), dampen(0.5f)
{}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void apos_t::join(const apos_t& other) {
    for (size_t i = 0; i < 3; ++i) {
        mins[i] = std::min(mins[i], other.mins[i]);
        maxs[i] = std::max(maxs[i], other.maxs[i]);
        sum[i] += other.sum[i];
    }
}

void apos_t::join(const Ego::Vector3f& other) {
    for (size_t i = 0; i < 3; ++i) {
        if (other[i] > 0.0f) {
            maxs[i] = std::max(maxs[i], other[i]);
        } else if (other[i] < 0.0f) {
            mins[i] = std::min(mins[i], other[i]);
        }
        sum[i] += other[i];
    }
}

void apos_t::join(const float t, const size_t index) {
    if (index > 2) {
		throw std::runtime_error("index out of bounds");
    }

    LOG_NAN(t);

    if (t > 0.0f) {
        maxs[index] = std::max(maxs[index], t);
    } else if (t < 0.0f) {
        mins[index] = std::min(mins[index], t);
    }

	sum[index] += t;
}

void apos_t::evaluate(const apos_t& self, Ego::Vector3f& dst) {
    dst = self.maxs + self.mins;
}

//--------------------------------------------------------------------------------------------

void phys_data_t::sum_acoll(const Ego::Vector3f& v)
{
    acoll.join(v);
}

void phys_data_t::sum_avel(const Ego::Vector3f& v)
{
    avel += v;
}

void phys_data_t::sum_aplat(const float val, const size_t index)
{
	if (index > 2)
	{
		throw std::runtime_error("index out of bounds");
	}

    LOG_NAN(val);

    aplat.join(val, index);
}

void phys_data_t::sum_avel(const float val, const size_t index)
{
    if (index > 2)
    {
		throw std::runtime_error("index out of bounds");
    }

    LOG_NAN(val);

    avel[index] += val;
}
