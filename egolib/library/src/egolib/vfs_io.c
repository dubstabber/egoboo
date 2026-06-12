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

/// @file egolib/vfs_io.c
/// @brief Binary typed I/O + text/char I/O + error translation for the Egoboo VFS.
/// @details Carved from vfs.c (fourth VFS slice, 2026-06-12).
///          Contains: _vfs_translate_error, vfs_finish_io, vfs_read/write (bulk),
///          vfs_read_Sint8/Uint8/.../float (9 readers), vfs_write<T> (9 specializations),
///          fake_physfs_vprintf, vfs_printf, vfs_getc/putc/ungetc/puts/rewind,
///          vfs_empty_temp_directories, vfs_removeDirectoryAndContents, vfs_copyFile,
///          vfs_getError.

#include <physfs.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdarg.h>

#include "egolib/vfs.h"
#include "egolib/vfs_internal.h"

#include "egolib/strutil.h"
#include "egolib/file_common.h"

//--------------------------------------------------------------------------------------------
void _vfs_translate_error(vfs_FILE *file)
{
    BAIL_IF_NOT_INIT();

    if (!file)
    {
        return;
    }

    {
        if (PHYSFS_eof(file->p))
        {
            SET_BIT(file->flags, VFS_FILE_FLAG_EOF);
        }
    }
}

//--------------------------------------------------------------------------------------------
/// @brief Apply the error-handling tail shared by every fixed-width vfs read/write helper:
///        update the file's error flag, translate the error on failure, and pass the raw
///        PhysFS result back to the caller unchanged.
/// @param file the file
/// @param ok whether the underlying PhysFS call succeeded
/// @param retval the raw PhysFS result the public helper returns
/// @return @a retval
static int vfs_finish_io( vfs_FILE& file, bool ok, int retval )
{
    if ( ok ) file.flags &= ~VFS_FILE_FLAG_ERROR;
    else      file.flags |= VFS_FILE_FLAG_ERROR;
    if ( !ok ) _vfs_translate_error( &file );
    return retval;
}

//--------------------------------------------------------------------------------------------
size_t vfs_read( void * buffer, size_t size, size_t count, vfs_FILE * pfile )
{
	bool error = false;
    size_t read_length;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    read_length = 0;
    {
        pfile->flags &= ~VFS_FILE_FLAG_ERROR;
        PHYSFS_sint64 retval = PHYSFS_read( pfile->p, buffer, size, count );

        if ( retval < 0 ) { error = true; pfile->flags |= VFS_FILE_FLAG_ERROR; }

        if ( !error ) read_length = retval;
    }

    if ( error ) _vfs_translate_error( pfile );

    return read_length;
}

//--------------------------------------------------------------------------------------------
size_t vfs_write( const void * buffer, size_t size, size_t count, vfs_FILE * pfile )
{
    bool error = false;
    size_t retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 0;
    {
        pfile->flags &= ~VFS_FILE_FLAG_ERROR;
        PHYSFS_sint64 write_length = PHYSFS_write( pfile->p, buffer, size, count );

        if ( write_length < 0 ) { error = true; pfile->flags |= VFS_FILE_FLAG_ERROR; }

        if ( !error ) retval = write_length;
    }

    if ( error ) _vfs_translate_error( pfile );

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_read_Sint8( vfs_FILE& file, int8_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_read( file.p, val, 1, sizeof(int8_t) );
    return vfs_finish_io( file, 1 == retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Uint8( vfs_FILE& file, uint8_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_read( file.p, val, 1, sizeof(uint8_t) );
    return vfs_finish_io( file, 1 == retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Sint16( vfs_FILE& file, int16_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readSLE16( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Uint16( vfs_FILE& file, uint16_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readULE16( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Sint32( vfs_FILE& file, int32_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readSLE32( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Uint32( vfs_FILE& file, uint32_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readULE32( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Sint64( vfs_FILE& file, int64_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readSLE64( file.p, (PHYSFS_sint64 *)val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_Uint64( vfs_FILE& file, uint64_t *val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_readULE64( file.p, (PHYSFS_uint64 *)val );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
int vfs_read_float( vfs_FILE& file, float * val )
{
    BAIL_IF_NOT_INIT();
    union { float f; uint32_t i; } convert;
    int retval = PHYSFS_readULE32( file.p, &convert.i );
    *val = convert.f;
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------

template <>
int vfs_write<int8_t>( vfs_FILE& file, const int8_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_write( file.p, &val, 1, sizeof(int8_t) );
    return vfs_finish_io( file, 1 == retval, retval );
}

template <>
int vfs_write<uint8_t>( vfs_FILE& file, const uint8_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_write( file.p, &val, 1, sizeof(uint8_t) );
    return vfs_finish_io( file, 1 == retval, retval );
}

template <>
int vfs_write<int16_t>( vfs_FILE& file, const int16_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeSLE16( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<uint16_t>( vfs_FILE& file, const uint16_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeULE16( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<int32_t>( vfs_FILE& file, const int32_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeSLE32( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<uint32_t>( vfs_FILE& file, const uint32_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeULE32( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<int64_t>( vfs_FILE& file, const int64_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeSLE64( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<uint64_t>( vfs_FILE& file, const uint64_t& val )
{
    BAIL_IF_NOT_INIT();
    int retval = PHYSFS_writeULE64( file.p, val );
    return vfs_finish_io( file, 0 != retval, retval );
}

template <>
int vfs_write<float>( vfs_FILE& file, const float& val )
{
    BAIL_IF_NOT_INIT();
    union { float f; uint32_t i; } convert;
    convert.f = val;
    int retval = PHYSFS_writeULE32( file.p, convert.i );
    return vfs_finish_io( file, 0 != retval, retval );
}

//--------------------------------------------------------------------------------------------
static int fake_physfs_vprintf( PHYSFS_File * pfile, const char *format, va_list args )
{
    // fake an actual streaming write to the file by writing the string to a
    // "large" buffer

    int written;
    char buffer[4098] = EMPTY_CSTR;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile || INVALID_CSTR( format ) ) return 0;

    written = vsnprintf( buffer, SDL_arraysize( buffer ), format, args );

    if ( written > 0 )
    {
        written = PHYSFS_write( pfile, buffer, sizeof( char ), written );
    }

    return written;
}

//--------------------------------------------------------------------------------------------
int vfs_printf( vfs_FILE * pfile, const char *format, ... )
{
    va_list args;
    int retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    va_start( args, format );
    {
        retval = fake_physfs_vprintf( pfile->p, format, args );
    }
    va_end( args );

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_ungetc( int c, vfs_FILE * pfile )
{
    int retval = 0;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    {
        // fake it
        int seeked = PHYSFS_seek(pfile->p, PHYSFS_tell(pfile->p) - 1);
        retval = c;

        if (!seeked) pfile->flags |= VFS_FILE_FLAG_ERROR;
        else         pfile->flags &= ~VFS_FILE_FLAG_ERROR;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_getc(vfs_FILE *file)
{
    int retval = 0;

    BAIL_IF_NOT_INIT();

    if (!file)
    {
        return 0;
    }

    retval = 0;
    {
        unsigned char cTmp;
        retval = PHYSFS_read(file->p, &cTmp, sizeof(cTmp), 1);

        if (-1 == retval)
        {
            file->flags |= VFS_FILE_FLAG_ERROR;
            retval = EOF; // MH: Set this explicitly - EOF can be defined as -1, but it does not have.
        }
        else if (0 == retval)
        {
            file->flags |= VFS_FILE_FLAG_EOF;
            retval = EOF;
        }
        else
        {
            retval = cTmp;
        }
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_putc(int c, vfs_FILE *file)
{
    int retval = 0;

    BAIL_IF_NOT_INIT();

    if (NULL == file)
    {
        return 0;
    }

    {
        IDLIB_DEBUG_ASSERT(0 <= c && c <= 0xff);
        unsigned char ch = static_cast<unsigned char>(c);
        retval = PHYSFS_write(file->p, &ch, 1, sizeof(unsigned char));
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_puts( const char * str , vfs_FILE * pfile )
{
    int retval = 0;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile || INVALID_CSTR( str ) ) return 0;

    {
        size_t len = strlen( str );

        retval = vfs_write( str, len, sizeof( char ), pfile );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
void vfs_empty_temp_directories()
{
    BAIL_IF_NOT_INIT();

    vfs_removeDirectoryAndContents("import");
    vfs_removeDirectoryAndContents("remote");
}

//--------------------------------------------------------------------------------------------
int vfs_rewind(vfs_FILE *file)
{
    BAIL_IF_NOT_INIT();

    if (!file)
    {
        return 0;
    }

    return vfs_seek(file, 0);
}

//--------------------------------------------------------------------------------------------
int vfs_removeDirectoryAndContents(const char * dirname) {
    // buffer the directory delete through PHYSFS, so that we so not access functions that
    // we have no right to! :)

    BAIL_IF_NOT_INIT();

    // make sure that this is a valid directory
    auto resolvedWriteFilename = vfs_resolveWriteFilename(dirname);
    if (!resolvedWriteFilename.first) return VFS_FALSE;
    if (!fs_fileIsDirectory(resolvedWriteFilename.second.c_str())) return VFS_FALSE;

    fs_removeDirectoryAndContents(resolvedWriteFilename.second.c_str());

    return VFS_TRUE;
}

//--------------------------------------------------------------------------------------------
int vfs_copyFile( const std::string& source, const std::string& target)
{
    char *dataPtr;
    size_t length;
    if (vfs_readEntireFile(source, &dataPtr, &length)) {
        bool retval = vfs_writeEntireFile(target, dataPtr, length);
        std::free(dataPtr);
        return retval;
    }
    return false;
}

//--------------------------------------------------------------------------------------------
const char * vfs_getError( void )
{
    /// @author ZF
    /// @details Returns the last error the PHYSFS system reported.

    static char errors[1024];
    const char * physfs_error, * file_error;
	//bool is_error;

    BAIL_IF_NOT_INIT();

    // load up a default
    strncpy( errors, "unknown error", SDL_arraysize( errors ) );

    // assume no error
    //is_error = false;

    // try to get the physfs error state;
    physfs_error = PHYSFS_getLastError();
    if ( NULL == physfs_error ) physfs_error = "no error";

    // try to get the stdio error state
    file_error = strerror( errno );

    snprintf( errors, SDL_arraysize( errors ), "c stdio says:\"%s\" -- physfs says:\"%s\"", file_error, physfs_error );

    return errors;
}
