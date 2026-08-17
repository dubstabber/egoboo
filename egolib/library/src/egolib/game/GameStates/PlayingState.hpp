//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/GameStates/PlayingState.hpp
/// @details Main state where the players are currently playing a module
/// @author Johan Jansen

#pragma once

#include "egolib/game/GameStates/GameState.hpp"
#include "egolib/game/IPlayingStateController.hpp"
#include "egolib/typedef.h"

//Forward declarations
class Object;
class IModuleStatus;
class IProfileSystem;

namespace Ego { namespace GUI {
class CharacterWindow;
class MiniMap;
class CharacterStatus;
class MessageLog;
} }

/// @brief Decision logic for PlayingState::~PlayingState()'s player-export-on-shutdown branch,
/// extracted so it is testable without constructing the (UIManager/GL-dependent) PlayingState
/// itself. True only when @a moduleStatus reports an export-valid module AND @a profileSystem
/// (used afterward to reload the saved-character list) is still installed.
///
/// These two can diverge on the abnormal teardown corridor: an exception escaping the main loop
/// makes Main.cpp call EngineContext::clearEngine() directly (bypassing GameEngine::uninitialize()),
/// which clears the EngineContext-owned profile-system registry (EngineContext.cpp's
/// clearProfileSystem()) before the game-state stack is destroyed (activeEngine.reset() is
/// clearEngine()'s last step). @a moduleStatus is a separate, GameSessionContext-owned registry
/// (installed/cleared only by beginModule()/quitModule(), never touched by clearEngine()), so it
/// stays live on that same corridor.
bool shouldExportPlayersOnShutdown(const IModuleStatus* moduleStatus, const IProfileSystem* profileSystem);

class PlayingState : public GameState, public IPlayingStateController
{
public:
    PlayingState();

    ~PlayingState();

    void update() override;

    void beginState() override;

    void draw(Ego::GUI::DrawingContext& drawingContext) override {
        drawContainer(drawingContext);
    }

    bool notifyKeyboardKeyPressed(const Ego::Events::KeyboardKeyPressedEvent& e) override;

    const std::shared_ptr<Ego::GUI::MiniMap>& getMiniMap() const;

    bool showMiniMap() override;

    void setMiniMapShowPlayerPosition(bool showPlayerPosition) override;

    void addMiniMapBlip(float x, float y, const std::shared_ptr<const Ego::Texture>& icon) override;

    void addMessageLogMessage(const std::string& message) override;

    void addStatusMonitor(ObjectRef objectRef) override;

    ObjectRef getStatusCharacterRef(size_t index) override;

    void displayCharacterWindow(uint8_t statusNumber) override;

    const std::shared_ptr<Ego::GUI::MessageLog>& getMessageLog() const;

    void endModuleInVictory() override;

protected:
    void drawContainer(Ego::GUI::DrawingContext& drawingContext) override;

private:
    void updateStatusBarPosition();

private:
    std::shared_ptr<Ego::GUI::MiniMap> _miniMap;
    std::shared_ptr<Ego::GUI::MessageLog> _messageLog;
    std::vector<std::weak_ptr<Ego::GUI::CharacterStatus>> _statusList;
    std::array<std::weak_ptr<Ego::GUI::CharacterWindow>, 8> _characterWindows;
};
