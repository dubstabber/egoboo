/// @file egolib/game/script_functions_quests.c
/// @brief Quests, teams, minimap, links, end messages, and stat monitor

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/MessageLog.hpp"

namespace
{
using FollowLinkByModuleNameFn = bool (*)(const std::string&, bool);

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

FollowLinkByModuleNameFn g_followLinkByModuleName = &link_follow_modname;

struct SelfProfilePolicyData
{
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    EVE_REF enchantRef = INVALID_EVE_REF;
    SKIN_T spellEffectSkin = ObjectProfile::NO_SKIN_OVERRIDE;
};

struct SelfProfileComparisonData
{
    ObjectProfileRef baseModelRef = ObjectProfileRef::Invalid;
    bool baseModelIsSpellbook = false;
    bool currentProfileMatchesBaseModel = false;
};

struct SelfProfilePolicyDataFull
{
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    EVE_REF enchantRef = INVALID_EVE_REF;
    SKIN_T spellEffectSkin = ObjectProfile::NO_SKIN_OVERRIDE;
    SelfProfileComparisonData comparison;
};

struct SelfProfileContext
{
    const ObjectProfile* profile = nullptr;
    std::string selfName;
    std::string className;
    SelfProfilePolicyDataFull policy;
};

struct FollowLinkRequest
{
    std::string selfName;
    std::string moduleName;
};

struct PresentationEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    std::shared_ptr<IPlayingStateController> playingState;
    std::shared_ptr<Ego::GUI::MiniMap> minimap;
};

struct SelfRoleContext
{
    Object* selfObject = nullptr;
    IAppearanceProfile* appearance = nullptr;
    ICharacterState* characterState = nullptr;
    IEnchantable* enchantable = nullptr;
    ITeamMember* teamMember = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    IWallet* wallet = nullptr;
};

struct SelfPresentationCompatibilityContext
{
    SelfRoleContext selfRole;
    PresentationEffectsContext presentation;
};

struct QuestCompatibilityContext
{
    Ego::QuestLog* targetQuestLog = nullptr;
};

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    ICharacterState* characterState = nullptr;
    IInventoryHolder* inventory = nullptr;
    ITeamMember* teamMember = nullptr;
    IEnchantable* enchantable = nullptr;
};

struct ModuleEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    GameModule* module = nullptr;
};

SelfRoleContext makeSelfRoleContext(const ai_state_t& self)
{
    SelfRoleContext context;
    context.selfObject = tryObject(self.getSelf());
    if (context.selfObject == nullptr)
    {
        return context;
    }

    context.appearance = static_cast<IAppearanceProfile*>(context.selfObject);
    context.characterState = static_cast<ICharacterState*>(context.selfObject);
    context.enchantable = static_cast<IEnchantable*>(context.selfObject);
    context.teamMember = static_cast<ITeamMember*>(context.selfObject);
    context.targetInfo = static_cast<const ITargetInfo*>(context.selfObject);
    context.wallet = static_cast<IWallet*>(context.selfObject);
    return context;
}

SelfProfileContext makeSelfProfileContext(const ai_state_t& self)
{
    SelfProfileContext context;
    Object* selfObject = tryObject(self.getSelf());
    if (selfObject == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile>& selfProfile = selfObject->getProfile();
    if (selfProfile == nullptr)
    {
        return context;
    }

    context.profile = selfProfile.get();
    context.selfName = selfObject->getName();
    context.className = selfProfile->getClassName();
    context.policy.profileRef = selfObject->getProfileID();
    context.policy.enchantRef = selfProfile->getEnchantRef();
    context.policy.spellEffectSkin = selfProfile->getSpellEffectType();
    context.policy.comparison.baseModelRef = selfObject->getBaseModelRef();
    context.policy.comparison.baseModelIsSpellbook = context.policy.comparison.baseModelRef == ObjectProfileRef(SPELLBOOK);
    context.policy.comparison.currentProfileMatchesBaseModel =
        context.policy.comparison.baseModelRef == context.policy.profileRef;
    return context;
}

ModuleEffectsContext makeModuleEffectsContext(const ai_state_t& self)
{
    ModuleEffectsContext context;
    context.selfRef = self.getSelf();
    context.module = gameSession().tryActiveModule();
    return context;
}

GameModule& compatibleModule(const ModuleEffectsContext& context)
{
    if (context.module != nullptr)
    {
        return *context.module;
    }

    return activeModule();
}

void giveGoodTeamExperience(const ModuleEffectsContext& context, int amount, XPType type)
{
    compatibleModule(context).giveTeamExperience(static_cast<TEAM_REF>(Team::TEAM_GOOD), amount, type);
}

PresentationEffectsContext makePresentationEffectsContext(const ai_state_t& self)
{
    PresentationEffectsContext context;
    context.selfRef = self.getSelf();
    context.playingState = EngineContext::get().tryActivePlayingState();
    context.minimap = context.playingState ? context.playingState->getMiniMap() : nullptr;
    return context;
}

SelfPresentationCompatibilityContext makeSelfPresentationCompatibilityContext(const ai_state_t& self)
{
    SelfPresentationCompatibilityContext context;
    context.selfRole = makeSelfRoleContext(self);
    context.presentation = makePresentationEffectsContext(self);
    return context;
}

bool setSelfTeam(SelfRoleContext& selfContext, TEAM_REF teamRef)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(teamRef);
    return true;
}

bool applySelfTeam(SelfPresentationCompatibilityContext& context, TEAM_REF teamRef)
{
    return setSelfTeam(context.selfRole, teamRef);
}

bool joinSelfTeamToResolvedTarget(const TargetCompatibilityContext& targetContext,
                                  SelfRoleContext& selfContext)
{
    if (targetContext.info == nullptr || selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(targetContext.info->getTeamRef());
    return true;
}

bool becomeSelfLeader(SelfRoleContext& selfContext)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->becomeTeamLeader();
    return true;
}

bool isSelfLeaderAlive(const SelfRoleContext& selfContext)
{
    return selfContext.targetInfo != nullptr &&
           teamLeaderRef(*selfContext.targetInfo) != ObjectRef::Invalid;
}

bool giveSelfTeamExperience(const script_state_t& state, SelfRoleContext& selfContext)
{
    if (state.distance < 0 || state.distance >= XP_COUNT)
    {
        return true;
    }

    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->giveTeamExperience(state.argument, static_cast<XPType>(state.distance));
    return true;
}

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    TargetCompatibilityContext context;
    context.targetRef = self.getTarget();
    context.info = tryTargetInfo(context.targetRef);
    context.characterState = tryCharacterState(context.targetRef);
    context.inventory = tryInventoryHolder(context.targetRef);
    context.teamMember = tryTeamMember(context.targetRef);
    context.enchantable = tryEnchantable(context.targetRef);
    return context;
}

bool setResolvedTargetTeam(const TargetCompatibilityContext& targetContext, TEAM_REF teamRef)
{
    if (targetContext.teamMember == nullptr)
    {
        return false;
    }

    targetContext.teamMember->setTeam(teamRef);
    return true;
}

bool showMiniMap(const PresentationEffectsContext& context)
{
    if (!context.minimap)
    {
        return false;
    }

    const bool wasHidden = !context.minimap->isVisible();
    context.minimap->setVisible(true);
    return wasHidden;
}

void showMiniMapPlayerPosition(const PresentationEffectsContext& context)
{
    if (context.minimap)
    {
        context.minimap->setShowPlayerPosition(true);
    }
}

const Object* tryUiObject(ObjectRef objectRef)
{
    return tryObject(objectRef);
}

void addMiniMapBlip(const PresentationEffectsContext& context, float x, float y, ObjectRef objectRef)
{
    const Object* object = tryUiObject(objectRef);
    if (!context.minimap || !object)
    {
        return;
    }

    context.minimap->addBlip(x, y, object->getIcon());
}

void addSelfMiniMapBlip(const PresentationEffectsContext& context, float x, float y)
{
    addMiniMapBlip(context, x, y, context.selfRef);
}

void addSelfStatusMonitor(const PresentationEffectsContext& context)
{
    if (!context.playingState)
    {
        return;
    }

    context.playingState->addStatusMonitor(context.selfRef);
}

void clearEndMessageText()
{
    g_endText.setText("");
}

bool addEndMessageText(Object& object, int messageIndex, script_state_t& state)
{
    return ::AddEndMessage(&object, messageIndex, &state);
}

bool addSelfEndMessageText(const PresentationEffectsContext& context, int messageIndex, script_state_t& state)
{
    Object* selfObject = tryObject(context.selfRef);
    return selfObject != nullptr &&
           addEndMessageText(*selfObject, messageIndex, state);
}

void logDeprecatedScriptFunctionUse(const std::string& functionName,
                                    const std::string& className)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning,
                                                           __FILE__,
                                                           __LINE__,
                                                           "deprecated script function ",
                                                           "`",
                                                           functionName,
                                                           "`",
                                                           " by class `",
                                                           className,
                                                           "`",
                                                           Log::EndOfEntry);
}

void publishDeprecatedEnableListenSkillWarning(const SelfProfileContext& context)
{
    logDeprecatedScriptFunctionUse("EnableListenSkill", context.className);
}

bool resolveFollowLinkRequest(const SelfProfileContext& context,
                              const int messageId,
                              FollowLinkRequest& request)
{
    if (!context.profile->isValidMessageID(messageId))
    {
        return false;
    }

    request.selfName = context.selfName;
    request.moduleName = context.profile->getMessage(messageId);
    return true;
}

void publishFollowLinkFailureMessage(const PresentationEffectsContext& context,
                                     const std::string& selfName)
{
    const std::string text = "That's too scary for " + selfName;
    if (context.playingState)
    {
        context.playingState->getMessageLog()->addMessage(text);
        return;
    }

    DisplayMsg_printf("%s", text.c_str());
}

bool tryFollowLink(const PresentationEffectsContext& context,
                   const FollowLinkRequest& request)
{
    const bool followed = g_followLinkByModuleName(request.moduleName, true);
    if (!followed)
    {
        publishFollowLinkFailureMessage(context, request.selfName);
    }

    return followed;
}

bool followLinkFromMessageId(const SelfProfileContext& context,
                             const PresentationEffectsContext& presentationContext,
                             const int messageId)
{
    FollowLinkRequest request;
    return resolveFollowLinkRequest(context, messageId, request) &&
           tryFollowLink(presentationContext, request);
}

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
    for (const std::shared_ptr<Ego::Player>& player : gameSession().playerList())
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

void publishEnemySense(const EnemySenseState& state)
{
    gameSession().publishEnemySense(state);
}

void resetEnemySense()
{
    gameSession().resetEnemySense();
}

void publishEnemySenseFromResolvedTarget(const TargetCompatibilityContext& targetContext,
                                         uint32_t idsz)
{
    if (targetContext.info != nullptr)
    {
        publishEnemySense(EnemySenseState(targetContext.info->getTeamRef(), idsz));
        return;
    }

    resetEnemySense();
}
} // namespace

void scr_systems_set_follow_link_by_modname_for_test(bool (*fn)(const std::string&, bool))
{
    g_followLinkByModuleName = fn ? fn : &link_follow_modname;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTargetTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTargetTeam()
    /// @author ZZ
    /// @details This function lets a character join a different team.  Used
    /// mostly for pets

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return joinSelfTeamToResolvedTarget(targetContext, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderKilled( script_state_t& state, ai_state_t& self )
{
    // IfLeaderKilled()
    /// @author ZZ
    /// @details This function proceeds if the team's leader died this update

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_LEADERKILLED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeLeader( script_state_t& state, ai_state_t& self )
{
    // BecomeLeader()
    /// @author ZZ
    /// @details This function makes the character the leader of the team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return becomeSelfLeader(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfLeaderIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the team has a leader

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfRoleContext selfContext = makeSelfRoleContext(self);
    return isSelfLeaderAlive(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowMap( script_state_t& state, ai_state_t& self )
{
    // ShowMap()
    /// @author ZZ
    /// @details This function shows the module's map.
    /// Fails if map already visible

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return showMiniMap(selfContext.presentation);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowYouAreHere( script_state_t& state, ai_state_t& self )
{
    // ShowYouAreHere()
    /// @author ZZ
    /// @details This function shows the blinking white blip on the map that represents the
    /// camera location

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    showMiniMapPlayerPosition(selfContext.presentation);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowBlipXY( script_state_t& state, ai_state_t& self )
{
    // ShowBlipXY( tmpx = "x", tmpy = "y", tmpargument = "color" )

    /// @author ZZ
    /// @details This function draws a blip on the map, and must be done each update
    if (!resolveSelfContext(self).isResolved()) return false;

    // Add a blip
    if ( state.argument >= 0 )
    {
        const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
        addSelfMiniMapBlip(selfContext.presentation, state.x, state.y);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearEndMessage( script_state_t& state, ai_state_t& self )
{
    // ClearEndMessage()
    /// @author ZZ
    /// @details This function empties the end-module text buffer

    if (!resolveSelfContext(self).isResolved()) return false;

    clearEndMessageText();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddEndMessage( script_state_t& state, ai_state_t& self )
{
    // AddEndMessage( tmpargument = "message" )
    /// @author ZZ
    /// @details This function appends a message to the end-module text buffer

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return addSelfEndMessageText(selfContext.presentation, state.argument, state);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddStat( script_state_t& state, ai_state_t& self )
{
    // AddStat()
    /// @author ZZ
    /// @details This function turns on an NPC's status display

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    addSelfStatusMonitor(selfContext.presentation);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the character itself join a specified team (A = 0, B = 1, 23 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetJoinTeam( script_state_t& state, ai_state_t& self )
{
    // TargetJoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the Target join a Team specified in tmpargument (A = 0, 25 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    return setResolvedTargetTeam(targetContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinEvilTeam( script_state_t& state, ai_state_t& self )
{
    // JoinEvilTeam()
    /// @author ZZ
    /// @details This function adds the character to the evil Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_EVIL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinNullTeam( script_state_t& state, ai_state_t& self )
{
    // JoinNullTeam()
    /// @author ZZ
    /// @details This function adds the character to the null Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_NULL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinGoodTeam( script_state_t& state, ai_state_t& self )
{
    // JoinGoodTeam()
    /// @author ZZ
    /// @details This function adds the character to the good Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfPresentationCompatibilityContext selfContext = makeSelfPresentationCompatibilityContext(self);
    return applySelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_GOOD));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToGoodTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToGoodTeam(  tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the G Team

    if (!resolveSelfContext(self).isResolved()) return false;

    if(state.distance < XP_COUNT)
    {
        const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
        giveGoodTeamExperience(moduleContext, state.argument, static_cast<XPType>(state.distance));
    }


    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableListenSkill( script_state_t& state, ai_state_t& self )
{
    // EnableListenSkill()
    /// @author ZF
    /// @details This function increases range from which sound can be heard by 33%

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);

    publishDeprecatedEnableListenSkillWarning(selfContext);
    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FollowLink( script_state_t& state, ai_state_t& self )
{
    // FollowLink( tmpargument = "index of next module name" )
    /// @author BB
    /// @details Skips to the next module!

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileContext selfContext = makeSelfProfileContext(self);
    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);

    return followLinkFromMessageId(selfContext, presentationContext, state.argument);
}


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


//--------------------------------------------------------------------------------------------
uint8_t scr_AddBlipAllEnemies( script_state_t& state, ai_state_t& self )
{
    // AddBlipAllEnemies()
    /// @author ZF
    /// @details show all enemies on the minimap who match the IDSZ given in tmpargument
    /// it show only the enemies of the AI Target

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetCompatibilityContext targetContext = makeTargetCompatibilityContext(self);
    publishEnemySenseFromResolvedTarget(targetContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTargetTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTargetTeam( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the target's team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return giveSelfTeamExperience(state, selfContext);
}
