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

/// @file egolib/vfs_rwops.c
/// @brief SDL_RWops adapter over the virtual file system.
/// @details Split out of vfs.c: this self-contained cluster wraps a vfs_FILE as an SDL_RWops so SDL
///          loaders (images, audio, fonts) can read through the VFS. It touches no VFS mount state and
///          uses vfs_FILE only opaquely (through the public vfs.h API), so it needs no VFS-internal
///          header. SDL_RWops is forward-declared in vfs.h, so this TU includes <SDL.h> for the full
///          struct definition it dereferences.

#include "egolib/vfs.h"

#include <SDL.h>

#include <cstdint>
#include <cstdio>   // SEEK_CUR / SEEK_END / SEEK_SET
#include <cstdlib>  // malloc / free
#include <string>

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
static int64_t vfs_rwops_size(SDL_RWops *context)
{
    vfs_FILE * pfile = static_cast<vfs_FILE *>(context->hidden.unknown.data1);
    return vfs_fileLength(pfile);
}

static int64_t vfs_rwops_seek( SDL_RWops * context, int64_t offset, int whence )
{
    vfs_FILE * pfile = static_cast<vfs_FILE *>(context->hidden.unknown.data1);
    long pos = vfs_tell(pfile);
    if (SEEK_CUR == whence)
    {
        pos += offset;
    }
    else if (SEEK_END == whence)
    {
        pos = vfs_fileLength(pfile) + offset;
    }
    else if (SEEK_SET == whence)
    {
        pos = offset;
    }
    vfs_seek(pfile, pos);
    return vfs_tell( pfile );
}

static size_t vfs_rwops_read(SDL_RWops *context, void *ptr, size_t size, size_t maxnum)
{
    vfs_FILE *file = (vfs_FILE *)(context->hidden.unknown.data1);
    if (vfs_isReading(file) != 1)
    {
        return 0;
    }
    return vfs_read(ptr, size, maxnum, file);
}

static size_t vfs_rwops_write(SDL_RWops *context, const void *ptr, size_t size, size_t num)
{
    vfs_FILE *file = (vfs_FILE *)(context->hidden.unknown.data1);
    if (vfs_isWriting(file) != 1)
    {
        return 0;
    }
    return vfs_write(ptr, size, num, file);
}

static int vfs_rwops_close(SDL_RWops *context)
{
    vfs_FILE *file = (vfs_FILE *)(context->hidden.unknown.data1);
    if (context->type)
    {
        vfs_close(file);
    }
    free(context);
    return 0;
}

static SDL_RWops *vfs_rwops_create(vfs_FILE *file, bool ownership)
{
    int isWriting = vfs_isWriting(file);
    if (-1 == isWriting)
    {
        return NULL;
    }
    // MH: I allocate the boolean variable tracking ownership after the SDL_RWops struct.
    // PF5: It's been moved to the type variable.
    SDL_RWops *rwops = (SDL_RWops *)malloc(sizeof(SDL_RWops));
    if (!rwops)
    {
        return NULL;
    }
    rwops->type = ownership;
    rwops->size = vfs_rwops_size;
    rwops->seek = vfs_rwops_seek;
    rwops->read = vfs_rwops_read;
    rwops->write = vfs_rwops_write;
    rwops->close = vfs_rwops_close;
    rwops->hidden.unknown.data1 = file;
    return rwops;
}

SDL_RWops *vfs_openRWops(vfs_FILE *file, bool ownership)
{
    SDL_RWops *rwops = vfs_rwops_create(file, ownership);
    if (!rwops) {
        return nullptr;
    }
    return rwops;
}

SDL_RWops *vfs_openRWopsRead(const std::string& pathname)
{
    vfs_FILE *file = vfs_openRead(pathname);
    if (!file) {
        return nullptr;
    }
    SDL_RWops *rwops = vfs_rwops_create(file, true);
    if (!rwops) {
        vfs_close(file);
        return nullptr;
    }
    return rwops;
}

SDL_RWops *vfs_openRWopsWrite(const std::string& pathname)
{
    vfs_FILE *file = vfs_openWrite(pathname);
    if (!file) {
        return nullptr;
    }
    SDL_RWops *rwops = vfs_rwops_create(file, true);
    if (!rwops) {
        vfs_close(file);
        return nullptr;
    }
    return rwops;
}

SDL_RWops *vfs_openRWopsAppend(const std::string& pathname)
{
    vfs_FILE *file = vfs_openAppend(pathname);
    if (!file) {
        return nullptr;
    }
    SDL_RWops *rwops = vfs_rwops_create(file, true);
    if (!rwops) {
        vfs_close(file);
        return nullptr;
    }
    return rwops;
}
