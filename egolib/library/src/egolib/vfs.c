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

// vfs_file_flags enum and vsf_file struct promoted to vfs_internal.h (2026-06-12).

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static bool _vfs_atexit_registered = false;
bool _vfs_initialized = false;   // definition; extern-declared in vfs_internal.h

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------

static void _vfs_exit();
static int _vfs_ensure_write_directory(const std::string& filename, bool is_directory);


// _vfs_translate_error and fake_physfs_vprintf moved to vfs_io.c (2026-06-12).

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

// vfs_read / vfs_write (bulk) + vfs_read_Sint8/... + vfs_write<T> specializations
// + fake_physfs_vprintf + vfs_printf + vfs_ungetc/getc/putc/puts/rewind
// + vfs_empty_temp_directories + vfs_removeDirectoryAndContents + vfs_copyFile
// + _vfs_translate_error + vfs_getError
// moved to vfs_io.c (2026-06-12).

// vfs_set_base_search_paths + vfs_readEntireFile + vfs_writeEntireFile
// moved to vfs_bulk.c (2026-06-12).

//--------------------------------------------------------------------------------------------
// SearchContext (ctors, dtor, predicates, enumerateFiles, hasData/getData, nextData)
// lives in vfs_search.c — alongside vfs_copyDirectory, which is its only client.
// SearchContext::hasData / getData live in vfs_search.c with the rest of SearchContext.
