//********************************************************************************************
//*
//*    This file is part of Cartman.
//*
//*    Cartman is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Cartman is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Cartman.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file cartman_typedef.h
/// @details base type definitions and config options

#pragma once

#include "cartman/cartman_config.h"
#include "egolib/integrations/math.hpp"   // Ego::Vector*/Point*/Rectangle*/Matrix* aliases

// The egolib math types were moved into the Ego:: namespace during the modernization.
// Cartman's legacy code refers to them unqualified, so surface them here (cartman_typedef.h
// is included almost everywhere in cartman). This mirrors the old global-namespace layout
// and keeps the port to a minimal diff (T3.5; see refactoring-documents/73-...).
using Ego::Vector2f;
using Ego::Vector3f;
using Ego::Point2f;
using Ego::Rectangle2f;
using Ego::Matrix4f4f;

// Forward declarations.
struct camera_t;
struct cartman_mpd_t;
struct cartman_mpd_tile_t;
struct select_lst_t;
struct ogl_surface_t;
struct s_tile_definition_t;

namespace Cartman {
namespace Gui {
struct Cursor;
struct Manager;
struct Window;
} // namespace GUI
} // namespace Cartman

