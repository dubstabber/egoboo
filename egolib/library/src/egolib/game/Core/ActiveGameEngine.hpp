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

/// @file egolib/game/Core/ActiveGameEngine.hpp
/// @brief Ownership-move seam for the running game engine (mirrors Ego::activeRenderer /
///        Ego::activeGraphicsSystem / Ego::GUI::activeUIManager).
/// @details Lets a consumer reach the installed @c GameEngine without depending on the app-layer
///          @c EngineContext. @c EngineContext owns the @c GameEngine (it is the single instance) and
///          delegates the install/clear of this seam from @c setEngine / @c clearEngine, so the
///          installed engine is always @c EngineContext::get().engine(). This frees the @c GameState
///          base class (which only needs the engine reference) of its sole game-core coupling. The
///          declaration deliberately keeps to a forward declaration of @c GameEngine and pulls in no
///          game-core header, so it can be included from lower-layer-bound translation units.

#pragma once

class GameEngine;

/// @brief Install the active game engine.
/// @param engine the game engine to install
/// @throw std::logic_error if a game engine is already installed
void installActiveGameEngine(GameEngine& engine);

/// @brief Clear the installed active game engine.
void clearActiveGameEngine();

/// @brief The installed active game engine, or @a nullptr if none is installed.
GameEngine* tryActiveGameEngine();

/// @brief The active game engine.
/// @throw std::logic_error if none is installed
GameEngine& activeGameEngine();
