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

/// @file egolib/game/mesh_fx.c
/// @brief ego_tile_info_t, mesh_wall_data_t, mpdfx_list_ary_t, mpdfx_lists_t implementations

#include "egolib/game/mesh.h"

//--------------------------------------------------------------------------------------------

mesh_wall_data_t::mesh_wall_data_t(const ego_mesh_t *mesh,
	                               const Ego::AxisAlignedBox2f& f,
	                               const IndexRect& i)
	: _mesh(mesh), _f(f), _i(i)
{
	if (nullptr == _mesh) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
	}
}

mesh_wall_data_t::mesh_wall_data_t(const ego_mesh_t *mesh, const Ego::Circle2f& circle)
	: _mesh(mesh),
	  _f(leastClosure(circle)),
	  _i(Index2D(0, 0), Index2D(0, 0))
{
	if (nullptr == mesh) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
	}
	_mesh = mesh;
	// Limit the coordinate rectangle to be in bounds.
    {
        auto min = Ego::Point2f(std::max(_f.get_min().x(), 0.0f),
                                std::max(_f.get_min().y(), 0.0f));
        auto max = Ego::Point2f(std::min(_f.get_max().x(), _mesh->_tmem._edge_x),
                                std::min(_f.get_max().y(), _mesh->_tmem._edge_y));
        _f = Ego::AxisAlignedBox2f(min, max);
    }
    // Limit the index rectangle to be in bounds.
    {
        auto min = Index2D(std::floor(_f.get_min().x() / Info<float>::Grid::Size()),
                           std::floor(_f.get_min().y() / Info<float>::Grid::Size()));
        auto max = Index2D(std::floor(_f.get_max().x() / Info<float>::Grid::Size()),
                           std::floor(_f.get_max().y() / Info<float>::Grid::Size()));
        _i = IndexRect(min, max);
    }
}

//--------------------------------------------------------------------------------------------

ego_tile_info_t::ego_tile_info_t() :
    _itile(0),
    _type(0),
    _img(0),
    _vrtstart(0),
    _fanoff(true),
    _ncache{0, 0, 0, 0},
	_lightingCache(),
	_vertexLightingCache(),
    _oct(),
	_base_fx(0), _pass_fx(0), _a(0), _l(0), _cache_frame(-1), _twist(TWIST_FLAT)
{
    //ctor
}

GRID_FX_BITS ego_tile_info_t::testFX(const GRID_FX_BITS bits) const {
	return getFX() & bits;
}

GRID_FX_BITS ego_tile_info_t::getFX() const {
	return _pass_fx;
}

bool ego_tile_info_t::setFX(const GRID_FX_BITS bits) {
	// Save the old bits.
	GRID_FX_BITS oldBits = getFX();

	// Modify the bits.
	_pass_fx = bits;

	// Get the new bits.
	GRID_FX_BITS newBits = getFX();

	// Return if the bits were actually modified.
	return oldBits != newBits;
}

bool ego_tile_info_t::addFX(const GRID_FX_BITS bits) {
	// Save the old bits.
	GRID_FX_BITS oldBits = getFX();

	// Modify the bits.
	SET_BIT(_pass_fx, bits);

	// Get the new bits.
	GRID_FX_BITS newBits = getFX();

	// Return if the bits were actually modified.
	return oldBits != newBits;
}

bool ego_tile_info_t::removeFX(const GRID_FX_BITS bits) {
	// Save the old bits.
	GRID_FX_BITS oldBits = getFX();

	// Modify the bits.
	UNSET_BIT(_pass_fx, bits);

	// Get the new bits.
	GRID_FX_BITS newBits = getFX();

	// Return if the bits were actually modified.
	return oldBits != newBits;
}

//--------------------------------------------------------------------------------------------

mpdfx_list_ary_t::mpdfx_list_ary_t()
	: elements() {
}

mpdfx_list_ary_t::~mpdfx_list_ary_t() {
}

void mpdfx_list_ary_t::clear()
{
    elements.clear();
}

void mpdfx_list_ary_t::push_back(const Index1D& element)
{
    elements.push_back(element);
}

//--------------------------------------------------------------------------------------------

mpdfx_lists_t::mpdfx_lists_t(const Ego::MeshInfo& info) {
	sha.elements.reserve(info.getTileCount());
	drf.elements.reserve(info.getTileCount());
	anm.elements.reserve(info.getTileCount());
	wat.elements.reserve(info.getTileCount());
	wal.elements.reserve(info.getTileCount());
	imp.elements.reserve(info.getTileCount());
	dam.elements.reserve(info.getTileCount());
	slp.elements.reserve(info.getTileCount());

	// the list needs to be resynched
	dirty = true;
}

mpdfx_lists_t::~mpdfx_lists_t() {
	// No memory, hence nothing is stored, hence nothing is dirty.
	dirty = false;
}

void mpdfx_lists_t::reset()
{
    // Clear the lists.
    sha.clear();
    drf.clear();
    anm.clear();
    wat.clear();
    wal.clear();
    imp.clear();
    dam.clear();
    slp.clear();

    // Everything has been reset. Force it to recalculate.
	dirty = true;
}

int mpdfx_lists_t::push( GRID_FX_BITS fx_bits, size_t value )
{
    int retval = 0;

    if ( 0 == fx_bits ) return true;

    if ( HAS_NO_BITS( fx_bits, MAPFX_SHA ) )
    {
        sha.push_back(value);
        {
            retval++;
        }

    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_REFLECTIVE ) )
    {
        drf.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_ANIM ) )
    {
        anm.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_WATER ) )
    {
        wat.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_WALL ) )
    {
        wal.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_IMPASS ) )
    {
        imp.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_DAMAGE ) )
    {
        dam.push_back(value);
        {
            retval++;
        }
    }

    if ( HAS_ALL_BITS( fx_bits, MAPFX_SLIPPY ) )
    {
        slp.push_back(value);
        {
            retval++;
        }
    }

    return retval;
}

bool mpdfx_lists_t::synch( const tile_mem_t& tmem, bool force )
{
    if ( 0 == tmem.getInfo().getTileCount()) return true;

    // don't re-calculate unless it is necessary
    if ( !force && !dirty ) return true;

    // !!reset the counts!!
	reset();

    for (size_t i = 0; i < tmem.getInfo().getTileCount(); i++ )
    {
		push(tmem.get(i).getFX(), i);
    }

    // we're done calculating
	dirty = false;

    return true;
}
