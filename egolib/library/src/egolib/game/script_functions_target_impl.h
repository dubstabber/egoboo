/// @file egolib/game/script_functions_target_impl.h
/// @brief Private anonymous-namespace helpers shared between the four
///        script_functions_target*.c translation units:
///          * script_functions_target.c          (state predicates)
///          * script_functions_target_identity.c (IDSZ identity queries)
///          * script_functions_target_orders.c   (orders + getters/mutators)
///          * script_functions_target_select.c   (target acquisition / selection)
///
/// Include this header ONLY from those translation units.  It defines
/// types and helper functions inside an anonymous namespace, so each TU
/// gets its own copy — consistent with the pattern used in all other
/// script_functions_*.c files.

#pragma once

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{

struct SelfTargetSelectorContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    const IScriptable* scriptable = nullptr;
    const ITargetInfo* info = nullptr;
    const IInventoryHolder* inventory = nullptr;
    const IPhysical* physical = nullptr;
    const IAppearanceProfile* appearance = nullptr;
};

struct TargetCompatibilityContext
{
    ObjectRef ref = ObjectRef::Invalid;
    const ITargetInfo* info = nullptr;
    IInventoryHolder* inventory = nullptr;
    IDamageable* damageable = nullptr;
    IScriptable* scriptable = nullptr;
    const IPhysical* physical = nullptr;
};

bool isFacing(const IPhysical& selfPhysical, const IPhysical& targetPhysical)
{
    FACING_T facing = FACING_T(vec_to_facing(targetPhysical.getPosX() - selfPhysical.getPosX(),
                                             targetPhysical.getPosY() - selfPhysical.getPosY()));
    facing -= FACING_T(selfPhysical.getFacingZ());
    return facing > 55535 || facing < 10000;
}

SelfTargetSelectorContext makeSelfTargetSelectorContext(const ai_state_t& self)
{
    SelfTargetSelectorContext context;
    context.selfRef = self.getSelf();
    context.scriptable = tryScriptable(context.selfRef);
    context.info = tryTargetInfo(context.selfRef);
    context.inventory = tryInventoryHolder(context.selfRef);
    context.physical = tryPhysical(context.selfRef);
    context.appearance = tryAppearanceProfile(context.selfRef);
    return context;
}

TargetCompatibilityContext makeTargetCompatibilityContext(ObjectRef objectRef)
{
    TargetCompatibilityContext context;
    context.ref = objectRef;
    context.info = tryTargetInfo(objectRef);
    context.inventory = tryInventoryHolder(objectRef);
    context.damageable = tryDamageable(objectRef);
    context.scriptable = tryScriptable(objectRef);
    context.physical = tryPhysical(objectRef);
    return context;
}

TargetCompatibilityContext makeTargetCompatibilityContext(const ai_state_t& self)
{
    return makeTargetCompatibilityContext(self.getTarget());
}

bool isLiveTargetRef(ObjectRef objectRef)
{
    return tryTargetInfo(objectRef) != nullptr;
}

const ITargetInfo* tryResolvedTargetInfo(const ai_state_t& self)
{
    return makeTargetCompatibilityContext(self).info;
}

bool trySetResolvedTarget(ai_state_t& self, ObjectRef objectRef)
{
    if (!isLiveTargetRef(objectRef))
    {
        return false;
    }

    self.setTarget(objectRef);
    return true;
}

bool trySetTargetFromHeldObject(ai_state_t& self,
                                const IInventoryHolder& holder,
                                slot_t slot)
{
    return trySetResolvedTarget(self, holder.getHeldObject(slot));
}

bool trySetTargetFromScriptableTarget(ai_state_t& self, const IScriptable* scriptableObject)
{
    return scriptableObject != nullptr &&
           trySetResolvedTarget(self, scriptableObject->getAITarget());
}

ObjectRef selfLastAttackerRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastAttacker() : ObjectRef::Invalid;
}

ObjectRef selfBumpedRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAIBumped() : ObjectRef::Invalid;
}

ObjectRef selfLastHitRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastHit() : ObjectRef::Invalid;
}

ObjectRef selfLastItemUsedRef(const SelfTargetSelectorContext& context)
{
    return context.scriptable != nullptr ? context.scriptable->getAILastItemUsed() : ObjectRef::Invalid;
}

ObjectRef selfTeamLeaderRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? teamLeaderRef(*context.info) : ObjectRef::Invalid;
}

ObjectRef selfTeamCallerForHelpRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? teamCallerForHelpRef(*context.info) : ObjectRef::Invalid;
}

ObjectRef selfHolderRef(const SelfTargetSelectorContext& context)
{
    return context.info != nullptr ? context.info->getHolderRef() : ObjectRef::Invalid;
}

bool trySetTargetFromPassageOccupant(ai_state_t& self,
                                     int passageId,
                                     const IDSZ2& occupantIdsz,
                                     BIT_FIELD targetingBits,
                                     const IDSZ2& requiredItem)
{
    const std::shared_ptr<Passage> passage = tryPassage(passageId);
    return passage != nullptr &&
           trySetResolvedTarget(self,
                                passage->whoIsBlockingPassage(self.getSelf(),
                                                              occupantIdsz,
                                                              targetingBits,
                                                              requiredItem));
}

ObjectRef findTargetForSelf(const SelfTargetSelectorContext& context,
                            float maxDistance,
                            const IDSZ2& idsz,
                            BIT_FIELD targetingBits)
{
    return chr_find_target(context.selfRef, maxDistance, idsz, targetingBits);
}

ObjectRef findWeaponForSelf(const SelfTargetSelectorContext& context,
                            float maxDistance,
                            const IDSZ2& weaponIdsz,
                            bool findRanged,
                            bool useLineOfSight)
{
    return FindWeapon(context.selfRef, maxDistance, weaponIdsz, findRanged, useLineOfSight);
}

} // anonymous namespace
