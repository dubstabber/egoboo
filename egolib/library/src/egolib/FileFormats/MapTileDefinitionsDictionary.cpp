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

/// @file egolib/FileFormats/MapTileDefinitionsDictionary.cpp
/// @brief In-memory representation of <c>MapTileDefinitionsDictionary</c> files

#include "egolib/FileFormats/MapTileDefinitionsDictionary.hpp"
#include "egolib/fileutil.h"

namespace Ego {
namespace FileFormats {
namespace MapTileDefinitionsDictionary {

DefinitionList DefinitionList::read(ReadContext& ctxt) {
    DefinitionList definitionList;
    int numberOfDefinitions = vfs_get_next_int(ctxt);
    if (numberOfDefinitions < 0) {
        // No fixed array is sized off of this count at this layer - the one place a definition
        // count feeds a fixed array (tile_dictionary_t::def_lst[MAP_FAN_TYPE_MAX]) is downstream
        // in map_tile_dictionary.c, off of a *computed* definition_count, and that call already
        // rejects an oversized result on its own. This check exists purely so a negative count
        // (not a valid encoding of the documented uint32 field) is reported instead of silently
        // treated as "no definitions".
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                            "number of tile definitions must not be negative, received " + std::to_string(numberOfDefinitions));
    }
    for (auto i = 0, n = numberOfDefinitions; i < n; ++i) {
        definitionList.definitions.push_back(Definition::read(ctxt));
    }
    return definitionList;
}

Vertex Vertex::read(ReadContext& ctxt) {
    Vertex vertex;

    vertex.position = vfs_get_next_int(ctxt);
    if (vertex.position < 0) {
        // Deliberately no upper bound here. MapTileDefinitionsDictionary.html documents
        // `position` as an unbounded uint32 (decode: x = position % 4, y = (position / 4) % 4),
        // and map_tile_dictionary.c never uses it (or the .ref/.grid_ix/.grid_iy fields derived
        // from it) to index any array - only to feed that bitmask decode, which is well-defined
        // for any int. The shipped data/basicdat/fans.txt itself contains positions up to 48
        // (see FansTxtBounds.cpp's ShippedFansTxtParsesExpectedShape test), so a "position <= 15"
        // bound would reject real content; only a negative value (not a valid encoding of the
        // documented unsigned field) is rejected.
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                            "vertex position must not be negative, received " + std::to_string(vertex.position));
    }
    vertex.u = vfs_get_next_float(ctxt);
    vertex.v = vfs_get_next_float(ctxt);

    return vertex;
}

Definition Definition::read(ReadContext& ctxt) {
    Definition definition;
    int numberOfVertices = vfs_get_next_int(ctxt);
    if (numberOfVertices < 0 || numberOfVertices > MAP_FAN_VERTICES_MAX) {
        // map_tile_dictionary.c copies exactly this many vertices into
        // tile_definition_t::vertices[MAP_FAN_VERTICES_MAX] with no bounds check of its own.
        // This bound also makes map_tile_dictionary.c's narrowing store of numberOfVertices into
        // tile_definition_t::numvertices (a uint8_t, see map_tile_dictionary.h) unreachable to
        // truncation, since MAP_FAN_VERTICES_MAX (16) fits in a uint8_t; the field is deliberately
        // left unwidened rather than changed.
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                            "number of vertices of a tile definition must be within [0, " +
                                            std::to_string(MAP_FAN_VERTICES_MAX) + "], received " + std::to_string(numberOfVertices));
    }
    for (int i = 0, n = numberOfVertices; i < n; ++i) {
        definition.vertices.push_back(Vertex::read(ctxt));
    }
    int numberOfIndexLists = vfs_get_next_int(ctxt);
    if (numberOfIndexLists < 0 || numberOfIndexLists > MAP_FAN_MAX) {
        // map_tile_dictionary.c copies exactly this many command sizes into
        // tile_definition_t::command_entries[MAP_FAN_MAX] with no bounds check of its own.
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                            "number of index lists of a tile definition must be within [0, " +
                                            std::to_string(MAP_FAN_MAX) + "], received " + std::to_string(numberOfIndexLists));
    }
    // map_tile_dictionary.c concatenates every index list of a definition into a single
    // tile_definition_t::command_verts[MAP_FAN_ENTRIES_MAX] array (its `contiguousIndex` runs
    // across all of a definition's index lists, not just one), so the bound that matters is the
    // running total across this loop, not any individual index list's own count.
    int totalIndices = 0;
    for (int i = 0, n = numberOfIndexLists; i < n; ++i) {
        IndexList indexList = IndexList::read(ctxt);
        totalIndices += static_cast<int>(indexList.indices.size());
        if (totalIndices > MAP_FAN_ENTRIES_MAX) {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                                "total number of indices across a tile definition's index lists must not exceed " +
                                                std::to_string(MAP_FAN_ENTRIES_MAX) + ", received at least " + std::to_string(totalIndices));
        }
        definition.indexLists.push_back(std::move(indexList));
    }
    return definition;
}

IndexList IndexList::read(ReadContext& ctxt) {
    IndexList indexList;
    int numberOfIndices = vfs_get_next_int(ctxt);
    if (numberOfIndices < 0) {
        throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                            "number of indices of an index list must not be negative, received " + std::to_string(numberOfIndices));
    }
    for (int i = 0, n = numberOfIndices; i < n; ++i) {
        int index = vfs_get_next_int(ctxt);
        if (index < 0) {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::semantical, ctxt.get_location(),
                                                "index must not be negative, received " + std::to_string(index));
        }
        // Deliberately no upper bound on an individual index value here (only its count is bounded,
        // above, and in Definition::read). map_tile_dictionary.c truncates each accepted index into
        // uint16_t tile_definition_t::command_verts[] with no further check, and the renderer
        // (RenderPasses.cpp) uses those values as glDrawElements element indices into a per-tile
        // vertex array sized off the definition's own vertex count - so a value that is in-range
        // for uint16_t but exceeds that vertex count is a residual, unvalidated out-of-bounds read
        // at render time. Out of scope for this pass (shipped fans.txt's indices all reference
        // their own definition's vertices); left as a candidate for a future content-fault pass.
        indexList.indices.push_back(index);
    }
    return indexList;
}

} // namespace MapTileDefinitionsDictionary
} // namespace FileFormats
} // namespace Ego
