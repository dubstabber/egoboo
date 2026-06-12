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

/// @file egolib/game/Physics/ObjectPhysics_internal.h
/// @brief Private shared helpers for the ObjectPhysics TU family.
/// @details This header is NOT part of the public API. It is included only by
///          ObjectPhysics.cpp, ObjectPhysics_terrain.cpp, and ObjectPhysics_attachment.cpp.
///          The five inline accessors are placed in namespace object_physics_detail (not
///          anonymous namespace) + declared inline so that every TU gets the same definitions
///          and -Wunused-function is suppressed for helpers a given TU does not call.
#pragma once

#include "egolib/game/Physics/ObjectPhysics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() / activeWorldUpdateCount()
#include "egolib/Physics/ICollisionWorld.hpp"     // activeCollisionWorld()
#include "egolib/Physics/MeshLookupTables.hpp"    // g_meshLookupTables
#include "egolib/Physics/PhysicalConstants.hpp"   // g_environment, STOP_BOUNCING, CHR_INFINITE_WEIGHT, MOUNTTOLERANCE

namespace Ego { namespace Physics { namespace object_physics_detail {

/// The entity world the physics step queries (object container), reached through the
/// lower-layer Ego::Entities::IObjectWorld seam rather than game/ (GameModule).
inline Ego::Entities::IObjectWorld& objectWorld()
{
    return Ego::Entities::activeObjectWorld();
}

/// The terrain world the physics step queries (slope/elevation/slippy/water). This is the
/// active GameModule, but reached through the lower-layer Ego::Physics::ICollisionWorld seam
/// rather than its game/mesh.h surface.
inline Ego::Physics::ICollisionWorld& collisionWorld()
{
    return Ego::Physics::activeCollisionWorld();
}

/// The active world's update tick, reached through the lower-layer activeWorldUpdateCount() seam
/// (sibling of objectWorld()) rather than GameSessionContext.
inline uint32_t worldUpdateCount()
{
    return Ego::Entities::activeWorldUpdateCount();
}

inline IScriptable& scriptable(Object& object)
{
    return object;
}

inline const std::shared_ptr<Object>& objectByRef(ObjectRef objectRef)
{
    return objectWorld().getObjectHandler()[objectRef];
}

}}} // Ego::Physics::object_physics_detail

namespace Ego { namespace Physics {
    using namespace object_physics_detail;
}}
