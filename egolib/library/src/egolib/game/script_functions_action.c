/// @file egolib/game/script_functions_action.c
/// @brief Animation, speech, messaging, and misc residual action dispatch entries.
///        Audio dispatch (10 entries) split off to script_functions_action_audio.c and
///        visual-effect dispatch (11 entries) to script_functions_action_visual.c on
///        2026-06-12; shared SelfActionContext + makeSelfActionContext + gameSession
///        promoted to script_functions_action_internal.h.

#include "egolib/game/script_functions_action_internal.h"
#include "egolib/Graphics/ICameraSystem.hpp"
#include "egolib/game/GUI/UIManager.hpp"

namespace
{
ICameraSystem& cameraSystem()
{
    return activeCameraSystem();
}

Ego::GUI::IUIManager* tryUIManager()
{
    return Ego::GUI::tryActiveUIManager();
}

bool startResolvedAnimation(IAnimationControl& animation, int actionIndex, bool overrideAction)
{
    const ModelAction action = animation.resolveModelAction(actionIndex);
    return animation.startAnimation(action, false, overrideAction);
}

bool sendSelfProfileMessage(const SelfActionContext& context,
                            int messageId,
                            script_state_t& state)
{
    return _display_message(context.selfRef,
                            context.profileRef(),
                            messageId,
                            &state);
}

template <typename Fn>
void forEachLiveActionObjectRef(Fn&& fn)
{
    for (const ObjectRef& objectRef : Ego::Entities::activeObjectRefs())
    {
        fn(objectRef);
    }
}

bool hasMatchingIdszProfile(ObjectRef objectRef, const ObjectProfile& profile)
{
    const IProfiled* profiled = tryProfiled(objectRef);
    if (profiled == nullptr || profiled->getProfile() == nullptr)
    {
        return false;
    }

    const ObjectProfile& objectProfile = *profiled->getProfile();
    for (int idszIndex = 0; idszIndex < IDSZ_COUNT; ++idszIndex)
    {
        if (profile.getIDSZ(idszIndex) != objectProfile.getIDSZ(idszIndex))
        {
            return false;
        }
    }

    return true;
}

bool hasLiveHolder(const ITargetInfo& objectTargetInfo)
{
    return tryTargetInfo(objectTargetInfo.getHolderRef()) != nullptr;
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DoAction( script_state_t& state, ai_state_t& self )
{
    // DoAction( tmpargument = "action" )
    /// @author ZZ
    /// @details This function makes the character do a given action if it isn't doing
    /// anything better.  Fails if the action is invalid or if the character is doing
    /// something else already

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);

    return startResolvedAnimation(*selfContext.animation, state.argument, false);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KeepAction( script_state_t& state, ai_state_t& self )
{
    // KeepAction()
    /// @author ZZ
    /// @details This function makes the character's animation stop on its last frame
    /// and stay there.  Usually used for dropped items

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.animation->setActionKeep(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetDoAction( script_state_t& state, ai_state_t& self )
{
    // TargetDoAction( tmpargument = "action" )
    /// @author ZZ
    /// @details The function makes the target start a new action, if it is valid for the model
    /// It will fail if the action is invalid or if the target is doing
    /// something else already

    if (!resolveSelfContext(self).isResolved()) return false;

    const IDamageable* targetDamageable = tryLivingDamageable(self.getTarget());
    IAnimationControl* targetAnimation = tryAnimationControl(self.getTarget());
    if ( targetDamageable != nullptr && targetAnimation != nullptr )
    {
        return startResolvedAnimation(*targetAnimation, state.argument, false);
    }

    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DoActionOverride( script_state_t& state, ai_state_t& self )
{
    // DoActionOverride( tmpargument = "action" )
    /// @author ZZ
    /// @details This function makes the character do a given action no matter what
    /// It will fail if the action is invalid

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);

    return startResolvedAnimation(*selfContext.animation, state.argument, true);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SendMessage( script_state_t& state, ai_state_t& self )
{
    // SendMessage( tmpargument = "message number" )
    /// @author ZZ
    /// @details This function sends a message to the players

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);

    return sendSelfProfileMessage(selfContext, state.argument, state);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CallForHelp( script_state_t& state, ai_state_t& self )
{
    // CallForHelp()
    /// @author ZZ
    /// @details This function calls all of the character's teammates for help.  The
    /// teammates must use IfCalledForHelp in their scripts

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.teamMember->callTeamForHelp();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkeepAction( script_state_t& state, ai_state_t& self )
{
    // UnkeepAction()
    /// @author ZZ
    /// @details This function is the opposite of KeepAction. It makes the current animation resume.

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.animation->setActionKeep(false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SendMessageNear( script_state_t& state, ai_state_t& self )
{
    // SendMessageNear( tmpargument = "message" )
    /// @author ZZ
    /// @details This function sends a message if the camera is in the nearby area

    int iTmp, min_distance;

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);
    const auto& oldPosition = selfContext.oldPosition();

    // iterate over all cameras and find the minimum distance
    min_distance = -1;
    for (const std::shared_ptr<Camera>& camera : cameraSystem().getCameraList())
    {
        iTmp = std::abs( oldPosition[kX] - camera->getTrackPosition()[kX] ) + std::fabs( oldPosition[kY] - camera->getTrackPosition()[kY] );

        if ( -1 == min_distance || iTmp < min_distance )
        {
            min_distance = iTmp;
        }
    }

    if ( min_distance < MSGDISTANCE )
    {
        return sendSelfProfileMessage(selfContext, state.argument, state);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeNameKnown( script_state_t& state, ai_state_t& self )
{
    // MakeNameKnown()
    /// @author ZZ
    /// @details This function makes the name of the character known, for identifying
    /// weapons and spells and such

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setNameKnown(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeUsageKnown( script_state_t& state, ai_state_t& self )
{
    // MakeUsageKnown()
    /// @author ZZ
    /// @details This function makes the usage known for this type of object
    /// For XP gains from using an unknown potion or such

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.profile->makeUsageKnown();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeAmmoKnown( script_state_t& state, ai_state_t& self )
{
    // MakeAmmoKnown()
    /// @author ZZ
    /// @details This function makes the character's ammo known ( for items )

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setAmmoKnown(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChildDoActionOverride( script_state_t& state, ai_state_t& self )
{
    // ChildDoActionOverride( tmpargument = action )

    /// @author ZZ
    /// @details This function lets a character set the action of the last character
    /// it spawned.  It also sets the current frame to the first frame of the
    /// action ( no interpolation from last frame ). If the cation is not valid for the model,
    /// the function will fail

    if (!resolveSelfContext(self).isResolved()) return false;

    IAnimationControl* childAnimation = tryAnimationControl(self.child);
    if ( childAnimation != nullptr )
    {
        return startResolvedAnimation(*childAnimation, state.argument, true);
    }

    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowTimer( script_state_t& state, ai_state_t& self )
{
    // ShowTimer( tmpargument = "time" )
    /// @author ZZ
    /// @details This function sets the value displayed by the module timer.
    /// For races and such.  50 clicks per second

    if (!resolveSelfContext(self).isResolved()) return false;

    timeron = true;
    timervalue = state.argument;

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeSimilarNamesKnown( script_state_t& state, ai_state_t& self )
{
    // MakeSimilarNamesKnown()
    /// @author ZZ
    /// @details This function makes the names of similar objects known.
    /// Checks all 6 IDSZ types to make sure they match.

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    forEachLiveActionObjectRef([&](ObjectRef objectRef)
    {
        if (!hasMatchingIdszProfile(objectRef, *selfContext.profile))
        {
            return;
        }

        IVisualControl* visual = tryVisualControl(objectRef);
        if (visual != nullptr)
        {
            visual->setNameKnown(true);
        }
    });

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CorrectActionForHand( script_state_t& state, ai_state_t& self )
{
    // CorrectActionForHand( tmpargument = "action" )
    /// @author ZZ
    /// @details This function changes tmpargument according to which hand the character
    /// is held in It turns ZA into ZA, ZB, ZC, or ZD.
    /// USAGE:  wizards casting spells

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);
    if ( hasLiveHolder(*selfContext.targetInfo) )
    {
        if ( selfContext.targetInfo->getAttachmentSlot() == SLOT_LEFT )
        {
            // A or B
            state.argument += Random::next(1);
        }
        else
        {
            // C or D
            state.argument += 2 + Random::next(1);
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetDoActionSetFrame( script_state_t& state, ai_state_t& self )
{
    // TargetDoActionSetFrame( tmpargument = "action" )
    /// @author ZZ
    /// @details This function starts the target doing the given action, and also sets
    /// the starting frame to the first of the animation ( so there is no
    /// interpolation 'cause it looks awful in some circumstances )
    /// It will fail if the action is invalid

    if (!resolveSelfContext(self).isResolved()) return false;

    IAnimationControl* targetAnimation = tryAnimationControl(self.getTarget());
    if ( targetAnimation != nullptr )
    {
        if (startResolvedAnimation(*targetAnimation, state.argument, true))
        {
            // remove the interpolation
            targetAnimation->removeInterpolation();

            return true;
        }
    }

    return false;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeNameUnknown( script_state_t& state, ai_state_t& self )
{
    // MakeNameUnknown()
    /// @author ZZ
    /// @details This function makes the name of an item/character unknown.
    /// Usage: Use if you have subspawning of creatures from a book.

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setNameKnown(false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TakePicture( script_state_t& state, ai_state_t& self )
{
    // TakePicture()
    /// @author ZF
    /// @details This function proceeds only if the screenshot was successful

    if (!resolveSelfContext(self).isResolved()) return false;

    Ego::GUI::IUIManager* uiManager = tryUIManager();
    if (uiManager == nullptr)
    {
        return false;
    }

    return uiManager->dumpScreenshot();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets all of the RTS speech registers to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMoveSpeech( script_state_t& state, ai_state_t& self )
{
    // SetMoveSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS move speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;

    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSecondMoveSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSecondMoveSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS movealt speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAttackSpeech( script_state_t& state, ai_state_t& self )
{
    // SetAttacksSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS attack speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAssistSpeech( script_state_t& state, ai_state_t& self )
{
    // SetAssistSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS assist speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTerrainSpeech( script_state_t& state, ai_state_t& self )
{
    // SetTerrainSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS terrain speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    // No longer supported.

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSelectSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSelectSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS select speech register to tmpargument

    if (!resolveSelfContext(self).isResolved()) return false;
    // No longer supported.

    return true;
}
