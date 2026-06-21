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

// EngineContext implementation -- self-owned service registries.
//
// The services whose installed pointer EngineContext owns DIRECTLY in a file-local static:
// the input system, image manager, font manager, texture-atlas manager, and GFX. (Contrast the
// seam-delegated services in EngineContext_services_seam.cpp, whose ownership has been moved to
// lower-layer active-X seams.) The engine-core lifecycle lives in EngineContext.cpp.

namespace
{
Ego::Input::IInputSystem* activeInputSystem = nullptr;
Ego::IImageManager* activeImageManager = nullptr;
Ego::IFontManager* activeFontManager = nullptr;
Ego::Graphics::ITextureAtlasManager* activeTextureAtlasManager = nullptr;
IGFX* activeGFX = nullptr;
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
