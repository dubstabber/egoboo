#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/Entities/IParticleHandler.hpp"
#include "egolib/Image/IImageManager.hpp"
#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Logic/IPerkHandler.hpp"
#include "egolib/Log/Target.hpp"
#include "egolib/Log/_Include.hpp"
#include "egolib/Profiles/IProfileSystem.hpp"
#include "egolib/Renderer/Renderer.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/Graphics/VideoBufferManagerSeam.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ActiveGameEngine.hpp"
#include "egolib/Graphics/IBillboardSystem.hpp"
#include "egolib/Graphics/ICameraSystem.hpp"
#include "egolib/game/Graphics/IGFX.hpp"
#include "egolib/game/Graphics/ITextureAtlasManager.hpp"
#include "egolib/game/GUI/UIManager.hpp"

#include <stdexcept>

namespace
{
std::unique_ptr<GameEngine> activeEngine;
Ego::Input::IInputSystem* activeInputSystem = nullptr;
Ego::IImageManager* activeImageManager = nullptr;
Ego::IFontManager* activeFontManager = nullptr;
Ego::Graphics::ITextureAtlasManager* activeTextureAtlasManager = nullptr;
IGFX* activeGFX = nullptr;
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
    installActiveGameEngine(*activeEngine);
}

void EngineContext::clearEngine()
{
    clearAudioSystem();
    clearInputSystem();
    clearPerkHandler();
    clearImageManager();
    clearFontManager();
    clearGraphicsSystem();
    clearTextureManager();
    clearParticleHandler();
    clearProfileSystem();
    clearCameraSystem();
    clearBillboardSystem();
    clearGFX();
    clearVideoBufferManager();
    clearRenderer();
    clearActiveGameEngine();
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
    installActiveAudioSystem(audioSystem);
}

void EngineContext::clearAudioSystem()
{
    clearActiveAudioSystem();
}

IAudioSystem* EngineContext::tryAudioSystem()
{
    return tryActiveAudioSystem();
}

const IAudioSystem* EngineContext::tryAudioSystem() const
{
    return tryActiveAudioSystem();
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

// The perk-handler ownership now lives in the lower-layer egolib/Logic seam
// (Ego::Perks::activePerkHandler); these methods are thin delegators so existing
// EngineContext callers and the install-via-EngineContext tests keep routing through
// the same single installed pointer.
void EngineContext::installPerkHandler(Ego::Perks::IPerkHandler& perkHandler)
{
    Ego::Perks::installActivePerkHandler(perkHandler);
}

void EngineContext::clearPerkHandler()
{
    Ego::Perks::clearActivePerkHandler();
}

Ego::Perks::IPerkHandler* EngineContext::tryPerkHandler()
{
    return Ego::Perks::tryActivePerkHandler();
}

const Ego::Perks::IPerkHandler* EngineContext::tryPerkHandler() const
{
    return Ego::Perks::tryActivePerkHandler();
}

Ego::Perks::IPerkHandler& EngineContext::perkHandler()
{
    return Ego::Perks::activePerkHandler();
}

const Ego::Perks::IPerkHandler& EngineContext::perkHandler() const
{
    return Ego::Perks::activePerkHandler();
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

// Graphics-system lifecycle delegates to the lower-layer ownership-move seam in
// egolib/Graphics/IGraphicsSystem.cpp (mirrors the audio-system / Log active-target moves), so
// lower-layer callers (the GUI toolkit) can reach the installed graphics system without depending
// on this upper-layer hub. Declarations are unchanged, so all existing callers keep working.
void EngineContext::installGraphicsSystem(Ego::IGraphicsSystem& graphicsSystem)
{
    Ego::installActiveGraphicsSystem(graphicsSystem);
}

void EngineContext::clearGraphicsSystem()
{
    Ego::clearActiveGraphicsSystem();
}

Ego::IGraphicsSystem* EngineContext::tryGraphicsSystem()
{
    return Ego::tryActiveGraphicsSystem();
}

const Ego::IGraphicsSystem* EngineContext::tryGraphicsSystem() const
{
    return Ego::tryActiveGraphicsSystem();
}

Ego::IGraphicsSystem& EngineContext::graphicsSystem()
{
    return Ego::activeGraphicsSystem();
}

const Ego::IGraphicsSystem& EngineContext::graphicsSystem() const
{
    return Ego::activeGraphicsSystem();
}

// Texture-manager lifecycle delegates to the lower-layer ownership-move seam in
// egolib/Graphics/ITextureManager.cpp (mirrors the graphics-system / audio-system / Log
// active-target moves), so lower-layer callers can reach the installed texture manager without
// depending on this upper-layer hub. Declarations are unchanged, so all existing callers keep working.
void EngineContext::installTextureManager(Ego::ITextureManager& textureManager)
{
    Ego::installActiveTextureManager(textureManager);
}

void EngineContext::clearTextureManager()
{
    Ego::clearActiveTextureManager();
}

Ego::ITextureManager* EngineContext::tryTextureManager()
{
    return Ego::tryActiveTextureManager();
}

const Ego::ITextureManager* EngineContext::tryTextureManager() const
{
    return Ego::tryActiveTextureManager();
}

Ego::ITextureManager& EngineContext::textureManager()
{
    return Ego::activeTextureManager();
}

const Ego::ITextureManager& EngineContext::textureManager() const
{
    return Ego::activeTextureManager();
}

void EngineContext::installParticleHandler(IParticleHandler& particleHandler)
{
    installActiveParticleHandler(particleHandler);
}

void EngineContext::clearParticleHandler()
{
    clearActiveParticleHandler();
}

IParticleHandler* EngineContext::tryParticleHandler()
{
    return tryActiveParticleHandler();
}

const IParticleHandler* EngineContext::tryParticleHandler() const
{
    return tryActiveParticleHandler();
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
    installActiveProfileSystem(profileSystem);
}

void EngineContext::clearProfileSystem()
{
    clearActiveProfileSystem();
}

IProfileSystem* EngineContext::tryProfileSystem()
{
    return tryActiveProfileSystem();
}

const IProfileSystem* EngineContext::tryProfileSystem() const
{
    return tryActiveProfileSystem();
}

IProfileSystem& EngineContext::profileSystem()
{
    return activeProfileSystem();
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
    installActiveCameraSystem(cameraSystem);
}

void EngineContext::clearCameraSystem()
{
    clearActiveCameraSystem();
}

ICameraSystem* EngineContext::tryCameraSystem()
{
    return tryActiveCameraSystem();
}

const ICameraSystem* EngineContext::tryCameraSystem() const
{
    return tryActiveCameraSystem();
}

ICameraSystem& EngineContext::cameraSystem()
{
    return activeCameraSystem();
}

const ICameraSystem& EngineContext::cameraSystem() const
{
    return activeCameraSystem();
}

// The billboard-system ownership now lives in the lower-layer egolib/Graphics seam
// (Ego::Graphics::activeBillboardSystem); these methods are thin delegators so the
// existing EngineContext callers and the install-via-EngineContext test stubs keep
// routing through the same single installed pointer.
void EngineContext::installBillboardSystem(Ego::Graphics::IBillboardSystem& billboardSystem)
{
    Ego::Graphics::installActiveBillboardSystem(billboardSystem);
}

void EngineContext::clearBillboardSystem()
{
    Ego::Graphics::clearActiveBillboardSystem();
}

Ego::Graphics::IBillboardSystem* EngineContext::tryBillboardSystem()
{
    return Ego::Graphics::tryActiveBillboardSystem();
}

const Ego::Graphics::IBillboardSystem* EngineContext::tryBillboardSystem() const
{
    return Ego::Graphics::tryActiveBillboardSystem();
}

Ego::Graphics::IBillboardSystem& EngineContext::billboardSystem()
{
    return Ego::Graphics::activeBillboardSystem();
}

const Ego::Graphics::IBillboardSystem& EngineContext::billboardSystem() const
{
    return Ego::Graphics::activeBillboardSystem();
}

void EngineContext::installTextureAtlasManager(Ego::Graphics::ITextureAtlasManager& textureAtlasManager)
{
    if (activeTextureAtlasManager)
    {
        throw std::logic_error("texture atlas manager already installed");
    }
    activeTextureAtlasManager = &textureAtlasManager;
}

void EngineContext::clearTextureAtlasManager()
{
    activeTextureAtlasManager = nullptr;
}

Ego::Graphics::ITextureAtlasManager* EngineContext::tryTextureAtlasManager()
{
    return activeTextureAtlasManager;
}

const Ego::Graphics::ITextureAtlasManager* EngineContext::tryTextureAtlasManager() const
{
    return activeTextureAtlasManager;
}

Ego::Graphics::ITextureAtlasManager& EngineContext::textureAtlasManager()
{
    Ego::Graphics::ITextureAtlasManager* currentTextureAtlasManager = tryTextureAtlasManager();
    if (!currentTextureAtlasManager)
    {
        throw std::logic_error("no active texture atlas manager");
    }
    return *currentTextureAtlasManager;
}

const Ego::Graphics::ITextureAtlasManager& EngineContext::textureAtlasManager() const
{
    const Ego::Graphics::ITextureAtlasManager* currentTextureAtlasManager = tryTextureAtlasManager();
    if (!currentTextureAtlasManager)
    {
        throw std::logic_error("no active texture atlas manager");
    }
    return *currentTextureAtlasManager;
}

void EngineContext::installGFX(IGFX& gfx)
{
    if (activeGFX)
    {
        throw std::logic_error("GFX already installed");
    }
    activeGFX = &gfx;
}

void EngineContext::clearGFX()
{
    activeGFX = nullptr;
}

IGFX* EngineContext::tryGFX()
{
    return activeGFX;
}

const IGFX* EngineContext::tryGFX() const
{
    return activeGFX;
}

IGFX& EngineContext::gfx()
{
    IGFX* currentGFX = tryGFX();
    if (!currentGFX)
    {
        throw std::logic_error("no active GFX");
    }
    return *currentGFX;
}

const IGFX& EngineContext::gfx() const
{
    const IGFX* currentGFX = tryGFX();
    if (!currentGFX)
    {
        throw std::logic_error("no active GFX");
    }
    return *currentGFX;
}

void EngineContext::installVideoBufferManager(idlib::video_buffer_manager& mgr)
{
    Ego::installActiveVideoBufferManager(mgr);
}

void EngineContext::clearVideoBufferManager()
{
    Ego::clearActiveVideoBufferManager();
}

idlib::video_buffer_manager* EngineContext::tryVideoBufferManager()
{
    return Ego::tryActiveVideoBufferManager();
}

const idlib::video_buffer_manager* EngineContext::tryVideoBufferManager() const
{
    return Ego::tryActiveVideoBufferManager();
}

idlib::video_buffer_manager& EngineContext::videoBufferManager()
{
    return Ego::activeVideoBufferManager();
}

const idlib::video_buffer_manager& EngineContext::videoBufferManager() const
{
    return Ego::activeVideoBufferManager();
}

void EngineContext::installConfig(egoboo_config_t& config)
{
    Ego::installActiveConfig(config);
}

void EngineContext::clearConfig()
{
    Ego::clearActiveConfig();
}

egoboo_config_t* EngineContext::tryConfig()
{
    return Ego::tryActiveConfig();
}

const egoboo_config_t* EngineContext::tryConfig() const
{
    return Ego::tryActiveConfig();
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

// The engine-installed log-target override now lives in the Log subsystem
// (Log::installActiveTarget / Log/_Include.cpp). These EngineContext entry points
// stay as thin downward delegators so existing engine-layer callers and the
// installed-vs-uninstalled semantics (notably the raw nullptr returned by
// tryLogTarget(), which vfs.c's bootstrap guard relies on) are preserved exactly.
void EngineContext::installLogTarget(Log::Target& logTarget)
{
    Log::installActiveTarget(logTarget);
}

void EngineContext::clearLogTarget()
{
    Log::clearActiveTarget();
}

Log::Target* EngineContext::tryLogTarget()
{
    return Log::tryInstalledTarget();
}

const Log::Target* EngineContext::tryLogTarget() const
{
    return Log::tryInstalledTarget();
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

// Renderer lifecycle delegates to the lower-layer ownership-move seam in
// egolib/Renderer/ActiveRenderer.cpp, so lower-layer callers (texture/font managers, the OpenGL
// backend) can reach the installed renderer without an upward dependency on this hub. Declarations
// are unchanged, so all existing callers keep working.
void EngineContext::installRenderer(Ego::Renderer& renderer)
{
    Ego::installActiveRenderer(renderer);
}

void EngineContext::clearRenderer()
{
    Ego::clearActiveRenderer();
}

Ego::Renderer* EngineContext::tryRenderer()
{
    return Ego::tryActiveRenderer();
}

const Ego::Renderer* EngineContext::tryRenderer() const
{
    return Ego::tryActiveRenderer();
}

Ego::Renderer& EngineContext::renderer()
{
    return Ego::activeRenderer();
}

const Ego::Renderer& EngineContext::renderer() const
{
    return Ego::activeRenderer();
}

uint32_t EngineContext::renderedFrameCount() const
{
    return engine().getNumberOfFramesRendered();
}

std::shared_ptr<IPlayingStateController> EngineContext::tryActivePlayingState() const
{
    const GameEngine* currentEngine = tryEngine();
    if (!currentEngine)
    {
        return nullptr;
    }
    return currentEngine->getActivePlayingState();
}

std::shared_ptr<IPlayingStateController> EngineContext::activePlayingState() const
{
    std::shared_ptr<IPlayingStateController> state = tryActivePlayingState();
    if (!state)
    {
        throw std::logic_error("no active playing state");
    }
    return state;
}
