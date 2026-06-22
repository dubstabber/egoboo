/// @file egolib/game/script_functions_movement_internal.h
/// @brief Shared self-movement context helpers for the split script_functions_movement* TUs

#pragma once

#include "egolib/game/script_functions_internal.h"

namespace script_movement_detail
{
struct SelfMovementContext
{
    IMovementControl* movement = nullptr;
    const IPhysical* physical = nullptr;

    bool isResolved() const
    {
        return movement != nullptr &&
               physical != nullptr;
    }
};

inline SelfMovementContext makeSelfMovementContext(const ai_state_t& self)
{
    SelfMovementContext context;
    const ObjectRef selfRef = self.getSelf();
    context.movement = tryMovementControl(selfRef);
    context.physical = tryPhysical(selfRef);
    return context;
}

inline bool hasResolvedSelf(const ai_state_t& self)
{
    return hasLiveSelf(self);
}
}

using namespace script_movement_detail;
