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

/// @file egolib/game/Core/ActiveGameEngine.cpp
/// @brief Definition of the active-game-engine ownership-move seam.

#include "egolib/game/Core/ActiveGameEngine.hpp"

#include <stdexcept>

namespace
{
GameEngine* g_activeGameEngine = nullptr;
}

void installActiveGameEngine(GameEngine& engine)
{
    if (g_activeGameEngine)
    {
        throw std::logic_error("game engine already installed");
    }
    g_activeGameEngine = &engine;
}

void clearActiveGameEngine()
{
    g_activeGameEngine = nullptr;
}

GameEngine* tryActiveGameEngine()
{
    return g_activeGameEngine;
}

GameEngine& activeGameEngine()
{
    if (!g_activeGameEngine)
    {
        throw std::logic_error("no active game engine");
    }
    return *g_activeGameEngine;
}
