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

/// @file egolib/game/mesh.c
/// @brief Wall/pressure physics queries: test_wall, get_pressure, get_diff, hit_wall,
///        water-aware getElevation; static class/global variable definitions.

#include "egolib/game/mesh.h"
#include "egolib/game/lighting.h"
#include "egolib/Physics/physics.h"
#include "egolib/Physics/PhysicalConstants.hpp"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Water.hpp"
#include "egolib/map_functions.h"  // twist_to_normal, cartman_calc_twist

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

//Static class variables
const std::shared_ptr<ego_tile_info_t> ego_tile_info_t::NULL_TILE = nullptr;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------

BIT_FIELD ego_mesh_t::test_wall(const BIT_FIELD bits, const mesh_wall_data_t& data) const
{
	// if there is no interaction with the mesh or the mesh is empty, return 0.
	if (EMPTY_BIT_FIELD == bits || 0 == _info.getTileCount() || _tmem.getInfo().getTileCount() == 0) {
		return EMPTY_BIT_FIELD;
	}

	// The bit accumulator.
	BIT_FIELD pass = 0;

	// Detect out of bounds in the x- and/or y-direction.
	// In that case, return "wall" and "impassable".
	if ((data._i.min().x() < 0 || data._i.max().x() >= data._mesh->_info.getTileCountX()) ||
		(data._i.min().y() < 0 || data._i.max().y() >= data._mesh->_info.getTileCountY())) {
		pass = (MAPFX_IMPASS | MAPFX_WALL) & bits;
		g_meshStats.boundTests++;
	}
	if (EMPTY_BIT_FIELD != pass) {
		return pass;
	}

	for (int iy = data._i.min().y(); iy <= data._i.max().y(); ++iy) {
		for (int ix = data._i.min().x(); ix <= data._i.max().x(); ++ix) {
			Index1D tileIndex(ix + iy * data._mesh->_tmem.getInfo().getTileCountX());
			BIT_FIELD pass = data._mesh->getTileInfo(tileIndex).testFX(bits);
			if (EMPTY_BIT_FIELD != pass) {
				return pass;
			}
			g_meshStats.mpdfxTests++;
		}
	}

	return pass;
}

BIT_FIELD ego_mesh_t::test_wall(const Ego::Vector3f& pos, const float radius, const BIT_FIELD bits) const {
	return test_wall(bits, mesh_wall_data_t(this, Ego::Circle2f(Ego::Point2f(pos[kX], pos[kY]), radius)));
}

float ego_mesh_t::get_pressure(const Ego::Vector3f& pos, float radius, const BIT_FIELD bits) const
{
    const float tile_area = Info<float>::Grid::Size() * Info<float>::Grid::Size();


    // deal with the optional parameters
    float loc_pressure = 0.0f;

    if (0 == bits) return 0;

    if ( 0 == _info.getTileCount() || _tmem.getInfo().getTileCount() == 0 ) return 0;

    // make an alias for the radius
    float loc_radius = radius;

    // set a minimum radius
    if ( 0.0f == loc_radius )
    {
        loc_radius = Info<float>::Grid::Size() * 0.5f;
    }

    // make sure it is positive
    loc_radius = std::abs( loc_radius );

    float fx_min = pos[kX] - loc_radius;
    float fx_max = pos[kX] + loc_radius;

    float fy_min = pos[kY] - loc_radius;
    float fy_max = pos[kY] + loc_radius;

    float obj_area = ( fx_max - fx_min ) * ( fy_max - fy_min );

    int ix_min = std::floor( fx_min / Info<float>::Grid::Size());
    int ix_max = std::floor( fx_max / Info<float>::Grid::Size());

    int iy_min = std::floor( fy_min / Info<float>::Grid::Size());
    int iy_max = std::floor( fy_max / Info<float>::Grid::Size());

    for ( int iy = iy_min; iy <= iy_max; iy++ )
    {
        bool tile_valid = true;

        float ty_min = ( iy + 0 ) * Info<float>::Grid::Size();
        float ty_max = ( iy + 1 ) * Info<float>::Grid::Size();

        if ( iy < 0 || iy >= _info.getTileCountY() )
        {
            tile_valid = false;
        }

        for ( int ix = ix_min; ix <= ix_max; ix++ )
        {
            bool is_blocked = false;

			float area_ratio;
            float ovl_x_min, ovl_x_max;
            float ovl_y_min, ovl_y_max;

            float tx_min = ( ix + 0 ) * Info<float>::Grid::Size();
            float tx_max = ( ix + 1 ) * Info<float>::Grid::Size();

            if ( ix < 0 || ix >= _info.getTileCountX() )
            {
                tile_valid = false;
            }

            if ( tile_valid )
            {
                Index1D itile = getTileIndex(Index2D(ix, iy));
                tile_valid = grid_is_valid( itile );
                if ( !tile_valid )
                {
                    is_blocked = true;
                }
                else
                {
                    is_blocked = 0 != _tmem.get(itile).testFX(bits);
                }
            }

            if ( !tile_valid )
            {
                is_blocked = true;
            }

            if ( is_blocked )
            {
                // hiting the mesh
                float min_area;

                // determine the area overlap of the tile with the
                // object's bounding box
                ovl_x_min = std::max( fx_min, tx_min );
                ovl_x_max = std::min( fx_max, tx_max );

                ovl_y_min = std::max( fy_min, ty_min );
                ovl_y_max = std::min( fy_max, ty_max );

                min_area = std::min( tile_area, obj_area );

                area_ratio = 0.0f;
                if ( ovl_x_min <= ovl_x_max && ovl_y_min <= ovl_y_max )
                {
                    if ( 0.0f == min_area )
                    {
                        area_ratio = 1.0f;
                    }
                    else
                    {
                        area_ratio  = ( ovl_x_max - ovl_x_min ) * ( ovl_y_max - ovl_y_min ) / min_area;
                    }
                }

                loc_pressure += area_ratio;

                g_meshStats.pressureTests++;
            }
        }
    }

    return loc_pressure;
}

//--------------------------------------------------------------------------------------------

Ego::Vector3f ego_mesh_t::get_diff(const Ego::Vector3f& pos, float radius, float center_pressure, const BIT_FIELD bits)
{
	/// @author BB
	/// @details determine the shortest "way out", but creating an array of "pressures"
	/// with each element representing the pressure when the object is moved in different directions
	/// by 1/2 a tile.

	const float jitter_size = Info<float>::Grid::Size() * 0.5f;
	std::array<float, 9> pressure_ary = {};
	float fx, fy;
	Ego::Vector3f diff = idlib::zero<Ego::Vector3f>();
	float   sum_diff = 0.0f;
	float   dpressure;

	int cnt;

	// Find the pressure for the 9 points of jittering around the current position.
	pressure_ary[4] = center_pressure;
	for (cnt = 0, fy = pos[kY] - jitter_size; fy <= pos[kY] + jitter_size; fy += jitter_size)
	{
		for (fx = pos[kX] - jitter_size; fx <= pos[kX] + jitter_size; fx += jitter_size, cnt++)
		{
			Ego::Vector3f jitter_pos(fx, fy, 0.0f);
			if (4 == cnt) continue;
			pressure_ary[cnt] = get_pressure(jitter_pos, radius, bits);
		}
	}

	// Determine the "minimum number of tiles to move" to get into a clear area.
	diff[kX] = diff[kY] = 0.0f;
	sum_diff = 0.0f;
	for (cnt = 0, fy = -0.5f; fy <= 0.5f; fy += 0.5f)
	{
		for (fx = -0.5f; fx <= 0.5f; fx += 0.5f, cnt++)
		{
			if (4 == cnt) continue;

			dpressure = (pressure_ary[cnt] - center_pressure);

			// Find the maximal pressure gradient == the minimal distance to move.
			if (0.0f != dpressure)
			{
				float   dist = pressure_ary[4] / dpressure;

				Ego::Vector2f tmp(dist * fx, dist * fy);

				float weight = 1.0f / dist;

				diff[XX] += tmp[YY] * weight;
				diff[YY] += tmp[XX] * weight;
				sum_diff += std::abs(weight);
			}
		}
	}
	// normalize the displacement by dividing by the weight...
	// unnecessary if the following normalization is kept in
	//if( sum_diff > 0.0f )
	//{
	//    diff[kX] /= sum_diff;
	//    diff[kY] /= sum_diff;
	//}

	// Limit the maximum displacement to less than one tile.
	if (std::abs(diff[kX]) + std::abs(diff[kY]) > 0.0f)
	{
		float fmax = std::max(std::abs(diff[kX]), std::abs(diff[kY]));

		diff[kX] /= fmax;
		diff[kY] /= fmax;
	}

	return diff;
}

BIT_FIELD ego_mesh_t::hit_wall(const Ego::Vector3f& pos, float radius, const BIT_FIELD bits, Ego::Vector2f& nrm, float *pressure, const mesh_wall_data_t& data) const {
	bool invalid;

	float  loc_pressure;

	bool needs_pressure = (NULL != pressure);
	bool needs_nrm = true;

	// deal with the optional parameters
	if (NULL == pressure) pressure = &loc_pressure;
	*pressure = 0.0f;

	nrm = idlib::zero<Ego::Vector2f>();


	// ego_mesh_test_wall() clamps pdata->ix_* and pdata->iy_* to valid values

	BIT_FIELD loc_pass = 0;
	nrm[kX] = nrm[kY] = 0.0f;
	for (int iy = data._i.min().y(); iy <= data._i.max().y(); iy++)
	{
		invalid = false;

		float ty_min = (iy + 0) * Info<float>::Grid::Size();
		float ty_max = (iy + 1) * Info<float>::Grid::Size();

		if (iy < 0 || iy >= _info.getTileCountY())
		{
			loc_pass |= (MAPFX_IMPASS | MAPFX_WALL);

			if (needs_nrm)
			{
				nrm[kY] += pos[kY] - (ty_max + ty_min) * 0.5f;
			}

			invalid = true;
			g_meshStats.boundTests++;
		}

		for (int ix = data._i.min().x(); ix <= data._i.max().x(); ix++)
		{
			float tx_min = (ix + 0) * Info<float>::Grid::Size();
			float tx_max = (ix + 1) * Info<float>::Grid::Size();

			if (ix < 0 || ix >= data._mesh->_info.getTileCountX())
			{
				loc_pass |= MAPFX_IMPASS | MAPFX_WALL;

				if (needs_nrm)
				{
					nrm[kX] += pos[kX] - (tx_max + tx_min) * 0.5f;
				}

				invalid = true;
				g_meshStats.boundTests++;
			}

			if (!invalid)
			{
				Index1D itile = getTileIndex(Index2D(ix, iy));
				if (grid_is_valid(itile))
				{
					BIT_FIELD mpdfx = data._mesh->getTileInfo(itile).getFX();
					bool is_blocked = HAS_SOME_BITS(mpdfx, bits);

					if (is_blocked)
					{
						SET_BIT(loc_pass, mpdfx);

						if (needs_nrm)
						{
							nrm[kX] += pos[kX] - (tx_max + tx_min) * 0.5f;
							nrm[kY] += pos[kY] - (ty_max + ty_min) * 0.5f;
						}
					}
				}
			}
		}
	}

	uint32_t pass = loc_pass & bits;

	if (0 == pass)
	{
		// if there is no impact at all, there is no normal and no pressure
		nrm = idlib::zero<Ego::Vector2f>();
		*pressure = 0.0f;
	}
	else
	{
		if (needs_nrm)
		{
			// special cases happen a lot. try to avoid computing the square root
			if (0.0f == nrm[kX] && 0.0f == nrm[kY])
			{
				// no normal does not mean no net pressure,
				// just that all the simplistic normal calculations balance
			}
			else if (0.0f == nrm[kX])
			{
				nrm[kY] = sgn(nrm[kY]);
			}
			else if (0.0f == nrm[kY])
			{
				nrm[kX] = sgn(nrm[kX]);
			}
			else
			{
				nrm = Ego::normalize(nrm).get_vector();
			}
		}

		if (needs_pressure)
		{
			*pressure = get_pressure(pos, radius, bits);
		}
	}

	return pass;
}

BIT_FIELD ego_mesh_t::hit_wall(const Ego::Vector3f& pos, const float radius, const BIT_FIELD bits, Ego::Vector2f& nrm, float * pressure) const
{
	return hit_wall(pos, radius, bits, nrm, pressure, mesh_wall_data_t(this, Ego::Circle2f(Ego::Point2f(pos[kX], pos[kY]), radius)));
}

//--------------------------------------------------------------------------------------------

float ego_mesh_t::getElevation(const Ego::Vector2f& p, bool waterwalk) const
{
    const float floorElevation = getElevation(p);
    water_instance_t& water = GameSessionContext::get().water();

    if (waterwalk && water._surface_level > floorElevation && water._is_water) {
        if (0 != test_fx(getTileIndex(p), MAPFX_WATER)) {
            return water._surface_level;
        }
    }
    return floorElevation;
}
