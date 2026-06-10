#include "gtest/gtest.h"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Image/IImageManager.hpp"
#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Logic/Perk.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Log/Target.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Renderer/DeferredTexture.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/game/Graphics/ICameraSystem.hpp"

#include <memory>
#include <stdexcept>

namespace
{

class StubAudioSystem : public IAudioSystem
{
public:
    SoundID loadSound(const std::string&) override { return -1; }
    void playMusic(MusicID, uint16_t = 0) override {}
    void playMusic(const std::string&, uint16_t = 0) override {}
    void stopMusic() override {}
    void fadeAllSounds() override {}
    int playSound(const Ego::Vector3f&, SoundID) override { return 0; }
    void playSoundLooped(SoundID, ObjectRef) override {}
    size_t stopObjectLoopingSounds(ObjectRef, SoundID = -1) override { return 0; }
    int playSoundFull(SoundID) override { return 0; }
    SoundID getGlobalSound(GlobalSound) const override { return 0; }
    void setMaxHearingDistance(float) override {}
    void setMusicVolume(int) override {}
    void setSoundEffectVolume(int) override {}
    void update() override {}
    void loadGlobalSounds() override {}
    void loadAllMusic() override {}
};

class StubInputSystem : public Ego::Input::IInputSystem
{
public:
    void update() override {}

    const Ego::Vector2f& getMouseMovement() const override
    {
        return _mouseMovement;
    }

    bool isMouseButtonDown(MouseButton) const override
    {
        return false;
    }

    bool isKeyDown(SDL_Keycode) const override
    {
        return false;
    }

    Ego::ModifierKeys getModifierKeys() const override
    {
        return {};
    }

private:
    Ego::Vector2f _mouseMovement{0.0f, 0.0f};
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

class StubProfileSystem : public IProfileSystem
{
public:
    void reset() override {}
    bool isLoaded(PRO_REF) const override { return false; }
    bool isLoaded(ObjectProfileRef) const override { return false; }
    ObjectProfileRef loadOneProfile(const std::string&, int) override { return ObjectProfileRef::Invalid; }
    const std::shared_ptr<ObjectProfile>& getProfile(PRO_REF) const override { return _objectProfile; }
    const std::shared_ptr<ObjectProfile>& getProfile(ObjectProfileRef) const override { return _objectProfile; }
    const std::unordered_map<PRO_REF, std::shared_ptr<ObjectProfile>>& getLoadedProfiles() const override { return _loadedProfiles; }
    const Ego::DeferredTexture& getSpellBookIcon(size_t) const override { return _spellbookIcon; }
    void loadModuleProfiles() override {}
    const std::vector<std::shared_ptr<ModuleProfile>>& getModuleProfiles() const override { return _moduleProfiles; }
    void loadAllSavedCharacters(const std::string&) override {}
    const std::vector<std::shared_ptr<LoadPlayerElement>>& getSavedPlayers() const override { return _savedPlayers; }
    void loadGlobalParticleProfiles() override {}
    bool isParticleProfileLoaded(PIP_REF) const override { return false; }
    const std::shared_ptr<ParticleProfile>& getParticleProfile(PIP_REF) const override { return _particleProfile; }
    PIP_REF loadParticleProfile(const std::string&, PIP_REF) override { return INVALID_PIP_REF; }
    void unloadParticleProfile(PIP_REF) override {}
    bool isEnchantProfileLoaded(EVE_REF) const override { return false; }
    const std::shared_ptr<EnchantProfile>& getEnchantProfile(EVE_REF) const override { return _enchantProfile; }
    EVE_REF loadEnchantProfile(const std::string&, EVE_REF) override { return INVALID_EVE_REF; }

private:
    Ego::DeferredTexture _spellbookIcon;
    std::shared_ptr<ObjectProfile> _objectProfile;
    std::shared_ptr<ParticleProfile> _particleProfile;
    std::shared_ptr<EnchantProfile> _enchantProfile;
    std::unordered_map<PRO_REF, std::shared_ptr<ObjectProfile>> _loadedProfiles;
    std::vector<std::shared_ptr<ModuleProfile>> _moduleProfiles;
    std::vector<std::shared_ptr<LoadPlayerElement>> _savedPlayers;
};

class StubCameraSystem : public ICameraSystem
{
public:
    void updateAll(const ego_mesh_t*) override {}
    void setNumberOfCameras(size_t) override {}
    const std::vector<std::shared_ptr<Camera>>& getCameraList() const override { return _cameraList; }
    std::shared_ptr<Camera> getMainCamera() const override { return nullptr; }
    std::shared_ptr<Camera> getCamera(ObjectRef) const override { return nullptr; }
    CameraOptions& getCameraOptions() override { return _cameraOptions; }
    void renderAll(std::function<void(std::shared_ptr<Camera>, std::shared_ptr<Ego::Graphics::TileList>, std::shared_ptr<Ego::Graphics::EntityList>)>) override {}
    bool isTileInMainCameraRenderList(const Index1D&) const override { return false; }

private:
    std::vector<std::shared_ptr<Camera>> _cameraList;
    CameraOptions _cameraOptions;
};

class StubBillboardSystem : public Ego::Graphics::IBillboardSystem
{
public:
    void update() override {}
    void reset() override {}
    std::shared_ptr<Ego::Graphics::Billboard> makeBillboard(ObjectRef,
                                                            const std::string&,
                                                            const Ego::Colour4f&,
                                                            const Ego::Colour4f&,
                                                            int,
                                                            BIT_FIELD,
                                                            float) override
    {
        return nullptr;
    }
};

class StubLogTarget : public Log::Target
{
public:
    using Log::Target::Target;

protected:
    void writev(Log::Level, const char *, va_list) override {}
};

class EngineContextFixture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EngineContext::get().clearLogTarget();
        EngineContext::get().clearConfig();
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearInputSystem();
        EngineContext::get().clearImageManager();
        EngineContext::get().clearParticleHandler();
        EngineContext::get().clearPerkHandler();
        EngineContext::get().clearProfileSystem();
        EngineContext::get().clearCameraSystem();
        EngineContext::get().clearBillboardSystem();
        EngineContext::get().clearEngine();
    }

    void TearDown() override
    {
        EngineContext::get().clearLogTarget();
        EngineContext::get().clearConfig();
        EngineContext::get().clearAudioSystem();
        EngineContext::get().clearInputSystem();
        EngineContext::get().clearImageManager();
        EngineContext::get().clearParticleHandler();
        EngineContext::get().clearPerkHandler();
        EngineContext::get().clearProfileSystem();
        EngineContext::get().clearCameraSystem();
        EngineContext::get().clearBillboardSystem();
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

TEST_F(EngineContextFixture, InputSystemThrowsWhenNoInputSystemIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryInputSystem(), nullptr);
    EXPECT_THROW(context.inputSystem(), std::logic_error);
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

TEST_F(EngineContextFixture, InstallInputSystemPublishesInstalledInputSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubInputSystem inputSystem;
    context.installInputSystem(inputSystem);

    EXPECT_EQ(context.tryInputSystem(), &inputSystem);
    EXPECT_EQ(&context.inputSystem(), &inputSystem);
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

TEST_F(EngineContextFixture, InstallInputSystemRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubInputSystem first;
    StubInputSystem second;
    context.installInputSystem(first);

    EXPECT_THROW(context.installInputSystem(second), std::logic_error);
    EXPECT_EQ(context.tryInputSystem(), &first);
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

TEST_F(EngineContextFixture, ClearInputSystemRemovesInstalledInputSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubInputSystem inputSystem;
    context.installInputSystem(inputSystem);

    context.clearInputSystem();

    EXPECT_EQ(context.tryInputSystem(), nullptr);
    EXPECT_THROW(context.inputSystem(), std::logic_error);
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

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledInputSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubInputSystem inputSystem;
    context.installInputSystem(inputSystem);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryInputSystem(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
    EXPECT_THROW(context.inputSystem(), std::logic_error);
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

TEST_F(EngineContextFixture, ProfileSystemThrowsWhenNoProfileSystemIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryProfileSystem(), nullptr);
    EXPECT_THROW(context.profileSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ConfigThrowsWhenNoConfigIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryConfig(), nullptr);
    EXPECT_THROW(context.config(), std::logic_error);
}

TEST_F(EngineContextFixture, ParticleHandlerThrowsWhenNoParticleHandlerIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryParticleHandler(), nullptr);
    EXPECT_THROW(context.particleHandler(), std::logic_error);
}

TEST_F(EngineContextFixture, CameraSystemThrowsWhenNoCameraSystemIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryCameraSystem(), nullptr);
    EXPECT_THROW(context.cameraSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, BillboardSystemThrowsWhenNoBillboardSystemIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryBillboardSystem(), nullptr);
    EXPECT_THROW(context.billboardSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, LogTargetThrowsWhenNoLogTargetIsInstalled)
{
    EngineContext& context = EngineContext::get();

    EXPECT_EQ(context.tryLogTarget(), nullptr);
    EXPECT_THROW(context.logTarget(), std::logic_error);
    EXPECT_EQ(Log::tryActiveTarget(), nullptr);
    EXPECT_THROW(Log::activeTarget(), std::logic_error);
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

TEST_F(EngineContextFixture, InstallProfileSystemPublishesInstalledProfileSystem)
{
    EngineContext& context = EngineContext::get();

    StubProfileSystem profileSystem;
    context.installProfileSystem(profileSystem);

    EXPECT_EQ(context.tryProfileSystem(), &profileSystem);
    EXPECT_EQ(&context.profileSystem(), &profileSystem);
}

TEST_F(EngineContextFixture, InstallCameraSystemPublishesInstalledCameraSystem)
{
    EngineContext& context = EngineContext::get();

    StubCameraSystem cameraSystem;
    context.installCameraSystem(cameraSystem);

    EXPECT_EQ(context.tryCameraSystem(), &cameraSystem);
    EXPECT_EQ(&context.cameraSystem(), &cameraSystem);
}

TEST_F(EngineContextFixture, InstallBillboardSystemPublishesInstalledBillboardSystem)
{
    EngineContext& context = EngineContext::get();

    StubBillboardSystem billboardSystem;
    context.installBillboardSystem(billboardSystem);

    EXPECT_EQ(context.tryBillboardSystem(), &billboardSystem);
    EXPECT_EQ(&context.billboardSystem(), &billboardSystem);
}

TEST_F(EngineContextFixture, InstallConfigPublishesInstalledConfig)
{
    EngineContext& context = EngineContext::get();

    egoboo_config_t config;
    context.installConfig(config);

    EXPECT_EQ(context.tryConfig(), &config);
    EXPECT_EQ(&context.config(), &config);
}

TEST_F(EngineContextFixture, InstallLogTargetPublishesInstalledLogTarget)
{
    EngineContext& context = EngineContext::get();

    StubLogTarget logTarget;
    context.installLogTarget(logTarget);

    EXPECT_EQ(context.tryLogTarget(), &logTarget);
    EXPECT_EQ(&context.logTarget(), &logTarget);
    EXPECT_EQ(Log::tryActiveTarget(), &logTarget);
    EXPECT_EQ(&Log::activeTarget(), &logTarget);
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

TEST_F(EngineContextFixture, InstallProfileSystemRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubProfileSystem first;
    StubProfileSystem second;
    context.installProfileSystem(first);

    EXPECT_THROW(context.installProfileSystem(second), std::logic_error);
    EXPECT_EQ(context.tryProfileSystem(), &first);
}

TEST_F(EngineContextFixture, InstallCameraSystemRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubCameraSystem first;
    StubCameraSystem second;
    context.installCameraSystem(first);

    EXPECT_THROW(context.installCameraSystem(second), std::logic_error);
    EXPECT_EQ(context.tryCameraSystem(), &first);
}

TEST_F(EngineContextFixture, InstallBillboardSystemRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubBillboardSystem first;
    StubBillboardSystem second;
    context.installBillboardSystem(first);

    EXPECT_THROW(context.installBillboardSystem(second), std::logic_error);
    EXPECT_EQ(context.tryBillboardSystem(), &first);
}

TEST_F(EngineContextFixture, InstallConfigRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    egoboo_config_t first;
    egoboo_config_t second;
    context.installConfig(first);

    EXPECT_THROW(context.installConfig(second), std::logic_error);
    EXPECT_EQ(context.tryConfig(), &first);
}

TEST_F(EngineContextFixture, InstallLogTargetRejectsDoubleInstall)
{
    EngineContext& context = EngineContext::get();

    StubLogTarget first;
    StubLogTarget second;
    context.installLogTarget(first);

    EXPECT_THROW(context.installLogTarget(second), std::logic_error);
    EXPECT_EQ(context.tryLogTarget(), &first);
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

TEST_F(EngineContextFixture, ClearProfileSystemRemovesInstalledProfileSystem)
{
    EngineContext& context = EngineContext::get();

    StubProfileSystem profileSystem;
    context.installProfileSystem(profileSystem);

    context.clearProfileSystem();

    EXPECT_EQ(context.tryProfileSystem(), nullptr);
    EXPECT_THROW(context.profileSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearCameraSystemRemovesInstalledCameraSystem)
{
    EngineContext& context = EngineContext::get();

    StubCameraSystem cameraSystem;
    context.installCameraSystem(cameraSystem);

    context.clearCameraSystem();

    EXPECT_EQ(context.tryCameraSystem(), nullptr);
    EXPECT_THROW(context.cameraSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearBillboardSystemRemovesInstalledBillboardSystem)
{
    EngineContext& context = EngineContext::get();

    StubBillboardSystem billboardSystem;
    context.installBillboardSystem(billboardSystem);

    context.clearBillboardSystem();

    EXPECT_EQ(context.tryBillboardSystem(), nullptr);
    EXPECT_THROW(context.billboardSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearConfigRemovesInstalledConfig)
{
    EngineContext& context = EngineContext::get();

    egoboo_config_t config;
    context.installConfig(config);

    context.clearConfig();

    EXPECT_EQ(context.tryConfig(), nullptr);
    EXPECT_THROW(context.config(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearLogTargetRemovesInstalledLogTarget)
{
    EngineContext& context = EngineContext::get();

    StubLogTarget logTarget;
    context.installLogTarget(logTarget);

    context.clearLogTarget();

    EXPECT_EQ(context.tryLogTarget(), nullptr);
    EXPECT_THROW(context.logTarget(), std::logic_error);
    EXPECT_EQ(Log::tryActiveTarget(), nullptr);
    EXPECT_THROW(Log::activeTarget(), std::logic_error);
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

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledProfileSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubProfileSystem profileSystem;
    context.installProfileSystem(profileSystem);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryProfileSystem(), nullptr);
    EXPECT_THROW(context.profileSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledCameraSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubCameraSystem cameraSystem;
    context.installCameraSystem(cameraSystem);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryCameraSystem(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
    EXPECT_THROW(context.cameraSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineAlsoRemovesInstalledBillboardSystem)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubBillboardSystem billboardSystem;
    context.installBillboardSystem(billboardSystem);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryBillboardSystem(), nullptr);
    EXPECT_THROW(context.engine(), std::logic_error);
    EXPECT_THROW(context.billboardSystem(), std::logic_error);
}

TEST_F(EngineContextFixture, ClearEngineKeepsInstalledConfig)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    egoboo_config_t config;
    context.installConfig(config);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryConfig(), &config);
    EXPECT_EQ(&context.config(), &config);
}

TEST_F(EngineContextFixture, ClearEngineKeepsInstalledLogTarget)
{
    EngineContext& context = EngineContext::get();
    context.setEngine(std::make_unique<GameEngine>());

    StubLogTarget logTarget;
    context.installLogTarget(logTarget);

    context.clearEngine();

    EXPECT_EQ(context.tryEngine(), nullptr);
    EXPECT_EQ(context.tryLogTarget(), &logTarget);
    EXPECT_EQ(&context.logTarget(), &logTarget);
}

} // namespace
