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

/// @file egolib/game/Entities/Object_internal.h
/// @brief Shared infrastructure for the split Object implementation files.

#pragma once

#define GAME_ENTITIES_PRIVATE 1
#include "egolib/Entities/Object.hpp"

#include "egolib/Profiles/_Include.hpp"
#include "egolib/Entities/ObjectHandler.hpp"
#include "egolib/Entities/ParticleHandler.hpp"
#include "egolib/Entities/Enchant.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/game.h"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/game/script_implementation.h"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"

// For the minimap.
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/GUI/MiniMap.hpp"
#include "egolib/game/Module/Module.hpp"

namespace object_detail
{
inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline GameModule* tryActiveModule()
{
    return gameSession().tryActiveModule();
}

inline GameModule& activeModule()
{
    return gameSession().activeModule();
}

inline uint32_t worldUpdateCount()
{
    return gameSession().worldUpdateCount();
}

inline uint32_t characterStatClock()
{
    return gameSession().characterStatClock();
}

inline std::shared_ptr<PlayingState> tryActivePlayingState()
{
    return EngineContext::get().tryActivePlayingState();
}

inline const std::shared_ptr<Object>& selfHandle(const Object& object)
{
    return activeModule().getObjectHandler()[object.getObjRef()];
}
} // namespace object_detail

using namespace object_detail;
