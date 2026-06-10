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

/// @file egolib/game/mesh_loader.c
/// @brief MeshLoader::convert and MeshLoader::operator() — map-file to ego_mesh_t conversion

#include "egolib/game/mesh.h"
#include "egolib/FileFormats/Globals.hpp"
#include "egolib/Log/_Include.hpp"

//--------------------------------------------------------------------------------------------

std::shared_ptr<ego_mesh_t> MeshLoader::convert(const map_t& source) const
{
    // Create a mesh.
    auto target = std::make_shared<ego_mesh_t>(Ego::MeshInfo(source._info.getVertexCount(), source._info.getTileCountX(), source._info.getTileCountY()));
	tile_mem_t& tmem_dst = target->_tmem;
	Ego::MeshInfo& info_dst = target->_info;

    // copy all the per-tile info
    for (Index1D cnt = 0; cnt < info_dst.getTileCount(); cnt++)
    {
        const tile_info_t& ptile_src = source._mem.tiles[cnt.i()];
        ego_tile_info_t& ptile_dst = tmem_dst.get(cnt);

        // do not BLANK_STRUCT_PTR() here, since these were constructed when they were allocated
        ptile_dst._type = ptile_src.type;
        ptile_dst._img  = ptile_src.img;

        // do not BLANK_STRUCT_PTR() here, since these were constructed when they were allocated
		ptile_dst._base_fx = ptile_src.fx;
		ptile_dst._twist   = ptile_src.twist;

        // set the local fx flags
		ptile_dst._pass_fx = ptile_dst._base_fx;

        // lcache is set in the constructor
        // nlst is set in the constructor
    }

    // copy all the per-vertex info
    for (size_t cnt = 0; cnt < source._info.getVertexCount(); cnt++ )
    {
		GLXvector3f& ppos_dst = tmem_dst._plst[cnt];
        GLXvector3f& pcol_dst = tmem_dst._clst[cnt];
        const map_vertex_t& pvrt_src = source._mem.vertices[cnt];

        // copy all info from map_mem_t
        ppos_dst[XX] = pvrt_src.pos[kX];
        ppos_dst[YY] = pvrt_src.pos[kY];
        ppos_dst[ZZ] = pvrt_src.pos[kZ];

        // default color
        pcol_dst[RR] = pcol_dst[GG] = pcol_dst[BB] = 0.0f;

        // tlist is set below
    }

    // copy some of the pre-calculated grid lighting
    for (uint32_t cnt = 0; cnt < info_dst.getTileCount(); cnt++ )
    {
        size_t vertex = tmem_dst.get(cnt)._vrtstart;
        ego_tile_info_t& ptile_dst = tmem_dst.get(cnt);
        const map_vertex_t& pvrt_src = source._mem.vertices[vertex];

		ptile_dst._a = pvrt_src.a;
		ptile_dst._l = 0.0f;
    }

	return target;
}

//--------------------------------------------------------------------------------------------
std::shared_ptr<ego_mesh_t> MeshLoader::operator()(const std::string& moduleName) const
{
	map_t map;
	// Load the map data.
	tile_dictionary_load_vfs("mp_data/fans.txt", tile_dict);
	if (!map.load("mp_data/level.mpd"))
	{
        Log::Entry entry(Log::Level::Error, __FILE__, __LINE__);
	    entry << "unable to load mesh of module `" << moduleName << "`" << Log::EndOfEntry;
        Log::activeTarget() << entry;
		throw idlib::runtime_error(__FILE__, __LINE__, entry.getText());
	}
	// Create the mesh from map.
	std::shared_ptr<ego_mesh_t> mesh = convert(map);
	if (!mesh)
	{
        auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to convert mesh of module ", "`",
                                    moduleName, "`", Log::EndOfEntry);
        Log::activeTarget() << e;
		throw idlib::runtime_error(__FILE__, __LINE__, e.getText());
	}
	mesh->finalize();
	return mesh;
}
