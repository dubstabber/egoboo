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

/// @file egolib/game/script_functions_state_internal.h
/// @brief Shared self-state-context helper for the two script_functions_state*.c TUs.
/// @details Private to script_functions_state.c / script_functions_state_inventory.c —
/// do not include from other script_functions_*.c TUs.

#pragma once

#include "egolib/game/script_functions_internal.h"

namespace script_state_detail
{
struct SelfStateContext
{
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    const IInventoryHolder* inventory = nullptr;
    const IScriptable* scriptable = nullptr;
    IVisualControl* visual = nullptr;

    bool isResolved() const
    {
        return object != nullptr &&
               profile != nullptr &&
               targetInfo != nullptr &&
               inventory != nullptr &&
               scriptable != nullptr &&
               visual != nullptr;
    }
};

inline SelfStateContext makeSelfStateContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    SelfStateContext context;
    context.object = resolvedSelf.object;
    context.profile = resolvedSelf.profile;
    if (!resolvedSelf.isResolved())
    {
        return context;
    }

    context.targetInfo = static_cast<const ITargetInfo*>(resolvedSelf.object);
    context.inventory = static_cast<const IInventoryHolder*>(resolvedSelf.object);
    context.scriptable = static_cast<const IScriptable*>(resolvedSelf.object);
    context.visual = static_cast<IVisualControl*>(resolvedSelf.object);
    return context;
}
}
using namespace script_state_detail;
