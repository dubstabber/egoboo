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

/// @file egolib/game/IPlayingStateController.hpp
/// @brief Lower-layer seam exposing the in-game (PlayingState) control surface that game-core code
///        reaches into — minimap/message-log/status access plus module-victory. Lets the game-core
///        library reach the active PlayingState without depending on the concrete PlayingState type
///        (which lives in the higher GameStates layer). PlayingState implements this interface, and
///        @c tryActivePlayingStateController() resolves it from the active game state via a
///        @c dynamic_cast whose source/target typeinfos (GameState, IPlayingStateController) are both
///        lower-layer — so no concrete-PlayingState link edge is created. This is the keystone that
///        lets the GameStates screens carve into an archive above egolib-library.

#pragma once

#include <memory>
#include <cstddef>
#include <cstdint>

#include "egolib/typedef.h"   // ObjectRef

namespace Ego { namespace GUI {
class MiniMap;
class MessageLog;
} }

/// @brief The control surface of the active in-game state, as needed by the game-core library.
class IPlayingStateController {
public:
    virtual ~IPlayingStateController() = default;

    /// @brief The in-game minimap widget.
    virtual const std::shared_ptr<Ego::GUI::MiniMap>& getMiniMap() const = 0;

    /// @brief The in-game scrolling message log widget.
    virtual const std::shared_ptr<Ego::GUI::MessageLog>& getMessageLog() const = 0;

    /// @brief The character bound to the given on-screen status slot.
    virtual ObjectRef getStatusCharacterRef(size_t index) = 0;

    /// @brief Add an on-screen status monitor for the given character.
    virtual void addStatusMonitor(ObjectRef objectRef) = 0;

    /// @brief Open the character info window for the given status slot.
    virtual void displayCharacterWindow(uint8_t statusNumber) = 0;

    /// @brief End the current module by pushing the victory screen.
    virtual void endModuleInVictory() = 0;
};
