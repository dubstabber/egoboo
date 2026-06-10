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

/// @file egolib/Mesh/ITerrainQuery.hpp
/// @brief Lower-layer read-only terrain query surface for AI/pathing code.

#pragma once

#include "egolib/Mesh/Info.hpp" // Index1D / Index2D

#include <cstdint>

namespace Ego
{
namespace Mesh
{

/// @brief Minimal tile-query surface used by pathfinding and line-of-sight code.
///
/// Implemented by the active game module as a thin adapter over ego_mesh_t. Keeping this
/// interface in the Mesh layer lets AI code query terrain without depending on game/mesh.h.
class ITerrainQuery
{
public:
    virtual ~ITerrainQuery() = default;

    /// @brief The tile index at grid coordinate @a tile, or Index1D::Invalid if out of range.
    virtual Index1D getTileIndex(const Index2D& tile) const = 0;

    /// @brief The subset of @a flags set on @a tile.
    virtual uint32_t testFX(const Index1D& tile, uint32_t flags) const = 0;

    /// @brief Whether any of @a flags block grid coordinate @a tile.
    virtual bool tileHasBits(const Index2D& tile, uint32_t flags) const = 0;

    /// @brief Whether @a tile is explicitly disabled by the map fan-off marker.
    virtual bool isFanOff(const Index1D& tile) const = 0;
};

} // namespace Mesh
} // namespace Ego
