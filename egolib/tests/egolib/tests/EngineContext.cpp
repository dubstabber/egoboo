#include "gtest/gtest.h"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"

#include <memory>
#include <stdexcept>

namespace
{

class StubAudioSystem : public IAudioSystem
{
public:
    void playMusic(MusicID, uint16_t = 0) override {}
    void playMusic(const std::string&, uint16_t = 0) override {}
    void stopMusic() override {}
    void fadeAllSounds() override {}
    int playSoundFull(SoundID) override { return 0; }
    SoundID getGlobalSound(GlobalSound) const override { return 0; }
    void setMusicVolume(int) override {}
    void setSoundEffectVolume(int) override {}
};

class EngineContextFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearEngine();
    }

    void TearDown() override
    {
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearEngine();
    }
};

TEST_F(EngineContextFixture, EngineThrowsWhenNoEngineIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
}

TEST_F(EngineContextFixture, SetEnginePublishesInstalledEngine)
{
    EngineContext& context = EngineContext::get();
    auto installed = std::make_unique<GameEngine>();
    GameEngine* installedPtr = installed.get();

    context.setEngine(std::move(installed));

    EXPECT_EQ(context.tryEngine(), installedPtr);
    EXPECT_EQ(&context.engine(), installedPtr);
    EXPECT_EQ(context.renderedFrameCount(), 0u);
}

TEST_F(EngineContextFixture, AudioSystemThrowsWhenNoAudioSystemIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryAudioSystem(), nullptr);
    EXPECT_THROW(context.audioSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, InstallAudioSystemPublishesInstalledAudioSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubAudioSystem audioSystem;
    context.installAudioSystem(audioSystem);

    EXPECT_EQ(context.tryAudioSystem(), &audioSystem);
    EXPECT_EQ(&context.audioSystem(), &audioSystem);
}

TEST_F(EngineContextFixture, SetEngineRejectsNullAndDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    EXPECT_THROW(context.setEngine(nullptr), std::logic_error);

    auto first = std::make_unique<GameEngine>();
    GameEngine* firstPtr = first.get();
    context.setEngine(std::move(first));

    EXPECT_THROW(context.setEngine(std::make_unique<GameEngine>()), std::logic_error);
    EXPECT_EQ(context.tryEngine(), firstPtr);
}

TEST_F(EngineContextFixture, InstallAudioSystemRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubAudioSystem first;
    StubAudioSystem second;
    context.installAudioSystem(first);

    EXPECT_THROW(context.installAudioSystem(second), std::logic_error);
    EXPECT_EQ(context.tryAudioSystem(), &first);
}

TEST_F(EngineContextFixture, ClearEngineRemovesInstalledEngine)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearAudioSystemRemovesInstalledAudioSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubAudioSystem audioSystem;
    context.installAudioSystem(audioSystem);

    context.clearAudioSystem();

    EXPECT_EQ(context.tryAudioSystem(), nullptr);
    EXPECT_THROW(context.audioSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledAudioSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubAudioSystem audioSystem;
    context.installAudioSystem(audioSystem);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryAudioSystem(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
    EXPECT_THROW(context.audioSystem(), std::logic_error);
}

} // namespace
