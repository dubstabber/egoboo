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

/// @brief Routines for reading and writing <tt>"controls.txt"</tt>.

#pragma once

#include "egolib/platform.h"
#include <string>

/// @brief Load input bindings for all devices from a controls.txt-format file.
/// @return @a true if the file was read to completion (all colons found), @a false otherwise
/// @remark <b>Miss contract.</b> A controls.txt that cannot be opened or read, or that is
///         short/truncated, simply yields @a false rather than throwing (pinned in
///         egolib/tests/egolib/tests/ControlSettingsFile.cpp). Device mappings already applied
///         before the failure point are NOT rolled back (also pinned); a missing/unopenable
///         file applies none, since the failure is caught before the parse loop runs.
/// @remark The contract covers idlib::runtime_error, raised by ReadContext's constructor
///         (egolib/fileutil.h) when the file cannot be read. The parse loop itself is provably
///         throw-proof for this scanner (ControlSettingsFile.cpp has the full rationale, shared
///         with LoadingState.cpp's loadGameTips), so idlib::hll::compilation_error cannot reach
///         this function today. Anything outside idlib::runtime_error - std::bad_alloc in
///         particular - still propagates, so do not wrap calls to this function in catch (...).
bool input_settings_load_vfs(const std::string& filename);
bool input_settings_save_vfs(const std::string& filename);
