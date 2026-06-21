/// @file egolib/game/script_functions_teams_presentation.c
/// @brief Character presentation (minimap, end messages, status monitor), module linking, and enemy-sense publication

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

struct TargetCompatibilityContext
{
    ObjectRef targetRef = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
};

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

PresentationEffectsContext makePresentationEffectsContext(const ai_state_t& self)
{
    PresentationEffectsContext context;
    context.selfRef = self.getSelf();
    context.playingState = EngineContext::get().tryActivePlayingState();
    context.minimap = context.playingState ? context.playingState->getMiniMap() : nullptr;
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
