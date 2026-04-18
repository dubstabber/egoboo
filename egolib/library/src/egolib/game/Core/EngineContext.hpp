#pragma once

#include "egolib/Audio/IAudioSystem.hpp"
#include "idlib/non_copyable.hpp"

#include <cstdint>
#include <memory>

class GameEngine;
class PlayingState;
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

    uint32_t renderedFrameCount() const;

    std::shared_ptr<PlayingState> tryActivePlayingState() const;
    std::shared_ptr<PlayingState> activePlayingState() const;

private:
    EngineContext() = default;
};
