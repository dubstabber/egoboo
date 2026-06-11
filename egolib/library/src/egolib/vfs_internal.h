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

/// @file egolib/vfs_internal.h
/// @brief Private shared state and helpers split across the VFS implementation TUs (vfs.c / vfs_mount.c).
/// @details NOT a public VFS header — do not include from outside the vfs.c/vfs_mount.c pair. It carries the
///          narrow coupling between the two: the init flag (owned by vfs.c, read by the BAIL_IF_NOT_INIT
///          guard the mount functions use) and the one mount helper vfs.c's path normalization still calls.
///          (This is distinct from egolib/VFS/internal.hpp, which is idlib::file_system path-parsing internals.)

#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

/// @brief Whether the VFS has been initialized. Defined in vfs.c; read by the BAIL_IF_NOT_INIT guard in
///        both vfs.c and vfs_mount.c.
extern bool _vfs_initialized;

/// @brief Throw if a VFS function is called before vfs_init().
#define BAIL_IF_NOT_INIT() \
	if(!_vfs_initialized) { \
		std::ostringstream os; \
		os << __FUNCTION__ << ": EgoLib VFS function called while the VFS was not initialized" << std::endl; \
		throw std::runtime_error(os.str()); \
	}

/// @brief Get if the specified path is equivalent to a virtual mount point.
/// @remark Defined in vfs_mount.c; called from vfs.c path normalization.
int _vfs_mount_info_search(const std::string& pathname);
