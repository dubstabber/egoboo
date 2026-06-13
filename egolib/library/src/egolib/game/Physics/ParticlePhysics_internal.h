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

/// @file egolib/game/Physics/ParticlePhysics_internal.h
/// @brief Private shared helpers for the ParticlePhysics TU family.
/// @details This header is NOT part of the public API. It is included only by
///          ParticlePhysics.cpp and ParticlePhysics_movement.cpp.
///          The three inline facade accessors are placed in namespace
///          particle_physics_detail (not anonymous namespace) + declared inline so that
///          every TU gets the same definitions and -Wunused-function is suppressed for
///          helpers a given TU does not call (e.g. objectWorld()/physical() are unused in
///          ParticlePhysics_movement.cpp; only collisionWorld() crosses the cut).
#pragma once

#include "egolib/game/Physics/ParticlePhysics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() object/team access (the entity-world seam)
#include "egolib/Physics/ICollisionWorld.hpp"    // activeCollisionWorld() terrain queries (the mesh-query seam)

namespace Ego { namespace Physics { namespace particle_physics_detail {

/// The entity world the physics step queries (object container + team list), reached through
/// the lower-layer Ego::Entities::IObjectWorld seam rather than game/ (GameModule /
/// GameSessionContext).
inline Ego::Entities::IObjectWorld& objectWorld()
{
    return Ego::Entities::activeObjectWorld();
}

/// The terrain world the physics step queries (slope/elevation/slippy/water), reached
/// through the lower-layer Ego::Physics::ICollisionWorld seam rather than game/mesh.h.
inline Ego::Physics::ICollisionWorld& collisionWorld()
{
    return Ego::Physics::activeCollisionWorld();
}

inline const IPhysical& physical(const Object& object)
{
    return object;
}

}}} // Ego::Physics::particle_physics_detail

namespace Ego { namespace Physics {
    using namespace particle_physics_detail;
}}
