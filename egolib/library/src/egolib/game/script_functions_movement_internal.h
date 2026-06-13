/// @file egolib/game/script_functions_movement_internal.h
/// @brief Shared self-movement context helpers for the split script_functions_movement* TUs

#pragma once

#include "egolib/game/script_functions_internal.h"

namespace script_movement_detail
{
struct SelfMovementContext
{
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    IMovementControl* movement = nullptr;
    const IPhysical* physical = nullptr;

    bool isResolved() const
    {
        return object != nullptr &&
               profile != nullptr &&
               movement != nullptr &&
               physical != nullptr;
    }
};

inline SelfMovementContext makeSelfMovementContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    SelfMovementContext context;
    context.object = resolvedSelf.object;
    context.profile = resolvedSelf.profile;
    if (!resolvedSelf.isResolved())
    {
        return context;
    }

    context.movement = static_cast<IMovementControl*>(resolvedSelf.object);
    context.physical = static_cast<const IPhysical*>(resolvedSelf.object);
    return context;
}

inline bool hasResolvedSelf(const ai_state_t& self)
{
    return resolveSelfContext(self).isResolved();
}
}

using namespace script_movement_detail;
