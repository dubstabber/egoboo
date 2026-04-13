#include "egolib/game/Core/EngineContext.hpp"

#include "egolib/game/Core/GameEngine.hpp"

#include <stdexcept>

EngineContext& EngineContext::get()
{
    static EngineContext instance;
    return instance;
}

GameEngine* EngineContext::tryEngine()
{
    return _gameEngine.get();
}

const GameEngine* EngineContext::tryEngine() const
{
    return _gameEngine.get();
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
