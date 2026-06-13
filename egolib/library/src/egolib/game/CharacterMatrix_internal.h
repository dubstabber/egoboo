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

/// @file egolib/game/CharacterMatrix_internal.h
/// @brief Private grip-geometry helpers shared between CharacterMatrix.c (the matrix-cache
///        pipeline) and CharacterMatrix_grip.c (grip geometry). Not part of the public API.

#pragma once

#include "egolib/game/CharacterMatrix.h"   // matrix_cache_t, ObjectRef, GRIP_VERTS
#include "egolib/integrations/math.hpp"    // Ego::Vector4f

// Defined in CharacterMatrix_grip.c; called from the matrix-cache pipeline in CharacterMatrix.c.
int get_grip_verts( uint16_t grip_verts[], const ObjectRef imount, int vrt_offset );
int convert_grip_to_global_points( const ObjectRef iholder, uint16_t grip_verts[], Ego::Vector4f   dst_point[] );
