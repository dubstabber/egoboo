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

/// @file egolib/Physics/ICollisionWorld.hpp
/// @brief Lower-layer collision-world query seam for the Collidable base.

#pragma once

#include "egolib/Mesh/Info.hpp"          // Index1D
#include "egolib/integrations/math.hpp"  // Ego::Vector2f

namespace Ego
{
namespace Physics
{

/// @brief The minimal spatial-world query surface the Collidable base needs to validate a
///        position update: an in-bounds check and a tile lookup.
///
///        It is implemented by the game-layer GameModule and installed for the lifetime of the
///        active module, which lets Collidable do position validation without depending on the
///        game/ layer (GameModule / mesh). This is the seam that keeps the Collidable base
///        game-free so it can live in a lower layer (a future egolib-physics sub-library).
class ICollisionWorld
{
public:
    virtual ~ICollisionWorld() = default;

    /// @brief Is the world position (@a x, @a y) within the playable map bounds?
    virtual bool isInside(float x, float y) const = 0;

    /// @brief The tile index of the tile at world point @a point.
    /// @return the tile index, or Index1D::Invalid if there is no tile at that point.
    virtual Index1D getTileIndex(const Ego::Vector2f& point) const = 0;
};

/// @brief Install @a world as the active collision world. Called by the game session when a
///        module becomes active. Passing @a nullptr is equivalent to clearCollisionWorld().
void installCollisionWorld(ICollisionWorld* world);

/// @brief Clear the active collision world. Called when the active module is torn down.
void clearCollisionWorld();

/// @brief The installed collision world, or @a nullptr if none is installed.
ICollisionWorld* tryActiveCollisionWorld();

/// @brief The installed collision world.
/// @throw std::logic_error if no collision world is installed.
ICollisionWorld& activeCollisionWorld();

} // namespace Physics
} // namespace Ego
