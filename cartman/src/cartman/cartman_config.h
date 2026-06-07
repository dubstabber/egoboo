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
//*
//********************************************************************************************

/// @file cartman/cartman_config.h
/// @brief Compile switches

#pragma once

#undef CARTMAN_DEBUG

// Cartman historically pulled the whole egolib uber-header (egolib/egolib.h) here.
// That header was deleted in the uber-header teardown (Pass 226). cartman_config.h is
// included across the editor (via cartman_typedef.h), so it carries cartman's core
// egolib dependency surface. This list will be trimmed as the T3.5 port matures --
// see refactoring-documents/73-cartman-build-integration-scouting.md.
#include "egolib/platform.h"
#include "egolib/typedef.h"
#include "idlib/idlib.hpp"
#include "idlib/math.hpp" // idlib::{orthographic_projection_matrix,look_at_matrix,scaling_matrix,identity}
#include "egolib/integrations/color.hpp"
#include "egolib/integrations/math.hpp"
#include "egolib/Renderer/Renderer.hpp"
#include "egolib/Graphics/GraphicsSystem.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp"
#include "egolib/Graphics/Font.hpp"
#include "egolib/Image/ImageManager.hpp"
#include "egolib/Time/Time.hpp"
#include "egolib/Mesh/Info.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/Core/System.hpp"    // Ego::Core::System
#include "egolib/Console/Console.hpp" // Ego::Core::Console
