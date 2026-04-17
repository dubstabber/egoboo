/// @file egolib/game/script_functions_action.c
/// @brief Animation, speech, sound, visual effects, and messaging functions

#include "egolib/game/script_functions_internal.h"


//--------------------------------------------------------------------------------------------
uint8_t scr_DoAction( script_state_t& state, ai_state_t& self )
{
    // DoAction( tmpargument = "action" )
    /// @author ZZ
    /// @details This function makes the character do a given action if it isn't doing
    /// anything better.  Fails if the action is invalid or if the character is doing
    /// something else already

    SCRIPT_FUNCTION_BEGIN();

    ModelAction action = pchr->getProfile()->getModel()->getAction( state.argument );

    returncode = false;
    if ( rv_success == pchr->inst.startAnimation(action, false, false) )
    {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_KeepAction( script_state_t& state, ai_state_t& self )
{
    // KeepAction()
    /// @author ZZ
    /// @details This function makes the character's animation stop on its last frame
    /// and stay there.  Usually used for dropped items

    SCRIPT_FUNCTION_BEGIN();

    pchr->inst.setActionKeep(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetDoAction( script_state_t& state, ai_state_t& self )
{
    // TargetDoAction( tmpargument = "action" )
    /// @author ZZ
    /// @details The function makes the target start a new action, if it is valid for the model
    /// It will fail if the action is invalid or if the target is doing
    /// something else already

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if ( objectHandler().exists( self.getTarget() ) )
    {
        Object * pself_target = objectHandler().get( self.getTarget() );

        if ( pself_target->isAlive() )
        {
            ModelAction action = pself_target->getProfile()->getModel()->getAction( state.argument );

            if ( rv_success == pself_target->inst.startAnimation(action, false, false) )
            {
                returncode = true;
            }
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DoActionOverride( script_state_t& state, ai_state_t& self )
{
    // DoActionOverride( tmpargument = "action" )
    /// @author ZZ
    /// @details This function makes the character do a given action no matter what
    /// It will fail if the action is invalid

    SCRIPT_FUNCTION_BEGIN();

    ModelAction action = pchr->getProfile()->getModel()->getAction(state.argument);

    returncode = false;
    if ( rv_success == pchr->inst.startAnimation(action, false, true) )
    {
        returncode = true;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SendMessage( script_state_t& state, ai_state_t& self )
{
    // SendMessage( tmpargument = "message number" )
    /// @author ZZ
    /// @details This function sends a message to the players

    SCRIPT_FUNCTION_BEGIN();

    returncode = _display_message( self.getSelf(), pchr->getProfileID().get(), state.argument, &state );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CallForHelp( script_state_t& state, ai_state_t& self )
{
    // CallForHelp()
    /// @author ZZ
    /// @details This function calls all of the character's teammates for help.  The
    /// teammates must use IfCalledForHelp in their scripts

    SCRIPT_FUNCTION_BEGIN();

    pchr->getTeam().callForHelp(objectHandler()[self.getSelf()]);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnkeepAction( script_state_t& state, ai_state_t& self )
{
    // UnkeepAction()
    /// @author ZZ
    /// @details This function is the opposite of KeepAction. It makes the current animation resume.

    SCRIPT_FUNCTION_BEGIN();

    pchr->inst.setActionKeep(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlaySound( script_state_t& state, ai_state_t& self )
{
    // PlaySound( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function plays one of the character's sounds.
    /// The sound fades out depending on its distance from the viewer

    SCRIPT_FUNCTION_BEGIN();

    if ( pchr->getOldPosition()[kZ] > PITNOSOUND )
    {
        AudioSystem::get().playSound(pchr->getOldPosition(), ppro->getSoundID(state.argument));
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashTarget( script_state_t& state, ai_state_t& self )
{
    // FlashTarget()
    /// @author ZZ
    /// @details This function makes the target flash

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    FlashObject( pself_target, 255 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetRedShift( script_state_t& state, ai_state_t& self )
{
    // SetRedShift( tmpargument = "red darkening" )
    /// @author ZZ
    /// @details This function sets the character's red shift ( 0 - 3 ), higher values
    /// making the character less red and darker

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBaseAttribute(Ego::Attribute::RED_SHIFT, Ego::Math::constrain(state.argument, 0, 6));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetGreenShift( script_state_t& state, ai_state_t& self )
{
    // SetGreenShift( tmpargument = "green darkening" )
    /// @author ZZ
    /// @details This function sets the character's green shift ( 0 - 3 ), higher values
    /// making the character less green and darker

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBaseAttribute(Ego::Attribute::GREEN_SHIFT, Ego::Math::constrain(state.argument, 0, 6));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetBlueShift( script_state_t& state, ai_state_t& self )
{
    // SetBlueShift( tmpargument = "blue darkening" )
    /// @author ZZ
    /// @details This function sets the character's blue shift ( 0 - 3 ), higher values
    /// making the character less blue and darker

    SCRIPT_FUNCTION_BEGIN();

    pchr->setBaseAttribute(Ego::Attribute::BLUE_SHIFT, Ego::Math::constrain(state.argument, 0, 6));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetLight( script_state_t& state, ai_state_t& self )
{
    // SetLight( tmpargument = "lighness" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 254 )
    /// 255 = no transparency

    SCRIPT_FUNCTION_BEGIN();

    pchr->setLight(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAlpha( script_state_t& state, ai_state_t& self )
{
    // SetAlpha( tmpargument = "alpha" )
    /// @author ZZ
    /// @details This function alters the character's transparency ( 0 - 255 )
    /// 255 = no transparency

    SCRIPT_FUNCTION_BEGIN();

    pchr->setAlpha(state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BlackTarget( script_state_t& state, ai_state_t& self )
{
    // BlackTarget()
    /// @author ZZ
    /// @details  The opposite of FlashTarget, causing the target to turn black

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    FlashObject( pself_target, 0 );

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SendMessageNear( script_state_t& state, ai_state_t& self )
{
    // SendMessageNear( tmpargument = "message" )
    /// @author ZZ
    /// @details This function sends a message if the camera is in the nearby area

    int iTmp, min_distance;

    SCRIPT_FUNCTION_BEGIN();

    // iterate over all cameras and find the minimum distance
    min_distance = -1;
    for(std::shared_ptr<Camera> camera : CameraSystem::get().getCameraList())
    {
        iTmp = std::abs( pchr->getOldPosition()[kX] - camera->getTrackPosition()[kX] ) + std::fabs( pchr->getOldPosition()[kY] - camera->getTrackPosition()[kY] );

        if ( -1 == min_distance || iTmp < min_distance )
        {
            min_distance = iTmp;
        }
    }

    if ( min_distance < MSGDISTANCE )
    {
        returncode = _display_message( self.getSelf(), pchr->getProfileID().get(), state.argument, &state );
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeNameKnown( script_state_t& state, ai_state_t& self )
{
    // MakeNameKnown()
    /// @author ZZ
    /// @details This function makes the name of the character known, for identifying
    /// weapons and spells and such

    SCRIPT_FUNCTION_BEGIN();

    pchr->setNameKnown(true);
    //           pchr->icon = true;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeUsageKnown( script_state_t& state, ai_state_t& self )
{
    // MakeUsageKnown()
    /// @author ZZ
    /// @details This function makes the usage known for this type of object
    /// For XP gains from using an unknown potion or such

    SCRIPT_FUNCTION_BEGIN();

    ppro->makeUsageKnown();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeAmmoKnown( script_state_t& state, ai_state_t& self )
{
    // MakeAmmoKnown()
    /// @author ZZ
    /// @details This function makes the character's ammo known ( for items )

    SCRIPT_FUNCTION_BEGIN();

    pchr->setAmmoKnown(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlaySoundLooped( script_state_t& state, ai_state_t& self )
{
    // PlaySoundLooped( tmpargument = "sound", tmpdistance = "frequency" )

    /// @author ZZ
    /// @details This function starts playing a continuous sound

    SCRIPT_FUNCTION_BEGIN();

    SoundID sound = ppro->getSoundID(state.argument);
    
    if ( INVALID_SOUND_ID == sound )
    {
        // Stop existing sound loop (if any)
        AudioSystem::get().stopObjectLoopingSounds(self.getSelf());
    }
    else
    {
        // check whatever might be playing on the channel now
        //ZF> TODO: check if character is already playing a looped sound first!
        AudioSystem::get().playSoundLooped(sound, self.getSelf());
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_StopSound( script_state_t& state, ai_state_t& self )
{
    // StopSound( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function stops the playing of a continuous sound!

    SCRIPT_FUNCTION_BEGIN();

    AudioSystem::get().stopObjectLoopingSounds(self.getSelf(), ppro->getSoundID(state.argument));

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if ( objectHandler().exists( self.child ) )
    {
        Object * pchild = objectHandler().get( self.child );

        ModelAction action = pchild->getProfile()->getModel()->getAction(state.argument);

        if ( rv_success == pchild->inst.startAnimation(action, false, true) )
        {
            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ShowTimer( script_state_t& state, ai_state_t& self )
{
    // ShowTimer( tmpargument = "time" )
    /// @author ZZ
    /// @details This function sets the value displayed by the module timer.
    /// For races and such.  50 clicks per second

    SCRIPT_FUNCTION_BEGIN();

    timeron = true;
    timervalue = state.argument;

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlaySoundVolume( script_state_t& state, ai_state_t& self )
{
    // PlaySoundVolume( argument = "sound", distance = "volume" )
    /// @author ZZ
    /// @details This function sets the volume of a sound and plays it

    SCRIPT_FUNCTION_BEGIN();

    if ( state.distance > 0 )
    {
        int channel = AudioSystem::get().playSound(pchr->getOldPosition(), ppro->getSoundID(Ego::Script::Interpreter::safeCast<int>(state.argument)));

        if ( channel != INVALID_SOUND_CHANNEL )
        {
            Mix_Volume( channel, ( 128 * Ego::Script::Interpreter::safeCast<int>(state.distance) ) / 100 );
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeSimilarNamesKnown( script_state_t& state, ai_state_t& self )
{
    // MakeSimilarNamesKnown()
    /// @author ZZ
    /// @details This function makes the names of similar objects known.
    /// Checks all 6 IDSZ types to make sure they match.

    int tTmp;
    uint16_t sTmp = 0;

    SCRIPT_FUNCTION_BEGIN();

    for(const std::shared_ptr<Object> &object : objectHandler().iterator())
    {

        sTmp = true;
        for ( tTmp = 0; tTmp < IDSZ_COUNT; tTmp++ )
        {
            if ( ppro->getIDSZ(tTmp) != object->getProfile()->getIDSZ(tTmp) )
            {
                sTmp = false;
            }
        }

        if ( sTmp )
        {
            object->setNameKnown(true);
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CorrectActionForHand( script_state_t& state, ai_state_t& self )
{
    // CorrectActionForHand( tmpargument = "action" )
    /// @author ZZ
    /// @details This function changes tmpargument according to which hand the character
    /// is held in It turns ZA into ZA, ZB, ZC, or ZD.
    /// USAGE:  wizards casting spells

    SCRIPT_FUNCTION_BEGIN();
    if ( objectHandler().exists( pchr->attachedto ) )
    {
        if ( pchr->inwhich_slot == SLOT_LEFT )
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

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SparkleIcon( script_state_t& state, ai_state_t& self )
{
    // SparkleIcon( tmpargument = "color" )
    /// @author ZZ
    /// @details This function starts little sparklies going around the character's icon

    SCRIPT_FUNCTION_BEGIN();
    if ( state.argument < COLOR_MAX )
    {
        if ( state.argument < -1 )
        {
            pchr->setSparkle(NOSPARKLE);
        }
        else
        {
            pchr->setSparkle(state.argument % COLOR_MAX);
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_UnsparkleIcon( script_state_t& state, ai_state_t& self )
{
    // UnsparkleIcon()
    /// @author ZZ
    /// @details This function stops little sparklies going around the character's icon

    SCRIPT_FUNCTION_BEGIN();

    pchr->setSparkle(NOSPARKLE);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlayFullSound( script_state_t& state, ai_state_t& self )
{
    // PlayFullSound( tmpargument = "sound", tmpdistance = "frequency" )
    /// @author ZZ
    /// @details This function plays one of the character's sounds .
    /// The sound will be heard at full volume by all players (Victory music)

    SCRIPT_FUNCTION_BEGIN();

    AudioSystem::get().playSoundFull(ppro->getSoundID(state.argument));

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if ( objectHandler().exists( self.getTarget() ) )
    {
        Object * pself_target = objectHandler().get( self.getTarget() );

        ModelAction action = pself_target->getProfile()->getModel()->getAction(state.argument );

        if ( rv_success == pself_target->inst.startAnimation(action, false, true) )
        {
            // remove the interpolation
            pself_target->inst.removeInterpolation();

            returncode = true;
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClearMusicPassage( script_state_t& state, ai_state_t& self )
{
    // ClearMusicPassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This clears the music for a specified passage

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
    if(passage) {
        passage->setMusic(Passage::NO_MUSIC);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PlayMusic( script_state_t& state, ai_state_t& self )
{
    // PlayMusic( tmpargument = "song number", tmpdistance = "fade time (msec)" )
    /// @author ZZ
    /// @details This function begins playing a new track of music

    SCRIPT_FUNCTION_BEGIN();

    int fadeTime = state.distance;
    if(fadeTime < 0) fadeTime = 0;

    AudioSystem::get().playMusic(state.argument, fadeTime);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMusicPassage( script_state_t& state, ai_state_t& self )
{
    // SetMusicPassage( tmpargument = "passage", tmpturn = "type", tmpdistance = "repetitions" )

    /// @author ZZ
    /// @details This function makes the given passage play music if a player enters it
    /// tmpargument is the passage to set and tmpdistance is the music track to play.

    SCRIPT_FUNCTION_BEGIN();

    std::shared_ptr<Passage> passage = activeModule().getPassageByID(state.argument);
    if(passage) {
        passage->setMusic(state.distance);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_StopMusic( script_state_t& state, ai_state_t& self )
{
    // StopMusic()
    /// @author ZZ
    /// @details This function stops the interactive music

    SCRIPT_FUNCTION_BEGIN();

    AudioSystem::get().stopMusic();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetVolumeNearestTeammate( script_state_t& state, ai_state_t& self )
{
    // SetVolumeNearestTeammate( tmpargument = "sound", tmpdistance = "distance" )
    /// @author ZZ
    /// @details This function lets insects buzz correctly.  The closest Team member
    /// is used to determine the overall sound level.

    SCRIPT_FUNCTION_BEGIN();

    //ZF> TODO: Not implemented

    /*PORT
    if(moduleactive && state.distance >= 0)
    {
    // Find the closest Teammate
    iTmp = 10000;
    sTmp = 0;
    while(sTmp < OBJECTS_MAX)
    {
    if(objectHandler().exists(sTmp) && ChrList.lst[sTmp].alive && ChrList.lst[sTmp].Team == pchr->Team)
    {
    distance = ABS(PCamera->track.x-ChrList.lst[sTmp].getOldPosition().x)+ABS(PCamera->track.y-ChrList.lst[sTmp].getOldPosition().y);
    if(distance < iTmp)  iTmp = distance;
    }
    sTmp++;
    }
    distance=iTmp+state.distance;
    volume = -distance;
    volume = volume<<VOLSHIFT;
    if(volume < VOLMIN) volume = VOLMIN;
    iTmp = CapStack.lst[pro_get_icap(pchr->getProfileID())].wavelist[pstate->argument];
    if(iTmp < numsound && iTmp >= 0 && soundon)
    {
    lpDSBuffer[iTmp]->SetVolume(volume);
    }
    }
    */

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeNameUnknown( script_state_t& state, ai_state_t& self )
{
    // MakeNameUnknown()
    /// @author ZZ
    /// @details This function makes the name of an item/character unknown.
    /// Usage: Use if you have subspawning of creatures from a book.

    SCRIPT_FUNCTION_BEGIN();

    pchr->setNameKnown(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TakePicture( script_state_t& state, ai_state_t& self )
{
    // TakePicture()
    /// @author ZF
    /// @details This function proceeds only if the screenshot was successful

    SCRIPT_FUNCTION_BEGIN();

    returncode = engine().getUIManager()->dumpScreenshot();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets all of the RTS speech registers to tmpargument

    SCRIPT_FUNCTION_BEGIN();

    //ZF> no longer supported
#if 0
    uint16_t sTmp = 0;
    for ( sTmp = SPEECH_BEGIN; sTmp <= SPEECH_END; sTmp++ )
    {
        pchr->sound_index[sTmp] = state.argument;
    }
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetMoveSpeech( script_state_t& state, ai_state_t& self )
{
    // SetMoveSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS move speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();

    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_MOVE] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSecondMoveSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSecondMoveSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS movealt speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_MOVEALT] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAttackSpeech( script_state_t& state, ai_state_t& self )
{
    // SetAttacksSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS attack speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_ATTACK] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetAssistSpeech( script_state_t& state, ai_state_t& self )
{
    // SetAssistSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS assist speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_ASSIST] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTerrainSpeech( script_state_t& state, ai_state_t& self )
{
    // SetTerrainSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS terrain speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_TERRAIN] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetSelectSpeech( script_state_t& state, ai_state_t& self )
{
    // SetSelectSpeech( tmpargument = "sound" )
    /// @author ZZ
    /// @details This function sets the RTS select speech register to tmpargument

    SCRIPT_FUNCTION_BEGIN();
    //ZF> no longer supported
#if 0
    pchr->sound_index[SPEECH_SELECT] = state.argument;
#endif

    SCRIPT_FUNCTION_END();
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

    SCRIPT_FUNCTION_BEGIN();

    if ( !ppro->isValidMessageID(state.argument) ) return false;

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

    returncode = NULL != GFX::get().getBillboardSystem().makeBillboard(self.getSelf(), ppro->getMessage(state.argument).c_str(), text_color, *tint, state.distance, Ego::Graphics::Billboard::Flags::Fade);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisplayCharge(script_state_t& state, ai_state_t& self)
{
    // DisplayCharge( tmpargument = "progress", tmpdistance = "max progress", tmpturn = "pip width" )
    /// @author ZF
    /// @details Draws a special progress bar this update frame

    SCRIPT_FUNCTION_BEGIN();

    //We ourselves must be a player or our holder must be one
    std::shared_ptr<Object> object = objectHandler()[pchr->getObjRef()];
    if(!object->isPlayer() && object->isBeingHeld()) {
        object = objectHandler()[pchr->attachedto];
    }

    //Only do this for players
    if(!object->isPlayer()) {
        returncode = false;
    }

    //Validate arguments
    else if(state.distance <= 0 || state.argument < 0)  {
        returncode = false;
    }

    //Render it!
    else {        
        returncode = true;

        const std::shared_ptr<Ego::Player>& player = activeModule().getPlayer(object->getPlayerNumber());
        player->setChargeBar(state.argument, state.distance, state.turn);
    }

    SCRIPT_FUNCTION_END();
}
