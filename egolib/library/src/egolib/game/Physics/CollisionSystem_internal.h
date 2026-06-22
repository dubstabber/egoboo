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

/// @file egolib/game/Physics/CollisionSystem_internal.h
/// @brief Private declaration shared by the CollisionSystem TU family.
/// @details NOT part of the public API. Included only by CollisionSystem.cpp (the caller,
///          CollisionSystem::handleCollision) and CollisionSystem_chr_chr.cpp (the definition).
///          do_chr_chr_collision was a file-scope static in CollisionSystem.cpp before the
///          2026-06-13 split; it is promoted to external linkage here so the chr-chr response
///          body can live in its own TU while still being called from the dispatcher.
#pragma once

class Object;

namespace Ego
{
namespace Physics
{

/// Resolve a character-character collision: estimate the contact normal, compute recoil
/// (restitution) impulses and pressure-separation displacement, and accumulate them onto
/// each object's phys data. Returns true if the pair genuinely interacted (a "bump").
/// Defined in CollisionSystem_chr_chr.cpp.
bool do_chr_chr_collision(Object& objectA, Object& objectB, float tmin, float tmax);

} // namespace Physics
} // namespace Ego
