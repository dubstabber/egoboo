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

/// @file egolib/Physics/MeshLookupTables.hpp
/// @brief Precomputed, mesh-independent twist lookup tables (surface normals, twist
///        facings, steep-hill slide velocity, flatness). These are pure terrain-physics
///        data: indexed by an 8-bit twist value, independent of any particular mesh, and
///        consumed by the lower-layer physics code. Relocated here from game/mesh.h (whose
///        own comment noted "this should be in map, not in mesh") so the physics code can
///        read them without depending on the game/ layer.

#pragma once

#include "egolib/_math.h"                // Facing
#include "egolib/integrations/math.hpp"  // Ego::Vector3f

/// Some look-up tables for meshes (and independent of the particular mesh).
/// Contains precomputed surface normals and steep hill acceleration.
struct MeshLookupTables {
	Ego::Vector3f twist_nrm[256];
	/// For surface normal of the mesh.
	Facing twist_facing_y[256];
	/// For surface normal of the mesh.
	Facing twist_facing_x[256];
	/// Precomputed velocity (acceleration?) for sliding (down?) steep hills.
	Ego::Vector3f twist_vel[256];
	/// Is (something) flat?
	bool twist_flat[256];
	MeshLookupTables();
};

extern MeshLookupTables g_meshLookupTables;
