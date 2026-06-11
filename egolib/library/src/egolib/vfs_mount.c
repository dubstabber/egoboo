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

/// @file egolib/vfs_mount.c
/// @brief VFS mount-point management, split out of vfs.c.
/// @details The mount-info registry (_vfs_mount_infos) and the functions that add/remove/query mount
///          points. This is the only cluster that owns the mount-info list and the vfs_path_data_t record,
///          so both live here; the only cross-TU coupling is the BAIL_IF_NOT_INIT guard / _vfs_initialized
///          flag (owned by vfs.c) and _vfs_mount_info_search (called from vfs.c path normalization) — both
///          routed through egolib/vfs_internal.h.

#include <physfs.h>

#include "egolib/vfs.h"
#include "egolib/vfs_internal.h"

#include "egolib/file_common.h"
#include "egolib/Log/_Include.hpp"
#include "egolib/strutil.h"
#include "egolib/endian.h"
#include "egolib/fileutil.h"
#include "egolib/Core/StringUtilities.hpp"

//--------------------------------------------------------------------------------------------
// The mount-info record and registry (owned by this TU).
//--------------------------------------------------------------------------------------------

struct s_vfs_path_data
{
    std::string mount;
    std::string full_path;
    std::string root_path;
    std::string relative_path;

    s_vfs_path_data()
        : mount(),
          full_path(),
          root_path(),
          relative_path() {}

    s_vfs_path_data(const s_vfs_path_data& other)
        : mount(other.mount),
          full_path(other.full_path),
          root_path(other.root_path),
          relative_path(other.relative_path) {}

    s_vfs_path_data& operator=(const s_vfs_path_data& other) {
        mount = other.mount;
        full_path = other.full_path;
        root_path = other.root_path;
        relative_path = other.relative_path;
        return *this;
    }
};
typedef struct s_vfs_path_data vfs_path_data_t;

static std::vector<vfs_path_data_t> _vfs_mount_infos;

//--------------------------------------------------------------------------------------------
// Static mount-info helpers (used only within this TU).
//--------------------------------------------------------------------------------------------

static bool _vfs_mount_info_add(const Ego::VfsPath& mountPoint, const std::string& rootPath, const std::string& relativePath);
static int _vfs_mount_info_matches(const Ego::VfsPath& mountPoint);
static int _vfs_mount_info_matches(const Ego::VfsPath& mountPoint, const std::string& localPath);
static bool _vfs_mount_info_remove(int cnt);

//--------------------------------------------------------------------------------------------
int vfs_add_mount_point( const std::string& rootPath, const Ego::FsPath& relativePath, const Ego::VfsPath& mountPoint, int append )
{
    int retval = -1;

    BAIL_IF_NOT_INIT();

    // If mount point is empty or a slash indicates the PhysFS root directory, not the root of the currently mounted volume.
    if ( mountPoint.empty() || mountPoint == Ego::VfsPath("/") ) return 0;

    Ego::FsPath dirname;
    if ( !rootPath.empty() && !relativePath.empty() )
    {
        // both the root path and the relative path are non-empty:
        // the directory is meant to be the concatenation of both.
        dirname = Ego::FsPath(rootPath + SYSTEM_SLASH_STR + relativePath.string());
    }
    else if ( !rootPath.empty() )
    {
        // the root path is non-empty, the relative path is empty:
        // the direcotry i meant to be the root path.
        dirname = Ego::FsPath(rootPath);
    }
    else if ( !relativePath.empty() )
    {
        // the root path is empty, the relative path is non-empty:
        // the directory is meant to be the relative path.
        dirname = relativePath;
    }
    else
    {
        // both the root path and the relative path are empty:
        // reject.
        return 0;
    }

    /// @note ZF@> 2010-06-30 vfs_convert_fname_sys() broke the Linux version
    /// @note BB@> 2010-06-30 the error in vfs_convert_fname_sys() might be fixed now
    /// @note PF@> 2015-01-01 this should be unneeded. root_path and relative_path should both
    ///                       sys-dependent paths, unless Windows does something strange?
#if 0
    std::string loc_dirname = vfs_convert_fname_sys( dirname );
#else
    Ego::FsPath loc_dirname = dirname;
#endif

    if ( _vfs_mount_info_add( mountPoint, rootPath, relativePath.string() ) )
    {
        retval = PHYSFS_mount( loc_dirname.string().c_str(), mountPoint.string().c_str(), append );
        if ( 0 == retval )
        {
            // go back and remove the mount info, since PHYSFS rejected the
            // data we gave it
            int i = _vfs_mount_info_matches( mountPoint, loc_dirname.string() );
            _vfs_mount_info_remove( i );
        }
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_remove_mount_point( const Ego::VfsPath& mountPoint )
{
    BAIL_IF_NOT_INIT();

    // don't allow it to remove the default directory
    if ( mountPoint.empty() || mountPoint == Ego::VfsPath("/") ) return 0;

    // assume we are going to fail
    int retval = 0;

    // see if we have the mount point
    int cnt = _vfs_mount_info_matches( mountPoint );

    // does it exist in the list?
    if ( cnt < 0 ) return false;

    while ( cnt >= 0 )
    {
        // we have to use the path name to remove the search path, not the mount point name
        PHYSFS_removeFromSearchPath( _vfs_mount_infos[cnt].full_path.c_str() );

        // remove the mount info from this index
        // PF> we remove it even if PHYSFS_removeFromSearchPath() fails or else we might get an infinite loop
        _vfs_mount_info_remove( cnt );

        cnt = _vfs_mount_info_matches( mountPoint );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
/// @brief Get if the specified path is equivalent to a virtual mount point.
/// @param pathname the pathname
/// @return #VFS_TRUE if @a pathname is equivalent to a virtual mount point, #VFS_FALSE otherwise
int _vfs_mount_info_search(const std::string& pathname) {
    BAIL_IF_NOT_INIT();

    if (pathname.empty()) return VFS_FALSE;

    // Get the sanitized pathname.
    auto sanitizedPathname = str_clean_path(pathname);

    for (const auto& mount_info : _vfs_mount_infos) {
        if (sanitizedPathname == mount_info.mount) {
            return VFS_TRUE;
        }

        if (sanitizedPathname == (std::string(mount_info.mount) + NET_SLASH_STR)) {
            return VFS_TRUE;
        }
    }

    return VFS_FALSE;
}

//--------------------------------------------------------------------------------------------
std::pair<bool, std::string> vfs_mount_info_strip_path( const std::string& path )
{
    BAIL_IF_NOT_INIT();

    // Strip any starting slashes.
    std::string path_2 = Ego::left_trim<char>(path, [](const char& chr) { return chr == NET_SLASH_CHR || chr == WIN32_SLASH_CHR; });

    // Find the first mount point path that is a prefix of the specified path.
    // If such a path is discovered, return the specified path with the prefix removed.
    for (const auto& mount_info : _vfs_mount_infos) {
        if (idlib::is_prefix(path_2, mount_info.mount)) {
            return std::make_pair(true, path_2.substr(mount_info.mount.length()));
        }
    }
    return std::make_pair(false, path);
}

//--------------------------------------------------------------------------------------------
int _vfs_mount_info_matches(const Ego::VfsPath& mountPoint) {
    BAIL_IF_NOT_INIT();

    // are there any in the list?
    if (_vfs_mount_infos.empty()) return -1;

    // Strip any starting slashes.
    auto tmp = Ego::VfsPath(Ego::left_trim<char>(mountPoint.string(), [](const char& chr) { return chr == NET_SLASH_CHR || chr == WIN32_SLASH_CHR; }));

    if (!tmp.empty()) {
        // find the first path info with the given mount_point
        for (auto cnt = 0; cnt < _vfs_mount_infos.size(); cnt++) {
            if (_vfs_mount_infos[cnt].mount == mountPoint.string()) {
                return cnt;
            }
        }
    }

    return -1;
}
int _vfs_mount_info_matches(const Ego::VfsPath& mountPoint, const std::string& local_path) {
    BAIL_IF_NOT_INIT();

    // are there any in the list?
    if (_vfs_mount_infos.empty()) return -1;

    // Strip any starting slashes.
    auto tmp = Ego::VfsPath(Ego::left_trim<char>(mountPoint.string(), [](const char& chr) { return chr == NET_SLASH_CHR || chr == WIN32_SLASH_CHR; }));

    if (!tmp.empty() && !local_path.empty()) {
        // find the first path info with the given mount_point and local_path
        for (auto cnt = 0; cnt < _vfs_mount_infos.size(); cnt++) {
            if (_vfs_mount_infos[cnt].mount == mountPoint.string() &&
                _vfs_mount_infos[cnt].full_path == local_path) {
                return cnt;
            }
        }
    } else if (!tmp.empty()) {
        // find the first path info with the given mount_point
        for (auto cnt = 0; cnt < _vfs_mount_infos.size(); cnt++) {
            if (_vfs_mount_infos[cnt].mount == mountPoint.string()) {
                return cnt;
            }
        }
    } else if (!local_path.empty()) {
        // find the first path info with the given local_path
        for (auto cnt = 0; cnt < _vfs_mount_infos.size(); cnt++) {
            if (_vfs_mount_infos[cnt].full_path == local_path) {
                return cnt;
            }
        }
    }

    return -1;
}

//--------------------------------------------------------------------------------------------
bool _vfs_mount_info_add(const Ego::VfsPath& mountPoint, const std::string& rootPath, const std::string& relativePath) {
    BAIL_IF_NOT_INIT();

    // If the mount point is empty, do nothing.
    if (mountPoint.empty()) return false;

    // make a complete version of the pathname
    std::string local_path;
    if (!rootPath.empty() && !relativePath.empty()) {
        local_path = rootPath + SLASH_STR + relativePath;
    } else if (!rootPath.empty()) {
        local_path = rootPath;
    } else if (!relativePath.empty()) {
        local_path = relativePath;
    } else {
        return false;
    }

    // do we want to add it?
    if (local_path.empty()) return false;

    if (_vfs_mount_info_matches(mountPoint, local_path) >= 0) return false;

    // strip any starting slashes
    auto tmp = Ego::VfsPath(Ego::left_trim<char>(mountPoint.string(), [](const char& chr) { return chr == NET_SLASH_CHR || chr == WIN32_SLASH_CHR; }));
    if (tmp.empty()) return false;

    // save the mount point in a list for later detection
    vfs_path_data_t path_data;
    path_data.mount = tmp.string();
    path_data.full_path = local_path;
    if (!rootPath.empty()) {
        path_data.root_path = rootPath;
    }
    if (!relativePath.empty()) {
        path_data.relative_path = relativePath;
    }
    _vfs_mount_infos.push_back(path_data);

    return true;
}

//--------------------------------------------------------------------------------------------
bool _vfs_mount_info_remove(int cnt)
{
    BAIL_IF_NOT_INIT();

    // does it exist in the list?
    if ( cnt < 0 || cnt >= _vfs_mount_infos.size() ) return false;

    // fill in the hole in the list
    _vfs_mount_infos.erase(_vfs_mount_infos.begin() + cnt);

    return true;
}
