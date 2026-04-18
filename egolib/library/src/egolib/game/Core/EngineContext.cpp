#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GUI/UIManager.hpp"

#include <stdexcept>

namespace
{
std::unique_ptr<GameEngine> activeEngine;
IAudioSystem* activeAudioSystem = nullptr;
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
