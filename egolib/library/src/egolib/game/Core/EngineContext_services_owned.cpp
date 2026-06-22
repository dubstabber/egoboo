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

// EngineContext implementation -- compatibility delegators for recently moved ownership seams.
//
// The installed pointers for input, image, font, texture-atlas, and GFX now live in their
// subsystem active-X seams. These EngineContext entry points remain so existing install/clear
// callers and tests keep using the same public context API while lower-layer callers can move off
// the app-layer hub.

void EngineContext::installInputSystem(Ego::Input::IInputSystem& inputSystem)
{
    Ego::Input::installActiveInputSystem(inputSystem);
}

void EngineContext::clearInputSystem()
{
    Ego::Input::clearActiveInputSystem();
}

Ego::Input::IInputSystem* EngineContext::tryInputSystem()
{
    return Ego::Input::tryActiveInputSystem();
}

const Ego::Input::IInputSystem* EngineContext::tryInputSystem() const
{
    return Ego::Input::tryActiveInputSystem();
}

Ego::Input::IInputSystem& EngineContext::inputSystem()
{
    return Ego::Input::activeInputSystem();
}

const Ego::Input::IInputSystem& EngineContext::inputSystem() const
{
    return Ego::Input::activeInputSystem();
}

void EngineContext::installImageManager(Ego::IImageManager& imageManager)
{
    Ego::installActiveImageManager(imageManager);
}

void EngineContext::clearImageManager()
{
    Ego::clearActiveImageManager();
}

Ego::IImageManager* EngineContext::tryImageManager()
{
    return Ego::tryActiveImageManager();
}

const Ego::IImageManager* EngineContext::tryImageManager() const
{
    return Ego::tryActiveImageManager();
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
    Ego::installActiveFontManager(fontManager);
}

void EngineContext::clearFontManager()
{
    Ego::clearActiveFontManager();
}

Ego::IFontManager* EngineContext::tryFontManager()
{
    return Ego::tryActiveFontManager();
}

const Ego::IFontManager* EngineContext::tryFontManager() const
{
    return Ego::tryActiveFontManager();
}

Ego::IFontManager& EngineContext::fontManager()
{
    return Ego::activeFontManager();
}

const Ego::IFontManager& EngineContext::fontManager() const
{
    return Ego::activeFontManager();
}

void EngineContext::installTextureAtlasManager(Ego::Graphics::ITextureAtlasManager& textureAtlasManager)
{
    Ego::Graphics::installActiveTextureAtlasManager(textureAtlasManager);
}

void EngineContext::clearTextureAtlasManager()
{
    Ego::Graphics::clearActiveTextureAtlasManager();
}

Ego::Graphics::ITextureAtlasManager* EngineContext::tryTextureAtlasManager()
{
    return Ego::Graphics::tryActiveTextureAtlasManager();
}

const Ego::Graphics::ITextureAtlasManager* EngineContext::tryTextureAtlasManager() const
{
    return Ego::Graphics::tryActiveTextureAtlasManager();
}

Ego::Graphics::ITextureAtlasManager& EngineContext::textureAtlasManager()
{
    return Ego::Graphics::activeTextureAtlasManager();
}

const Ego::Graphics::ITextureAtlasManager& EngineContext::textureAtlasManager() const
{
    return Ego::Graphics::activeTextureAtlasManager();
}

void EngineContext::installGFX(IGFX& gfx)
{
    installActiveGFX(gfx);
}

void EngineContext::clearGFX()
{
    clearActiveGFX();
}

IGFX* EngineContext::tryGFX()
{
    return tryActiveGFX();
}

const IGFX* EngineContext::tryGFX() const
{
    return tryActiveGFX();
}

IGFX& EngineContext::gfx()
{
    return activeGFX();
}

const IGFX& EngineContext::gfx() const
{
    return activeGFX();
}
