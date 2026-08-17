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

/// @file egolib/game/Logic/QuestLog.cpp
/// @author Zefz aka Johan Jansen

#include "egolib/game/Logic/QuestLog.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/fileutil.h"  // ReadContext, make_unique<ReadContext>

#include "idlib/exception.hpp"  // idlib::runtime_error
#include "idlib/hll.hpp"        // idlib::hll::compilation_error

namespace Ego
{

namespace {

/// @brief Log one skipped quest.txt for QuestLog::loadFromFile.
/// @remark Goes through Log::tryActiveTarget() rather than Log::activeTarget(): the latter
///         falls through to Log::get(), which throws std::logic_error when the logging system
///         is not initialized (Log/_Include.cpp). loadFromFile promises never to throw for a
///         malformed file, so its own diagnostic must not be able to throw either.
void logQuestParseFailure(const std::string& filePath, std::string reason)
{
    Log::Target* logTarget = Log::tryActiveTarget();
    if (!logTarget) return;

    // Flatten the message to one line so a single malformed file yields a single log record.
    // idlib::runtime_error::to_string() is deliberately multi-line (runtime_error.hpp emits
    // "runtime error:", the raise site, and the message on separate lines); this arm is also
    // reached for idlib::hll::compilation_error, whose to_string() is already single-line
    // (compilation_error.hpp), so the flatten is a harmless no-op there.
    for (char& c : reason)
    {
        if (c == '\n' || c == '\r') c = ' ';
    }

    *logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                     "unable to parse quest file ", "`", filePath, "/quest.txt", "`",
                                     ": ", reason, Log::EndOfEntry);
}

} // namespace

QuestLog::QuestLog() :
    _questLog()
{
    //ctor
}

int QuestLog::getQuestProgress(const IDSZ2 &questID) const
{
    const auto& result = _questLog.find(questID);
    if(result == _questLog.end()) {
        return QUEST_NONE;
    }

    return result->second;
}

bool QuestLog::hasActiveQuest(const IDSZ2& questID) const
{
    int progress = getQuestProgress(questID);
    return progress != QUEST_NONE && progress != QUEST_BEATEN;
}

bool QuestLog::isBeaten(const IDSZ2& questID) const
{
    return getQuestProgress(questID) == QUEST_BEATEN;
}

int QuestLog::operator[] (const IDSZ2& questID) const
{
    return getQuestProgress(questID);
}

void QuestLog::setQuestProgress(const IDSZ2& questID, const int progress)
{
    _questLog[questID] = progress;
}

bool QuestLog::loadFromFile(const std::string& filePath)
{
    // blank out the existing map
    _questLog.clear();

    // Try to open a context
    std::unique_ptr<ReadContext> ctxt = nullptr;
    try {
        ctxt = std::make_unique<ReadContext>(filePath + "/quest.txt");
    } catch (...) {
        return false;
    }

    // Load each IDSZ.
    //
    // readIDSZ() and readIntegerLiteral() raise idlib::hll::compilation_error
    // (ReadContext.cpp/ReadContext_literals.cpp) on a truncated or malformed quest.txt -
    // e.g. a colon with nothing after it, a missing '[' where an IDSZ is expected, or a
    // non-numeric quest level. skipToColon(true) does not throw Ego::Script::MissingDelimiterError
    // on end-of-input (it is called with optional=true, so skipToDelimiter returns false instead
    // - see ReadContext.cpp); its remaining throw path, a scanner read error, raises the same
    // idlib::hll::compilation_error caught below. A malformed file is ordinary, player-editable
    // content, not a precondition violation, so it gets the same cleared-and-false outcome as a
    // missing file above, plus a log record, rather than letting the exception escape into
    // loadPlayerQuestLog -> LoadPlayerElement's constructor -> ProfileSystem::loadAllSavedCharacters,
    // which has no try around that construction.
    try
    {
        while (ctxt->skipToColon(true))
        {
            IDSZ2 idsz = ctxt->readIDSZ();
            int  level = ctxt->readIntegerLiteral();

            // Try to add a single quest to the map
            _questLog[idsz] = level;
        }
    }
    // idlib::hll::compilation_error (and its subclass Ego::Script::MissingDelimiterError) is
    // the only idlib type the parse calls above can raise - the ReadContext construction that
    // can raise idlib::runtime_error (see vfs_readEntireFile via the Scanner constructor, and
    // ModuleProfile::moduleHasIDSZ for the fuller rationale) sits above in its own guard, not
    // in this try. The runtime_error arm below mirrors moduleHasIDSZ's pair as a defensive
    // tripwire: idlib declares idlib::exception with no base class, so a std::exception handler
    // would not see either type, and catch (...) is avoided so std::bad_alloc keeps propagating.
    catch (const idlib::hll::compilation_error& ex)
    {
        logQuestParseFailure(filePath, ex.to_string());
        _questLog.clear();
        return false;
    }
    catch (const idlib::runtime_error& ex)
    {
        logQuestParseFailure(filePath, ex.to_string());
        _questLog.clear();
        return false;
    }

    return true;
}

bool QuestLog::exportToFile(const std::string& filePath) const
{
    // Write a new quest file with all the quests
    vfs_FILE* filewrite = vfs_openWrite(filePath + "/quest.txt");
    if (!filewrite) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to create quest file ", "`", filePath, "`", Log::EndOfEntry);
        return false;
    }

    vfs_printf(filewrite, "// This file keeps order of all the quests for this player\n");
    vfs_printf(filewrite, "// The number after the IDSZ shows the quest level. %i means it is completed.", QUEST_BEATEN);

    // Iterate through every element in the IDSZ map
    for(const auto& node : _questLog)
    {
        // Write every single quest to the quest log
        vfs_printf(filewrite, "\n:[%4s] %i", node.first.toString().c_str(), node.second);
    }

    // Clean up and return
    vfs_close(filewrite);
    return true;    
}

} //Ego