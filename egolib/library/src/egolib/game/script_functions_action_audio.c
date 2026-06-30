/// @file egolib/game/script_functions_action_audio.c
/// @brief Audio dispatch entries — sound effects + music control. Split off
///        script_functions_action.c on 2026-06-12 (10 entries, ~250 lines).
/// @details Shared infrastructure (SelfActionContext / makeSelfActionContext / gameSession)
///          lives in script_functions_action_internal.h. Audio-only helpers (audioSystem)
///          live in this TU's anonymous namespace.

#include "egolib/game/script_functions_action_internal.h"
#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/Audio/IAudioSystem.hpp"

namespace
{
IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}
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
