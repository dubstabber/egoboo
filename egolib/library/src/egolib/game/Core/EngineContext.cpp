#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Image/IImageManager.hpp"
#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Log/Target.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Graphics/IBillboardSystem.hpp"
#include "egolib/game/Graphics/ICameraSystem.hpp"
#include "egolib/game/GUI/UIManager.hpp"

#include <stdexcept>

namespace
{
std::unique_ptr<GameEngine> activeEngine;
IAudioSystem* activeAudioSystem = nullptr;
Ego::Input::IInputSystem* activeInputSystem = nullptr;
Ego::Perks::IPerkHandler* activePerkHandler = nullptr;
Ego::IImageManager* activeImageManager = nullptr;
Ego::IFontManager* activeFontManager = nullptr;
Ego::IGraphicsSystem* activeGraphicsSystem = nullptr;
IParticleHandler* activeParticleHandler = nullptr;
IProfileSystem* activeProfileSystem = nullptr;
ICameraSystem* activeCameraSystem = nullptr;
Ego::Graphics::IBillboardSystem* activeBillboardSystem = nullptr;
egoboo_config_t* activeConfig = nullptr;
Log::Target* activeLogTarget = nullptr;
}

EngineContext& EngineContext::get()
{
    static EngineContext instance;
    return instance;
}

void EngineContext::setEngine(std::unique_ptr<GameEngine> engine)
{
    if (!engine)
    {
        throw std::logic_error("cannot install null game engine");
    }
    if (activeEngine)
    {
        throw std::logic_error("game engine already installed");
    }
    activeEngine = std::move(engine);
}

void EngineContext::clearEngine()
{
    clearAudioSystem();
    clearInputSystem();
    clearPerkHandler();
    clearImageManager();
    clearFontManager();
    clearGraphicsSystem();
    clearParticleHandler();
    clearProfileSystem();
    clearCameraSystem();
    clearBillboardSystem();
    activeEngine.reset();
}

GameEngine* EngineContext::tryEngine()
{
    return activeEngine.get();
}

const GameEngine* EngineContext::tryEngine() const
{
    return activeEngine.get();
}

GameEngine& EngineContext::engine()
{
    GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        throw std::logic_error("no active game engine");
    }
    return *currentEngine;
}

const GameEngine& EngineContext::engine() const
{
    const GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        throw std::logic_error("no active game engine");
    }
    return *currentEngine;
}

Ego::GUI::UIManager* EngineContext::tryUIManager()
{
    GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        return nullptr;
    }
    return currentEngine->getUIManager().get();
}

const Ego::GUI::UIManager* EngineContext::tryUIManager() const
{
    const GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        return nullptr;
    }
    return currentEngine->getUIManager().get();
}

Ego::GUI::UIManager& EngineContext::uiManager()
{
    Ego::GUI::UIManager* currentUIManager = tryUIManager();
    if (!currentUIManager)
    {
        throw std::logic_error("no active ui manager");
    }
    return *currentUIManager;
}

const Ego::GUI::UIManager& EngineContext::uiManager() const
{
    const Ego::GUI::UIManager* currentUIManager = tryUIManager();
    if (!currentUIManager)
    {
        throw std::logic_error("no active ui manager");
    }
    return *currentUIManager;
}

void EngineContext::installAudioSystem(IAudioSystem& audioSystem)
{
    if (activeAudioSystem)
    {
        throw std::logic_error("audio system already installed");
    }
    activeAudioSystem = &audioSystem;
}

void EngineContext::clearAudioSystem()
{
    activeAudioSystem = nullptr;
}

IAudioSystem* EngineContext::tryAudioSystem()
{
    return activeAudioSystem;
}

const IAudioSystem* EngineContext::tryAudioSystem() const
{
    return activeAudioSystem;
}

IAudioSystem& EngineContext::audioSystem()
{
    IAudioSystem* currentAudioSystem = tryAudioSystem();
    if (!currentAudioSystem)
    {
        throw std::logic_error("no active audio system");
    }
    return *currentAudioSystem;
}

const IAudioSystem& EngineContext::audioSystem() const
{
    const IAudioSystem* currentAudioSystem = tryAudioSystem();
    if (!currentAudioSystem)
    {
        throw std::logic_error("no active audio system");
    }
    return *currentAudioSystem;
}

void EngineContext::installInputSystem(Ego::Input::IInputSystem& inputSystem)
{
    if (activeInputSystem)
    {
        throw std::logic_error("input system already installed");
    }
    activeInputSystem = &inputSystem;
}

void EngineContext::clearInputSystem()
{
    activeInputSystem = nullptr;
}

Ego::Input::IInputSystem* EngineContext::tryInputSystem()
{
    return activeInputSystem;
}

const Ego::Input::IInputSystem* EngineContext::tryInputSystem() const
{
    return activeInputSystem;
}

Ego::Input::IInputSystem& EngineContext::inputSystem()
{
    Ego::Input::IInputSystem* currentInputSystem = tryInputSystem();
    if (!currentInputSystem)
    {
        throw std::logic_error("no active input system");
    }
    return *currentInputSystem;
}

const Ego::Input::IInputSystem& EngineContext::inputSystem() const
{
    const Ego::Input::IInputSystem* currentInputSystem = tryInputSystem();
    if (!currentInputSystem)
    {
        throw std::logic_error("no active input system");
    }
    return *currentInputSystem;
}

void EngineContext::installPerkHandler(Ego::Perks::IPerkHandler& perkHandler)
{
    if (activePerkHandler)
    {
        throw std::logic_error("perk handler already installed");
    }
    activePerkHandler = &perkHandler;
}

void EngineContext::clearPerkHandler()
{
    activePerkHandler = nullptr;
}

Ego::Perks::IPerkHandler* EngineContext::tryPerkHandler()
{
    return activePerkHandler;
}

const Ego::Perks::IPerkHandler* EngineContext::tryPerkHandler() const
{
    return activePerkHandler;
}

Ego::Perks::IPerkHandler& EngineContext::perkHandler()
{
    Ego::Perks::IPerkHandler* currentPerkHandler = tryPerkHandler();
    if (!currentPerkHandler)
    {
        throw std::logic_error("no active perk handler");
    }
    return *currentPerkHandler;
}

const Ego::Perks::IPerkHandler& EngineContext::perkHandler() const
{
    const Ego::Perks::IPerkHandler* currentPerkHandler = tryPerkHandler();
    if (!currentPerkHandler)
    {
        throw std::logic_error("no active perk handler");
    }
    return *currentPerkHandler;
}

void EngineContext::installImageManager(Ego::IImageManager& imageManager)
{
    if (activeImageManager)
    {
        throw std::logic_error("image manager already installed");
    }
    activeImageManager = &imageManager;
}

void EngineContext::clearImageManager()
{
    activeImageManager = nullptr;
}

Ego::IImageManager* EngineContext::tryImageManager()
{
    return activeImageManager;
}

const Ego::IImageManager* EngineContext::tryImageManager() const
{
    return activeImageManager;
}

Ego::IImageManager& EngineContext::imageManager()
{
    Ego::IImageManager* currentImageManager = tryImageManager();
    if (!currentImageManager)
    {
        throw std::logic_error("no active image manager");
    }
    return *currentImageManager;
}

const Ego::IImageManager& EngineContext::imageManager() const
{
    const Ego::IImageManager* currentImageManager = tryImageManager();
    if (!currentImageManager)
    {
        throw std::logic_error("no active image manager");
    }
    return *currentImageManager;
}

void EngineContext::installFontManager(Ego::IFontManager& fontManager)
{
    if (activeFontManager)
    {
        throw std::logic_error("font manager already installed");
    }
    activeFontManager = &fontManager;
}

void EngineContext::clearFontManager()
{
    activeFontManager = nullptr;
}

Ego::IFontManager* EngineContext::tryFontManager()
{
    return activeFontManager;
}

const Ego::IFontManager* EngineContext::tryFontManager() const
{
    return activeFontManager;
}

Ego::IFontManager& EngineContext::fontManager()
{
    Ego::IFontManager* currentFontManager = tryFontManager();
    if (!currentFontManager)
    {
        throw std::logic_error("no active font manager");
    }
    return *currentFontManager;
}

const Ego::IFontManager& EngineContext::fontManager() const
{
    const Ego::IFontManager* currentFontManager = tryFontManager();
    if (!currentFontManager)
    {
        throw std::logic_error("no active font manager");
    }
    return *currentFontManager;
}

void EngineContext::installGraphicsSystem(Ego::IGraphicsSystem& graphicsSystem)
{
    if (activeGraphicsSystem)
    {
        throw std::logic_error("graphics system already installed");
    }
    activeGraphicsSystem = &graphicsSystem;
}

void EngineContext::clearGraphicsSystem()
{
    activeGraphicsSystem = nullptr;
}

Ego::IGraphicsSystem* EngineContext::tryGraphicsSystem()
{
    return activeGraphicsSystem;
}

const Ego::IGraphicsSystem* EngineContext::tryGraphicsSystem() const
{
    return activeGraphicsSystem;
}

Ego::IGraphicsSystem& EngineContext::graphicsSystem()
{
    Ego::IGraphicsSystem* currentGraphicsSystem = tryGraphicsSystem();
    if (!currentGraphicsSystem)
    {
        throw std::logic_error("no active graphics system");
    }
    return *currentGraphicsSystem;
}

const Ego::IGraphicsSystem& EngineContext::graphicsSystem() const
{
    const Ego::IGraphicsSystem* currentGraphicsSystem = tryGraphicsSystem();
    if (!currentGraphicsSystem)
    {
        throw std::logic_error("no active graphics system");
    }
    return *currentGraphicsSystem;
}

void EngineContext::installParticleHandler(IParticleHandler& particleHandler)
{
    if (activeParticleHandler)
    {
        throw std::logic_error("particle handler already installed");
    }
    activeParticleHandler = &particleHandler;
}

void EngineContext::clearParticleHandler()
{
    activeParticleHandler = nullptr;
}

IParticleHandler* EngineContext::tryParticleHandler()
{
    return activeParticleHandler;
}

const IParticleHandler* EngineContext::tryParticleHandler() const
{
    return activeParticleHandler;
}

IParticleHandler& EngineContext::particleHandler()
{
    IParticleHandler* currentParticleHandler = tryParticleHandler();
    if (!currentParticleHandler)
    {
        throw std::logic_error("no active particle handler");
    }
    return *currentParticleHandler;
}

const IParticleHandler& EngineContext::particleHandler() const
{
    const IParticleHandler* currentParticleHandler = tryParticleHandler();
    if (!currentParticleHandler)
    {
        throw std::logic_error("no active particle handler");
    }
    return *currentParticleHandler;
}

void EngineContext::installProfileSystem(IProfileSystem& profileSystem)
{
    if (activeProfileSystem)
    {
        throw std::logic_error("profile system already installed");
    }
    activeProfileSystem = &profileSystem;
}

void EngineContext::clearProfileSystem()
{
    activeProfileSystem = nullptr;
}

IProfileSystem* EngineContext::tryProfileSystem()
{
    return activeProfileSystem;
}

const IProfileSystem* EngineContext::tryProfileSystem() const
{
    return activeProfileSystem;
}

IProfileSystem& EngineContext::profileSystem()
{
    IProfileSystem* currentProfileSystem = tryProfileSystem();
    if (!currentProfileSystem)
    {
        throw std::logic_error("no active profile system");
    }
    return *currentProfileSystem;
}

const IProfileSystem& EngineContext::profileSystem() const
{
    const IProfileSystem* currentProfileSystem = tryProfileSystem();
    if (!currentProfileSystem)
    {
        throw std::logic_error("no active profile system");
    }
    return *currentProfileSystem;
}

void EngineContext::installCameraSystem(ICameraSystem& cameraSystem)
{
    if (activeCameraSystem)
    {
        throw std::logic_error("camera system already installed");
    }
    activeCameraSystem = &cameraSystem;
}

void EngineContext::clearCameraSystem()
{
    activeCameraSystem = nullptr;
}

ICameraSystem* EngineContext::tryCameraSystem()
{
    return activeCameraSystem;
}

const ICameraSystem* EngineContext::tryCameraSystem() const
{
    return activeCameraSystem;
}

ICameraSystem& EngineContext::cameraSystem()
{
    ICameraSystem* currentCameraSystem = tryCameraSystem();
    if (!currentCameraSystem)
    {
        throw std::logic_error("no active camera system");
    }
    return *currentCameraSystem;
}

const ICameraSystem& EngineContext::cameraSystem() const
{
    const ICameraSystem* currentCameraSystem = tryCameraSystem();
    if (!currentCameraSystem)
    {
        throw std::logic_error("no active camera system");
    }
    return *currentCameraSystem;
}

void EngineContext::installBillboardSystem(Ego::Graphics::IBillboardSystem& billboardSystem)
{
    if (activeBillboardSystem)
    {
        throw std::logic_error("billboard system already installed");
    }
    activeBillboardSystem = &billboardSystem;
}

void EngineContext::clearBillboardSystem()
{
    activeBillboardSystem = nullptr;
}

Ego::Graphics::IBillboardSystem* EngineContext::tryBillboardSystem()
{
    return activeBillboardSystem;
}

const Ego::Graphics::IBillboardSystem* EngineContext::tryBillboardSystem() const
{
    return activeBillboardSystem;
}

Ego::Graphics::IBillboardSystem& EngineContext::billboardSystem()
{
    Ego::Graphics::IBillboardSystem* currentBillboardSystem = tryBillboardSystem();
    if (!currentBillboardSystem)
    {
        throw std::logic_error("no active billboard system");
    }
    return *currentBillboardSystem;
}

const Ego::Graphics::IBillboardSystem& EngineContext::billboardSystem() const
{
    const Ego::Graphics::IBillboardSystem* currentBillboardSystem = tryBillboardSystem();
    if (!currentBillboardSystem)
    {
        throw std::logic_error("no active billboard system");
    }
    return *currentBillboardSystem;
}

void EngineContext::installConfig(egoboo_config_t& config)
{
    if (activeConfig)
    {
        throw std::logic_error("config already installed");
    }
    activeConfig = &config;
}

void EngineContext::clearConfig()
{
    activeConfig = nullptr;
}

egoboo_config_t* EngineContext::tryConfig()
{
    return activeConfig;
}

const egoboo_config_t* EngineContext::tryConfig() const
{
    return activeConfig;
}

egoboo_config_t& EngineContext::config()
{
    egoboo_config_t* currentConfig = tryConfig();
    if (!currentConfig)
    {
        throw std::logic_error("no active config");
    }
    return *currentConfig;
}

const egoboo_config_t& EngineContext::config() const
{
    const egoboo_config_t* currentConfig = tryConfig();
    if (!currentConfig)
    {
        throw std::logic_error("no active config");
    }
    return *currentConfig;
}

void EngineContext::installLogTarget(Log::Target& logTarget)
{
    if (activeLogTarget)
    {
        throw std::logic_error("log target already installed");
    }
    activeLogTarget = &logTarget;
}

void EngineContext::clearLogTarget()
{
    activeLogTarget = nullptr;
}

Log::Target* EngineContext::tryLogTarget()
{
    return activeLogTarget;
}

const Log::Target* EngineContext::tryLogTarget() const
{
    return activeLogTarget;
}

Log::Target& EngineContext::logTarget()
{
    Log::Target* currentLogTarget = tryLogTarget();
    if (!currentLogTarget)
    {
        throw std::logic_error("no active log target");
    }
    return *currentLogTarget;
}

const Log::Target& EngineContext::logTarget() const
{
    const Log::Target* currentLogTarget = tryLogTarget();
    if (!currentLogTarget)
    {
        throw std::logic_error("no active log target");
    }
    return *currentLogTarget;
}

uint32_t EngineContext::renderedFrameCount() const
{
    return engine().getNumberOfFramesRendered();
}

std::shared_ptr<PlayingState> EngineContext::tryActivePlayingState() const
{
    const GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        return nullptr;
    }
    return currentEngine->getActivePlayingState();
}

std::shared_ptr<PlayingState> EngineContext::activePlayingState() const
{
    std::shared_ptr<PlayingState> state = tryActivePlayingState();
    if (!state)
    {
        throw std::logic_error("no active playing state");
    }
    return state;
}
