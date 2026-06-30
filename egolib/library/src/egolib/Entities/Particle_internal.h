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

/// @file egolib/Entities/Particle_internal.h
/// @brief Shared infrastructure for the split Particle implementation files.

#pragma once

#define GAME_ENTITIES_PRIVATE 1

#include "egolib/Entities/Particle.hpp"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Entities/_Include.hpp"

namespace particle_detail
{
inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline GameModule& activeModule()
{
    return gameSession().activeModule();
}

inline ObjectHandler& worldObjectHandler()
{
    return Ego::Entities::activeObjectHandler();
}

inline Object* tryWorldObject(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveObject(objectRef);
}

inline const Object* tryWorldConstObject(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveConstObject(objectRef);
}

inline bool worldObjectExists(ObjectRef objectRef)
{
    return Ego::Entities::activeObjectExists(objectRef);
}

inline IModuleEnvironment& moduleEnvironment()
{
    return activeModuleEnvironment();
}

inline water_instance_t& moduleWater()
{
    return moduleEnvironment().water();
}

inline std::shared_ptr<ego_mesh_t> moduleMesh()
{
    return moduleEnvironment().mesh();
}

inline ISessionState& sessionState()
{
    if (ISessionState* session = tryActiveSessionState())
    {
        return *session;
    }

    return gameSession();
}

inline uint32_t worldUpdateCount()
{
    return sessionState().worldUpdateCount();
}
} // namespace particle_detail

using namespace particle_detail;
