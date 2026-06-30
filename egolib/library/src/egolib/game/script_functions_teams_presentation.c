/// @file egolib/game/script_functions_teams_presentation.c
/// @brief Character presentation (minimap, end messages, status monitor), module linking, and enemy-sense publication

#include "egolib/game/script_functions_internal.h"
#include "egolib/Profiles/IProfileSystem.hpp"

namespace
{
using FollowLinkByModuleNameFn = bool (*)(const std::string&, bool);

FollowLinkByModuleNameFn g_followLinkByModuleName = &link_follow_modname;

struct FollowLinkRequest
{
    std::string selfName;
    std::string moduleName;
};

struct PresentationEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    std::shared_ptr<IPlayingStateController> playingState;
};

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
};

PresentationEffectsContext makePresentationEffectsContext(const ai_state_t& self)
{
    PresentationEffectsContext context;
    context.selfRef = self.getSelf();
    context.playingState = tryActivePlayingState();
    return context;
}

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    TargetCompatibilityContext context;
    context.targetRef = self.getTarget();
    context.info = tryTargetInfo(context.targetRef);
    return context;
}

bool showMiniMap(const PresentationEffectsContext& context)
{
    if (!context.playingState)
    {
        return false;
    }

    return context.playingState->showMiniMap();
}

void showMiniMapPlayerPosition(const PresentationEffectsContext& context)
{
    if (context.playingState)
    {
        context.playingState->setMiniMapShowPlayerPosition(true);
    }
}

std::shared_ptr<const Ego::Texture> resolveMiniMapIcon(ObjectRef objectRef)
{
    const IProfiled* profiled = tryProfiled(objectRef);
    const ITargetInfo* info = tryTargetInfo(objectRef);
    if (profiled == nullptr || info == nullptr)
    {
        return nullptr;
    }

    const std::shared_ptr<ObjectProfile>& profile = profiled->getProfile();
    if (profile == nullptr)
    {
        return nullptr;
    }

    const SKIN_T spellEffectType = profile->getSpellEffectType();
    if (spellEffectType == ObjectProfile::NO_SKIN_OVERRIDE)
    {
        return profile->getIcon(info->getSkin()).get_ptr();
    }

    return activeProfileSystem().getSpellBookIcon(spellEffectType).get_ptr();
}

void addMiniMapBlip(const PresentationEffectsContext& context, float x, float y, ObjectRef objectRef)
{
    if (!context.playingState)
    {
        return;
    }

    const std::shared_ptr<const Ego::Texture> icon = resolveMiniMapIcon(objectRef);
    if (icon)
    {
        context.playingState->addMiniMapBlip(x, y, icon);
    }
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

bool addEndMessageText(ObjectRef objectRef, int messageIndex, script_state_t& state)
{
    return ::AddEndMessage(objectRef, messageIndex, &state);
}

bool addSelfEndMessageText(const PresentationEffectsContext& context, int messageIndex, script_state_t& state)
{
    return addEndMessageText(context.selfRef, messageIndex, state);
}

void logDeprecatedScriptFunctionUse(const std::string& functionName,
                                    const std::string& className)
{
    Log::activeTarget() << Log::Entry::create(Log::Level::Warning,
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

void publishDeprecatedEnableListenSkillWarning(const SelfProfileSnapshot& context)
{
    logDeprecatedScriptFunctionUse("EnableListenSkill", context.className);
}

bool resolveFollowLinkRequest(const SelfProfileSnapshot& context,
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
        context.playingState->addMessageLogMessage(text);
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

bool followLinkFromMessageId(const SelfProfileSnapshot& context,
                             const PresentationEffectsContext& presentationContext,
                             const int messageId)
{
    FollowLinkRequest request;
    return resolveFollowLinkRequest(context, messageId, request) &&
           tryFollowLink(presentationContext, request);
}

void publishEnemySense(const EnemySenseState& state)
{
    activeSessionStatePublisher().publishEnemySense(state);
}

void resetEnemySense()
{
    activeSessionStatePublisher().resetEnemySense();
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
uint8_t scr_ShowMap( script_state_t& state, ai_state_t& self )
{
    // ShowMap()
    /// @author ZZ
    /// @details This function shows the module's map.
    /// Fails if map already visible

    if (!resolveSelfContext(self).isResolved()) return false;
    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);
    return showMiniMap(presentationContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowYouAreHere( script_state_t& state, ai_state_t& self )
{
    // ShowYouAreHere()
    /// @author ZZ
    /// @details This function shows the blinking white blip on the map that represents the
    /// camera location

    if (!resolveSelfContext(self).isResolved()) return false;

    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);
    showMiniMapPlayerPosition(presentationContext);

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
        const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);
        addSelfMiniMapBlip(presentationContext, state.x, state.y);
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

    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);
    return addSelfEndMessageText(presentationContext, state.argument, state);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddStat( script_state_t& state, ai_state_t& self )
{
    // AddStat()
    /// @author ZZ
    /// @details This function turns on an NPC's status display

    if (!resolveSelfContext(self).isResolved()) return false;

    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);
    addSelfStatusMonitor(presentationContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableListenSkill( script_state_t& state, ai_state_t& self )
{
    // EnableListenSkill()
    /// @author ZF
    /// @details This function increases range from which sound can be heard by 33%

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfProfileSnapshot selfContext = makeSelfProfileSnapshot(self);
    if (!selfContext.isResolved()) return false;

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

    SelfProfileSnapshot selfContext = makeSelfProfileSnapshot(self);
    if (!selfContext.isResolved()) return false;
    const PresentationEffectsContext presentationContext = makePresentationEffectsContext(self);

    return followLinkFromMessageId(selfContext, presentationContext, state.argument);
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
