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

/// @file egolib/game/game_internal.h
/// @brief Shared infrastructure for the split game implementation files.

#pragma once

#include "egolib/game/game.h"

#include "egolib/egolib.h"
#include "egolib/FileFormats/Globals.hpp"

#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/link.h"
#include "egolib/game/script_implementation.h"
#include "egolib/game/egoboo.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Physics/CollisionSystem.hpp"
#include "egolib/game/physics.h"
#include "egolib/game/Physics/PhysicalConstants.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/GUI/MessageLog.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/graphic_fan.h"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Graphics/Billboard.hpp"

namespace game_detail
{

inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline GameModule& activeModule()
{
    return gameSession().activeModule();
}

inline uint32_t& worldUpdateCount()
{
    return gameSession().worldUpdateCount();
}

inline uint32_t& characterStatClock()
{
    return gameSession().characterStatClock();
}

inline std::shared_ptr<PlayingState> activePlayingState()
{
    return EngineContext::get().activePlayingState();
}

inline std::shared_ptr<PlayingState> tryActivePlayingState()
{
    return EngineContext::get().tryActivePlayingState();
}

} // namespace game_detail

using namespace game_detail;
