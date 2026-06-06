//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************
// @Author Johan Jansen
//

#pragma once

#include <SDL_mixer.h>
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/Math/_Include.hpp"

static constexpr int INVALID_SOUND_CHANNEL = -1;
static constexpr SoundID INVALID_SOUND_ID = -1;

/// Data needed to store and manipulate a looped sound
class LoopingSound
{
public:
    LoopingSound(ObjectRef ownerRef, SoundID soundID) :
        _channel(INVALID_SOUND_CHANNEL),
        _ownerRef(ownerRef),
        _soundID(soundID)
    {
        /* Nothing to do. */
    }

    LoopingSound(const LoopingSound&) = delete;
    LoopingSound& operator=(const LoopingSound&) = delete;

    inline int getChannel() const
    {
        return _channel;
    }
    
    inline const ObjectRef& getOwnerRef() const
    {
        return _ownerRef; // Immutable object references can be returned by constant reference.
    }
    
    inline void setChannel(int channel)
    {
        _channel = channel;
    }

    inline SoundID getSoundID() const
    {
        return _soundID;
    }


private:
    int _channel;
    const ObjectRef _ownerRef;
    const SoundID _soundID;
};


class AudioSystem;

struct AudioSystemCreateFunctor
{
	AudioSystem *operator()() const;
};

struct AudioSystemDestroyFunctor
{
	void operator()(AudioSystem *p) const;
};

class AudioSystem : public IAudioSystem,
                    public idlib::singleton<AudioSystem, AudioSystemCreateFunctor, AudioSystemDestroyFunctor>
{
public:
    static constexpr int MIX_HIGH_QUALITY = 44100;

	// TODO: re-add constexpr when we drop VS 2013 support
	// Workaround for compilers without constexpr support (VS 2013);
	// only integral types can be initialized in-class for a const static member.
	static const float DEFAULT_MAX_DISTANCE;    ///< Default max hearing distance (10 tiles)

protected:
    friend AudioSystemCreateFunctor;
    friend AudioSystemDestroyFunctor;
    AudioSystem();
    virtual ~AudioSystem();

public:
    void loadGlobalSounds();

    /**
     * @brief
     *  Stop music.
     */
    void stopMusic() override;

    /**
     * @brief
     *  Resume music.
     */
    void resumeMusic();

    /**
     * Stop all sounds that are playing over time.
     */
    void fadeAllSounds() override;

    /**
    * @author ZF
    * @details This functions plays a specified track loaded into memory
    **/
    void playMusic(const MusicID musicID, const uint16_t fadetime = 0) override;

    /**
    * @author ZF
    * @details This functions plays a specified track loaded into memory
    **/
    void playMusic(const std::string& songName, const uint16_t fadetime = 0) override;

    SoundID loadSound(const std::string &fileName) override;

    /// @author ZF
    /// @details This function loads all of the music sounds
    void loadAllMusic();

    /**
     * @brief
     *  Updates all looping sounds (called preiodically by the game engine).
     */
    void updateLoopingSounds();

    void update() override;

    /**
     * @brief
	 *  Stop looping of sounds of an specified owner.
	 * @param ownerRef
	 *  the owner
	 * @param soundID
	 *  the sound ID of the sound to stop. If INVALID_SOUND_ID is passed, then all sounds of the owner are stopped
	 * @return
	 *  the number of sounds stopped by the call to this method
     */
    size_t stopObjectLoopingSounds(ObjectRef ownerRef, const SoundID soundID = INVALID_SOUND_ID) override;

    /**
     * @todo
     *  MH: "reconfigure" should be offered by all sub-systems, it is a useful concept.
     * @param cfg
     *  the configuration
     * @remark
     *  The audio system will adjust its settings to the specified configuration.
     *  and update the given configuration with information about its new settings.
     */
    void download(egoboo_config_t &cfg);
    void upload(egoboo_config_t& cfg);

	/**
	 * @brief
	 *  Play a sound at the given position.
	 * @param position
	 *  the position
	 * @param soundID
	 *  the sound ID
	 * @return
	 *  the channel the sound is played over
	 */
    int playSound(const Ego::Vector3f& position, const SoundID soundID) override;

    /**
     * @brief
     *  Loop the specified sound until it is explicitly stopped.
	 * @param soundID
	 *  the sound ID
	 * @param ownerRef
	 *  the reference of the object the sound is owned by
     */
    void playSoundLooped(const SoundID soundID, ObjectRef ownerRef) override;

    /// @author ZF
    /// @details This function plays a specified sound at full possible volume and returns which channel it's using
    int playSoundFull(SoundID soundID) override;

    inline SoundID getGlobalSound(GlobalSound id) const override
    {
        return _globalSounds[id];
    }

    /**
    * @brief
    *   sets the maximum distance from where sounds will be played.
    *   Sounds played far away will have lower volume than those nearby
    * @param distance
    *    distance to hear the sound. Normally a multiple of GRID_FSIZE
    *    e.g GRID_FSIZE*5 means 5 tiles.
    * @remark
    *   Default value is a the DEFAULT_MAX_DISTANCE constant found in this class
    **/
    void setMaxHearingDistance(const float distance) override;

    /**
    * @brief
    *   Sets how loud music should be played
    * @param value
    *   between 0 (none) and 128 (max volume)
    **/
    void setMusicVolume(int value) override;

    /**
    * @brief
    *   Sets how all current and future sound effects will be played
    * @param value
    *   between 0 (none) and 128 (max volume)
    **/
    void setSoundEffectVolume(int value) override;

private:
    /**
    * @brief Loads one music track. Returns nullptr if it fails
    **/
    MusicID loadMusic(const std::string &fileName);

    /**
     * @brief applies 3D spatial effect to the specified sound (using volume and panning)
     * @param channel
     *  the channel
     * @param distance
     *  the distance between the sound origin and the listener
     * @param soundPosition
     *  the sound origin
     */
    void mixAudioPosition3D(const int channel, float distance, const Ego::Vector3f& soundPosition);

    /**
     * @brief
     *  Calculates distance between the sound origin and the players.
     * @param soundPosition
     *  the sound origin
     * @return
     *  the distance between the sound origin and the players
     */
    float getSoundDistance(const Ego::Vector3f& soundPosition);

    /**
     * @brief
     *  Updates one looping sound effect.
     * @param sound
     *  the looping sound effect
     */
    void updateLoopingSound(const std::shared_ptr<LoopingSound>& sound);

private:
    std::unordered_map<std::string, Mix_Music*> _musicLoaded;    //Maps song names to music data
    std::unordered_map<MusicID, std::string> _musicIDToNameMap;   //Maps MusicID to song names
    std::vector<Mix_Chunk*> _soundsLoaded;
    std::array<SoundID, GSND_COUNT> _globalSounds;

    std::forward_list<std::shared_ptr<LoopingSound>> _loopingSounds;
    std::string _currentSongPlaying;
    float _maxSoundDistance;                                            ///< How far away can we hear sound effects?
};
