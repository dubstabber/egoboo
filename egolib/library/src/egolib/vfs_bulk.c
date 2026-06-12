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

/// @file egolib/vfs_bulk.c
/// @brief High-level bulk file I/O — pure public-API consumer.
/// @details Carved from vfs.c (fourth VFS slice, 2026-06-12).
///          Contains: vfs_set_base_search_paths, vfs_readEntireFile (both overloads),
///          vfs_writeEntireFile.
///          This TU only uses the public vfs.h surface; it does NOT include vfs_internal.h.

#include <physfs.h>

#include <cstdlib>
#include <functional>

#include "egolib/vfs.h"
#include "egolib/vfs_internal.h"
#include "egolib/file_common.h"

//--------------------------------------------------------------------------------------------
void vfs_set_base_search_paths( void )
{
    BAIL_IF_NOT_INIT();

    // Put write dir first in search path...
    PHYSFS_addToSearchPath( fs_getUserDirectory().c_str(), 0 );

    // Put base path on search path...
    PHYSFS_addToSearchPath( fs_getDataDirectory().c_str(), 1 );

    // Put config path on search path...
    PHYSFS_addToSearchPath(fs_getConfigDirectory().c_str(), 1);
}

//--------------------------------------------------------------------------------------------
void vfs_readEntireFile(const std::string& pathname, std::function<void(size_t, const char *)> receive) {
    auto deleter = [](vfs_FILE *file) { if (file) vfs_close(file); };
    std::unique_ptr<vfs_FILE, decltype(deleter)> file(vfs_openRead(pathname), deleter);
    if (!file) {
        throw idlib::runtime_error(__FILE__, __LINE__, "unable to open file `" + pathname + "` for reading");
    }
    // Read in 2048 Byte chunks.
    char buffer[2048];
    while (!vfs_eof(file.get())) {
        size_t read = vfs_read(buffer, 1, 2048, file.get());
        if (vfs_error(file.get())) {
            throw idlib::runtime_error(__FILE__, __LINE__, "error while reading file `" + pathname + "`");
        }
        // If not a short read, invoke receive.
        if (0 != read) {
            receive(read, buffer);
        }
    }
    file = nullptr;
}

bool vfs_readEntireFile(const std::string& pathname, char **data, size_t *length) {
    if (!data || !length) {
        return false;
    }
    vfs_FILE *file = vfs_openRead(pathname);
    if (!file) {
        return false;
    }
    long fileLen = vfs_fileLength(file);

    if (fileLen == -1)
    {
        // file length isn't known
        size_t pos = 0;
        size_t bufferSize = 1024;
        char *buffer = (char *) malloc(bufferSize);
        if (buffer == nullptr)
        {
            vfs_close(file);
            return false;
        }
        while (!vfs_eof(file))
        {
            size_t read = vfs_read(buffer + pos, 1, bufferSize - pos, file);
            pos += read;
            if (vfs_error(file))
            {
                free(buffer);
                vfs_close(file);
                return false;
            }
            if (vfs_eof(file)) break;
            if (0 == read) continue;
            char *newBuffer = (char *)realloc(buffer, pos + 1024);
            if (newBuffer == nullptr)
            {
                free(buffer);
                vfs_close(file);
                return false;
            }
            buffer = newBuffer;
        }
        *data = buffer;
        *length = pos;
    }
    else
    {
        size_t pos = 0;
        char *buffer = (char *) malloc(fileLen);
        if (buffer == nullptr)
        {
            vfs_close(file);
            return false;
        }
        while (pos < fileLen)
        {
            size_t read = vfs_read(buffer + pos, 1, fileLen - pos, file);
            pos += read;
            if (vfs_error(file))
            {
                free(buffer);
                vfs_close(file);
                return false;
            }
            if (vfs_eof(file)) break;
        }
        *data = buffer;
        *length = pos;
    }

    vfs_close(file);
    return true;
}

//--------------------------------------------------------------------------------------------
bool vfs_writeEntireFile(const std::string& pathname, const char *data, const size_t length)
{
    if (!data) {
        return false;
    }
    vfs_FILE *pfile = vfs_openWrite(pathname);
    if (nullptr == pfile) {
        return false;
    }
    size_t pos = 0;
    while (pos < length) {
        size_t written = vfs_write(data + pos, 1, length - pos, pfile);
        pos += written;
        if (vfs_error(pfile))
        {
            vfs_close(pfile);
            return false;
        }
    }
    vfs_close(pfile);
    return true;
}
