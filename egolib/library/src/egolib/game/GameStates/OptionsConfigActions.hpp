#pragma once

#include <functional>
#include <string>

struct egoboo_config_t;

namespace Ego::GameStates::Internal::OptionsConfigActions
{

int musicVolume();
void applyMusicVolume(int value);

int soundEffectVolume();
void applySoundEffectVolume(int value);

int soundChannelCount();
void applySoundChannelCount(int value, const std::function<void(int)>& channelAllocator);

std::string footstepEffectsLabel();
void toggleFootstepEffects();

std::string fullscreenLabel();
void toggleFullscreen(const std::function<void(bool)>& applyFullscreenMode);

std::string shadowsLabel();
void cycleShadows();

std::string textureQualityLabel();
void cycleTextureQuality();

std::string anisotropyLabel();
void cycleAnisotropy();
bool anisotropySupported();

std::string antiAliasingLabel();
void toggleAntiAliasing();

std::string hdTexturesLabel();
void toggleHDTextures();

void selectResolution(int width, int height);
bool isResolutionSelected(int width, int height);

void saveConfig(const std::function<void(egoboo_config_t&)>& uploader);

} // namespace Ego::GameStates::Internal::OptionsConfigActions
