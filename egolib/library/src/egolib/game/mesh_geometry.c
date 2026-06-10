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

/// @file egolib/game/mesh_geometry.c
/// @brief tile_mem_t, ego_mesh_t geometry methods: bbox, normals, twist, texture, finalize

#include "egolib/game/mesh.h"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/map_functions.h"  // cartman_calc_twist

//--------------------------------------------------------------------------------------------

static void warnNumberOfVertices(const char *file, int line, size_t numberOfVertices)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                     "mesh has too many vertices - ", numberOfVertices,
                                     " number of vertices requested, but maximum number of vertices is ",
                                     MAP_VERTICES_MAX);
}

//--------------------------------------------------------------------------------------------

tile_mem_t::tile_mem_t(const Ego::MeshInfo& info)
	: _tileList(info.getTileCount()), _info(info), _bbox() {
	// If the number of vertices exceeds the limits ...
	if (info.getVertexCount() > MAP_VERTICES_MAX) {
		// ... emit a warning.
		warnNumberOfVertices(__FILE__, __LINE__, info.getVertexCount());
	}
	// Set the mesh edge info.
	_edge_x = (info.getTileCountX() + 1) * Info<int>::Grid::Size();
	_edge_y = (info.getTileCountY() + 1) * Info<int>::Grid::Size();
	// Allocate the arrays.
	_plst = std::make_unique<GLXvector3f[]>(info.getVertexCount());
	_tlst = std::make_unique<GLXvector2f[]>(info.getVertexCount());
	_clst = std::make_unique<GLXvector3f[]>(info.getVertexCount());
	_nlst = std::make_unique<GLXvector3f[]>(info.getVertexCount());
}

tile_mem_t::~tile_mem_t() {
}

void tile_mem_t::computeVertexIndices(const tile_dictionary_t& dict)
{
	size_t vertexIndex = 0;
	for (size_t i = 0; i < _info.getTileCount(); ++i) {
		get(i)._vrtstart = vertexIndex;

		uint8_t type = get(i)._type;

		// Throw away any remaining upper bits.
		type &= 0x3F;

		const tile_definition_t *def = dict.get(type);
		if (!def) continue;

		vertexIndex += def->numvertices;
	}

	if (vertexIndex != _info.getVertexCount()) {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "wrong number of vertices: received ",
                                         vertexIndex, ", expected ", _info.getVertexCount(), Log::EndOfEntry);
	}
}

//--------------------------------------------------------------------------------------------

ego_mesh_t::ego_mesh_t(const Ego::MeshInfo& mesh_info)
	: _info(mesh_info), _tmem(mesh_info), _fxlists(mesh_info) {
}

ego_mesh_t::~ego_mesh_t() {
}

//--------------------------------------------------------------------------------------------

void ego_mesh_t::make_bbox()
{
    _tmem._bbox = Ego::AxisAlignedBox3f(Ego::Point3f(_tmem._plst[0][XX], _tmem._plst[0][YY], _tmem._plst[0][ZZ]),
		                                Ego::Point3f(_tmem._plst[0][XX], _tmem._plst[0][YY], _tmem._plst[0][ZZ]));

	for (Index1D cnt = 0; cnt < _info.getTileCount(); cnt++)
	{
        ego_tile_info_t& ptile = _tmem.get(cnt);
        oct_bb_t& poct = ptile._oct;


        ptile._itile = cnt.i();

		tile_definition_t *pdef = tile_dict.get(ptile._type & 0x3F);
		if (NULL == pdef) continue;

		// Initialize the octagonal bounding box of the tile with the first vertex of the tile ...
        size_t mesh_vrt = _tmem.get(cnt)._vrtstart;
        oct_vec_v2_t ovec = oct_vec_v2_t(Ego::Vector3f(_tmem._plst[mesh_vrt][0], _tmem._plst[mesh_vrt][1],_tmem._plst[mesh_vrt][2]));
        poct = oct_bb_t(ovec);
        mesh_vrt++;

        // ... then add the other vertex of the tile to it.
        for (uint8_t i = 1, n = pdef->numvertices; i < n; i++, mesh_vrt++ )
        {
            ovec = oct_vec_v2_t(Ego::Vector3f(_tmem._plst[mesh_vrt][0],_tmem._plst[mesh_vrt][1],_tmem._plst[mesh_vrt][2]));
            poct.join(ovec);
        }

		/// @todo: This test is not up-2-date anymore.
		///        If you join a bbox with an ovec, the box is never(!) empty.
		///        However, it still might be desirable not to have boxes with
		///        width, height and depth of one. Should be evaluated asap.
        // ensure that NO tile has zero volume.
        // if a tile is declared to have all the same height, it will accidentally be called "empty".
        if (poct._empty || (std::abs(poct._maxs[OCT_X] - poct._mins[OCT_X]) +
                            std::abs(poct._maxs[OCT_Y] - poct._mins[OCT_Y]) +
                            std::abs(poct._maxs[OCT_Z] - poct._mins[OCT_Z])) < std::numeric_limits<float>::epsilon())
        {
            ovec[OCT_X] = ovec[OCT_Y] = ovec[OCT_Z] = 0.1;
            ovec[OCT_XY] = ovec[OCT_YX] = idlib::sqrt_two<float>() * ovec[OCT_X];
            oct_bb_t::self_grow(poct, ovec);
        }

        // Add the bounds of the tile to the bounds of the mesh.
        _tmem._bbox.join(poct.toAxisAlignedBox());
    }
}

//--------------------------------------------------------------------------------------------

void ego_mesh_t::make_normals()
{
    // test for mesh

    // set the default normal for each fan, based on the calculated twist value
    for (Index1D fan0 = 0; fan0 < _tmem.getInfo().getTileCount(); fan0++ )
    {
        uint8_t twist = _tmem.get(fan0)._twist;

        _tmem._nlst[fan0.i()][XX] = g_meshLookupTables.twist_nrm[twist][kX];
        _tmem._nlst[fan0.i()][YY] = g_meshLookupTables.twist_nrm[twist][kY];
        _tmem._nlst[fan0.i()][ZZ] = g_meshLookupTables.twist_nrm[twist][kZ];
    }

	int      edge_is_crease[4];
	Ego::Vector3f nrm_lst[4], vec_sum;
	float    weight_lst[4];

    // find an "average" normal of each corner of the tile
    for (size_t iy = 0; iy < _info.getTileCountY(); iy++ )
    {
        for (size_t ix = 0; ix < _info.getTileCountX(); ix++ )
        {
            constexpr int ix_off[4] = {0, 1, 1, 0};
            constexpr int iy_off[4] = {0, 0, 1, 1};

            Index1D fan0 = getTileIndex(Index2D(ix, iy));
			if (!grid_is_valid(fan0)) {
				continue;
			}

            nrm_lst[0][kX] = _tmem._nlst[fan0.i()][XX];
            nrm_lst[0][kY] = _tmem._nlst[fan0.i()][YY];
            nrm_lst[0][kZ] = _tmem._nlst[fan0.i()][ZZ];

            // for each corner of this tile
            for (int i = 0; i < 4; i++ )
            {
                // the offset list needs to be shifted depending on what i is
                int dx, dy;
                size_t shift = ( 6 - i ) % 4;
                if ( 1 == ix_off[(4-shift) % 4] ){
                  dx = -1;
                }
                else {
                  dx = 0;
                }
                if ( 1 == iy_off[(4-shift) % 4] ){
                  dy = -1;
                }
                else{
                    dy = 0;
                }

                int loc_ix_off[4];
                int loc_iy_off[4];
                for (int k = 0; k < 4; k++ )
                {
                    loc_ix_off[k] = ix_off[( 4-shift + k ) % 4 ] + dx;
                    loc_iy_off[k] = iy_off[( 4-shift + k ) % 4 ] + dy;
                }

                // cache the normals
                // nrm_lst[0] is already known.
                for (int j = 1; j < 4; j++ )
                {
                    int jx = static_cast<int>(ix) + loc_ix_off[j];
                    int jy = static_cast<int>(iy) + loc_iy_off[j];

                    Index1D fan1 = getTileIndex(Index2D(jx, jy));

                    if ( grid_is_valid( fan1 ) )
                    {
                        nrm_lst[j][kX] = _tmem._nlst[fan1.i()][XX];
                        nrm_lst[j][kY] = _tmem._nlst[fan1.i()][YY];
                        nrm_lst[j][kZ] = _tmem._nlst[fan1.i()][ZZ];

                        if ( nrm_lst[j][kZ] < 0 )
                        {
                            nrm_lst[j] = -nrm_lst[j];
                        }
                    }
                    else
                    {
                        nrm_lst[j] = Ego::Vector3f(0.0f, 0.0f, 1.0f);
                    }
                }

                // find the creases
                for (int j = 0; j < 4; j++ )
                {
                    float vdot;
                    int m = ( j + 1 ) % 4;

                    vdot = Ego::dot(nrm_lst[j], nrm_lst[m]);

                    edge_is_crease[j] = (vdot < idlib::inv_sqrt_two<float>());

                    weight_lst[j] = Ego::dot(nrm_lst[j], nrm_lst[0]);
                }

                weight_lst[0] = 1.0f;
                if ( edge_is_crease[0] )
                {
                    // this means that there is a crease between tile 0 and 1
                    weight_lst[1] = 0.0f;
                }

                if ( edge_is_crease[3] )
                {
                    // this means that there is a crease between tile 0 and 3
                    weight_lst[3] = 0.0f;
                }

                if ( edge_is_crease[0] && edge_is_crease[3] )
                {
                    // this means that there is a crease between tile 0 and 1
                    // and a crease between tile 0 and 3, isolating tile 2
                    weight_lst[2] = 0.0f;
                }

                vec_sum = nrm_lst[0];
                for (int j = 1; j < 4; j++ )
                {
                    if ( weight_lst[j] > 0.0f )
                    {
                        vec_sum[kX] += nrm_lst[j][kX] * weight_lst[j];
                        vec_sum[kY] += nrm_lst[j][kY] * weight_lst[j];
                        vec_sum[kZ] += nrm_lst[j][kZ] * weight_lst[j];
                    }
                }

				vec_sum = Ego::normalize(vec_sum).get_vector();

                _tmem.get(fan0)._ncache[i][XX] = vec_sum[kX];
                _tmem.get(fan0)._ncache[i][YY] = vec_sum[kY];
                _tmem.get(fan0)._ncache[i][ZZ] = vec_sum[kZ];
            }
        }
    }
}

//--------------------------------------------------------------------------------------------

void ego_mesh_t::remove_ambient() {
	/// @brief Remove ambient.
	uint8_t min_vrt_a = 255;
	for (Index1D i = 0; i < _info.getTileCount(); ++i) {
		min_vrt_a = std::min(min_vrt_a, _tmem.get(i)._a);
	}
	for (Index1D i = 0; i < _info.getTileCount(); ++i) {
		_tmem.get(i)._a = _tmem.get(i)._a - min_vrt_a;
	}
}

void ego_mesh_t::recalc_twist() {
	// @brief recalculate the twist.
	for (Index1D i = 0; i < _info.getTileCount(); ++i) {
		uint8_t twist = get_fan_twist(i);
		_tmem.get(i)._twist = twist;
	}
}

bool ego_mesh_t::set_texture(const Index1D& index1D, uint16_t image)
{
	if (!grid_is_valid(index1D)) {
		return false;
	}

	// Get the upper and lower bits for this tile image.
	uint16_t tile_value = _tmem.get(index1D)._img;
	uint16_t tile_lower = image & TILE_LOWER_MASK;
	uint16_t tile_upper = tile_value & TILE_UPPER_MASK;

	// Set the actual image.
	_tmem.get(index1D)._img = tile_upper | tile_lower;

	// Update the pre-computed texture info.
	return update_texture(index1D);
}

bool ego_mesh_t::update_texture(const Index1D& i)
{
	if (!grid_is_valid(i)) {
		return false;
	}
	const ego_tile_info_t& tile = _tmem.get(i);
	uint8_t type = tile._type & 0x3F;

	tile_definition_t *pdef = tile_dict.get(type);
	if (!pdef) return false;

	size_t mesh_vrt = tile._vrtstart;
	for (uint16_t tile_vrt = 0; tile_vrt < pdef->numvertices; tile_vrt++, mesh_vrt++) {
		_tmem._tlst[mesh_vrt][SS] = pdef->vertices[tile_vrt].u;
		_tmem._tlst[mesh_vrt][TT] = pdef->vertices[tile_vrt].v;
	}

	return true;
}

void ego_mesh_t::make_texture() {
	/// @brief Set the texture coordinate for every vertex.
	for (Index1D i = 0; i < _info.getTileCount(); ++i) {
		update_texture(i);
	}
}

void ego_mesh_t::finalize()
{
	// (1) (Re)compute the vertex indices.
	_tmem.computeVertexIndices(tile_dict);
	remove_ambient();
	recalc_twist();
	make_normals();
	make_bbox();
	make_texture();

	// create some lists to make searching the mesh tiles easier
	_fxlists.synch(_tmem, true);
}
