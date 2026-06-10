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

/// @file egolib/Physics/MeshLookupTables.cpp
/// @brief Storage + precomputation of the global mesh-independent twist lookup tables.

#include "egolib/Physics/MeshLookupTables.hpp"

#include "egolib/map_functions.h"                // twist_to_normal
#include "egolib/Math/_Include.hpp"              // vec_to_facing, kX/kY/kZ

MeshLookupTables g_meshLookupTables;

MeshLookupTables::MeshLookupTables() {
	for (size_t cnt = 0; cnt < 256; cnt++)
	{
		Ego::Vector3f nrm;

		twist_to_normal(cnt, nrm, 1.0f);

		twist_nrm[cnt] = nrm;

		twist_facing_x[cnt] = Facing((FACING_T)(-vec_to_facing(nrm[kZ], nrm[kY])));
		twist_facing_y[cnt] = Facing((FACING_T)(+vec_to_facing(nrm[kZ], nrm[kX])));

		// this is about 5 degrees off of vertical
		twist_flat[cnt] = (nrm[kZ] > 0.9945f);
	}
}
