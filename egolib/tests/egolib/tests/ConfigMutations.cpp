#include "gtest/gtest.h"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Graphics/SDL/Utilities.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GameStates/OptionsConfigActions.hpp"

#include <vector>

namespace
{
namespace Actions = Ego::GameStates::Internal::OptionsConfigActions;

class StubAudioSystem : public IAudioSystem
{
public:
    void playMusic(MusicID, uint16_t = 0) override {}
    void playMusic(const std::string&, uint16_t = 0) override {}
    void stopMusic() override {}
    void fadeAllSounds() override {}
    int playSoundFull(SoundID soundID) override
    {
        playedSounds.push_back(soundID);
        return 0;
    }
    SoundID getGlobalSound(GlobalSound id) const override
    {
        return 1000 + static_cast<SoundID>(id);
    }
    void setMusicVolume(int value) override
    {
        musicVolume = value;
    }
    void setSoundEffectVolume(int value) override
    {
        soundEffectVolume = value;
    }

    int musicVolume = -1;
    int soundEffectVolume = -1;
    std::vector<SoundID> playedSounds;
};

class InstalledConfigMutationFixture : public ::testing::Test
{
protected:
    egoboo_config_t config;
    StubAudioSystem audioSystem;

    void SetUp() override
    {
        auto& context = EngineContext::get();
        context.clearAudioSystem();
        if (context.tryConfig())
        {
            context.clearConfig();
        }

        context.installConfig(config);
        context.installAudioSystem(audioSystem);
    }

    void TearDown() override
    {
        auto& context = EngineContext::get();
        context.clearAudioSystem();
        if (context.tryConfig())
        {
            context.clearConfig();
        }
    }
};

TEST_F(InstalledConfigMutationFixture, MusicVolumeUsesInstalledConfigAndAudioSystem)
{
    Actions::applyMusicVolume(77);
    EXPECT_EQ(Actions::musicVolume(), 77);
    EXPECT_EQ(config.sound_music_volume.getValue(), 77);
    EXPECT_TRUE(config.sound_music_enable.getValue());
    EXPECT_EQ(audioSystem.musicVolume, 77);

    Actions::applyMusicVolume(0);
    EXPECT_EQ(config.sound_music_volume.getValue(), 0);
    EXPECT_FALSE(config.sound_music_enable.getValue());
    EXPECT_EQ(audioSystem.musicVolume, 0);
}

TEST_F(InstalledConfigMutationFixture, SoundEffectVolumeUsesInstalledConfigAndAudioSystem)
{
    Actions::applySoundEffectVolume(55);
    EXPECT_EQ(Actions::soundEffectVolume(), 55);
    EXPECT_EQ(config.sound_effects_volume.getValue(), 55);
    EXPECT_TRUE(config.sound_effects_enable.getValue());
    EXPECT_EQ(audioSystem.soundEffectVolume, 55);
    ASSERT_EQ(audioSystem.playedSounds.size(), 1u);
    EXPECT_EQ(audioSystem.playedSounds.front(), audioSystem.getGlobalSound(GSND_BUTTON_CLICK));

    Actions::applySoundEffectVolume(0);
    EXPECT_EQ(config.sound_effects_volume.getValue(), 0);
    EXPECT_FALSE(config.sound_effects_enable.getValue());
}

TEST_F(InstalledConfigMutationFixture, SoundChannelCountUsesInstalledConfigAndAllocatorCallback)
{
    int allocatedChannels = -1;

    Actions::applySoundChannelCount(24, [&](int count)
    {
        allocatedChannels = count;
    });

    EXPECT_EQ(Actions::soundChannelCount(), 24);
    EXPECT_EQ(config.sound_channel_count.getValue(), 24);
    EXPECT_EQ(allocatedChannels, 24);
}

TEST_F(InstalledConfigMutationFixture, FootstepToggleAndSaveUseInstalledConfig)
{
    config.sound_footfallEffects_enable.setValue(false);
    EXPECT_EQ(Actions::footstepEffectsLabel(), "No");

    Actions::toggleFootstepEffects();
    EXPECT_TRUE(config.sound_footfallEffects_enable.getValue());
    EXPECT_EQ(Actions::footstepEffectsLabel(), "Yes");

    egoboo_config_t* uploadedConfig = nullptr;
    Actions::saveConfig([&](egoboo_config_t& currentConfig)
    {
        uploadedConfig = &currentConfig;
    });

    EXPECT_EQ(uploadedConfig, &config);
}

TEST_F(InstalledConfigMutationFixture, FullscreenToggleUsesInstalledConfigAndCallback)
{
    bool fullscreenState = false;
    config.graphic_fullscreen.setValue(false);

    Actions::toggleFullscreen([&](bool enabled)
    {
        fullscreenState = enabled;
    });

    EXPECT_TRUE(config.graphic_fullscreen.getValue());
    EXPECT_TRUE(fullscreenState);
    EXPECT_EQ(Actions::fullscreenLabel(), "Enabled");
}

TEST_F(InstalledConfigMutationFixture, ShadowsCycleThroughOffLowHighAndBackOff)
{
    config.graphic_shadows_enable.setValue(false);
    config.graphic_shadows_highQuality_enable.setValue(false);

    EXPECT_EQ(Actions::shadowsLabel(), "Off");

    Actions::cycleShadows();
    EXPECT_TRUE(config.graphic_shadows_enable.getValue());
    EXPECT_FALSE(config.graphic_shadows_highQuality_enable.getValue());
    EXPECT_EQ(Actions::shadowsLabel(), "Low");

    Actions::cycleShadows();
    EXPECT_TRUE(config.graphic_shadows_enable.getValue());
    EXPECT_TRUE(config.graphic_shadows_highQuality_enable.getValue());
    EXPECT_EQ(Actions::shadowsLabel(), "High");

    Actions::cycleShadows();
    EXPECT_FALSE(config.graphic_shadows_enable.getValue());
    EXPECT_FALSE(config.graphic_shadows_highQuality_enable.getValue());
    EXPECT_EQ(Actions::shadowsLabel(), "Off");
}

TEST_F(InstalledConfigMutationFixture, TextureQualityCyclesLowMediumHighAndBackLow)
{
    config.graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::nearest);
    config.graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::nearest);
    config.graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::none);

    EXPECT_EQ(Actions::textureQualityLabel(), "Low");

    Actions::cycleTextureQuality();
    EXPECT_EQ(Actions::textureQualityLabel(), "Medium");
    EXPECT_EQ(config.graphic_textureFilter_minFilter.getValue(), idlib::texture_filter_method::linear);
    EXPECT_EQ(config.graphic_textureFilter_mipMapFilter.getValue(), idlib::texture_filter_method::none);

    Actions::cycleTextureQuality();
    EXPECT_EQ(Actions::textureQualityLabel(), "High");
    EXPECT_EQ(config.graphic_textureFilter_mipMapFilter.getValue(), idlib::texture_filter_method::linear);

    Actions::cycleTextureQuality();
    EXPECT_EQ(Actions::textureQualityLabel(), "Low");
    EXPECT_EQ(config.graphic_textureFilter_minFilter.getValue(), idlib::texture_filter_method::nearest);
}

TEST_F(InstalledConfigMutationFixture, AnisotropyCyclesAndRollsOverToDisabled)
{
    config.graphic_anisotropy_enable.setValue(false);
    config.graphic_anisotropy_levels.setValue(1.0f);

    EXPECT_TRUE(Actions::anisotropySupported());
    EXPECT_EQ(Actions::anisotropyLabel(), "Disabled");

    Actions::cycleAnisotropy();
    EXPECT_TRUE(config.graphic_anisotropy_enable.getValue());
    EXPECT_FLOAT_EQ(config.graphic_anisotropy_levels.getValue(), 1.0f);
    EXPECT_EQ(Actions::anisotropyLabel(), "x1");

    config.graphic_anisotropy_enable.setValue(true);
    config.graphic_anisotropy_levels.setValue(16.0f);
    Actions::cycleAnisotropy();
    EXPECT_FALSE(config.graphic_anisotropy_enable.getValue());
    EXPECT_FLOAT_EQ(config.graphic_anisotropy_levels.getValue(), 0.0f);
    EXPECT_EQ(Actions::anisotropyLabel(), "Disabled");
}

TEST_F(InstalledConfigMutationFixture, AntiAliasingAndHdTexturesToggleThroughInstalledConfig)
{
    config.graphic_antialiasing.setValue(0);
    EXPECT_EQ(Actions::antiAliasingLabel(), "Disabled");
    Actions::toggleAntiAliasing();
    EXPECT_EQ(config.graphic_antialiasing.getValue(), 1);
    EXPECT_EQ(Actions::antiAliasingLabel(), "Enabled");

    config.graphic_hd_textures_enable.setValue(false);
    EXPECT_EQ(Actions::hdTexturesLabel(), "Disabled");
    Actions::toggleHDTextures();
    EXPECT_TRUE(config.graphic_hd_textures_enable.getValue());
    EXPECT_EQ(Actions::hdTexturesLabel(), "Enabled");
}

TEST_F(InstalledConfigMutationFixture, ResolutionSelectionUsesInstalledConfig)
{
    Actions::selectResolution(1280, 720);

    EXPECT_EQ(config.graphic_resolution_horizontal.getValue(), 1280);
    EXPECT_EQ(config.graphic_resolution_vertical.getValue(), 720);
    EXPECT_TRUE(Actions::isResolutionSelected(1280, 720));
    EXPECT_FALSE(Actions::isResolutionSelected(800, 600));
}

TEST_F(InstalledConfigMutationFixture, AliasingRequirementUsesInstalledConfig)
{
    config.graphic_antialiasing.setValue(2);
    AliasingRequirement requirement;

    config.graphic_antialiasing.setValue(4);
    requirement.reset();
    EXPECT_EQ(config.graphic_antialiasing.getValue(), 2);

    EXPECT_TRUE(requirement.relax());
    EXPECT_EQ(config.graphic_antialiasing.getValue(), 1);
    EXPECT_TRUE(requirement.relax());
    EXPECT_EQ(config.graphic_antialiasing.getValue(), 0);
    EXPECT_FALSE(requirement.relax());
}

TEST_F(InstalledConfigMutationFixture, FullscreenRequirementUsesInstalledConfig)
{
    config.graphic_fullscreen.setValue(true);
    FullscreenRequirement requirement;

    config.graphic_fullscreen.setValue(false);
    requirement.reset();
    EXPECT_TRUE(config.graphic_fullscreen.getValue());

    EXPECT_TRUE(requirement.relax());
    EXPECT_FALSE(config.graphic_fullscreen.getValue());
    EXPECT_FALSE(requirement.relax());
}

} // namespace
