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

/// @file egolib/Platform/file_linux.c
/// @brief Implementation of the linux system-dependent filesystem functions
/// @details

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <limits.h>
#include <cstring>
#include <string>
#include "egolib/file_common.h"
#include "egolib/strutil.h"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

extern int sys_fs_init(const char *root_dir);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// Paths that the game will deal with
static char _dataPath[PATH_MAX]     = EMPTY_CSTR;
static char _userPath[PATH_MAX] = EMPTY_CSTR;
static char _configPath[PATH_MAX]   = EMPTY_CSTR;

namespace
{
std::string ensureTrailingSlash(std::string path)
{
    if (!path.empty() && path.back() != '/')
    {
        path.push_back('/');
    }
    return path;
}

std::string trimTrailingSlash(std::string path)
{
    while (path.size() > 1 && path.back() == '/')
    {
        path.pop_back();
    }
    return path;
}

std::string parentPath(const std::string& path)
{
    const std::string trimmed = trimTrailingSlash(path);
    const std::string::size_type pos = trimmed.find_last_of('/');
    if (pos == std::string::npos)
    {
        return ".";
    }
    if (pos == 0)
    {
        return "/";
    }
    return trimmed.substr(0, pos);
}

bool hasProjectMarkers(const std::string& root)
{
    const std::string normalizedRoot = trimTrailingSlash(root);
    return fs_fileIsDirectory(normalizedRoot + "/data")
        && fs_fileIsDirectory(normalizedRoot + "/egolib");
}

std::string findProjectRoot(const char *root_dir)
{
    const char *envDataDir = getenv("EGOBOO_DATA_DIR");
    if (envDataDir && *envDataDir)
    {
        const std::string dataPath = trimTrailingSlash(envDataDir);
        const std::string candidate = parentPath(dataPath);
        if (hasProjectMarkers(candidate))
        {
            return candidate;
        }
    }

    char *applicationPath = SDL_GetBasePath();
    std::string searchRoot = applicationPath ? applicationPath : "./";
    if (applicationPath)
    {
        SDL_free(applicationPath);
    }
    else if (root_dir && *root_dir)
    {
        searchRoot = root_dir;
    }

    std::string candidate = trimTrailingSlash(searchRoot);
    for (int i = 0; i < 8; ++i)
    {
        if (hasProjectMarkers(candidate))
        {
            return candidate;
        }

        const std::string parent = parentPath(candidate);
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }

    return trimTrailingSlash(searchRoot);
}

void ensureDirectoryChain(const std::string& path)
{
    if (path.empty() || fs_fileIsDirectory(path))
    {
        return;
    }

    const std::string parent = parentPath(path);
    if (!parent.empty() && parent != path)
    {
        ensureDirectoryChain(parent);
    }

    if (!fs_fileIsDirectory(path))
    {
        fs_createDirectory(path);
    }
}
}

int sys_fs_init(const char *root_dir)
{
    printf("initializing filesystem services\n");

#if defined(_NIX_PREFIX) && defined(PREFIX)
    // the access to these directories is completely unknown
    // The default setting from the Makefile is to set PREFIX = "/usr/local",
    // so that the program will compile and install just like any other
    // .rpm or .deb package.

    // grab the user's home directory
    char *userHome = getenv("HOME");
    snprintf(_userPath, SDL_arraysize(_userPath), "%s/.egoboo-2.x", userHome);

    snprintf(_configPath, SDL_arraysize(_configPath), "%s/etc/egoboo-2.x/", PREFIX);
    snprintf(_dataPath, SDL_arraysize(_dataPath), "%s/share/games/egoboo-2.x/", PREFIX);
#elif !defined(_NIX_PREFIX) && defined(_DEBUG)
    // assume we are debugging using the "install directory" rather than using a real installation
    strncpy(_configPath, ".", SDL_arraysize(_configPath));
    strncpy(_dataPath, ".", SDL_arraysize(_dataPath));
    strncpy(_userPath, ".", SDL_arraysize(_userPath));
#else
    std::string dataPath;
    const char *envDataDir = getenv("EGOBOO_DATA_DIR");
    if (envDataDir && *envDataDir)
    {
        dataPath = envDataDir;
    }
    else
    {
        char *applicationPath = SDL_GetBasePath();
        dataPath = applicationPath ? applicationPath : "./";
        if (applicationPath)
        {
            SDL_free(applicationPath);
        }
    }
    dataPath = ensureTrailingSlash(dataPath);

    const std::string projectRoot = findProjectRoot(root_dir);
    const std::string runtimeRoot = trimTrailingSlash(projectRoot) + "/.egoboo-runtime";

    const char *envUserDir = getenv("EGOBOO_USER_DIR");
    const std::string userPath = (envUserDir && *envUserDir) ? envUserDir : runtimeRoot + "/user";
    const std::string configPath = runtimeRoot + "/config";

    strncpy(_dataPath, dataPath.c_str(), SDL_arraysize(_dataPath));
    _dataPath[SDL_arraysize(_dataPath) - 1] = '\0';
    strncpy(_userPath, ensureTrailingSlash(userPath).c_str(), SDL_arraysize(_userPath));
    _userPath[SDL_arraysize(_userPath) - 1] = '\0';
    strncpy(_configPath, ensureTrailingSlash(configPath).c_str(), SDL_arraysize(_configPath));
    _configPath[SDL_arraysize(_configPath) - 1] = '\0';
#endif

    // the log file cannot be started until there is a user data path to dump the file into
    // so dump this debug info to stdout
    printf("Game directories are:\n"
           "\tData: %s\n"
           "\tUser: %s\n"
           "\tConfiguration: %s\n",
           _dataPath, _userPath, _configPath);

    ensureDirectoryChain(trimTrailingSlash(_configPath));
    ensureDirectoryChain(trimTrailingSlash(_userPath));
    return 0;
}

std::string fs_getDataDirectory()
{
    return _dataPath;
}

std::string fs_getUserDirectory()
{
    return _userPath;
}

std::string fs_getConfigDirectory()
{
    return _configPath;
}
