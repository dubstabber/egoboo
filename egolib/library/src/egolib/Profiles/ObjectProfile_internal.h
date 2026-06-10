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

/// @file egolib/Profiles/ObjectProfile_internal.h
/// @brief Shared infrastructure for the split ObjectProfile implementation files.

#pragma once

#define EGOLIB_PROFILES_PRIVATE 1
#include "egolib/Profiles/ObjectProfile.hpp"

#include "egolib/Entities/_Include.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/FileFormats/template.h"
#include "egolib/Math/Random.hpp"

static const SkinInfo INVALID_SKIN = SkinInfo();
