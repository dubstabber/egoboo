/// @file egolib/game/script_functions_action.c
/// @brief Animation, speech, sound, visual effects, and messaging functions

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/GUI/UIManager.hpp"

namespace
{
struct SelfActionContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    IAnimationControl* animation = nullptr;
    IVisualControl* visual = nullptr;
    ITeamMember* teamMember = nullptr;
    const ITargetInfo* targetInfo = nullptr;

    bool isResolved() const
    {
        return selfRef != ObjectRef::Invalid &&
               object != nullptr &&
               profile != nullptr &&
               animation != nullptr &&
               visual != nullptr &&
               teamMember != nullptr &&
               targetInfo != nullptr;
    }

    const Ego::Vector3f& oldPosition() const
    {
        return object->getOldPosition();
    }

    PRO_REF profileRef() const
    {
        return object->getProfileID().get();
    }

    SoundID soundID(int soundIndex) const
    {
        return profile->getSoundID(soundIndex);
    }

    bool hasMessageID(int messageId) const
    {
        return profile->isValidMessageID(messageId);
    }

    const std::string& messageText(int messageId) const
    {
        return profile->getMessage(messageId);
    }
};

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

ICameraSystem& cameraSystem()
{
    return EngineContext::get().cameraSystem();
}

Ego::Graphics::IBillboardSystem& billboardSystem()
{
    return EngineContext::get().billboardSystem();
}

Ego::GUI::UIManager* tryUIManager()
{
    return EngineContext::get().tryUIManager();
}

SelfActionContext makeSelfActionContext(const ai_state_t& self)
{
    SelfActionContext context;
    context.selfRef = self.getSelf();
    context.object = tryObject(context.selfRef);
    if (context.object == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile> profile = context.object->getProfile();
    if (profile == nullptr)
    {
        context.object = nullptr;
        return context;
    }

    context.profile = profile.get();
    context.animation = static_cast<IAnimationControl*>(context.object);
    context.visual = static_cast<IVisualControl*>(context.object);
    context.teamMember = static_cast<ITeamMember*>(context.object);
    context.targetInfo = static_cast<const ITargetInfo*>(context.object);
    return context;
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
    ObjectHandler* handler = gameSession().tryObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const ObjectRef& objectRef : handler->objectRefIterator())
    {
        fn(objectRef);
    }
}

bool hasMatchingIdszProfile(ObjectRef objectRef, const ObjectProfile& profile)
{
    const Object* object = tryObject(objectRef);
    if (object == nullptr)
    {
        return false;
    }

    for (int idszIndex = 0; idszIndex < IDSZ_COUNT; ++idszIndex)
    {
        if (profile.getIDSZ(idszIndex) != object->getProfile()->getIDSZ(idszIndex))
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

std::shared_ptr<Ego::Graphics::Billboard> tryMakeBillboard(const SelfActionContext& context,
                                                           const std::string& text,
                                                           const Ego::Colour4f& textColor,
                                                           const Ego::Colour4f& tint,
                                                           int lifetime)
{
    return billboardSystem().makeBillboard(context.selfRef,
                                           text,
                                           textColor,
                                           tint,
                                           lifetime,
                                           Ego::Graphics::Billboard::Flags::Fade);
}

const ITargetInfo* resolveChargeTarget(const SelfActionContext& selfContext)
{
    if (!selfContext.isResolved())
    {
        return nullptr;
    }

    const ITargetInfo* chargeTarget = selfContext.targetInfo;
    if (!chargeTarget->isPlayer() && chargeTarget->isBeingHeld())
    {
        chargeTarget = tryTargetInfo(chargeTarget->getHolderRef());
    }

    return chargeTarget;
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

    const ITargetInfo* target = tryTargetInfo(self.getTarget());
    IAnimationControl* targetAnimation = tryAnimationControl(self.getTarget());
    if ( target != nullptr && targetAnimation != nullptr && target->isAlive() )
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
uint8_t scr_PlaySound( script_state_t& state, ai_state_t& self )
{
    // PlaySound( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function plays one of the character's sounds.
    /// The sound fades out depending on its distance from the viewer

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    const auto& oldPosition = selfContext.oldPosition();
    if ( oldPosition[kZ] > PITNOSOUND )
    {
        audioSystem().playSound(oldPosition, selfContext.soundID(state.argument));
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashTarget( script_state_t& state, ai_state_t& self )
{
    // FlashTarget()
    /// @author ZZ
    /// @details This function makes the target flash

    if (!resolveSelfContext(self).isResolved()) return false;

    IVisualControl* targetVisual = tryVisualControl(self.getTarget());
    if (targetVisual == nullptr)
    {
        return false;
    }

    targetVisual->flash(255);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetRedShift( script_state_t& state, ai_state_t& self )
{
    // SetRedShift( tmpargument = "red darkening" )
    /// @author ZZ
    /// @details This function sets the character's red shift ( 0 - 3 ), higher values
    /// making the character less red and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setRedShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetGreenShift( script_state_t& state, ai_state_t& self )
{
    // SetGreenShift( tmpargument = "green darkening" )
    /// @author ZZ
    /// @details This function sets the character's green shift ( 0 - 3 ), higher values
    /// making the character less green and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setGreenShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBlueShift( script_state_t& state, ai_state_t& self )
{
    // SetBlueShift( tmpargument = "blue darkening" )
    /// @author ZZ
    /// @details This function sets the character's blue shift ( 0 - 3 ), higher values
    /// making the character less blue and darker

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setBlueShift(Ego::Math::constrain(state.argument, 0, 6));

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetLight( script_state_t& state, ai_state_t& self )
{
    // SetLight( tmpargument = "lighness" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 254 )
    /// 255 = no transparency

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setLight(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAlpha( script_state_t& state, ai_state_t& self )
{
    // SetAlpha( tmpargument = "alpha" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 255 )
    /// 255 = no transparency

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setAlpha(state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BlackTarget( script_state_t& state, ai_state_t& self )
{
    // BlackTarget()
    /// @author ZZ
    /// @details  The opposite of FlashTarget, causing the target to turn black

    if (!resolveSelfContext(self).isResolved()) return false;

    IVisualControl* targetVisual = tryVisualControl(self.getTarget());
    if (targetVisual == nullptr)
    {
        return false;
    }

    targetVisual->flash(0);

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
uint8_t scr_PlaySoundLooped( script_state_t& state, ai_state_t& self )
{
    // PlaySoundLooped( tmpargument = "sound", tmpdistance = "frequency" )

    /// @author ZZ
    /// @details This function starts playing a continuous sound

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    SoundID sound = selfContext.soundID(state.argument);
    
    if ( INVALID_SOUND_ID == sound )
    {
        // Stop existing sound loop (if any)
        audioSystem().stopObjectLoopingSounds(self.getSelf());
    }
    else
    {
        // check whatever might be playing on the channel now
        //ZF> TODO: check if character is already playing a looped sound first!
        audioSystem().playSoundLooped(sound, self.getSelf());
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_StopSound( script_state_t& state, ai_state_t& self )
{
    // StopSound( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function stops the playing of a continuous sound!

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    audioSystem().stopObjectLoopingSounds(self.getSelf(), selfContext.soundID(state.argument));

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
uint8_t scr_PlaySoundVolume( script_state_t& state, ai_state_t& self )
{
    // PlaySoundVolume( argument = "sound", distance = "volume" )
    /// @author ZZ
    /// @details This function sets the volume of a sound and plays it

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    if ( state.distance > 0 )
    {
        int channel = audioSystem().playSound(selfContext.oldPosition(),
                                              selfContext.soundID(Ego::Script::Interpreter::safeCast<int>(state.argument)));

        if ( channel != INVALID_SOUND_CHANNEL )
        {
            Mix_Volume( channel, ( 128 * Ego::Script::Interpreter::safeCast<int>(state.distance) ) / 100 );
        }
    }

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
uint8_t scr_SparkleIcon( script_state_t& state, ai_state_t& self )
{
    // SparkleIcon( tmpargument = "color" )
    /// @author ZZ
    /// @details This function starts little sparklies going around the character's icon

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);
    if ( state.argument < COLOR_MAX )
    {
        if ( state.argument < -1 )
        {
            selfContext.visual->setSparkle(NOSPARKLE);
        }
        else
        {
            selfContext.visual->setSparkle(state.argument % COLOR_MAX);
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnsparkleIcon( script_state_t& state, ai_state_t& self )
{
    // UnsparkleIcon()
    /// @author ZZ
    /// @details This function stops little sparklies going around the character's icon

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    selfContext.visual->setSparkle(NOSPARKLE);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlayFullSound( script_state_t& state, ai_state_t& self )
{
    // PlayFullSound( tmpargument = "sound", tmpdistance = "frequency" )
    /// @author ZZ
    /// @details This function plays one of the character's sounds .
    /// The sound will be heard at full volume by all players (Victory music)

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfActionContext selfContext = makeSelfActionContext(self);
    audioSystem().playSoundFull(selfContext.soundID(state.argument));

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
uint8_t scr_ClearMusicPassage( script_state_t& state, ai_state_t& self )
{
    // ClearMusicPassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This clears the music for a specified passage

    if (!resolveSelfContext(self).isResolved()) return false;

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);
    if(passage) {
        passage->setMusic(Passage::NO_MUSIC);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlayMusic( script_state_t& state, ai_state_t& self )
{
    // PlayMusic( tmpargument = "song number", tmpdistance = "fade time (msec)" )
    /// @author ZZ
    /// @details This function begins playing a new track of music

    if (!resolveSelfContext(self).isResolved()) return false;

    int fadeTime = state.distance;
    if(fadeTime < 0) fadeTime = 0;

    audioSystem().playMusic(state.argument, fadeTime);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMusicPassage( script_state_t& state, ai_state_t& self )
{
    // SetMusicPassage( tmpargument = "passage", tmpturn = "type", tmpdistance = "repetitions" )

    /// @author ZZ
    /// @details This function makes the given passage play music if a player enters it
    /// tmpargument is the passage to set and tmpdistance is the music track to play.

    if (!resolveSelfContext(self).isResolved()) return false;

    const std::shared_ptr<Passage> passage = tryPassage(state.argument);
    if(passage) {
        passage->setMusic(state.distance);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_StopMusic( script_state_t& state, ai_state_t& self )
{
    // StopMusic()
    /// @author ZZ
    /// @details This function stops the interactive music

    if (!resolveSelfContext(self).isResolved()) return false;

    audioSystem().stopMusic();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetVolumeNearestTeammate( script_state_t& state, ai_state_t& self )
{
    // SetVolumeNearestTeammate( tmpargument = "sound", tmpdistance = "distance" )
    /// @author ZZ
    /// @details This function lets insects buzz correctly.  The closest Team member
    /// is used to determine the overall sound level.

    if (!resolveSelfContext(self).isResolved()) return false;

    // TODO: no current runtime implementation.

    return true;
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

    Ego::GUI::UIManager* uiManager = tryUIManager();
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


//--------------------------------------------------------------------------------------------
uint8_t scr_DrawBillboard( script_state_t& state, ai_state_t& self )
{
    // DrawBillboard( tmpargument = "message", tmpdistance = "duration", tmpturn = "color" )
    /// @author ZF
    /// @details This function draws one of those billboards above the character

    const auto text_color = Ego::Colour4f(Ego::Colour4b(255, 255, 255, 255));

    //List of avalible colours
    const auto tint_red  = Ego::Colour4f{ 1.00f, 0.25f, 0.25f, 1.00f };
    const auto tint_purple = Ego::Colour4f{ 0.88f, 0.75f, 1.00f, 1.00f };
    const auto tint_white = Ego::Colour4f{ 1.00f, 1.00f, 1.00f, 1.00f };
    const auto tint_yellow = Ego::Colour4f{ 1.00f, 1.00f, 0.75f, 1.00f };
    const auto tint_green = Ego::Colour4f{ 0.25f, 1.00f, 0.25f, 1.00f };
    const auto tint_blue = Ego::Colour4f{ 0.25f, 0.25f, 1.00f, 1.00f };

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    if ( !selfContext.hasMessageID(state.argument) ) return false;

    auto* tint = &tint_white;
    //Figure out which color to use
    switch ( state.turn )
    {
        case COLOR_WHITE:   tint = &tint_white;   break;
        case COLOR_RED:     tint = &tint_red;     break;
        case COLOR_PURPLE:  tint = &tint_purple;  break;
        case COLOR_YELLOW:  tint = &tint_yellow;  break;
        case COLOR_GREEN:   tint = &tint_green;   break;
        case COLOR_BLUE:    tint = &tint_blue;    break;
    }

    return nullptr != tryMakeBillboard(selfContext,
                                       selfContext.messageText(state.argument),
                                       text_color,
                                       *tint,
                                       state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisplayCharge(script_state_t& state, ai_state_t& self)
{
    // DisplayCharge( tmpargument = "progress", tmpdistance = "max progress", tmpturn = "pip width" )
    /// @author ZF
    /// @details Draws a special progress bar this update frame

    if (!resolveSelfContext(self).isResolved()) return false;
    const SelfActionContext selfContext = makeSelfActionContext(self);

    //We ourselves must be a player or our holder must be one
    const ITargetInfo* chargeTarget = resolveChargeTarget(selfContext);

    //Only do this for players
    if (chargeTarget == nullptr || !chargeTarget->isPlayer()) {
        return false;
    }

    //Validate arguments
    else if(state.distance <= 0 || state.argument < 0)  {
        return false;
    }

    //Render it!
    else {        
        const std::shared_ptr<Ego::Player> player = tryPlayer(*chargeTarget);
        if (player == nullptr)
        {
            return false;
        }
        else
        {
            player->setChargeBar(state.argument, state.distance, state.turn);
        }
    }

    return true;
}
