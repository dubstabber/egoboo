/// @file egolib/game/script_functions_spawn.c
/// @brief Character/particle spawning, destruction, and lifecycle management

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
struct SpawnSelfContext
{
    Object& object;
    ObjectProfile* profile = nullptr;
    IScriptable* scriptable = nullptr;
    const ITargetInfo* targetInfo = nullptr;
    const IInventoryHolder* inventory = nullptr;
    ILifecycleControl* lifecycle = nullptr;
    std::string name;
    std::string className;
};

struct SpawnAttachmentTargetContext
{
    ObjectRef ref = ObjectRef::Invalid;
    IInventoryHolder* inventory = nullptr;
};

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

IScriptable& scriptable(Object& object)
{
    return object;
}

const ITargetInfo& targetInfo(const Object& object)
{
    return object;
}

const IInventoryHolder& inventoryHolder(const Object& object)
{
    return object;
}

ICharacterState& characterState(Object& object)
{
    return object;
}

ILifecycleControl& lifecycleControl(Object& object)
{
    return object;
}

IMovementControl& movementControl(Object& object)
{
    return object;
}

bool isLiveSpawnObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
}

template <typename Fn>
void forEachLiveSpawnObjectRef(Fn&& fn)
{
    ObjectHandler* handler = gameSession().tryObjectHandler();
    if (handler == nullptr)
    {
        return;
    }

    for (const auto& object : handler->iterator())
    {
        const ObjectRef ref = object != nullptr ? object->getObjRef() : ObjectRef::Invalid;
        if (ref != ObjectRef::Invalid)
        {
            fn(ref);
        }
    }
}

std::shared_ptr<Object> resolveSpawnObjectHandle(ObjectRef objectRef)
{
    ObjectHandler* handler = gameSession().tryObjectHandler();
    if (handler == nullptr || !handler->exists(objectRef))
    {
        return nullptr;
    }

    return (*handler)[objectRef];
}

SpawnSelfContext makeSpawnSelfContext(Object& object)
{
    const std::shared_ptr<ObjectProfile> profile = object.getProfile();
    return SpawnSelfContext{
        object,
        profile.get(),
        static_cast<IScriptable*>(&object),
        static_cast<const ITargetInfo*>(&object),
        static_cast<const IInventoryHolder*>(&object),
        static_cast<ILifecycleControl*>(&object),
        object.getName(),
        profile ? profile->getClassName() : std::string()
    };
}

SpawnSelfContext makeSpawnSelfContext(const ai_state_t& self)
{
    const ResolvedSelfContext resolvedSelf = resolveSelfContext(self);
    return makeSpawnSelfContext(*resolvedSelf.object);
}

bool resolveSpawnAttachmentTarget(const ai_state_t& self,
                                  SpawnAttachmentTargetContext& context)
{
    context.ref = self.getTarget();
    context.inventory = tryInventoryHolder(context.ref);
    return context.inventory != nullptr;
}

void logSelfCopySpawnFailure(const SpawnSelfContext& selfContext)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning,
                                                           __FILE__,
                                                           __LINE__,
                                                           "object ",
                                                           "`",
                                                           selfContext.name,
                                                           "`",
                                                           " failed to spawn a copy of itself",
                                                           Log::EndOfEntry);
}

void logUnsafeSelfCopySpawnFailure(const SpawnSelfContext& selfContext)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning,
                                                           __FILE__,
                                                           __LINE__,
                                                           "object ",
                                                           "`",
                                                           selfContext.name,
                                                           "`",
                                                           " failed to spawn a copy of itself (no safe location)",
                                                           Log::EndOfEntry);
}

void logAttachedCharacterSpawnFailure(const SpawnSelfContext& selfContext,
                                      int profileIndex)
{
    EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning,
                                                           __FILE__,
                                                           __LINE__,
                                                           "object ",
                                                           "`",
                                                           selfContext.name,
                                                           "`",
                                                           "/",
                                                           "`",
                                                           selfContext.className,
                                                           "`",
                                                           " failed to spawn profile index ",
                                                           profileIndex,
                                                           Log::EndOfEntry);
}

void inheritSpawnScriptState(IScriptable& child, const ai_state_t& self)
{
    child.setAIPassage(self.passage);
    child.setAIOwner(self.owner);
}

void publishGrabbedAlert(IScriptable& child)
{
    child.addAIAlertBits(ALERTIF_GRABBED);
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

void applySpawnVelocity(IMovementControl& movement, Facing facing, int distance)
{
    movement.setVelocity(movement.getVelocity() +
                         Ego::Vector3f(std::cos(facing) * distance,
                                       std::sin(facing) * distance,
                                       0.0f));
}

void publishSpawnDismount(ILifecycleControl& lifecycle, ObjectRef dismountObjectRef)
{
    lifecycle.setDismountTimer(Object::PHYS_DISMOUNT_TIME);
    lifecycle.setDismountObject(dismountObjectRef);
}

void publishSpawnChildState(Object& child, bool inheritKurse, ai_state_t& self)
{
    self.child = child.getObjRef();

    ICharacterState& childState = characterState(child);
    childState.setKursed(inheritKurse);

    inheritSpawnScriptState(scriptable(child), self);
    publishSpawnDismount(lifecycleControl(child), self.getSelf());
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

ObjectRef resolveHolderOrSelfRef(const Object& object)
{
    const ObjectRef holderRef = targetInfo(object).getHolderRef();
    return isLiveSpawnObjectRef(holderRef) ? holderRef : object.getObjRef();
}

ObjectRef resolveSpawnParticleOwnerRef(const Object& object)
{
    ObjectRef ownerRef = resolveHolderOrSelfRef(object);
    if (targetInfo(object).isMount())
    {
        const ObjectRef riderRef = inventoryHolder(object).getHeldObject(SLOT_LEFT);
        if (isLiveSpawnObjectRef(riderRef))
        {
            ownerRef = riderRef;
        }
    }

    return ownerRef;
}

ObjectRef resolveLowestAttachmentOrSelfRef(ObjectRef selfRef)
{
    const ObjectRef attachmentRef = chr_get_lowest_attachment(selfRef, true);
    return isLiveSpawnObjectRef(attachmentRef) ? attachmentRef : selfRef;
}

std::shared_ptr<Object> spawnCharacterAt(const Ego::Vector3f& position,
                                         ObjectProfileRef profile,
                                         TEAM_REF teamRef,
                                         Facing facing)
{
    return activeModule().spawnObject(position, profile, teamRef, 0, facing, "", ObjectRef::Invalid);
}

void setModuleRespawnValid(bool valid)
{
    activeModule().setRespawnValid(valid);
}

std::shared_ptr<Object> spawnCharacterLikeSelf(const SpawnSelfContext& selfContext,
                                               const Ego::Vector3f& position,
                                               Facing facing)
{
    return spawnCharacterAt(position,
                            selfContext.object.getProfileID(),
                            selfContext.object.getTeamRef(),
                            facing);
}

bool publishAttachedChildState(Object& child,
                               ai_state_t& self)
{
    self.child = child.getObjRef();
    inheritSpawnScriptState(scriptable(child), self);
    return true;
}

bool publishCopiedChildState(const SpawnSelfContext& selfContext,
                             const std::shared_ptr<Object>& child,
                             ai_state_t& self)
{
    if (child == nullptr)
    {
        return false;
    }

    publishSpawnChildState(*child, selfContext.object.isKursed(), self);
    return true;
}

bool finalizeSafeSelfCopySpawn(const SpawnSelfContext& selfContext,
                               const std::shared_ptr<Object>& child,
                               ai_state_t& self,
                               int initialVelocity)
{
    if (child == nullptr)
    {
        logSelfCopySpawnFailure(selfContext);
        return false;
    }

    if (!child->hasSafePosition())
    {
        logUnsafeSelfCopySpawnFailure(selfContext);
        child->requestTerminate();
        return true;
    }

    const Facing turn = selfContext.object.getFacingZ() + ATK_BEHIND;
    applySpawnVelocity(movementControl(*child), turn, initialVelocity);
    return publishCopiedChildState(selfContext, child, self);
}

bool spawnPoofForSelf(ObjectRef selfRef)
{
    const std::shared_ptr<Object> selfObject = resolveSpawnObjectHandle(selfRef);
    if (selfObject == nullptr)
    {
        return false;
    }

    EngineContext::get().particleHandler().spawnPoof(selfObject);
    return true;
}

std::shared_ptr<Ego::Particle> spawnLocalParticleForSelf(const SpawnSelfContext& selfContext,
                                                         const Ego::Vector3f& position,
                                                         Facing facing,
                                                         LocalParticleProfileRef profile,
                                                         ObjectRef attachedObjectRef,
                                                         int distance,
                                                         ObjectRef ownerRef)
{
    return EngineContext::get().particleHandler().spawnLocalParticle(position,
                                                                     facing,
                                                                     ObjectProfileRef(selfContext.object.getProfileID()),
                                                                     profile,
                                                                     attachedObjectRef,
                                                                     distance,
                                                                     selfContext.object.getTeamRef(),
                                                                     ownerRef,
                                                                     ParticleRef::Invalid,
                                                                     0,
                                                                     ObjectRef::Invalid);
}

bool tryAttachParticleToResolvedSelf(const std::shared_ptr<Ego::Particle>& particle,
                                     ObjectRef selfRef,
                                     int vertex,
                                     int xOffset,
                                     int yOffset)
{
    const std::shared_ptr<Object> selfObject = resolveSpawnObjectHandle(selfRef);
    if (particle == nullptr || selfObject == nullptr)
    {
        return false;
    }

    particle->placeAtVertex(selfObject, vertex);
    particle->attach(ObjectRef::Invalid);

    Ego::Vector3f adjustedPosition = particle->getPosition();
    adjustedPosition.z() += particle->getProfile()->getSpawnPositionOffsetZ().base;

    adjustedPosition.x() += xOffset;
    if (EMPTY_BIT_FIELD != particle->test_wall(adjustedPosition))
    {
        adjustedPosition.x() = particle->getPosX();

        adjustedPosition.y() += yOffset;
        if (EMPTY_BIT_FIELD != particle->test_wall(adjustedPosition))
        {
            adjustedPosition.y() = particle->getPosY();
        }
    }

    particle->setPosition(adjustedPosition);
    return true;
}

bool tryAttachSpawnedInventoryChild(const SpawnSelfContext& selfContext,
                                    const SpawnAttachmentTargetContext& targetContext,
                                    const std::shared_ptr<Object>& child,
                                    ai_state_t& self)
{
    if (child == nullptr)
    {
        return false;
    }

    if (!Inventory::add_item(targetContext.ref,
                             child->getObjRef(),
                             selfContext.object.getFirstFreeInventorySlot(),
                             true))
    {
        child->requestTerminate();
        return true;
    }

    publishGrabbedAlert(*child);
    child->setHolderRef(targetContext.ref);
    scr_run_chr_script(child->getObjRef());
    child->setHolderRef(ObjectRef::Invalid);
    return publishAttachedChildState(*child, self);
}

bool tryAttachSpawnedGripChild(const SpawnAttachmentTargetContext& targetContext,
                               uint8_t grip,
                               const std::shared_ptr<Object>& child,
                               ai_state_t& self)
{
    if (child == nullptr)
    {
        return false;
    }

    const slot_t slot = (grip == ATTACH_LEFT) ? SLOT_LEFT : SLOT_RIGHT;
    if (isLiveSpawnObjectRef(targetContext.inventory->getHeldObject(slot)))
    {
        child->requestTerminate();
        return true;
    }

    const grip_offset_t gripOffset = (grip == ATTACH_LEFT) ? GRIP_LEFT : GRIP_RIGHT;
    if (child->attachToObject(targetContext.ref, gripOffset))
    {
        scr_run_chr_script(child->getObjRef());
    }

    return publishAttachedChildState(*child, self);
}

bool resolveSpawnAttachedCharacterPlacement(const SpawnSelfContext& selfContext,
                                            const SpawnAttachmentTargetContext& targetContext,
                                            uint8_t grip,
                                            const std::shared_ptr<Object>& child,
                                            ai_state_t& self)
{
    if (grip == ATTACH_INVENTORY)
    {
        return tryAttachSpawnedInventoryChild(selfContext, targetContext, child, self);
    }

    if (grip == ATTACH_LEFT || grip == ATTACH_RIGHT)
    {
        return tryAttachSpawnedGripChild(targetContext, grip, child, self);
    }

    return child != nullptr && publishAttachedChildState(*child, self);
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropWeapons( script_state_t& state, ai_state_t& self )
{
    // DropWeapons()
    /// @author ZZ
    /// @details This function drops the character's in-hand items.  It will also
    /// buck the rider if the character is a mount

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    // This funtion drops the character's in hand items/riders
    const bool selfIsMount = selfContext.targetInfo->isMount();
    dropHeldObject(*selfContext.inventory, SLOT_LEFT, selfIsMount);
    dropHeldObject(*selfContext.inventory, SLOT_RIGHT, selfIsMount);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GoPoof( script_state_t& state, ai_state_t& self )
{
    // GoPoof()
    /// @author ZZ
    /// @details This function flags the character to be removed from the game entirely.
    /// This doesn't work on players

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    returncode = trySetSelfPoofTime(self, selfContext.targetInfo->isPlayer());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropKeys( script_state_t& state, ai_state_t& self )
{
    // DropKeys()
    /// @author ZZ
    /// @details This function drops all of the keys in the character's inventory.
    /// This does NOT drop keys in the character's hands.

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->dropKeys();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnCharacter( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacter( tmpx = "x", tmpy = "y", tmpturn = "turn", tmpdistance = "speed" )

    /// @author ZZ
    /// @details This function spawns a character of the same type as the spawner.
    /// This function spawns a character, failing if x,y is invalid
    /// This is horribly complicated to use, so see ANIMATE.OBJ for an example
    /// tmpx and tmpy give the coodinates, tmpturn gives the new character's
    /// direction, and tmpdistance gives the new character's initial velocity

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(static_cast<float>(state.x),
                                 static_cast<float>(state.y),
                                 selfContext.object.getPosZ());
    const std::shared_ptr<Object> child = spawnCharacterLikeSelf(selfContext,
                                                                 position,
                                                                 Facing(Ego::Math::clipBits<16>(state.turn)));
    returncode = finalizeSafeSelfCopySpawn(selfContext, child, self, state.distance);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnCharacter( script_state_t& state, ai_state_t& self )
{
    // RespawnCharacter()
    /// @author ZZ
    /// @details This function respawns the character at its starting location.
    /// Often used with the Clean functions

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->respawn();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DetachFromHolder( script_state_t& state, ai_state_t& self )
{
    // DetachFromHolder()
    /// @author ZZ
    /// @details This function drops the character or makes it get off its mount
    /// Can be used to make slippery weapons, or to make certain characters
    /// incapable of wielding certain weapons. "A troll can't grab a torch"

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = tryDetachSelfFromHolder(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CleanUp( script_state_t& state, ai_state_t& self )
{
    // CleanUp()
    /// @author ZZ
    /// @details This function tells all the dead characters on the team to clean
    /// themselves up.  Usually done by the boss creature every second or so

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    const TEAM_REF selfTeam = selfContext.targetInfo->getTeamRef();
    forEachLiveSpawnObjectRef([&](ObjectRef listenerRef)
    {
        publishCleanUpForSameTeamListener(selfTeam, listenerRef);
    });

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnParticle(tmpargument = "particle", tmpdistance = "character vertex", tmpx = "offset x", tmpy = "offset y" )
    /// @author ZZ
    /// @details This function spawns a particle, offset from the character's location

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveSpawnParticleOwnerRef(selfContext.object);
    const std::shared_ptr<Ego::Particle> particle =
        spawnLocalParticleForSelf(selfContext,
                                  selfContext.object.getPosition(),
                                  Facing(uint16_t(selfContext.object.getFacingZ())),
                                  LocalParticleProfileRef(state.argument),
                                  self.getSelf(),
                                  state.distance,
                                  ownerRef);

    returncode = (particle != nullptr);
    if (returncode)
    {
        returncode = tryAttachParticleToResolvedSelf(particle,
                                                     self.getSelf(),
                                                     state.distance,
                                                     state.x,
                                                     state.y);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisaffirmCharacter( script_state_t& state, ai_state_t& self )
{
    // DisaffirmCharacter()
    /// @author ZZ
    /// @details This function removes all the attached particles from a character
    /// ( stuck arrows, flames, etc )

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    disaffirm_attached_particles(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ReaffirmCharacter( script_state_t& state, ai_state_t& self )
{
    // ReaffirmCharacter()
    /// @author ZZ
    /// @details This function makes sure it has all of its reaffirmation particles
    /// attached to it. Used to make the torch light again

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    reaffirm_attached_particles(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedParticle( tmpargument = "particle", tmpdistance = "vertex" )
    /// @author ZZ
    /// @details This function spawns a particle attached to the character

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveLowestAttachmentOrSelfRef(self.getSelf());
    returncode = nullptr != spawnLocalParticleForSelf(selfContext,
                                                      selfContext.object.getPosition(),
                                                      idlib::canonicalize(selfContext.object.getFacingZ()),
                                                      LocalParticleProfileRef(state.argument),
                                                      self.getSelf(),
                                                      state.distance,
                                                      ownerRef);
    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);

    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    returncode = nullptr != spawnLocalParticleForSelf(selfContext,
                                                      position,
                                                      idlib::canonicalize(selfContext.object.getFacingZ()),
                                                      LocalParticleProfileRef(state.argument),
                                                      ObjectRef::Invalid,
                                                      0,
                                                      ownerRef);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushValid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushValid()
    /// @author ZZ
    /// @details This function makes a character able to be crushed by closing doors
    /// and such

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->setCanBeCrushed(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PoofTarget( script_state_t& state, ai_state_t& self )
{
    // PoofTarget()
    /// @author ZZ
    /// @details This function removes the target from the game, failing if the
    /// target is a player

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    IInventoryHolder* targetInventory = tryInventoryHolder(self.getTarget());
    if (targetInventory == nullptr)
    {
        return false;
    }

    returncode = false;
    if (!targetInventory->isPlayer())             //Do not poof players
    {
        returncode = true;
        if ( self.getTarget() == self.getSelf() )
        {
            // Poof self later
            trySetSelfPoofTime(self, false, 1);
        }
        else
        {
            // Poof others now
            IScriptable* targetScriptable = tryScriptable(self.getTarget());
            if (targetScriptable == nullptr)
            {
                return false;
            }

            publishImmediatePoof(*targetScriptable);
            self.setTarget(self.getSelf());
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnPoof( script_state_t& state, ai_state_t& self )
{
    // SpawnPoof
    /// @author ZZ
    /// @details This function makes a lovely little poof at the character's location.
    /// The poof form and particle types are set in data.txt

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = spawnPoofForSelf(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildState( script_state_t& state, ai_state_t& self )
{
    // SetChildState( tmpargument = "state" )
    /// @author ZZ
    /// @details This function lets a character set the state of the last character it
    /// spawned

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetChildState(self.child, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedSizedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedSizedParticle( tmpargument = "particle", tmpdistance = "vertex", tmpturn = "size" )
    /// @author ZZ
    /// @details This function spawns a particle of the specific size attached to the
    /// character. For spell charging effects

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    const std::shared_ptr<Ego::Particle> particle =
        spawnLocalParticleForSelf(selfContext,
                                  selfContext.object.getPosition(),
                                  idlib::canonicalize(selfContext.object.getFacingZ()),
                                  LocalParticleProfileRef(state.argument),
                                  self.getSelf(),
                                  state.distance,
                                  ownerRef);

    returncode = (particle != nullptr);

    if ( returncode )
    {
        particle->setSize(state.turn);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedFacedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedFacedParticle(  tmpargument = "particle", tmpdistance = "vertex", tmpturn = "turn" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character, facing the
    /// same direction given by tmpturn

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    returncode = nullptr != spawnLocalParticleForSelf(selfContext,
                                                      selfContext.object.getPosition(),
                                                      Facing(Ego::Math::clipBits<16>(state.turn)),
                                                      LocalParticleProfileRef(state.argument),
                                                      self.getSelf(),
                                                      state.distance,
                                                      ownerRef);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedHolderParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedHolderParticle( tmpargument = "particle", tmpdistance = "vertex" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character's holder, or to the character if no holder

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    returncode = nullptr != spawnLocalParticleForSelf(selfContext,
                                                      selfContext.object.getPosition(),
                                                      idlib::canonicalize(selfContext.object.getFacingZ()),
                                                      LocalParticleProfileRef(state.argument),
                                                      ownerRef,
                                                      state.distance,
                                                      ownerRef);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnCharacterXYZ( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacterXYZ( tmpx = "x", tmpy = "y", tmpdistance = "z", tmpturn = "turn" )
    /// @author ZZ
    /// @details This function spawns a character of the same type at a specific location, failing if x,y,z is invalid

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(float(state.x), float(state.y), float(state.distance));
    const std::shared_ptr<Object> child = spawnCharacterLikeSelf(selfContext,
                                                                 position,
                                                                 Facing(Ego::Math::clipBits<16>(state.turn)));
    if (child == nullptr)
    {
        logSelfCopySpawnFailure(selfContext);
        returncode = false;
    }
    else
    {
        returncode = publishCopiedChildState(selfContext, child, self);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactCharacterXYZ( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacterXYZ( tmpargument = "slot", tmpx = "x", tmpy = "y", tmpdistance = "z", tmpturn = "turn" )
    /// @author ZZ
    /// @details This function spawns a character at a specific location, using a
    /// specific model type, failing if x,y,z is invalid
    /// DON'T USE THIS FOR EXPORTABLE ITEMS OR CHARACTERS,
    /// AS THE MODEL SLOTS MAY VARY FROM MODULE TO MODULE.

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    const std::shared_ptr<Object> child = spawnCharacterAt(position,
                                                           ObjectProfileRef(static_cast<PRO_REF>(state.argument)),
                                                           selfContext.object.getTeamRef(),
                                                           Facing(Ego::Math::clipBits<16>(state.turn)));

    if (!child)
    {
        returncode = false;
    }
    else
    {
        returncode = publishCopiedChildState(selfContext, child, self);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactChaseParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactChaseParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position,
    /// that will home in on the character's target

    std::shared_ptr<Ego::Particle> particle;

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    particle = spawnLocalParticleForSelf(selfContext,
                                         position,
                                         idlib::canonicalize(selfContext.object.getFacingZ()),
                                         LocalParticleProfileRef(state.argument),
                                         ObjectRef::Invalid,
                                         0,
                                         ownerRef);

    returncode = (particle != nullptr);

    if ( returncode )
    {
        particle->setTarget(self.getTarget());
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropItems( script_state_t& state, ai_state_t& self )
{
    // DropItems()
    /// @author ZZ
    /// @details This function drops all of the items the character is holding

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->dropAllItems();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnTarget( script_state_t& state, ai_state_t& self )
{
    // RespawnTarget()
    /// @author ZZ
    /// @details This function respawns the target at its current location

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    ILifecycleControl* targetLifecycle = tryLifecycleControl(self.getTarget());
    if (targetLifecycle == nullptr)
    {
        return false;
    }

    targetLifecycle->respawnInPlace();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_NotAnItem( script_state_t& state, ai_state_t& self )
{
    // NotAnItem()
    /// @author ZZ
    /// @details This function makes the character a non-item character.
    /// Usage: Used for spells that summon creatures

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->setItem(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildAmmo( script_state_t& state, ai_state_t& self )
{
    // SetChildAmmo( tmpargument = "none" )
    /// @author ZZ
    /// @details This function sets the ammo of the last character spawned by this character

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetChildAmmo(self.child, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IdentifyTarget( script_state_t& state, ai_state_t& self )
{
    // IdentifyTarget()
    /// @author ZZ
    /// @details This function reveals the target's name, ammo, and usage
    /// Proceeds if the target was unknown

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    returncode = identifyResolvedTarget(*selfContext.profile, self.getTarget());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetKeys( script_state_t& state, ai_state_t& self )
{
    // DropTargetKeys()
    /// @author ZZ
    /// @details This function makes the Target drops keys in inventory (Not inhand)

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    ILifecycleControl* targetLifecycle = tryLifecycleControl(self.getTarget());
    if (targetLifecycle == nullptr)
    {
        return false;
    }

    targetLifecycle->dropKeys();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushInvalid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushInvalid()
    /// @author ZZ
    /// @details This function makes doors unable to close on this object

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->setCanBeCrushed(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageTime( script_state_t& state, ai_state_t& self )
{
    // SetDamageTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function makes the character invincible for a little while

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetSelfDamageTimer(self, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactParticleEndSpawn( script_state_t& state, ai_state_t& self )
{
    // SpawnExactParticleEndSpawn( tmpargument = "particle", tmpturn = "state", tmpx = "x", tmpy = "y", tmpdistance = "z" )

    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position.
    /// When the particle ends, a character is spawned at its final location.
    /// The character is the same type of whatever spawned the particle.

    std::shared_ptr<Ego::Particle> particle;

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    const Ego::Vector3f position(float(state.x),
                                 float(state.y),
                                 float(state.distance));
    particle = spawnLocalParticleForSelf(selfContext,
                                         position,
                                         idlib::canonicalize(selfContext.object.getFacingZ()),
                                         LocalParticleProfileRef(state.argument),
                                         ObjectRef::Invalid,
                                         0,
                                         ownerRef);

    returncode = (particle != nullptr);

    if ( returncode )
    {
        particle->endspawn_characterstate = state.turn;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnPoofSpeedSpacingDamage( script_state_t& state, ai_state_t& self )
{
    // SpawnPoofSpeedSpacingDamage( tmpx = "xy speed", tmpy = "xy spacing", tmpargument = "damage" )

    /// @author ZZ
    /// @details This function makes a lovely little poof at the character's location,
    /// adjusting the xy speed and spacing and the base damage
    /// Temporarily adjust the values for the particle type

    //ZF> Note: This script function seems to be only used by the Fireball spell, so its use is VERY limited

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    PIP_REF ipip = selfContext.profile->getParticlePoofProfile();
    if ( INVALID_PIP_REF == ipip) return false;
    const std::shared_ptr<ParticleProfile> &ppip = EngineContext::get().profileSystem().getParticleProfile(ipip);

    returncode = false;
    if (ppip != nullptr)
    {
        const float velOffsetBase = static_cast<float>(state.x);
        const float posOffsetBase = static_cast<float>(state.y);
        const float damage_rand = ppip->damage.length();

        Facing facing_z = selfContext.object.getFacingZ();
        for (int cnt = 0; cnt < selfContext.profile->getParticlePoofAmount(); cnt++)
        {
            auto poofParticle = EngineContext::get().particleHandler().spawnParticle(selfContext.object.getOldPosition(),
                                                                                     facing_z,
                                                                                     selfContext.profile->getSlotNumber(),
                                                                                     ipip,
                                                                                     ObjectRef::Invalid,
                                                                                     GRIP_LAST,
                                                                                     selfContext.object.getTeamRef(),
                                                                                     selfContext.scriptable->getAIOwner(),
                                                                                     ParticleRef::Invalid,
                                                                                     cnt);

            // set some values
            if(poofParticle) {

                //Add random horizontal velocity offset
                Ego::Vector2f xyVelOffset = Ego::Vector2f(velOffsetBase + Random::next(ppip->getSpawnVelocityOffsetXY().rand), velOffsetBase + Random::next(ppip->getSpawnVelocityOffsetXY().rand));
                poofParticle->setVelocity(poofParticle->getVelocity() +
                                          Ego::Vector3f(xyVelOffset.x(), xyVelOffset.y(), 0.0f));

                //Add random horizontal position offset
                Ego::Vector2f xyPosOffset = Ego::Vector2f(posOffsetBase + Random::next(ppip->getSpawnPositionOffsetXY().rand), posOffsetBase + Random::next(ppip->getSpawnPositionOffsetXY().rand));
                poofParticle->setPosition(poofParticle->getPosX() + xyPosOffset.x(), poofParticle->getPosY() + xyPosOffset.y(), poofParticle->getPosZ());

                //Adjust damage
                poofParticle->damage.base = FP8_TO_FLOAT(state.argument);
                poofParticle->damage.rand = ppip->damage.lower() + damage_rand;

                //Success! We have spawned at least one poof
                returncode = true;
            }

            facing_z += Facing(selfContext.profile->getParticlePoofFacingAdd());
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableRespawn( script_state_t& state, ai_state_t& self )
{
    // EnableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button on

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    setModuleRespawnValid(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableRespawn( script_state_t& state, ai_state_t& self )
{
    // DisableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button off

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    setModuleRespawnValid(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedCharacter( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedCharacter( tmpargument = "profile", tmpx = "x", tmpy = "y", tmpdistance = "z" )

    /// @author ZF
    /// @details This function spawns a character defined in tmpargument to the characters AI target using
    /// the slot specified in tmpdistance (LEFT, RIGHT or INVENTORY). Fails if the inventory or
    /// grip specified is full or already in use.
    /// DON'T USE THIS FOR EXPORTABLE ITEMS OR CHARACTERS,
    /// AS THE MODEL SLOTS MAY VARY FROM MODULE TO MODULE.
    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    SpawnAttachmentTargetContext targetContext;
    if (!resolveSpawnAttachmentTarget(self, targetContext))
    {
        return false;
    }

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(float(state.x), float(state.y), float(state.distance));
    const std::shared_ptr<Object> child = spawnCharacterAt(position,
                                                           ObjectProfileRef((PRO_REF)state.argument),
                                                           selfContext.object.getTeamRef(),
                                                           FACE_NORTH);
    returncode = child != nullptr;

    if (!returncode)
    {
        logAttachedCharacterSpawnFailure(selfContext, state.argument);
    }
    else
    {
        const uint8_t grip = Ego::Math::constrain<int>(state.distance,
                                                       ATTACH_INVENTORY,
                                                       ATTACH_RIGHT);
        returncode = resolveSpawnAttachedCharacterPlacement(selfContext,
                                                            targetContext,
                                                            grip,
                                                            child,
                                                            self);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToChild( script_state_t& state, ai_state_t& self )
{
    // SetTargetToChild()
    /// @author ZF
    /// @details This function sets the target to the character it spawned last (also called it's "child")

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetTargetToChild(self);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageThreshold( script_state_t& state, ai_state_t& self )
{
    // SetDamageThreshold()
    /// @author ZF
    /// @details This sets the damage treshold for this character. Damage below the threshold is ignored

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    const int v = state.argument;
    if (v > 0)
    {
        selfContext.lifecycle->setDamageThreshold(v);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MorphToTarget( script_state_t& state, ai_state_t& self )
{
    // MorphToTarget()
    /// @author ZF
    /// @details This morphs the character into the target
    /// Also set size and keeps the previous AI type

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    IMorphControl* selfMorph = tryMorphControl(self.getSelf());
    IMorphControl* targetMorph = tryMorphControl(self.getTarget());
    if (selfMorph == nullptr || targetMorph == nullptr)
    {
        return false;
    }

    selfMorph->polymorphObject(targetMorph->getBaseModelRef(), targetMorph->getSkin());

    // let the resizing take some time
    selfMorph->setTargetFat(targetMorph->getFat());
    selfMorph->setResizeTimeRemaining(Object::SIZETIME);

    // Keep the current AI script behavior unchanged.

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildContent( script_state_t& state, ai_state_t& self )
{
    // SetChildContent( tmpargument = "content" )
    /// @author ZF
    /// @details This function lets a character set the content of the last character it
    /// spawned last

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetChildContent(self.child, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableInvictus( script_state_t& state, ai_state_t& self )
{
    // EnableInvictus()
    /// @author ZF
    /// @details This function makes the character invulerable

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetSelfInvincibility(self, true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableInvictus( script_state_t& state, ai_state_t& self )
{
    // DisableInvictus()
    /// @author ZF
    /// @details This function makes the character not invulerable

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    returncode = trySetSelfInvincibility(self, false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetSize( script_state_t& state, ai_state_t& self )
{
    // SetTargetSize( tmpargument = "percent" )
    /// @author ZF
    /// @details This changes the AI target's size

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;

    IMorphControl* targetMorph = tryMorphControl(self.getTarget());
    if (targetMorph == nullptr)
    {
        return false;
    }

    targetMorph->setTargetFat(targetMorph->getTargetFat() * state.argument / 100.0f);
    targetMorph->setResizeTimeRemaining(targetMorph->getResizeTimeRemaining() + Object::SIZETIME);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableStealth( script_state_t& state, ai_state_t& self )
{
    // EnableStealth()
    /// @author ZF
    /// @details Makes the object enter stealth mode. Returns true if it is now hidden from others.

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    if (selfContext.targetInfo->isStealthed()) {
        returncode = false;
    }
    else {
        returncode = selfContext.lifecycle->activateStealth();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableStealth( script_state_t& state, ai_state_t& self )
{
    // DisableStealth()
    /// @author ZF
    /// @details Makes the object exit stealth mode. Returns true if it exited stealth mode.

    uint8_t returncode = true;
    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    returncode = selfContext.targetInfo->isStealthed();
    selfContext.lifecycle->deactivateStealth();

    SCRIPT_FUNCTION_END();
}
