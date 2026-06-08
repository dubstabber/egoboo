#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "egolib/integrations/math.hpp"
#include "egolib/typedef.h"

typedef int MusicID;
typedef int SoundID;

/// Pre defined global particle sounds
enum GlobalSound : uint8_t
{
    GSND_COINGET,       //Coin grabbed
    GSND_DEFEND,        //Attack deflected clink
    GSND_SPLISH,        //Raindrop
    GSND_SPLOSH,        //Hit water
    GSND_COINFALL,      //Coin hits ground
    GSND_LEVELUP,       //Character gains level
    GSND_PITFALL,       //Character falls down a pit
    GSND_SHIELDBLOCK,   //Shield block sound
    GSND_BUTTON_CLICK,  //GUI button clicked
    GSND_GAME_READY,    //Finished loading module
    GSND_PERK_SELECT,   //Selected new perk
    GSND_GUI_HOVER,     //Mouse over sound effect
    GSND_DODGE,         //Dodged attack
    GSND_CRITICAL_HIT,  //Critical Hit
    GSND_DISINTEGRATE,  //Disintegrated
    GSND_DRUMS,         //Used for "Too Silly to Die" perk
    GSND_ANGEL_CHOIR,   //Angel Choir
    GSND_STEALTH,       //Enter stealth
    GSND_STEALTH_END,   //Exit stealth
    GSND_COUNT
};

class IAudioSystem
{
public:
    virtual ~IAudioSystem() = default;

    virtual SoundID loadSound(const std::string& fileName) = 0;
    virtual void playMusic(MusicID musicID, uint16_t fadetime = 0) = 0;
    virtual void playMusic(const std::string& songName, uint16_t fadetime = 0) = 0;
    virtual void stopMusic() = 0;
    virtual void fadeAllSounds() = 0;
    virtual int playSound(const Ego::Vector3f& position, SoundID soundID) = 0;
    virtual void playSoundLooped(SoundID soundID, ObjectRef ownerRef) = 0;
    virtual size_t stopObjectLoopingSounds(ObjectRef ownerRef, SoundID soundID = -1) = 0;
    virtual int playSoundFull(SoundID soundID) = 0;
    virtual SoundID getGlobalSound(GlobalSound id) const = 0;
    virtual void setMaxHearingDistance(float distance) = 0;
    virtual void setMusicVolume(int value) = 0;
    virtual void setSoundEffectVolume(int value) = 0;
    virtual void update() = 0;
};

/// @brief Install the active audio system (the audio system the engine context publishes).
/// @param audioSystem the audio system to install
/// @throw std::logic_error if an audio system is already installed
/// @remark Subsystem-owned ownership for the installed audio-system pointer (mirrors the Log
///         active-target ownership move); EngineContext delegates its audio-system lifecycle here.
void installActiveAudioSystem(IAudioSystem& audioSystem);

/// @brief Clear the installed active audio system.
void clearActiveAudioSystem();

/// @brief The installed active audio system, or @a nullptr if none is installed.
IAudioSystem* tryActiveAudioSystem();

/// @brief The active audio system.
/// @throw std::logic_error if none is installed
/// @remark Lower-layer seam mirroring @c Log::activeTarget(): returns the INSTALLED audio system (which may
///         be a test stub), preserving the swappable-install indirection the audio tests assert on, and
///         throws when none is installed (no lenient singleton fallback) to match the engine-context accessor.
IAudioSystem& activeAudioSystem();
