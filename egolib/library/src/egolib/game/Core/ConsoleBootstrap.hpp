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

/// @file egolib/game/Core/ConsoleBootstrap.hpp
/// @details RAII composition-root for the in-game developer console.

#pragma once

#include "idlib/non_copyable.hpp"

#include <functional>

/// @brief RAII composition-root for the in-game developer console.
///
/// Construction sizes and initializes the console from the active graphics
/// window and subscribes the built-in command handler (grog/daze/exit).
/// Destruction uninitializes the console. The handler's only dependency on game
/// state — whether a module is currently being played — is injected as a
/// predicate so this object does not depend on the concrete @c GameEngine or
/// playing-state types.
class ConsoleBootstrap : private idlib::non_copyable
{
public:
    /// @param isPlaying predicate returning @c true while a module is being played.
    explicit ConsoleBootstrap(std::function<bool()> isPlaying);
    ~ConsoleBootstrap();
};
