/// @file egolib/game/script_functions_spawn.c
/// @brief EgoScript dispatch entries for character lifecycle, drop/cleanup, identification,
///        and miscellaneous "spawn-adjacent" state mutations.
///        The actual spawn calls live in script_functions_spawn_character.c (character spawn /
///        respawn / attach / morph) and script_functions_spawn_particle.c (particle spawn / poof /
///        affirm-attached-particles).

#include "egolib/game/script_functions_spawn_internal.h"

namespace
{
template <typename Fn>
void forEachLiveSpawnObjectRef(Fn&& fn)
{
    ObjectHandler* handler = Ego::Entities::tryActiveObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const ObjectRef& ref : handler->objectRefIterator())
    {
        if (ref != ObjectRef::Invalid)
        {
            fn(ref);
        }
    }
}

void publishCleanedUpState(IScriptable& listener)
{
    listener.addAIAlertBits(ALERTIF_CLEANEDUP);
}

bool trySetChildState(ObjectRef childRef, int stateValue)
{
    IScriptable* child = tryScriptable(childRef);
    if (child == nullptr)
    {
        return false;
    }

    child->setAIStateValue(stateValue);
    return true;
}

bool trySetChildContent(ObjectRef childRef, int contentValue)
{
    IScriptable* child = tryScriptable(childRef);
    if (child == nullptr)
    {
        return false;
    }

    child->setAIContent(contentValue);
    return true;
}

bool trySetChildAmmo(ObjectRef childRef, int ammoValue)
{
    ICharacterState* child = tryCharacterState(childRef);
    if (child == nullptr)
    {
        return false;
    }

    child->setAmmo(Ego::Math::constrain(ammoValue, 0, 0xFFFF));
    return true;
}

void publishImmediatePoof(IScriptable& target)
{
    target.setAIPoofTime(worldUpdateCount());
}

bool trySetSelfPoofTime(ai_state_t& self, bool isPlayer, uint32_t updateOffset = 0)
{
    if (isPlayer)
    {
        return false;
    }

    self.poof_time = worldUpdateCount() + updateOffset;
    return true;
}

bool identifyResolvedTarget(ObjectProfile& selfProfile, ObjectRef targetRef)
{
    ICharacterState* targetState = tryCharacterState(targetRef);
    const ITargetInfo* targetInfoRole = tryTargetInfo(targetRef);
    IVisualControl* targetVisual = tryVisualControl(targetRef);
    if (targetState == nullptr || targetInfoRole == nullptr || targetVisual == nullptr)
    {
        return false;
    }

    bool identifiedUnknownName = !targetInfoRole->isNameKnown();
    if (targetState->getAmmoMax() != 0)
    {
        targetVisual->setAmmoKnown(true);
    }

    targetVisual->setNameKnown(true);
    selfProfile.makeUsageKnown();
    return identifiedUnknownName;
}

bool tryDetachSelfFromHolder(ObjectRef objectRef)
{
    const ITargetInfo* selfInfo = tryTargetInfo(objectRef);
    ILifecycleControl* lifecycle = tryLifecycleControl(objectRef);
    if (selfInfo == nullptr || lifecycle == nullptr || !isLiveSpawnObjectRef(selfInfo->getHolderRef()))
    {
        return false;
    }

    return lifecycle->detachFromHolder(true, true);
}

bool trySetTargetToChild(ai_state_t& self)
{
    if (!isLiveSpawnObjectRef(self.child))
    {
        return false;
    }

    self.setTarget(self.child);
    return true;
}

bool trySetSelfDamageTimer(ai_state_t& self, int damageTime)
{
    IDamageable* selfDamageable = tryDamageable(self.getSelf());
    if (selfDamageable == nullptr)
    {
        return false;
    }

    selfDamageable->setDamageTimer(static_cast<uint8_t>(Ego::Math::constrain(damageTime, 0, 0xFFFF)));
    return true;
}

bool trySetSelfInvincibility(ai_state_t& self, bool invincible)
{
    IDamageable* selfDamageable = tryDamageable(self.getSelf());
    if (selfDamageable == nullptr)
    {
        return false;
    }

    selfDamageable->setInvincible(invincible);
    return true;
}

void publishCleanupTimerForDeadListener(const ITargetInfo& listenerInfo, IScriptable& listener)
{
    if (!listenerInfo.isAlive())
    {
        listener.setAITimer(worldUpdateCount() + 2);  // Don't let it think too much...
    }
}

void publishCleanUpForSameTeamListener(TEAM_REF teamRef, ObjectRef listenerRef)
{
    const ITargetInfo* listenerInfo = tryTargetInfo(listenerRef);
    IScriptable* listener = tryScriptable(listenerRef);
    if (listenerInfo == nullptr || listener == nullptr)
    {
        return;
    }

    if (teamRef != listenerInfo->getTeamRef())
    {
        return;
    }

    publishCleanupTimerForDeadListener(*listenerInfo, *listener);
    publishCleanedUpState(*listener);
}

void applyDismountVelocity(IMovementControl& movement)
{
    movement.setVelocity({movement.getVelocity().x(),
                        movement.getVelocity().y(),
                        Object::DISMOUNTZVEL});
    movement.setJumpTimer(Object::JUMPDELAY);
    movement.movePosition(0.0f, 0.0f, Object::DISMOUNTZVEL);
}

void dropHeldObject(const IInventoryHolder& holder, slot_t slot, bool holderIsMount)
{
    const ObjectRef itemRef = holder.getHeldObject(slot);
    ILifecycleControl* itemLifecycle = tryLifecycleControl(itemRef);
    IMovementControl* itemMovement = tryMovementControl(itemRef);
    if (itemLifecycle == nullptr || itemMovement == nullptr)
    {
        return;
    }

    itemLifecycle->detachFromHolder(true, true);
    if (holderIsMount)
    {
        applyDismountVelocity(*itemMovement);
    }
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropWeapons( script_state_t& state, ai_state_t& self )
{
    // DropWeapons()
    /// @author ZZ
    /// @details This function drops the character's in-hand items.  It will also
    /// buck the rider if the character is a mount

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    // This funtion drops the character's in hand items/riders
    const bool selfIsMount = selfContext.targetInfo->isMount();
    dropHeldObject(*selfContext.inventory, SLOT_LEFT, selfIsMount);
    dropHeldObject(*selfContext.inventory, SLOT_RIGHT, selfIsMount);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GoPoof( script_state_t& state, ai_state_t& self )
{
    // GoPoof()
    /// @author ZZ
    /// @details This function flags the character to be removed from the game entirely.
    /// This doesn't work on players

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    return trySetSelfPoofTime(self, selfContext.targetInfo->isPlayer());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropKeys( script_state_t& state, ai_state_t& self )
{
    // DropKeys()
    /// @author ZZ
    /// @details This function drops all of the keys in the character's inventory.
    /// This does NOT drop keys in the character's hands.

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->dropKeys();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DetachFromHolder( script_state_t& state, ai_state_t& self )
{
    // DetachFromHolder()
    /// @author ZZ
    /// @details This function drops the character or makes it get off its mount
    /// Can be used to make slippery weapons, or to make certain characters
    /// incapable of wielding certain weapons. "A troll can't grab a torch"

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return tryDetachSelfFromHolder(self.getSelf());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CleanUp( script_state_t& state, ai_state_t& self )
{
    // CleanUp()
    /// @author ZZ
    /// @details This function tells all the dead characters on the team to clean
    /// themselves up.  Usually done by the boss creature every second or so

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    const TEAM_REF selfTeam = selfContext.targetInfo->getTeamRef();
    forEachLiveSpawnObjectRef([&](ObjectRef listenerRef)
    {
        publishCleanUpForSameTeamListener(selfTeam, listenerRef);
    });

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushValid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushValid()
    /// @author ZZ
    /// @details This function makes a character able to be crushed by closing doors
    /// and such

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->setCanBeCrushed(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PoofTarget( script_state_t& state, ai_state_t& self )
{
    // PoofTarget()
    /// @author ZZ
    /// @details This function removes the target from the game, failing if the
    /// target is a player

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    if (targetInventory->isPlayer())             //Do not poof players
    {
        return false;
    }

    if ( self.getTarget() == self.getSelf() )
    {
        // Poof self later
        trySetSelfPoofTime(self, false, 1);
        return true;
    }

    // Poof others now
    IScriptable* targetScriptable = tryScriptable(self.getTarget());
    if (targetScriptable == nullptr)
    {
        return false;
    }

    publishImmediatePoof(*targetScriptable);
    self.setTarget(self.getSelf());
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildState( script_state_t& state, ai_state_t& self )
{
    // SetChildState( tmpargument = "state" )
    /// @author ZZ
    /// @details This function lets a character set the state of the last character it
    /// spawned

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetChildState(self.child, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropItems( script_state_t& state, ai_state_t& self )
{
    // DropItems()
    /// @author ZZ
    /// @details This function drops all of the items the character is holding

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->dropAllItems();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnTarget( script_state_t& state, ai_state_t& self )
{
    // RespawnTarget()
    /// @author ZZ
    /// @details This function respawns the target at its current location

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    ILifecycleControl* targetLifecycle = tryLifecycleControl(self.getTarget());
    if (targetLifecycle == nullptr)
    {
        return false;
    }

    targetLifecycle->respawnInPlace();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_NotAnItem( script_state_t& state, ai_state_t& self )
{
    // NotAnItem()
    /// @author ZZ
    /// @details This function makes the character a non-item character.
    /// Usage: Used for spells that summon creatures

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->setItem(false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildAmmo( script_state_t& state, ai_state_t& self )
{
    // SetChildAmmo( tmpargument = "none" )
    /// @author ZZ
    /// @details This function sets the ammo of the last character spawned by this character

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetChildAmmo(self.child, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IdentifyTarget( script_state_t& state, ai_state_t& self )
{
    // IdentifyTarget()
    /// @author ZZ
    /// @details This function reveals the target's name, ammo, and usage
    /// Proceeds if the target was unknown

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    return identifyResolvedTarget(*selfContext.profile, self.getTarget());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetKeys( script_state_t& state, ai_state_t& self )
{
    // DropTargetKeys()
    /// @author ZZ
    /// @details This function makes the Target drops keys in inventory (Not inhand)

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    ILifecycleControl* targetLifecycle = tryLifecycleControl(self.getTarget());
    if (targetLifecycle == nullptr)
    {
        return false;
    }

    targetLifecycle->dropKeys();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushInvalid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushInvalid()
    /// @author ZZ
    /// @details This function makes doors unable to close on this object

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->setCanBeCrushed(false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageTime( script_state_t& state, ai_state_t& self )
{
    // SetDamageTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function makes the character invincible for a little while

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetSelfDamageTimer(self, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToChild( script_state_t& state, ai_state_t& self )
{
    // SetTargetToChild()
    /// @author ZF
    /// @details This function sets the target to the character it spawned last (also called it's "child")

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetTargetToChild(self);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageThreshold( script_state_t& state, ai_state_t& self )
{
    // SetDamageThreshold()
    /// @author ZF
    /// @details This sets the damage treshold for this character. Damage below the threshold is ignored

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    const int v = state.argument;
    if (v > 0)
    {
        selfContext.lifecycle->setDamageThreshold(v);
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildContent( script_state_t& state, ai_state_t& self )
{
    // SetChildContent( tmpargument = "content" )
    /// @author ZF
    /// @details This function lets a character set the content of the last character it
    /// spawned last

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetChildContent(self.child, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableInvictus( script_state_t& state, ai_state_t& self )
{
    // EnableInvictus()
    /// @author ZF
    /// @details This function makes the character invulerable

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetSelfInvincibility(self, true);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableInvictus( script_state_t& state, ai_state_t& self )
{
    // DisableInvictus()
    /// @author ZF
    /// @details This function makes the character not invulerable

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    return trySetSelfInvincibility(self, false);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetSize( script_state_t& state, ai_state_t& self )
{
    // SetTargetSize( tmpargument = "percent" )
    /// @author ZF
    /// @details This changes the AI target's size

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    IMorphControl* targetMorph = tryMorphControl(self.getTarget());
    if (targetMorph == nullptr)
    {
        return false;
    }

    targetMorph->setTargetFat(targetMorph->getTargetFat() * state.argument / 100.0f);
    targetMorph->setResizeTimeRemaining(targetMorph->getResizeTimeRemaining() + Object::SIZETIME);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableStealth( script_state_t& state, ai_state_t& self )
{
    // EnableStealth()
    /// @author ZF
    /// @details Makes the object enter stealth mode. Returns true if it is now hidden from others.

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    if (selfContext.targetInfo->isStealthed()) {
        return false;
    }

    return selfContext.lifecycle->activateStealth();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableStealth( script_state_t& state, ai_state_t& self )
{
    // DisableStealth()
    /// @author ZF
    /// @details Makes the object exit stealth mode. Returns true if it exited stealth mode.

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    const bool wasStealthed = selfContext.targetInfo->isStealthed();
    selfContext.lifecycle->deactivateStealth();

    return wasStealthed;
}
