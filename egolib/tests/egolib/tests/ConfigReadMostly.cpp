#include "gtest/gtest.h"

#define private public
#include "egolib/game/GUI/MessageLog.hpp"
#undef private
#include "egolib/Renderer/RendererInfo.hpp"
#include "egolib/Script/script.h"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Module/Fog.hpp"
#include "egolib/game/Module/Water.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/script_variables.h"

#include <string>

namespace
{

class StubRendererInfo : public Ego::RendererInfo
{
public:
    std::string getRenderer() const override { return "stub"; }
    std::string getVendor() const override { return "stub"; }
    std::string getVersion() const override { return "stub"; }
    bool isAnisotropySupported() const noexcept override { return true; }
    float getMinimumSupportedAnisotropy() const noexcept override { return 1.0f; }
    float getMaximumSupportedAnisotropy() const noexcept override { return 16.0f; }
    int getMaximumTextureSize() const noexcept override { return 1024; }
    std::string toString() const override { return "stub"; }
};

class InstalledConfigFixture : public ::testing::Test
{
protected:
    egoboo_config_t config;

    void SetUp() override
    {
        auto& context = EngineContext::get();
        if (context.tryConfig())
        {
            context.clearConfig();
        }
    }

    void TearDown() override
    {
        auto& context = EngineContext::get();
        if (context.tryConfig())
        {
            context.clearConfig();
        }
    }

    void installConfig()
    {
        EngineContext::get().installConfig(config);
    }
};

class ScopedGouraudShadingOverride
{
public:
    explicit ScopedGouraudShadingOverride(bool enabled) :
        _previousValue(gfx.gouraudShading_enable)
    {
        gfx.gouraudShading_enable = enabled;
    }

    ~ScopedGouraudShadingOverride()
    {
        gfx.gouraudShading_enable = _previousValue;
    }

private:
    bool _previousValue;
};

TEST_F(InstalledConfigFixture, RendererInfoReadsInstalledConfigOnConstruction)
{
    config.graphic_anisotropy_enable.setValue(true);
    config.graphic_anisotropy_levels.setValue(8.0f);
    config.graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::nearest);
    config.graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::linear);
    config.graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::none);
    installConfig();

    StubRendererInfo rendererInfo;

    EXPECT_TRUE(rendererInfo.isAnisotropyDesired());
    EXPECT_FLOAT_EQ(rendererInfo.getDesiredAnisotropy(), 8.0f);
    EXPECT_EQ(rendererInfo.getDesiredMinimizationFilter(), idlib::texture_filter_method::nearest);
    EXPECT_EQ(rendererInfo.getDesiredMaximizationFilter(), idlib::texture_filter_method::linear);
    EXPECT_EQ(rendererInfo.getDesiredMipMapFilter(), idlib::texture_filter_method::none);
}

TEST_F(InstalledConfigFixture, RendererInfoTracksInstalledConfigValueChanges)
{
    installConfig();
    StubRendererInfo rendererInfo;

    int anisotropyDesiredChanges = 0;
    int anisotropyLevelChanges = 0;
    int minFilterChanges = 0;
    int maxFilterChanges = 0;
    int mipMapFilterChanges = 0;

    auto anisotropyDesiredConnection = rendererInfo.AnisotropyDesiredChanged.subscribe([&]()
    {
        ++anisotropyDesiredChanges;
    });
    auto anisotropyLevelConnection = rendererInfo.DesiredAnisotropyChanged.subscribe([&]()
    {
        ++anisotropyLevelChanges;
    });
    auto minFilterConnection = rendererInfo.DesiredMinimizationFilterChanged.subscribe([&]()
    {
        ++minFilterChanges;
    });
    auto maxFilterConnection = rendererInfo.DesiredMaximizationFilterChanged.subscribe([&]()
    {
        ++maxFilterChanges;
    });
    auto mipMapFilterConnection = rendererInfo.DesiredMipMapFilterChanged.subscribe([&]()
    {
        ++mipMapFilterChanges;
    });

    config.graphic_anisotropy_enable.setValue(true);
    config.graphic_anisotropy_levels.setValue(4.0f);
    config.graphic_textureFilter_minFilter.setValue(idlib::texture_filter_method::nearest);
    config.graphic_textureFilter_magFilter.setValue(idlib::texture_filter_method::nearest);
    config.graphic_textureFilter_mipMapFilter.setValue(idlib::texture_filter_method::none);

    EXPECT_TRUE(rendererInfo.isAnisotropyDesired());
    EXPECT_FLOAT_EQ(rendererInfo.getDesiredAnisotropy(), 4.0f);
    EXPECT_EQ(rendererInfo.getDesiredMinimizationFilter(), idlib::texture_filter_method::nearest);
    EXPECT_EQ(rendererInfo.getDesiredMaximizationFilter(), idlib::texture_filter_method::nearest);
    EXPECT_EQ(rendererInfo.getDesiredMipMapFilter(), idlib::texture_filter_method::none);
    EXPECT_EQ(anisotropyDesiredChanges, 1);
    EXPECT_EQ(anisotropyLevelChanges, 1);
    EXPECT_EQ(minFilterChanges, 1);
    EXPECT_EQ(maxFilterChanges, 1);
    EXPECT_EQ(mipMapFilterChanges, 1);

    anisotropyDesiredConnection.disconnect();
    anisotropyLevelConnection.disconnect();
    minFilterConnection.disconnect();
    maxFilterConnection.disconnect();
    mipMapFilterConnection.disconnect();
}

TEST_F(InstalledConfigFixture, FogUploadUsesInstalledConfigToggle)
{
    installConfig();

    fog_instance_t fog{};
    wawalite_fog_t source;
    source.found = true;
    source.top = 0.5f;
    source.bottom = 0.0f;

    config.graphic_fog_enable.setValue(false);
    fog.upload(source);
    EXPECT_FALSE(fog._on);

    config.graphic_fog_enable.setValue(true);
    fog.upload(source);
    EXPECT_TRUE(fog._on);
    EXPECT_FLOAT_EQ(fog._distance, 0.5f);
}

TEST_F(InstalledConfigFixture, WaterUploadAndLevelUseInstalledTwoLayerToggle)
{
    installConfig();

    ScopedGouraudShadingOverride disableGouraudShading(false);

    wawalite_water_t source;
    source.layer_count = 2;
    source.layer[0].z = 1.0f;
    source.layer[0].amp = 0.5f;
    source.layer[0].light_add = 10;
    source.layer[1].z = 5.0f;
    source.layer[1].amp = 1.0f;
    source.layer[1].light_add = 20;

    water_instance_t water{};

    config.graphic_twoLayerWater_enable.setValue(false);
    water.upload(source);
    EXPECT_EQ(water._layer_count, 1);
    EXPECT_FLOAT_EQ(water.get_level(), 1.5f);

    config.graphic_twoLayerWater_enable.setValue(true);
    water.upload(source);
    EXPECT_EQ(water._layer_count, 2);
    EXPECT_FLOAT_EQ(water.get_level(), 6.0f);
}

TEST_F(InstalledConfigFixture, MessageLogReadsInstalledConfigForDurationAndLimit)
{
    installConfig();
    Ego::GUI::MessageLog messageLog;

    config.hud_messageDuration.setValue(432);
    config.hud_simultaneousMessages_max.setValue(7);

    EXPECT_EQ(messageLog.messageDurationTicks(), 4320u);
    EXPECT_EQ(messageLog.messageLimit(), 7u);

    config.hud_messageDuration.setValue(125);
    config.hud_simultaneousMessages_max.setValue(3);

    EXPECT_EQ(messageLog.messageDurationTicks(), 1250u);
    EXPECT_EQ(messageLog.messageLimit(), 3u);
}

TEST_F(InstalledConfigFixture, ScriptDifficultyReadUsesInstalledConfig)
{
    installConfig();

    script_state_t scriptState{};
    ai_state_t aiState{};

    config.game_difficulty.setValue(Ego::GameDifficulty::Hard);
    EXPECT_EQ(load_VARDIFFICULTY(scriptState, aiState, nullptr, nullptr, nullptr, nullptr),
              static_cast<uint32_t>(Ego::GameDifficulty::Hard));

    config.game_difficulty.setValue(Ego::GameDifficulty::Easy);
    EXPECT_EQ(load_VARDIFFICULTY(scriptState, aiState, nullptr, nullptr, nullptr, nullptr),
              static_cast<uint32_t>(Ego::GameDifficulty::Easy));
}

} // namespace
