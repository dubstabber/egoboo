#pragma once

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Image/IImageManager.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/game/Graphics/IBillboardSystem.hpp"
#include "egolib/game/Graphics/ICameraSystem.hpp"
#include "idlib/non_copyable.hpp"

#include <cstdint>
#include <memory>

class GameEngine;
class PlayingState;
struct egoboo_config_t;
namespace Log { struct Target; }
namespace Ego { namespace GUI { class UIManager; } }

class EngineContext : private idlib::non_copyable
{
public:
    static EngineContext& get();

    void setEngine(std::unique_ptr<GameEngine> engine);
    void clearEngine();

    GameEngine* tryEngine();
    const GameEngine* tryEngine() const;

    GameEngine& engine();
    const GameEngine& engine() const;

    Ego::GUI::UIManager* tryUIManager();
    const Ego::GUI::UIManager* tryUIManager() const;

    Ego::GUI::UIManager& uiManager();
    const Ego::GUI::UIManager& uiManager() const;

    void installAudioSystem(IAudioSystem& audioSystem);
    void clearAudioSystem();

    IAudioSystem* tryAudioSystem();
    const IAudioSystem* tryAudioSystem() const;

    IAudioSystem& audioSystem();
    const IAudioSystem& audioSystem() const;

    void installPerkHandler(Ego::Perks::IPerkHandler& perkHandler);
    void clearPerkHandler();

    Ego::Perks::IPerkHandler* tryPerkHandler();
    const Ego::Perks::IPerkHandler* tryPerkHandler() const;

    Ego::Perks::IPerkHandler& perkHandler();
    const Ego::Perks::IPerkHandler& perkHandler() const;

    void installImageManager(Ego::IImageManager& imageManager);
    void clearImageManager();

    Ego::IImageManager* tryImageManager();
    const Ego::IImageManager* tryImageManager() const;

    Ego::IImageManager& imageManager();
    const Ego::IImageManager& imageManager() const;

    void installParticleHandler(IParticleHandler& particleHandler);
    void clearParticleHandler();

    IParticleHandler* tryParticleHandler();
    const IParticleHandler* tryParticleHandler() const;

    IParticleHandler& particleHandler();
    const IParticleHandler& particleHandler() const;

    void installProfileSystem(IProfileSystem& profileSystem);
    void clearProfileSystem();

    IProfileSystem* tryProfileSystem();
    const IProfileSystem* tryProfileSystem() const;

    IProfileSystem& profileSystem();
    const IProfileSystem& profileSystem() const;

    void installCameraSystem(ICameraSystem& cameraSystem);
    void clearCameraSystem();

    ICameraSystem* tryCameraSystem();
    const ICameraSystem* tryCameraSystem() const;

    ICameraSystem& cameraSystem();
    const ICameraSystem& cameraSystem() const;

    void installBillboardSystem(Ego::Graphics::IBillboardSystem& billboardSystem);
    void clearBillboardSystem();

    Ego::Graphics::IBillboardSystem* tryBillboardSystem();
    const Ego::Graphics::IBillboardSystem* tryBillboardSystem() const;

    Ego::Graphics::IBillboardSystem& billboardSystem();
    const Ego::Graphics::IBillboardSystem& billboardSystem() const;

    void installConfig(egoboo_config_t& config);
    void clearConfig();

    egoboo_config_t* tryConfig();
    const egoboo_config_t* tryConfig() const;

    egoboo_config_t& config();
    const egoboo_config_t& config() const;

    void installLogTarget(Log::Target& logTarget);
    void clearLogTarget();

    Log::Target* tryLogTarget();
    const Log::Target* tryLogTarget() const;

    Log::Target& logTarget();
    const Log::Target& logTarget() const;

    uint32_t renderedFrameCount() const;

    std::shared_ptr<PlayingState> tryActivePlayingState() const;
    std::shared_ptr<PlayingState> activePlayingState() const;

private:
    EngineContext() = default;
};
