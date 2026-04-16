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

/// @file egolib/game/Module/Module_internal.h
/// @brief Shared infrastructure for the split GameModule implementation files.

#pragma once

#include "egolib/game/Module/Module.hpp"

#include "egolib/Math/Random.hpp"
#include "egolib/Logic/Team.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/Logic/TreasureTables.hpp"

#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/game.h"
#include "egolib/game/graphic.h"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/CharacterMatrix.h"

#include "egolib/game/Physics/CollisionSystem.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace module_detail
{
inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline uint32_t& worldUpdateCount()
{
    return gameSession().worldUpdateCount();
}

inline uint32_t& characterStatClock()
{
    return gameSession().characterStatClock();
}

inline import_list_t& importList()
{
    return gameSession().importList();
}

inline bool& overrideSlots()
{
    return gameSession().overrideSlots();
}

class OverrideSlotsScope
{
public:
    explicit OverrideSlotsScope(bool& slotOverride) :
        _slotOverride(slotOverride)
    {
        _slotOverride = true;
    }

    ~OverrideSlotsScope()
    {
        _slotOverride = false;
    }

private:
    bool& _slotOverride;
};
} // namespace module_detail

using namespace module_detail;
