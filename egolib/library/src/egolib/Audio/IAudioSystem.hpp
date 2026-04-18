#pragma once

#include <cstdint>
#include <string>

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

    virtual void playMusic(MusicID musicID, uint16_t fadetime = 0) = 0;
    virtual void playMusic(const std::string& songName, uint16_t fadetime = 0) = 0;
    virtual void stopMusic() = 0;
    virtual void fadeAllSounds() = 0;
    virtual int playSoundFull(SoundID soundID) = 0;
    virtual SoundID getGlobalSound(GlobalSound id) const = 0;
    virtual void setMusicVolume(int value) = 0;
    virtual void setSoundEffectVolume(int value) = 0;
};
