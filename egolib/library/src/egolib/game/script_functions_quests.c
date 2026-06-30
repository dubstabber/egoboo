/// @file egolib/game/script_functions_quests.c
/// @brief Quests and quest log management

#include "egolib/game/script_functions_internal.h"

namespace
{
struct QuestCompatibilityContext
{
    Ego::QuestLog* targetQuestLog = nullptr;
};

Ego::QuestLog* resolvedTargetQuestLog(const ai_state_t& self)
{
    const ITargetInfo* target = tryTargetInfo(self.getTarget());
    return target != nullptr ? tryQuestLog(*target) : nullptr;
}

QuestCompatibilityContext makeQuestCompatibilityContext(const ai_state_t& self)
{
    QuestCompatibilityContext context;
    context.targetQuestLog = resolvedTargetQuestLog(self);
    return context;
}

bool addQuestIfMissing(Ego::QuestLog& questLog, const IDSZ2& idsz, int progress)
{
    if (questLog.hasActiveQuest(idsz) || questLog.isBeaten(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, std::max(progress, 0));
    return true;
}

bool adjustActiveQuestLevel(Ego::QuestLog& questLog, const IDSZ2& idsz, int delta)
{
    if (delta == 0 || !questLog.hasActiveQuest(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, questLog[idsz] + delta);
    return true;
}

bool beatActiveQuest(Ego::QuestLog& questLog, const IDSZ2& idsz)
{
    if (!questLog.hasActiveQuest(idsz))
    {
        return false;
    }

    questLog.setQuestProgress(idsz, Ego::QuestLog::QUEST_BEATEN);
    return true;
}

bool raiseQuestLevelIfHigher(Ego::QuestLog& questLog, const IDSZ2& idsz, int progress)
{
    if (progress <= 0 || questLog.isBeaten(idsz) || progress <= questLog[idsz])
    {
        return false;
    }

    questLog.setQuestProgress(idsz, progress);
    return true;
}

template <typename Fn>
bool updatePlayerQuestLogs(Fn&& fn)
{
    bool updated = false;
    for (const std::shared_ptr<Ego::Player>& player : activeSessionState().playerList())
    {
        if (player != nullptr && fn(player->getQuestLog()))
        {
            updated = true;
        }
    }

    return updated;
}

bool addResolvedQuest(const QuestCompatibilityContext& context, const IDSZ2& idsz, int progress)
{
    return context.targetQuestLog != nullptr &&
           addQuestIfMissing(*context.targetQuestLog, idsz, progress);
}

bool adjustResolvedQuestLevel(const QuestCompatibilityContext& context, const IDSZ2& idsz, int delta)
{
    return context.targetQuestLog != nullptr &&
           adjustActiveQuestLevel(*context.targetQuestLog, idsz, delta);
}

bool beatQuestForAllPlayers(const IDSZ2& idsz)
{
    return updatePlayerQuestLogs([&](Ego::QuestLog& questLog) { return beatActiveQuest(questLog, idsz); });
}

bool raiseQuestForAllPlayers(const IDSZ2& idsz, int progress)
{
    return updatePlayerQuestLogs([&](Ego::QuestLog& questLog)
    {
        return raiseQuestLevelIfHigher(questLog, idsz, progress);
    });
}
} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuest( script_state_t& state, ai_state_t& self )
{
    // AddQuest( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into the targets quest.txt to 0

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const QuestCompatibilityContext questContext = makeQuestCompatibilityContext(self);
    return addResolvedQuest(questContext, idsz, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // BeatQuestAllPlayers()
    /// @author ZF
    /// @details This function marks a IDSZ in the targets quest.txt as beaten
    ///               returns true if at least one quest got marked as beaten.

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    return beatQuestForAllPlayers(idsz);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetQuestLevel( script_state_t& state, ai_state_t& self )
{
    // SetQuestLevel( tmpargument = "idsz", distance = "adjustment" )
    /// @author ZF
    /// @details This function modifies the quest level for a specific quest IDSZ
    /// tmpargument specifies quest idsz (tmpargument) and the adjustment (tmpdistance, which may be negative)

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    const QuestCompatibilityContext questContext = makeQuestCompatibilityContext(self);
    return adjustResolvedQuestLevel(questContext, idsz, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddQuestAllPlayers( script_state_t& state, ai_state_t& self )
{
    // AddQuestAllPlayers( tmpargument = "quest idsz" )
    /// @author ZF
    /// @details This function adds a quest idsz set in tmpargument into all local player's quest logs
    /// The quest level Is set to tmpdistance if the level Is not already higher

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDSZ2 idsz = Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument);
    return raiseQuestForAllPlayers(idsz, state.distance);
}
