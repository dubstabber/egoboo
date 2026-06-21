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

// EngineContext implementation -- seam-delegated service registries.
//
// The services whose ownership has been moved to lower-layer active-X ownership seams; these
// EngineContext entry points are thin downward delegators (audio, perk handler, graphics
// system, texture manager, particle handler, profile system, camera system, billboard system,
// video-buffer manager, config, log target, renderer). The directly-owned services live in
// EngineContext_services_owned.cpp and the engine-core lifecycle in EngineContext.cpp.

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
