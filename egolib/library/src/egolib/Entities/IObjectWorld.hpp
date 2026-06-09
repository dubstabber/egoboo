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

/// @file egolib/Entities/IObjectWorld.hpp
/// @brief Lower-layer entity-world access seam for the physics step.

#pragma once

#include <vector>

// The handle types returned by this seam are both lower-layer / game-free:
//   ObjectHandler -> egolib/Entities/ObjectHandler.hpp
//   Team          -> egolib/Logic/Team.hpp
// so the interface can return them by reference without dragging in any game/ header.
class ObjectHandler;
class Team;

namespace Ego
{
namespace Entities
{

/// @brief The minimal entity-world surface the physics step needs: access to the object
///        container and the team list of the active world.
///
///        It is implemented by the game-layer GameModule and installed for the lifetime of
///        the active module, which lets the physics translation units
///        (ObjectPhysics / ParticlePhysics / CollisionSystem / particle_collision) reach the
///        object/team world without depending on the game/ layer (GameModule /
///        GameSessionContext). This is the sibling of Ego::Physics::ICollisionWorld (which
///        already seams the terrain/mesh queries) and, together with it, keeps the physics
///        TUs game-free at the symbol level — a prerequisite to a future egolib-physics
///        sub-library.
///
///        Both ObjectHandler and Team are already lower-layer types, so they are returned by
///        reference; every existing call (exists/get/operator[]/iterator/findObjectRefs on the
///        handler, indexing on the team list) is reached through the returned reference. The
///        physics TUs are read-only consumers of this world (no spawn / insert / remove), so
///        spawnObject() deliberately stays on GameModule and is not part of this seam.
class IObjectWorld
{
public:
    virtual ~IObjectWorld() = default;

    /// @brief The object container of the active world.
    virtual ObjectHandler& getObjectHandler() = 0;

    /// @brief The team list of the active world.
    virtual std::vector<Team>& getTeamList() = 0;
};

/// @brief Install @a world as the active object world. Called by the game session when a
///        module becomes active. Passing @a nullptr is equivalent to clearObjectWorld().
void installObjectWorld(IObjectWorld* world);

/// @brief Clear the active object world. Called when the active module is torn down.
void clearObjectWorld();

/// @brief The installed object world, or @a nullptr if none is installed.
IObjectWorld* tryActiveObjectWorld();

/// @brief The installed object world.
/// @throw std::logic_error if no object world is installed.
IObjectWorld& activeObjectWorld();

} // namespace Entities
} // namespace Ego
