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

/// @file egolib/game/script_functions_spawn_internal.h
/// @brief Shared spawn-helper infrastructure for the three script_functions_spawn*.c TUs.
/// @details Private to script_functions_spawn.c / script_functions_spawn_particle.c /
/// script_functions_spawn_character.c — do not include from other script_functions_*.c TUs.

#pragma once

#include "egolib/game/script_functions_internal.h"

namespace script_spawn_detail
{
struct SpawnSelfContext
{
    ObjectRef ref = ObjectRef::Invalid;
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    ObjectProfile* profile = nullptr;
    const IPhysical* physical = nullptr;
    IScriptable* scriptable = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    const IInventoryHolder* inventory = nullptr;
    ILifecycleControl* lifecycle = nullptr;
    Ego::Vector3f oldPosition;
    std::string name;
    std::string className;

    bool isResolved() const
    {
        return ref != ObjectRef::Invalid &&
               profile != nullptr &&
               physical != nullptr &&
               scriptable != nullptr &&
               targetInfo != nullptr &&
               inventory != nullptr &&
               lifecycle != nullptr;
    }
};

inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline bool isLiveSpawnObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
}

inline SpawnSelfContext makeSpawnSelfContextFromObject(Object& object)
{
    const std::shared_ptr<ObjectProfile> profile = object.getProfile();
    return SpawnSelfContext{
        object.getObjRef(),
        object.getProfileID(),
        profile.get(),
        static_cast<const IPhysical*>(&object),
        static_cast<IScriptable*>(&object),
        static_cast<const ITargetInfo*>(&object),
        static_cast<const IInventoryHolder*>(&object),
        static_cast<ILifecycleControl*>(&object),
        object.getOldPosition(),
        object.getName(),
        profile ? profile->getClassName() : std::string()
    };
}

inline SpawnSelfContext resolveSpawnSelfContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    if (!resolvedSelf.isResolved())
    {
        return {};
    }

    return makeSpawnSelfContextFromObject(*resolvedSelf.object);
}
}
using namespace script_spawn_detail;
