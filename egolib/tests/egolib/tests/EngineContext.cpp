#include "gtest/gtest.h"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Image/IImageManager.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Logic/Perk.hpp"
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

class StubPerkHandler : public Ego::Perks::IPerkHandler
{
public:
    const Ego::Perks::Perk& getPerk(Ego::Perks::PerkID) const override { return _perk; }
    Ego::Perks::PerkID fromString(const std::string&) const override { return Ego::Perks::NR_OF_PERKS; }

private:
    Ego::Perks::Perk _perk;
};

class StubImageManager : public Ego::IImageManager
{
public:
    std::shared_ptr<SDL_Surface> getDefaultImage() const override { return nullptr; }
    std::shared_ptr<SDL_Surface> createImage(size_t, size_t, size_t, const Ego::pixel_descriptor&, void*) const override { return nullptr; }
    std::shared_ptr<SDL_Surface> createImage(size_t, size_t, const Ego::pixel_descriptor&) const override { return nullptr; }
    void save_as_png(const std::shared_ptr<SDL_Surface>&, const std::string&) const override {}
    bool imageExistsWithKnownExtension(const std::string&) const override { return false; }
    std::shared_ptr<SDL_Surface> loadImageWithKnownExtension(const std::string&, std::string*) const override { return nullptr; }
};

class StubParticleHandler : public IParticleHandler
{
public:
    void updateAllParticles() override {}
    void download(egoboo_config_t&) override {}
    void upload(egoboo_config_t&) override {}
    size_t getDisplayLimit() const override { return 0; }
    void setDisplayLimit(size_t) override {}
    void clear() override {}
    const std::shared_ptr<Ego::Particle>& operator[](ParticleRef) override { return _invalidParticle; }
    std::shared_ptr<Ego::Particle> spawnLocalParticle(const Ego::Vector3f&,
                                                      const Facing&,
                                                      ObjectProfileRef,
                                                      const LocalParticleProfileRef&,
                                                      ObjectRef,
                                                      uint16_t,
                                                      TEAM_REF,
                                                      ObjectRef,
                                                      ParticleRef,
                                                      int,
                                                      ObjectRef) override
    {
        return nullptr;
    }
    std::shared_ptr<Ego::Particle> spawnParticle(const Ego::Vector3f&,
                                                 const Facing&,
                                                 ObjectProfileRef,
                                                 PIP_REF,
                                                 ObjectRef,
                                                 uint16_t,
                                                 TEAM_REF,
                                                 ObjectRef,
                                                 ParticleRef,
                                                 int,
                                                 ObjectRef,
                                                 bool) override
    {
        return nullptr;
    }
    std::shared_ptr<Ego::Particle> spawnGlobalParticle(const Ego::Vector3f&,
                                                       const Facing&,
                                                       const LocalParticleProfileRef&,
                                                       int,
                                                       bool) override
    {
        return nullptr;
    }
    size_t getCount() const override { return 0; }
    size_t getFreeCount() const override { return 0; }
    std::shared_ptr<const Ego::Texture> getLightParticleTexture() override { return nullptr; }
    std::shared_ptr<const Ego::Texture> getTransparentParticleTexture() override { return nullptr; }
    void spawnPoof(const std::shared_ptr<Object>&) override {}
    void spawnDefencePing(const std::shared_ptr<Object>&, const std::shared_ptr<Object>&) override {}

protected:
    ParticleList::const_iterator beginActiveParticles() override { return _particles.cbegin(); }
    ParticleList::const_iterator endActiveParticles() override { return _particles.cend(); }
    void lockParticles() override {}
    void unlockParticles() override {}

private:
    ParticleList _particles;
    std::shared_ptr<Ego::Particle> _invalidParticle;
};

class EngineContextFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearImageManager();
        EngineContext::get().clearParticleHandler();
        EngineContext::get().clearPerkHandler();
        EngineContext::get().clearEngine();
    }

    void TearDown() override
    {
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearImageManager();
        EngineContext::get().clearParticleHandler();
        EngineContext::get().clearPerkHandler();
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

TEST_F(EngineContextFixture, PerkHandlerThrowsWhenNoPerkHandlerIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryPerkHandler(), nullptr);
    EXPECT_THROW(context.perkHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, ImageManagerThrowsWhenNoImageManagerIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryImageManager(), nullptr);
    EXPECT_THROW(context.imageManager(), std::logic_error);
}

TEST_F(EngineContextFixture, ParticleHandlerThrowsWhenNoParticleHandlerIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryParticleHandler(), nullptr);
    EXPECT_THROW(context.particleHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, InstallPerkHandlerPublishesInstalledPerkHandler)
{
    EngineContext& context = EngineContext::get();

    StubPerkHandler perkHandler;
    context.installPerkHandler(perkHandler);

    EXPECT_EQ(context.tryPerkHandler(), &perkHandler);
    EXPECT_EQ(&context.perkHandler(), &perkHandler);
}

TEST_F(EngineContextFixture, InstallImageManagerPublishesInstalledImageManager)
{
    EngineContext& context = EngineContext::get();

    StubImageManager imageManager;
    context.installImageManager(imageManager);

    EXPECT_EQ(context.tryImageManager(), &imageManager);
    EXPECT_EQ(&context.imageManager(), &imageManager);
}

TEST_F(EngineContextFixture, InstallParticleHandlerPublishesInstalledParticleHandler)
{
    EngineContext& context = EngineContext::get();

    StubParticleHandler particleHandler;
    context.installParticleHandler(particleHandler);

    EXPECT_EQ(context.tryParticleHandler(), &particleHandler);
    EXPECT_EQ(&context.particleHandler(), &particleHandler);
}

TEST_F(EngineContextFixture, InstallPerkHandlerRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubPerkHandler first;
    StubPerkHandler second;
    context.installPerkHandler(first);

    EXPECT_THROW(context.installPerkHandler(second), std::logic_error);
    EXPECT_EQ(context.tryPerkHandler(), &first);
}

TEST_F(EngineContextFixture, InstallImageManagerRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubImageManager first;
    StubImageManager second;
    context.installImageManager(first);

    EXPECT_THROW(context.installImageManager(second), std::logic_error);
    EXPECT_EQ(context.tryImageManager(), &first);
}

TEST_F(EngineContextFixture, InstallParticleHandlerRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubParticleHandler first;
    StubParticleHandler second;
    context.installParticleHandler(first);

    EXPECT_THROW(context.installParticleHandler(second), std::logic_error);
    EXPECT_EQ(context.tryParticleHandler(), &first);
}

TEST_F(EngineContextFixture, ClearPerkHandlerRemovesInstalledPerkHandler)
{
    EngineContext& context = EngineContext::get();

    StubPerkHandler perkHandler;
    context.installPerkHandler(perkHandler);

    context.clearPerkHandler();

    EXPECT_EQ(context.tryPerkHandler(), nullptr);
    EXPECT_THROW(context.perkHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearImageManagerRemovesInstalledImageManager)
{
    EngineContext& context = EngineContext::get();

    StubImageManager imageManager;
    context.installImageManager(imageManager);

    context.clearImageManager();

    EXPECT_EQ(context.tryImageManager(), nullptr);
    EXPECT_THROW(context.imageManager(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearParticleHandlerRemovesInstalledParticleHandler)
{
    EngineContext& context = EngineContext::get();

    StubParticleHandler particleHandler;
    context.installParticleHandler(particleHandler);

    context.clearParticleHandler();

    EXPECT_EQ(context.tryParticleHandler(), nullptr);
    EXPECT_THROW(context.particleHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledPerkHandler)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubPerkHandler perkHandler;
    context.installPerkHandler(perkHandler);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryPerkHandler(), nullptr);
    EXPECT_THROW(context.perkHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledImageManager)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubImageManager imageManager;
    context.installImageManager(imageManager);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryImageManager(), nullptr);
    EXPECT_THROW(context.imageManager(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledParticleHandler)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubParticleHandler particleHandler;
    context.installParticleHandler(particleHandler);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryParticleHandler(), nullptr);
    EXPECT_THROW(context.particleHandler(), std::logic_error);
}

} // namespace
