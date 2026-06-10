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

/// @file egolib/game/mesh_query.c
/// @brief Pure index/elevation/FX query operations on ego_mesh_t

#include "egolib/game/mesh.h"
#include "egolib/map_functions.h"  // cartman_calc_twist, CARTMAN_FIXNUM, CARTMAN_SLOPE

//--------------------------------------------------------------------------------------------

bool ego_mesh_t::tile_has_bits( const Index2D& i, const BIT_FIELD bits ) const
{
    // Figure out which tile we are on.
    Index1D j = getTileIndex(i);

    // Everything outside the map bounds is wall and impassable.
    if (!grid_is_valid(j))
    {
        return HAS_SOME_BITS((MAPFX_IMPASS | MAPFX_WALL), bits);
    }

    // Since we KNOW that this is in range, allow raw access to the data structure.
    GRID_FX_BITS fx = _tmem.get(j).getFX();

    return HAS_SOME_BITS(fx, bits);
}

bool ego_mesh_t::grid_is_valid(const Index1D& i) const
{
	g_meshStats.boundTests++;
    return _info.isValid(i);
}

Ego::Vector2f toWorldLT(const Index2D i) {
    return Ego::Vector2f((float)i.x(), (float)i.y()) * Info<float>::Grid::Size();
}

float ego_mesh_t::getElevation(const Ego::Vector2f& p) const
{
    Index1D i1 = getTileIndex(p);
	if (!grid_is_valid(i1)) {
		return 0;
	}

    // Get the height of each fan corner.
    float z0 = _tmem._plst[_tmem.get(i1)._vrtstart + 0][ZZ];
    float z1 = _tmem._plst[_tmem.get(i1)._vrtstart + 1][ZZ];
    float z2 = _tmem._plst[_tmem.get(i1)._vrtstart + 2][ZZ];
    float z3 = _tmem._plst[_tmem.get(i1)._vrtstart + 3][ZZ];

    //Calculate where on the tile we are relative to top left corner of the tile (0,0)
    Ego::Vector2f posOnTile = Ego::Vector2f(static_cast<float>(static_cast<int>(p.x()) % Info<int>::Grid::Size()),
                                            static_cast<float>(static_cast<int>(p.y()) % Info<int>::Grid::Size()));

    // Get the weighted height of each side.
    float zleft = (z0 * (Info<float>::Grid::Size() - posOnTile.y()) + z3 * posOnTile.y()) / Info<float>::Grid::Size();
    float zright = (z1 * (Info<float>::Grid::Size() - posOnTile.y()) + z2 * posOnTile.y()) / Info<float>::Grid::Size();
    float zdone = (zleft * (Info<float>::Grid::Size() - posOnTile.x()) + zright * posOnTile.x()) / Info<float>::Grid::Size();

    return zdone;
}

Index1D ego_mesh_t::getTileIndex(const Ego::Vector2f& p) const
{
    if (p.x() >= 0.0f && p.x() < _tmem._edge_x &&
		p.y() >= 0.0f && p.y() < _tmem._edge_y)
    {
        // Map world coordinates to a tile index.
        // This function does not assume the point to be within the bounds of the mesh.
        // If a point is passed which is outside the bounds, the resulting index will
        // be invalid w.r.t. to the mesh.
        Index2D i2 = Index2D(static_cast<int>(p.x()) / Info<int>::Grid::Size(),
                             static_cast<int>(p.y()) / Info<int>::Grid::Size());

        return getTileIndex(i2);
    }
    return Index1D::Invalid;
}

Index1D ego_mesh_t::getTileIndex(const Index2D& i) const
{
    if (!_info.isValid(i)) {
        return Index1D::Invalid;
    }
	return _info.map(i);
}

bool ego_mesh_t::clear_fx( const Index1D& i, const BIT_FIELD flags )
{
	g_meshStats.boundTests++;
    if (!_info.isValid(i)) {
        return false;
    }
	g_meshStats.mpdfxTests++;

    if (_tmem.get(i).removeFX(flags)) {
        _fxlists.dirty = true;
        return true;
    } else {
        return false;
    }
}

bool ego_mesh_t::add_fx(const Index1D& i, const BIT_FIELD flags)
{
    // Validate tile index.
	g_meshStats.boundTests++;
    if (!_info.isValid(i)) {
        return false;
    }

    // Succeed only of something actually changed.
	g_meshStats.mpdfxTests++;
    bool retval = _tmem.get(i).addFX(flags);

    if ( retval )
    {
        _fxlists.dirty = true;
    }

    return retval;
}

uint32_t ego_mesh_t::test_fx(const Index1D& i, const BIT_FIELD flags) const
{
    // test for a trivial value of flags
    if (EMPTY_BIT_FIELD == flags) return 0;

    // test for invalid tile
	g_meshStats.boundTests++;
    if (!_info.isValid(i)) {
        return flags & (MAPFX_WALL | MAPFX_IMPASS);
    }

    // if the tile is actually labelled as MAP_FANOFF, ignore it completely
    if (_tmem.get(i).isFanOff())
    {
        return 0;
    }

	g_meshStats.mpdfxTests++;
    return _tmem.get(i).testFX(flags);
}

ego_tile_info_t& ego_mesh_t::getTileInfo(const Index1D& i) {
    _info.assertValid(i);
	return _tmem.get(i);
}

const ego_tile_info_t& ego_mesh_t::getTileInfo(const Index1D& i) const {
    _info.assertValid(i);
    return _tmem.get(i);
}

uint8_t ego_mesh_t::get_twist(const Index1D& i) const
{
    if (!_info.isValid(i)) {
        return TWIST_FLAT;
    }
    return _tmem.get(i)._twist;
}

uint8_t ego_mesh_t::get_fan_twist(const Index1D& i) const
{
    if (!_info.isValid(i)) {
        return TWIST_FLAT;
    }
    const ego_tile_info_t& info = _tmem.get(i);
    // if the tile is actually labelled as MAP_FANOFF, ignore it completely
	if (info.isFanOff())
    {
        return TWIST_FLAT;
    }
    size_t vrtstart = info._vrtstart;

    float z0 = _tmem._plst[vrtstart + 0][ZZ];
    float z1 = _tmem._plst[vrtstart + 1][ZZ];
    float z2 = _tmem._plst[vrtstart + 2][ZZ];
    float z3 = _tmem._plst[vrtstart + 3][ZZ];

    float zx = CARTMAN_FIXNUM * (z0 + z3 - z1 - z2) / CARTMAN_SLOPE;
    float zy = CARTMAN_FIXNUM * (z2 + z3 - z0 - z1) / CARTMAN_SLOPE;

    return cartman_calc_twist(zx, zy);
}

float ego_mesh_t::get_max_vertex_0(const Index2D& i) const
{
	Index1D j = getTileIndex(i);
    if (!_info.isValid(j)) {
        return 0.0f;
    }

    const ego_tile_info_t& tile = _tmem.get(j);

	size_t vstart = tile._vrtstart;
	size_t vcount = std::min(static_cast<size_t>(4), _tmem.getInfo().getVertexCount());

	size_t cnt;
	size_t ivrt = vstart;
	float zmax = _tmem._plst[ivrt][ZZ];
	for (ivrt++, cnt = 1; cnt < vcount; ivrt++, cnt++)
	{
		zmax = std::max(zmax, _tmem._plst[ivrt][ZZ]);
	}

	return zmax;
}

float ego_mesh_t::get_max_vertex_1(const Index2D& i, float xmin, float ymin, float xmax, float ymax) const
{
	static const int ix_off[4] = { 1, 1, 0, 0 };
	static const int iy_off[4] = { 0, 1, 1, 0 };

	Index1D j = getTileIndex(i);

    if (!_info.isValid(j)) {
        return 0.0f;
    }

	size_t vstart = _tmem.get(j)._vrtstart;
	size_t vcount = std::min((size_t)4, _tmem.getInfo().getVertexCount());

	float zmax = -1e6;
	for (size_t ivrt = vstart, cnt = 0; cnt < vcount; ivrt++, cnt++)
	{
		GLXvector3f& vert = _tmem._plst[ivrt];

		// we are evaluating the height based on the grid, not the actual vertex positions
		float fx = (i.x() + ix_off[cnt]) * Info<float>::Grid::Size();
		float fy = (i.y() + iy_off[cnt]) * Info<float>::Grid::Size();

		if (fx >= xmin && fx <= xmax && fy >= ymin && fy <= ymax)
		{
			zmax = std::max(zmax, vert[ZZ]);
		}
	}

	if (-1e6 == zmax) zmax = 0.0f;

	return zmax;
}
