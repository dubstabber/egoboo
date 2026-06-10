#include "egolib/game/GameStates/OptionsConfigActions.hpp"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/egoboo_setup.h"

namespace Ego::GameStates::Internal::OptionsConfigActions
{
namespace
{
egoboo_config_t& config()
{
    return Ego::activeConfig();
}
}

int musicVolume()
{
    return config().sound_music_volume.getValue();
}

void applyMusicVolume(int value)
{
    config().sound_music_volume.setValue(value);
    config().sound_music_enable.setValue(value > 0);
    activeAudioSystem().setMusicVolume(value);
}

int soundEffectVolume()
{
    return config().sound_effects_volume.getValue();
}

void applySoundEffectVolume(int value)
{
    config().sound_effects_volume.setValue(value);
    config().sound_effects_enable.setValue(value > 0);

    auto& audioSystem = activeAudioSystem();
    audioSystem.setSoundEffectVolume(value);
    audioSystem.playSoundFull(audioSystem.getGlobalSound(GSND_BUTTON_CLICK));
}

int soundChannelCount()
{
    return config().sound_channel_count.getValue();
}

void applySoundChannelCount(int value, const std::function<void(int)>& channelAllocator)
{
    config().sound_channel_count.setValue(value);
    channelAllocator(config().sound_channel_count.getValue());
}

std::string footstepEffectsLabel()
{
    return config().sound_footfallEffects_enable.getValue() ? "Yes" : "No";
}

void toggleFootstepEffects()
{
    config().sound_footfallEffects_enable.setValue(!config().sound_footfallEffects_enable.getValue());
}

std::string fullscreenLabel()
{
    return config().graphic_fullscreen.getValue() ? "Enabled" : "Disabled";
}

void toggleFullscreen(const std::function<void(bool)>& applyFullscreenMode)
{
    config().graphic_fullscreen.setValue(!config().graphic_fullscreen.getValue());
    applyFullscreenMode(config().graphic_fullscreen.getValue());
}

std::string shadowsLabel()
{
    if (!config().graphic_shadows_enable.getValue())
    {
        return "Off";
    }
    return config().graphic_shadows_highQuality_enable.getValue() ? "High" : "Low";
}

void cycleShadows()
{
    if (!config().graphic_shadows_enable.getValue())
    {
        config().graphic_shadows_enable.setValue(true);
        config().graphic_shadows_highQuality_enable.setValue(false);
    }
    else if (!config().graphic_shadows_highQuality_enable.getValue())
    {
        config().graphic_shadows_highQuality_enable.setValue(true);
    }
    else
    {
        config().graphic_shadows_enable.setValue(false);
        config().graphic_shadows_highQuality_enable.setValue(false);
    }
}

std::string textureQualityLabel()
{
    if (config().graphic_textureFilter_mipMapFilter.getValue() == idlib::texture_filter_method::linear)
    {
        return "High";
    }
    if (config().graphic_textureFilter_minFilter.getValue() == idlib::texture_filter_method::linear)
    {
        return "Medium";
    }
    if (config().graphic_textureFilter_minFilter.getValue() == idlib::texture_filter_method::nearest)
    {
        return "Low";
    }
    return "Unknown";
}

void cycleTextureQuality()
{
    if (config().graphic_textureFilter_minFilter.getValue() == idlib::texture_filter_method::nearest)
    {
        config().graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::linear);
        config().graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::linear);
        config().graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::none);
    }
    else if (config().graphic_textureFilter_mipMapFilter.getValue() == idlib::texture_filter_method::none)
    {
        config().graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::linear);
        config().graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::linear);
        config().graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::linear);
    }
    else
    {
        config().graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::nearest);
        config().graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::nearest);
        config().graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::none);
    }
}

std::string anisotropyLabel()
{
    if (!config().graphic_anisotropy_enable.getValue() || config().graphic_anisotropy_levels.getValue() <= 0)
    {
        return "Disabled";
    }
    return std::string("x") + std::to_string(static_cast<int>(config().graphic_anisotropy_levels.getValue()));
}

void cycleAnisotropy()
{
    if (!config().graphic_anisotropy_enable.getValue())
    {
        config().graphic_anisotropy_enable.setValue(true);
        config().graphic_anisotropy_levels.setValue(1.0f);
    }
    else
    {
        config().graphic_anisotropy_levels.setValue(
            static_cast<int>(config().graphic_anisotropy_levels.getValue()) << 1);

        if (config().graphic_anisotropy_levels.getValue() > config().graphic_anisotropy_levels.getMaxValue())
        {
            config().graphic_anisotropy_levels.setValue(0.0f);
            config().graphic_anisotropy_enable.setValue(false);
        }
    }
}

bool anisotropySupported()
{
    return config().graphic_anisotropy_levels.getMaxValue() > 0;
}

std::string antiAliasingLabel()
{
    return config().graphic_antialiasing.getValue() ? "Enabled" : "Disabled";
}

void toggleAntiAliasing()
{
    config().graphic_antialiasing.setValue(!config().graphic_antialiasing.getValue());
}

std::string hdTexturesLabel()
{
    return config().graphic_hd_textures_enable.getValue() ? "Enabled" : "Disabled";
}

void toggleHDTextures()
{
    config().graphic_hd_textures_enable.setValue(!config().graphic_hd_textures_enable.getValue());
}

void selectResolution(int width, int height)
{
    config().graphic_resolution_horizontal.setValue(width);
    config().graphic_resolution_vertical.setValue(height);
}

bool isResolutionSelected(int width, int height)
{
    return config().graphic_resolution_horizontal.getValue() == width &&
           config().graphic_resolution_vertical.getValue() == height;
}

void saveConfig(const std::function<void(egoboo_config_t&)>& uploader)
{
    uploader(config());
}

} // namespace Ego::GameStates::Internal::OptionsConfigActions
