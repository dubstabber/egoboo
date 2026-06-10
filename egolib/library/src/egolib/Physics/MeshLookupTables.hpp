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
///        facings, flatness). These are pure terrain-geometry data: indexed by an 8-bit
///        twist value, independent of any particular mesh, derived only from base-layer
///        math (twist_to_normal / vec_to_facing). Originally in game/mesh.h (whose own
///        comment noted "this should be in map, not in mesh"); now a foundation-base
///        module so both the lower-layer mesh geometry (mesh_geometry.c) and the higher
///        physics/graphics consumers can read it without an upward dependency.

#pragma once

#include "egolib/_math.h"                // Facing
#include "egolib/integrations/math.hpp"  // Ego::Vector3f

/// Some look-up tables for meshes (and independent of the particular mesh).
/// Contains precomputed surface normals and twist facings.
struct MeshLookupTables {
	Ego::Vector3f twist_nrm[256];
	/// For surface normal of the mesh.
	Facing twist_facing_y[256];
	/// For surface normal of the mesh.
	Facing twist_facing_x[256];
	/// Is (something) flat?
	bool twist_flat[256];
	MeshLookupTables();
};

extern MeshLookupTables g_meshLookupTables;
