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

/// @file egolib/Profiles/ModuleProfile.cpp
/// @author Johan Jansen

#define EGOLIB_PROFILES_PRIVATE 1
#include "egolib/Profiles/ModuleProfile.hpp"

#include "egolib/Core/StringUtilities.hpp"

#include "egolib/egoboo_setup.h"
#include "egolib/Log/_Include.hpp"

#include "egolib/vfs.h"
#include "egolib/strutil.h"
#include "egolib/fileutil.h"
#include "egolib/platform.h"

#include "idlib/exception.hpp"  // idlib::runtime_error
#include "idlib/hll.hpp"        // idlib::hll::compilation_error

const uint8_t ModuleProfile::RESPAWN_ANYTIME;
static const size_t SUMMARYLINES = 8;

ModuleProfile::ModuleProfile() :
    _loaded(false),
    _name("*UNKNOWN*"),
    _rank(0),
    _reference(),
    _importAmount(1),
    _allowExport(false),
    _minPlayers(0),
    _maxPlayers(0),
    _respawnValid(false),
    _summary(),
    _unlockQuest(IDSZ2::None),
    _unlockQuestLevel(-1), //-1 means none
    _moduleType(FILTER_SIDE_QUEST),
    _beaten(false),
    _icon(),
    _vfsPath(_name),
    _folderName(_name)
{}

ModuleProfile::~ModuleProfile()
{
}

bool ModuleProfile::isModuleUnlocked() const
{
    // First check if we are in developers mode or that the right module has been beaten before.
    if (Ego::activeConfig().debug_developerMode_enable.getValue())
    {
        return true;
    }

    if (moduleHasIDSZ(_folderName, _unlockQuest))
    {
        return true;
    }

//ZF> TODO: re-enable
/*
    if (base.importamount > 0)
    {
        // If that did not work, then check all selected players directories, but only if it isn't a starter module
        for(const std::shared_ptr<LoadPlayerElement> &player : _selectedPlayerList)
        {
            // find beaten quests or quests with proper level
            if(!player->hasQuest(base.unlockquest.id, base.unlockquest.level)) {
                return false;
            }
        }
    }
*/

    return true;
}

ModuleFilter ModuleProfile::getModuleType() const
{
    return _moduleType;
}


std::shared_ptr<ModuleProfile> ModuleProfile::loadFromFile(const std::string &folderPath)
{
    // see if we can open menu.txt file (required)
    ReadContext ctxt(folderPath + "/gamedat/menu.txt");

    //Allocate memory
    std::shared_ptr<ModuleProfile> result = std::make_shared<ModuleProfile>();

    // Read basic data
    result->_name = vfs_get_next_string_lit(ctxt);

    result->_reference = vfs_get_next_name(ctxt);

    result->_unlockQuest = vfs_get_next_idsz(ctxt);
    ctxt.skipWhiteSpaces();
    if (!ctxt.ise(ctxt.NEW_LINE()) && !ctxt.ise(ctxt.END_OF_INPUT()))
    {
        result->_unlockQuestLevel = ctxt.readIntegerLiteral();
    }
    result->_importAmount = vfs_get_next_int(ctxt);
    result->_allowExport  = vfs_get_next_bool(ctxt);
    result->_minPlayers = vfs_get_next_int(ctxt);
    result->_maxPlayers = vfs_get_next_int(ctxt);

    switch (vfs_get_next_printable(ctxt))
    {
        case 'T':
            result->_respawnValid = true;
        break;

        case 'A':
            result->_respawnValid = RESPAWN_ANYTIME;
        break;

        default:
            result->_respawnValid = false;
        break;
    }

    // Skip RTS option.
    vfs_get_next_printable(ctxt);

    std::string buffer = Ego::trim_ws(vfs_get_next_string_lit(ctxt));
    result->_rank = buffer.length();

    // convert the special ranks of "unranked" or "-" ("rank 0")
    if ( '-' == buffer[0] || 'U' == idlib::to_upper(buffer[0]) )
    {
        result->_rank = 0;
    }

    // Read the summary
    for (size_t cnt = 0; cnt < SUMMARYLINES; cnt++)
    {
        // load the string
        result->_summary.push_back(vfs_get_next_string_lit(ctxt));
    }

    // Assume default module type as a sidequest
    result->_moduleType = FILTER_SIDE_QUEST;

    // Read expansions
    while (ctxt.skipToColon(true))
    {
        IDSZ2 idsz = ctxt.readIDSZ();

        // Read module type
        if ( idsz == IDSZ2('T', 'Y', 'P', 'E') )
        {
            // parse the expansion value
            switch (idlib::to_upper(ctxt.readPrintable()))
            {
                case 'M': result->_moduleType = FILTER_MAIN; break;
                case 'S': result->_moduleType = FILTER_SIDE_QUEST; break;
                case 'T': result->_moduleType = FILTER_TOWN; break;
                case 'F': result->_moduleType = FILTER_FUN; break;
                //case 'S': result->_moduleType = FILTER_STARTER; break;
            }
        }
        else if ( idsz == IDSZ2('B', 'E', 'A', 'T') )
        {
            result->_beaten = true;
        }
    }

    //Done!
    result->_loaded = true;

    // save the module path
    result->_vfsPath = folderPath;

    // load title image
    result->_icon = Ego::DeferredTexture(folderPath + "/gamedat/title");

    //Strip the prefix path and get just the folder name of the module
    result->_folderName = folderPath.substr(folderPath.find_last_of('/', folderPath.size() - 1) + 1);
                       /* folderPath.substr(folderPath.find_first_of('/') + 1); */

    return result;
}

namespace {

/// @brief Log one skipped module for ModuleProfile::moduleHasIDSZ.
/// @remark Goes through Log::tryActiveTarget() rather than Log::activeTarget(): the latter
///         falls through to Log::get(), which throws std::logic_error when the logging system
///         is not initialized (Log/_Include.cpp). moduleHasIDSZ promises never to throw for a
///         content fault, so its own diagnostic must not be able to throw either.
void logIdszMiss(const std::string& moduleName, const std::string& pathname, std::string reason)
{
    Log::Target* logTarget = Log::tryActiveTarget();
    if (!logTarget) return;

    // idlib::runtime_error::to_string() is deliberately multi-line (runtime_error.hpp emits
    // "runtime error:", the raise site and the message on separate lines), so flatten it to
    // keep one skipped module to one log record.
    for (char& c : reason)
    {
        if (c == '\n' || c == '\r') c = ' ';
    }

    *logTarget << Log::Entry::create(Log::Level::Debug, __FILE__, __LINE__,
                                     "module ", "`", moduleName, "`",
                                     " does not have the requested IDSZ: unable to read ",
                                     "`", pathname, "`", ": ", reason, Log::EndOfEntry);
}

} // namespace

bool ModuleProfile::moduleHasIDSZ(const std::string& szModName, const IDSZ2& idsz)
{
    /// @author ZZ
    /// @details This function returns true if the named module has the required IDSZ
    ///          See ModuleProfile.hpp for the miss contract this implements.
    bool foundidsz;

    // These two answers need no I/O at all, so they are settled before the try below:
    // a "no requirement" query is vacuously satisfied, and the sentinel name never is.
    // (IDSZ2::None is IDSZ2('N','O','N','E') - see IDSZ.cpp - not a zero value.)
    if ( idsz == IDSZ2::None ) return true;

    if ( szModName == "NONE" ) return false;

    std::string newLoadName = "mp_modules/" + szModName + "/gamedat/menu.txt";

    try
    {
        ReadContext ctxt(newLoadName);

        // Read basic data
        ctxt.skipToColon(false);  // Name of module...  Doesn't matter
        ctxt.skipToColon(false);  // Reference directory...
        ctxt.skipToColon(false);  // Reference IDSZ...
        ctxt.skipToColon(false);  // Import...
        ctxt.skipToColon(false);  // Export...
        ctxt.skipToColon(false);  // Min players...
        ctxt.skipToColon(false);  // Max players...
        ctxt.skipToColon(false);  // Respawn...
        ctxt.skipToColon(false);  // BAD! NOT USED
        ctxt.skipToColon(false);  // Rank...

        // Summary...
        for (size_t cnt = 0; cnt < SUMMARYLINES; cnt++)
        {
            ctxt.skipToColon(false);
        }

        // Now check expansions
        foundidsz = false;
        while (ctxt.skipToColon(true))
        {
            if ( ctxt.readIDSZ() == idsz )
            {
                foundidsz = true;
                break;
            }
        }

        return foundidsz;
    }
    // These are the only two types raised anywhere inside the try above, and the header
    // documents both as a miss rather than a fault:
    //
    //  - idlib::runtime_error (idlib/exception/runtime_error.hpp:41) is raised by
    //    vfs_readEntireFile (vfs_bulk.c:56) through the Ego::Script::Scanner constructor
    //    (Script/Scanner.hpp, whose @throw documents it), which ReadContext derives from
    //    (fileutil.h:56). This is the common case: the named module is not installed, has no
    //    gamedat/menu.txt, or the file cannot be read.
    //
    //  - idlib::hll::compilation_error (idlib/hll/compilation_error.hpp:60) and its subclass
    //    Ego::Script::MissingDelimiterError (Script/Errors.hpp:29) come out of the fixed
    //    skipToColon(false) sequence above on a menu.txt that is shorter than the header
    //    plus summary block, and out of readIDSZ on a malformed expansion. Those two files -
    //    ReadContext.cpp and ReadContext_literals.cpp - raise nothing else.
    //
    // Note what these arms do NOT buy, so nobody mistakes them for a narrowing they are not:
    // idlib::runtime_error and idlib::hll::compilation_error are today the ONLY two direct
    // subclasses of idlib::exception in the tree, so this pair is currently equivalent in
    // reach to catching the base. Worse, idlib files its programming-error types UNDER
    // runtime_error - invalid_argument_error -> argument_null_error /
    // argument_out_of_bounds_error, unhandled_switch_case_error, and idlib's assertion type
    // itself (idlib::debug_assertion_failed_error, idlib/debug/debug_assertion_failed_error.hpp,
    // raised by IDLIB_DEBUG_ASSERT when _DEBUG is defined). So this pair cannot separate a
    // programming error from a read failure either, and no rearrangement of these two arms
    // would: that would take a typeid check or a change inside idlib, both out of scope here.
    // What the form does buy is a tripwire for a future branch that hangs off idlib::exception
    // directly rather than off runtime_error. catch (...) is avoided for the same reason, and
    // so that std::bad_alloc keeps propagating.
    //
    // The tree of idlib types this reasoning depends on is asserted, not just asserted-in-a-
    // comment, in ContentFaultMissContracts.cpp (ExceptionHierarchyIsUnrelatedToStdException).
    //
    // The reason an explicit arm is needed at all: idlib::exception
    // (idlib/exception/exception.hpp:64) is declared with no base class, so none of this is
    // visible to a catch (const std::exception&).
    catch (const idlib::hll::compilation_error& ex)
    {
        logIdszMiss(szModName, newLoadName, ex.to_string());
        return false;
    }
    catch (const idlib::runtime_error& ex)
    {
        logIdszMiss(szModName, newLoadName, ex.to_string());
        return false;
    }
}

bool ModuleProfile::moduleAddIDSZ(const std::string& szModName, const IDSZ2& idsz)
{
    /// @author ZZ
    /// @details This function appends an IDSZ to the module's menu.txt file

    vfs_FILE *filewrite;
    bool retval = false;

    // Only add if there isn't one already
    if ( !moduleHasIDSZ( szModName, idsz ) )
    {
        // make sure that the file exists in the user data directory since we are WRITING to it
        std::string source_file = "mp_modules/" + szModName + "/gamedat/menu.txt";
        std::string target_file = "/modules/" + szModName + "/gamedat/menu.txt";
        // The copy is what makes the append meaningful: vfs_openAppend does not create the
        // target's directory (only vfs_openWrite calls _vfs_ensure_write_directory), so on a
        // first append the copy is what puts a populated menu.txt in the user directory. This
        // return value used to be discarded, which was harmless only because moduleHasIDSZ
        // *threw* for an unreadable menu.txt and so never reached this branch. It now reports
        // that case as a miss (see the contract in ModuleProfile.hpp), so the guard has to be
        // explicit here instead.
        //
        // The guard covers exactly one of the two miss modes: a source menu.txt that cannot be
        // OPENED. It does not cover a source that opens but does not PARSE, because vfs_copyFile
        // goes through the char**/size_t* overload of vfs_readEntireFile (vfs_bulk.c), which
        // fails only on vfs_openRead and never parses anything. A truncated or malformed
        // menu.txt is therefore copied verbatim and appended to, leaving a target that
        // ProfileSystem::loadModuleProfiles still cannot parse - and, because moduleHasIDSZ
        // keeps reporting a miss for it, a repeated scr_AddIDSZ appends the same expansion
        // again. Closing that would take a tri-state result from moduleHasIDSZ distinguishing a
        // miss from a fault; it is not attempted here, and it is not a regression: before this
        // pass the same input threw out of the script VM and took the process down. No shipped
        // module has a malformed expansion (all 42 checked).
        if ( !vfs_copyFile( source_file, target_file ) )
        {
            return false;
        }

        // Try to open the file in append mode
        filewrite = vfs_openAppend(target_file);
        if ( NULL != filewrite )
        {
            // output the expansion IDSZ
            vfs_printf( filewrite, "\n:[%s]", idsz.toString().c_str() );

            // end the line
            vfs_printf( filewrite, "\n" );

            // success
            retval = true;

            // close the file
            vfs_close( filewrite );
        }
    }

    return retval;
}
