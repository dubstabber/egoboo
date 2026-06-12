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

/// @file egolib/vfs_search.c
/// @brief SearchContext — file enumeration backed by PhysFS — plus vfs_copyDirectory.
/// @details Carved out of vfs.c (final slice) so the foundation-base VFS implementation no longer
///          tops a thousand lines. Reaches into vfs.c through the narrow vfs_internal.h seam
///          (to_physfs_path + VFS_PATH/VFS_MAX_PATH).

#include <physfs.h>

#include <cstdio>

#include "egolib/vfs.h"
#include "egolib/vfs_internal.h"

#include "egolib/strutil.h"
#include "egolib/platform.h"
#include "egolib/Log/_Include.hpp"

//--------------------------------------------------------------------------------------------
std::vector<std::string> SearchContext::enumerateFiles(const Ego::VfsPath& pathname) {
    std::vector<std::string> result;
    const auto temporary = to_physfs_path(pathname.string());
    char **fileList = PHYSFS_enumerateFiles(temporary.c_str());
    if (!fileList) {
        throw std::runtime_error("unable to enumerate files");
    }
    for (char **file = fileList; nullptr != *file; ++file) {
        try {
            result.push_back(*file);
        } catch (...) {
            PHYSFS_freeList(fileList);
            std::rethrow_exception(std::current_exception());
        }
    }
    PHYSFS_freeList(fileList);
    return result;
}

SearchContext::SearchContext(const Ego::VfsPath& searchPath, const Ego::Extension& searchExtension, uint32_t searchBits)
    : file_list_2(), file_list_iterator_2(), path_2(), bare(false), predicates(), found_2() {
    // Bare search results?
    if (VFS_SEARCH_BARE == (VFS_SEARCH_BARE & searchBits)) {
        bare = true;
    }
    // Filter by type?
    predicates.push_back(makePredicate(searchBits));
    // Enumerate using PhysFS.
    path_2 = vfs_convert_fname(searchPath);
    file_list_2 = enumerateFiles(path_2);
    // Filter by extension?
    predicates.push_back(makePredicate(searchExtension.to_string()));
    // Begin iteration.
    file_list_iterator_2 = file_list_2.begin();
    // Search the first acceptable filename.
    for (; file_list_iterator_2 != file_list_2.cend(); file_list_iterator_2++) {
        /// @todo: Possibly virtual function call. Not acceptable.
        if (predicate(Ego::VfsPath(*file_list_iterator_2))) {
            break;
        }
    }
    if (file_list_iterator_2 != file_list_2.cend()) {
        if (bare) {
            found_2 = Ego::VfsPath(*file_list_iterator_2);
        } else {
            /// @todo Possibly virtual function call. Not acceptable.
            found_2 = path_2 + Ego::VfsPath(NETWORK_SLASH_STR) + Ego::VfsPath(*file_list_iterator_2);
        }
    }
}

SearchContext::SearchContext(const Ego::VfsPath& searchPath, uint32_t searchBits) :
    file_list_2(), file_list_iterator_2(), path_2(), bare(false), predicates(), found_2() {
    // Bare search results?
    if (VFS_SEARCH_BARE == (VFS_SEARCH_BARE & searchBits)) {
        bare = true;
    }
    // Filter by type?
    predicates.push_back(makePredicate(searchBits));
    // Enumerate using PhysFS.
    path_2 = vfs_convert_fname(searchPath);
    file_list_2 = enumerateFiles(path_2);

    // Begin iteration.
    file_list_iterator_2 = file_list_2.begin();
    // Search the first acceptable filename.
    for (; file_list_iterator_2 != file_list_2.cend(); file_list_iterator_2++) {
        /// @todo: Possibly virtual function call. Not acceptable.
        if (predicate(Ego::VfsPath(*file_list_iterator_2))) {
            break;
        }
    }
    if (file_list_iterator_2 != file_list_2.cend()) {
        if (bare) {
            found_2 = Ego::VfsPath(*file_list_iterator_2);
        } else {
            /// @todo Possibly virtual function call. Not acceptable.
            found_2 = path_2 + Ego::VfsPath(NETWORK_SLASH_STR) + Ego::VfsPath(*file_list_iterator_2);
        }
    }
}

SearchContext::SearchContext(uint32_t searchBits)
    : SearchContext(Ego::VfsPath("/"), searchBits)
{ /* Intentionally empty. */}

SearchContext::SearchContext(const Ego::Extension& searchExtension, uint32_t searchBits)
    : SearchContext(Ego::VfsPath("/"), searchExtension, searchBits)
{ /* Intentionally empty. */ }

SearchContext::~SearchContext()
{ /* Intentionally empty. */ }

//--------------------------------------------------------------------------------------------
std::function<bool(const Ego::VfsPath&)> SearchContext::makePredicate(uint32_t searchBits) {
    auto predicate = [searchBits](const Ego::VfsPath& path) {
        if (VFS_SEARCH_ALL != (searchBits & VFS_SEARCH_ALL)) {
            bool isDirectory = vfs_isDirectory(path.string());
            bool isFile = !isDirectory;
            if (isFile) {
                return (VFS_SEARCH_FILE == (VFS_SEARCH_FILE & searchBits));
            }
            if (isDirectory) {
                return (VFS_SEARCH_DIR == (VFS_SEARCH_DIR & searchBits));
            }
        }
        return true;
    };
    return predicate;
}

std::function<bool(const Ego::VfsPath&)> SearchContext::makePredicate(std::string extension) {
    auto predicate = [extension](const Ego::VfsPath& path) {
        return path.getExtension() == extension;
    };
    return predicate;
}

bool SearchContext::predicate(const Ego::VfsPath& path) const {
    auto fullPath = path_2 + Ego::VfsPath(NETWORK_SLASH_STR) + path;
    // Apply predicates.
    for (const auto& predicate : predicates) {
        if (nullptr != predicate) {
            if (!predicate(fullPath)) {
                return false;
            }
        }
    }
    return true;
}

void SearchContext::nextData()
{
    // if there are no files, return an error value

    BAIL_IF_NOT_INIT();

    // Hit the end? Nothing to do.
    if (this->file_list_iterator_2 == this->file_list_2.cend()) {
        return;
    }

    // Increment at least once. Increment until a file is accepted or the end is reached.
    while (true) {
        this->file_list_iterator_2++;
        if (this->file_list_iterator_2 == this->file_list_2.cend()) break;
        if (this->predicate(Ego::VfsPath(*this->file_list_iterator_2))) break;
    }

    // If no suitable file was found ...
    if (this->file_list_iterator_2 == this->file_list_2.cend()) {
        // ... return.
        this->found_2 = Ego::VfsPath();
        return;
    }

    if (this->bare) {
        this->found_2 = Ego::VfsPath(*this->file_list_iterator_2);
    } else {
        this->found_2 = path_2 + Ego::VfsPath(NETWORK_SLASH_STR) + Ego::VfsPath(*this->file_list_iterator_2);
    }
}

bool SearchContext::hasData() const {
    return this->file_list_iterator_2 != this->file_list_2.cend();
}

const Ego::VfsPath& SearchContext::getData() const {
    return this->found_2;
}

//--------------------------------------------------------------------------------------------
int vfs_copyDirectory( const char *sourceDir, const char *destDir )
{
    /// @author ZZ
    /// @details This function copies all files in a directory
    VFS_PATH srcPath = EMPTY_CSTR, destPath = EMPTY_CSTR;

    SearchContext *ctxt;

    BAIL_IF_NOT_INIT();

    if ( INVALID_CSTR( sourceDir ) || INVALID_CSTR( destDir ) )
    {
        return VFS_FALSE;
    }

    // make sure the destination directory exists
    if ( !vfs_mkdir( destDir ) )
    {
        return VFS_FALSE;
    }

    // get the a filename that we are allowed to write to
    //snprintf( szDst, SDL_arraysize( szDst ), "%s",  vfs_resolveWriteFilename( destDir ) );
    //real_dst = szDst;

    // List all the files in the directory
    ctxt = new SearchContext(vfs_convert_fname(sourceDir), VFS_SEARCH_FILE | VFS_SEARCH_BARE );
    if (!ctxt) return VFS_FALSE;
    while (ctxt->hasData())
    {
        auto fileName = ctxt->getData();
        // Ignore files that begin with a .
        if ( '.' != fileName.string()[0] )
        {
            snprintf( srcPath, SDL_arraysize( srcPath ), "%s/%s", sourceDir, fileName.string().c_str() );
            snprintf( destPath, SDL_arraysize( destPath ), "%s/%s", destDir, fileName.string().c_str() );

            if ( !vfs_copyFile( srcPath, destPath ) )
            {
                Log::activeTarget() << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__, "failed to copy from ", "`",
                                                 srcPath, "`", " to ", "`", destPath, "`", ": ", vfs_getError(),
                                                 Log::EndOfEntry);
            }
        }
        ctxt->nextData();
    }
    delete ctxt;
    ctxt = nullptr;

    return VFS_TRUE;
}
