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

namespace Ego { namespace GUI { 
class CharacterWindow;
class MiniMap;
class CharacterStatus;
class MessageLog;
} }

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

    const std::shared_ptr<Ego::GUI::MiniMap>& getMiniMap() const override;

    void addStatusMonitor(ObjectRef objectRef) override;

    ObjectRef getStatusCharacterRef(size_t index) override;

    void displayCharacterWindow(uint8_t statusNumber) override;

    const std::shared_ptr<Ego::GUI::MessageLog>& getMessageLog() const override;

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
