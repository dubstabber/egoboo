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

#include <physfs.h>

#include "egolib/typedef.h"

/// The following flags set in vfs_file::flags provide information about the state of a file.
typedef enum vfs_file_flags
{

    /// End of the file encountered.
    VFS_FILE_FLAG_EOF = (1 << 0),

    /// Error was encountered.
    VFS_FILE_FLAG_ERROR = (1 << 1),

    /// The file is opened for writing.
    VFS_FILE_FLAG_WRITING = (1 << 2),

    /// The file is opened for reading.
    VFS_FILE_FLAG_READING = (1 << 3),

} vfs_file_flagss;

/// A container holding a PHYSFS file handle and translated error states
struct vsf_file
{
    BIT_FIELD flags;
    PHYSFS_File *p;
};

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

/// @brief Strip leading slash characters from a path so PhysFS sees a relative key.
/// @remark Defined in vfs.c; called from vfs_search.c (SearchContext::enumerateFiles) plus the
///         in-file vfs_open* / vfs_mkdir / vfs_exists / vfs_isDirectory paths in vfs.c.
std::string to_physfs_path(const std::string& pathname);

/// @brief Fixed-size path buffer used by VFS helpers that build paths via snprintf
///        (vfs.c's vfs_get_version + vfs_search.c's vfs_copyDirectory).
/// @remark Lives in the internal header so both TUs see the same VFS_MAX_PATH bound.
#define VFS_MAX_PATH 1024
typedef char VFS_PATH[VFS_MAX_PATH];
