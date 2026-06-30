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

#include "egolib/typedef.h"  // ObjectRef

#include <cstdint>
#include <vector>

// The handle types returned by this seam are both lower-layer / game-free:
//   ObjectHandler -> egolib/Entities/ObjectHandler.hpp
//   Team          -> egolib/Logic/Team.hpp
// so the interface can return them by reference without dragging in any game/ header.
class Object;
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
///        reference; every existing call (exists/get/getHandle/iterator/findObjectRefs on the
///        handler, indexing on the team list) is reached through the returned reference. The
///        physics TUs are read-only consumers of this world (no spawn / insert / remove), so
///        spawnObjectRef() deliberately stays on GameModule and is not part of this seam.
class IObjectWorld
{
public:
    virtual ~IObjectWorld() = default;

    /// @brief The object container of the active world.
    virtual ObjectHandler& getObjectHandler() = 0;

    /// @brief The live object for @a objectRef, or @a nullptr if the ref is invalid/stale.
    virtual Object* tryObject(ObjectRef objectRef) = 0;

    /// @brief The live object for @a objectRef, or @a nullptr if the ref is invalid/stale.
    virtual const Object* tryObject(ObjectRef objectRef) const = 0;

    /// @brief Whether @a objectRef currently resolves to a live object.
    virtual bool hasObject(ObjectRef objectRef) const = 0;

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

/// @brief The live object in the installed world, or @a nullptr if no world is installed
///        or the ref is invalid/stale.
Object* tryActiveObject(ObjectRef objectRef);

/// @brief The live object in the installed world, or @a nullptr if no world is installed
///        or the ref is invalid/stale.
const Object* tryActiveConstObject(ObjectRef objectRef);

/// @brief Whether the installed world contains a live object for @a objectRef.
bool activeObjectExists(ObjectRef objectRef);

/// @name Active-world update clock
/// @brief A sibling seam to the object world: the active world's update-tick counter, which the
///        physics step reads to compare entity timestamps (e.g. onwhichplatform_update) against the
///        current tick. The counter is owned and incremented by the game session; this seam exposes
///        a read-only view via an installed pointer that aliases the session-owned value (so reads
///        see the live count) and keeps the physics translation units off GameSessionContext. It is
///        a separate accessor rather than a method on IObjectWorld so the game session can install
///        it directly, without GameModule round-tripping back into the session, and so the object
///        world interface stays focused on the object/team containers.
/// @{

/// @brief Install @a counter as the active world's update-tick source, aliasing the session-owned
///        counter. Called by the game session alongside installObjectWorld(). Passing @a nullptr is
///        equivalent to clearWorldUpdateCounter().
void installWorldUpdateCounter(const uint32_t* counter);

/// @brief Clear the active world's update-tick source. Called when the active module is torn down.
void clearWorldUpdateCounter();

/// @brief The active world's update tick, or 0 if no counter is installed (matching the
///        session counter's reset-to-0 value outside an active module). Read-only.
uint32_t activeWorldUpdateCount();

/// @}

} // namespace Entities
} // namespace Ego
