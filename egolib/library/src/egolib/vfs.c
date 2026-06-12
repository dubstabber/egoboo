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

/// @file egolib/vfs.c
/// @brief Implementation of the Egoboo virtual file system
/// @details

#include <physfs.h>

#include <cstdio>

#include "egolib/vfs.h"
#include "egolib/vfs_internal.h"

#include "egolib/file_common.h"
#include "egolib/Log/_Include.hpp"

#include "egolib/strutil.h"
#include "egolib/endian.h"
#include "egolib/fileutil.h"
#include "egolib/Core/StringUtilities.hpp"

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------

/**
 * @brief
 *  If this is and @a _DEBUG are both defined,
 *  the VFS system runs in debug mode.
 */
#undef _VFS_DEBUG

 //--------------------------------------------------------------------------------------------
// VFS_PATH / VFS_MAX_PATH live in vfs_internal.h so both vfs.c and vfs_search.c see them.

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

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


//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static bool _vfs_atexit_registered = false;
bool _vfs_initialized = false;   // definition; extern-declared in vfs_internal.h

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static void _vfs_exit();
static int _vfs_ensure_write_directory(const std::string& filename, bool is_directory);


static void _vfs_translate_error(vfs_FILE *file);



static int fake_physfs_vprintf(PHYSFS_File *file, const char *format, va_list args);

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int vfs_init(const char *argv0, const char *root_dir)
{
    std::string temp_path;

    if (fs_init(root_dir))
    {
        return 1;
    }
    
    if (!fs_fileIsDirectory(fs_getDataDirectory()))
    {
        auto entry = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "the data path ", "`", fs_getDataDirectory(), "`", " is not a directory", Log::EndOfEntry);
        if (auto* logTarget = Log::tryInstalledTarget())
        {
            *logTarget << entry;
        }
        else
        {
            std::fprintf(stderr, "%s\n", entry.getText().c_str());
        }
        return 1;
    }

    if (_vfs_initialized)
    {
        return 0;
    }

    if (!PHYSFS_init(argv0))
    {
        return 1;
    }
    PHYSFS_permitSymbolicLinks(1);
    // Append the data directory to the search directories.
    temp_path = std::string(fs_getDataDirectory()) + SLASH_STR;
    if (!PHYSFS_mount(temp_path.c_str(), "/", 1))
    {
        PHYSFS_deinit();
        return 1;
    }

    //---- !!!! make sure the basic directories exist !!!!

    // Ensure that the /user directory exists.
    if (!fs_fileIsDirectory(fs_getUserDirectory()))
    {
        fs_createDirectory(fs_getUserDirectory()); ///< @todo Error handling!
    }

    // Ensure that the /user/debug directory exists.
    if (!fs_fileIsDirectory(fs_getUserDirectory()))
    {
        printf("WARNING: cannot create write directory %s\n", fs_getUserDirectory().c_str());
    }
    else
    {
        temp_path = std::string(fs_getUserDirectory()) + "/debug";
        temp_path = str_convert_slash_sys(temp_path);
        fs_createDirectory(temp_path.c_str());
    }

    // Set the write directory to the root user directory.
    if (!PHYSFS_setWriteDir(fs_getUserDirectory().c_str()))
    {
        PHYSFS_deinit();
        return 1;
    }

    if (!_vfs_atexit_registered)
    {
        atexit(_vfs_exit); /// @todo Error handling?
        _vfs_atexit_registered = true;
    }

    _vfs_initialized = true;
    return 0;
}

//--------------------------------------------------------------------------------------------
void _vfs_exit()
{
    PHYSFS_deinit();
}

//--------------------------------------------------------------------------------------------
std::string vfs_getLinkedVersion()
{
    PHYSFS_Version version;
    PHYSFS_getLinkedVersion(&version);        // Linked version number
    std::stringstream stream;
    stream << version.major << "." << version.minor << "." << version.patch;
    return stream.str();
}

std::string vfs_getVersion()
{
    PHYSFS_Version version;
    PHYSFS_VERSION(&version);                 // Compiled version number
    std::stringstream stream;
    stream << version.major << "." << version.minor << "." << version.patch;
    return stream.str();
}

//--------------------------------------------------------------------------------------------

bool validate(const std::string& source, std::string& target) {
    try {
        target = Ego::VfsPath(source).string();
        return true;
    } catch (...) {
        return false;
    }
}

std::string to_physfs_path(const std::string& pathname)
{
    return Ego::left_trim<char>(pathname, [](const char& chr) {
        return chr == NET_SLASH_CHR || chr == WIN32_SLASH_CHR;
    });
}

vfs_FILE *vfs_openRead(const std::string& pathname)
{
    BAIL_IF_NOT_INIT();

    std::string temporary;
    if (!validate(pathname,temporary)) {
        return nullptr;
    }
    temporary = to_physfs_path(temporary);

    PHYSFS_File *ftmp = PHYSFS_openRead(temporary.c_str());
    if (!ftmp)
    {
    #if defined(_DEBUG) && defined(_VFS_DEBUG)
        log_warning("unable to open file `%s` for reading - reason: %s\n", pathname.c_str(), PHYSFS_getLastError());
    #endif
        return nullptr;
    }

	vfs_FILE *vfs_file;
	try {
		vfs_file = new vfs_FILE();
	} catch (...) {
        PHYSFS_close(ftmp);
        return nullptr;
    }

    vfs_file->flags = VFS_FILE_FLAG_READING;
    vfs_file->p = ftmp;

    return vfs_file;
}

vfs_FILE *vfs_openWrite(const std::string& pathname)
{
    BAIL_IF_NOT_INIT();

    std::string temporary;
    if (!validate(pathname, temporary)) {
        return nullptr;
    }
    temporary = to_physfs_path(temporary);

    // Make sure that the output directory exists.
    if (!_vfs_ensure_write_directory(temporary, false))
    {
        return NULL;
    }

    // Open the PhysFS file.
    PHYSFS_File *ftmp = PHYSFS_openWrite(temporary.c_str());
    if (!ftmp)
    {
    #if defined(_DEBUG) && defined(_VFS_DEBUG)
        log_warning("unable to open file `%s` for writing - reason: %s\n", pathname.c_str(), PHYSFS_getLastError());
    #endif
        return NULL;
    }

    // Open the VFS file.
	vfs_FILE *vfs_file;
	try {
		vfs_file = new vfs_FILE();
	} catch (...) {
		PHYSFS_close(ftmp);
		return nullptr;
	}
    vfs_file->flags = VFS_FILE_FLAG_WRITING;
    vfs_file->p = ftmp;

    return vfs_file;
}

vfs_FILE *vfs_openAppend(const std::string& pathname)
{
    BAIL_IF_NOT_INIT();

    std::string temporary;
    if (!validate(pathname, temporary)) {
        return nullptr;
    }

    PHYSFS_File *ftmp = PHYSFS_openAppend(temporary.c_str());
    if (!ftmp)
    {
    #if defined(_DEBUG) && defined(_VFS_DEBUG)
        log_warning("unable to open file `%s` for appending - reason: %s\n", pathname.c_str(), PHYSFS_getLastError());
    #endif
        return NULL;
    }

	vfs_FILE *vfs_file;
	try {
		vfs_file = new vfs_FILE();
	} catch (...) {
        PHYSFS_close(ftmp);
        return nullptr;
    }
    vfs_file->flags = VFS_FILE_FLAG_WRITING;
    vfs_file->p = ftmp;

    return vfs_file;
}

//--------------------------------------------------------------------------------------------
Ego::VfsPath vfs_convert_fname(const Ego::VfsPath& path) {
    BAIL_IF_NOT_INIT();
    static const auto slash = Ego::VfsPath(Ego::VfsPath::getDefaultPathSeparator());
    // If the path is empty ...
    if (path.empty()) {
        // ... assume root.
        return slash;
    }
    if (_vfs_mount_info_search(path.string().c_str()) || NETWORK_SLASH_CHR == path.string()[0]) {
        return path;
    } else {
        return (slash + path);
    }
}

Ego::VfsPath vfs_convert_fname(const std::string& pathString) {
    return vfs_convert_fname(Ego::VfsPath(pathString));
}

//--------------------------------------------------------------------------------------------
void vfs_listSearchPaths( void )
{
    //JJ> Lists all search paths that PhysFS uses (for debug use)

    char **i;

    BAIL_IF_NOT_INIT();

    printf( "LISTING ALL PHYSFS SEARCH PATHS:\n" );
    printf( "----------------------------------\n" );
    for ( i = PHYSFS_getSearchPath(); *i != NULL; i++ )   printf( "[%s] is in the search path.\n", *i );
    printf( "----------------------------------\n" );
}

//--------------------------------------------------------------------------------------------
std::pair<bool, std::string> vfs_resolveReadFilename( const std::string& filename )
{
    BAIL_IF_NOT_INIT();

    if (filename.empty()) {
        std::make_pair(false, filename);
    }

    // make a temporary copy of the given filename with system-dependent slashes
    // to see if the filename is already resolved
    std::string filename_specific = str_convert_slash_sys(filename);

    if (fs_fileExists(filename_specific)) {
        return std::make_pair(true, filename_specific);
    }

    // The specified filename (in system-specific notation) does not exist.
    // Convert the filename in PhysFS-specific notation.
    filename_specific = vfs_convert_fname(Ego::VfsPath(filename_specific)).string();

    // If the specified filename denotes an existing file or directory, then this file or directory must have a containing directory.
    const char *prefix = PHYSFS_getRealDir(filename_specific.c_str());
    if (nullptr == prefix) {
        return std::make_pair(false, filename);
    }
    // The specified filename denotes an existing file or directory.
    if (PHYSFS_isDirectory(filename_specific.c_str())) {
        // If it denotes a directory then it must be splittable into a prefix and a suffix.
        auto suffix = vfs_mount_info_strip_path(filename_specific.c_str());
        if (suffix.first) {
            return std::make_pair(true, (Ego::VfsPath(prefix) + Ego::VfsPath(suffix.second)).string(Ego::VfsPath::Kind::System));
        } else {
            return std::make_pair(true, (Ego::VfsPath(prefix) + Ego::VfsPath("/")).string(Ego::VfsPath::Kind::System));
        }
    } else {
        // The specified filename denotes a file.
        auto suffix = vfs_mount_info_strip_path(filename_specific.c_str());
        if (suffix.first) {
            return std::make_pair(true, (Ego::VfsPath(prefix) + Ego::VfsPath(suffix.second)).string(Ego::VfsPath::Kind::System));
        } else {
            return std::make_pair(false, filename);
        }
    }
}

//--------------------------------------------------------------------------------------------
std::pair<bool, std::string> vfs_resolveWriteFilename(const std::string& filename) {
    // Validate state.
    BAIL_IF_NOT_INIT();
    // Validate arguments.
    if (filename.empty()) {
        return std::make_pair(false, filename);
    }
    // Get the write directory.
    const char *writeDirectory = PHYSFS_getWriteDir();
    if (!writeDirectory) {
        throw std::runtime_error("unable to get write directory");
    }
    // Append the filename to the write directory.
    auto resolvedFilename = Ego::VfsPath(writeDirectory) + Ego::VfsPath(filename);
    // Ensure system-specific encoding of the resolved filename.
    return std::make_pair(true, resolvedFilename.string(Ego::VfsPath::Kind::System));
}

//--------------------------------------------------------------------------------------------
int _vfs_ensure_write_directory( const std::string& filename, bool is_directory )
{
    /// @author BB
    /// @details
    BAIL_IF_NOT_INIT();

    if ( filename.empty() ) return 0;

    // make a working copy of the filename
    // and make sure that PHYSFS gets the filename with the slashes it wants
    std::string temp_dirname = vfs_convert_fname(Ego::VfsPath(filename)).string();

    // grab the system-independent path relative to the write directory
    if ( !is_directory && !vfs_isDirectory( temp_dirname ) )
    {
        size_t index = temp_dirname.rfind(NET_SLASH_CHR);
        if (std::string::npos == index)
        {
            temp_dirname = NET_SLASH_STR;
        }
        else
        {
            temp_dirname = temp_dirname.substr(0, index);
        }

        if ( temp_dirname.length() == 0 )
        {
            temp_dirname = C_SLASH_STR;
        }
    }

    // call mkdir() on this directory. PHYSFS will automatically generate the
    // directories needed between the write directory and the specified directory
    int retval = 1;
    if ( temp_dirname != "" )
    {
        retval = vfs_mkdir( temp_dirname.c_str() );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------

int vfs_isReading(vfs_FILE *file)
{
    if (!file)
    {
        return -1;
    }
    return VFS_FILE_FLAG_READING == (file->flags & VFS_FILE_FLAG_READING);
}

int vfs_isWriting(vfs_FILE *file)
{
    if (!file)
    {
        return -1;
    }
    return VFS_FILE_FLAG_WRITING == (file->flags & VFS_FILE_FLAG_WRITING);
}

int vfs_close(vfs_FILE *file)
{
    BAIL_IF_NOT_INIT();

    if (!file)
    {
        return 0;
    }

    int retval = 0;
    {
        retval = PHYSFS_close(file->p);
		delete file;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_flush( vfs_FILE * pfile )
{
    int retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 0;
    {
        retval = PHYSFS_flush( pfile->p );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
int vfs_eof( vfs_FILE * pfile )
{
    int retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    // check our own end-of-file condition
    if ( 0 != ( pfile->flags & VFS_FILE_FLAG_EOF ) )
    {
        return pfile->flags & VFS_FILE_FLAG_EOF;
    }

    retval = 1;
    {
        retval = PHYSFS_eof( pfile->p );
    }

    if ( 0 != retval )
    {
        pfile->flags |= VFS_FILE_FLAG_EOF;
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_error( vfs_FILE * pfile )
{
    int retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 1;
    {
        retval = VFS_FILE_FLAG_ERROR == (pfile->flags & VFS_FILE_FLAG_ERROR);
        //retval = ( NULL != PHYSFS_getLastError() );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
long vfs_tell( vfs_FILE * pfile )
{
    long retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 0;
    {
        retval = PHYSFS_tell( pfile->p );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
int vfs_seek( vfs_FILE * pfile, long offset )
{
    int retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 0;
    {
        // reset the flags
        pfile->flags &= ~(VFS_FILE_FLAG_EOF | VFS_FILE_FLAG_ERROR);

        retval = PHYSFS_seek( pfile->p, offset );
        if (retval == 0) pfile->flags &= ~VFS_FILE_FLAG_ERROR;
        else             pfile->flags |= VFS_FILE_FLAG_ERROR;
    }

    if ( 0 != offset )
    {
        // set an eof flag if we set it to seek past the end of the file
        vfs_eof( pfile );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
long vfs_fileLength( vfs_FILE * pfile )
{
    long retval;

    BAIL_IF_NOT_INIT();

    if ( NULL == pfile ) return 0;

    retval = 0;
    {
        retval = PHYSFS_fileLength( pfile->p );
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
bool vfs_mkdir(const std::string& pathname) {
    BAIL_IF_NOT_INIT();
    std::string temporary = to_physfs_path(Ego::VfsPath(pathname).string());
    if (!PHYSFS_mkdir(temporary.c_str())) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "PHYSF_mkdir(", pathname, ") failed: ", vfs_getError());
        return false;
    }
    return true;
}

bool vfs_delete_file(const std::string& pathname)
{
    BAIL_IF_NOT_INIT();

    std::string temporary = to_physfs_path(Ego::VfsPath(pathname).string());

    if (!PHYSFS_delete(temporary.c_str())) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "PHYSF_delete(", pathname, ") failed: ", vfs_getError(), Log::EndOfEntry);
        return false;
    }
    return true;
}

bool vfs_exists(const std::string& pathname) {
    BAIL_IF_NOT_INIT();
    std::string temporary = to_physfs_path(Ego::VfsPath(pathname).string());
    return (0 != PHYSFS_exists(temporary.c_str()));
}

bool vfs_isDirectory(const std::string& pathname) {
    BAIL_IF_NOT_INIT();
    std::string temporary = to_physfs_path(Ego::VfsPath(pathname).string());
    return 0 != PHYSFS_isDirectory(temporary.c_str());
}

//--------------------------------------------------------------------------------------------
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
int fake_physfs_vprintf( PHYSFS_File * pfile, const char *format, va_list args )
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
// SearchContext (ctors, dtor, predicates, enumerateFiles, hasData/getData, nextData)
// lives in vfs_search.c — alongside vfs_copyDirectory, which is its only client.

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
// vfs_copyDirectory lives in vfs_search.c (it directly instantiates SearchContext).

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

//--------------------------------------------------------------------------------------------
// SearchContext::hasData / getData live in vfs_search.c with the rest of SearchContext.
//--------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------
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
