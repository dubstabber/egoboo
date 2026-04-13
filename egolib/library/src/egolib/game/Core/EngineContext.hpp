#pragma once

#include "idlib/non_copyable.hpp"

#include <memory>

class GameEngine;
class PlayingState;

class EngineContext : private idlib::non_copyable
{
public:
    static EngineContext& get();

    GameEngine* tryEngine();
    const GameEngine* tryEngine() const;

    GameEngine& engine();
    const GameEngine& engine() const;

    std::shared_ptr<PlayingState> tryActivePlayingState() const;
    std::shared_ptr<PlayingState> activePlayingState() const;

private:
    EngineContext() = default;
};
