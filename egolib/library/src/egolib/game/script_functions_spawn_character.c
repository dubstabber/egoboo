/// @file egolib/game/script_functions_spawn_character.c
/// @brief EgoScript dispatch entries that spawn, respawn, attach, or morph characters.

#include "egolib/game/script_functions_spawn_internal.h"

namespace
{
struct SpawnAttachmentTargetContext
{
    ObjectRef ref = ObjectRef::Invalid;
    IInventoryHolder* inventory = nullptr;
};

IScriptable& scriptable(Object& object)
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

void publishSpawnChildState(Object& child,
                            bool inheritKurse,
                            ObjectRef dismountObjectRef,
                            ai_state_t& self)
{
    self.child = child.getObjRef();

    ICharacterState& childState = characterState(child);
    childState.setKursed(inheritKurse);

    inheritSpawnScriptState(scriptable(child), self);
    publishSpawnDismount(lifecycleControl(child), dismountObjectRef);
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
                            selfContext.profileRef,
                            selfContext.targetInfo->getTeamRef(),
                            facing);
}

bool publishAttachedChildState(IScriptable& child,
                               ai_state_t& self)
{
    self.child = child.getObjRef();
    inheritSpawnScriptState(child, self);
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

    publishSpawnChildState(*child, selfContext.targetInfo->isKursed(), selfContext.ref, self);
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

    const Facing turn = selfContext.physical->getFacingZ() + ATK_BEHIND;
    applySpawnVelocity(movementControl(*child), turn, initialVelocity);
    return publishCopiedChildState(selfContext, child, self);
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
                             selfContext.inventory->getFirstFreeInventorySlot(),
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
uint8_t scr_SpawnCharacter( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacter( tmpx = "x", tmpy = "y", tmpturn = "turn", tmpdistance = "speed" )

    /// @author ZZ
    /// @details This function spawns a character of the same type as the spawner.
    /// This function spawns a character, failing if x,y is invalid
    /// This is horribly complicated to use, so see ANIMATE.OBJ for an example
    /// tmpx and tmpy give the coodinates, tmpturn gives the new character's
    /// direction, and tmpdistance gives the new character's initial velocity

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(static_cast<float>(state.x),
                                 static_cast<float>(state.y),
                                 selfContext.physical->getPosZ());
    const std::shared_ptr<Object> child = spawnCharacterLikeSelf(selfContext,
                                                                 position,
                                                                 Facing(Ego::Math::clipBits<16>(state.turn)));
    return finalizeSafeSelfCopySpawn(selfContext, child, self, state.distance);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnCharacter( script_state_t& state, ai_state_t& self )
{
    // RespawnCharacter()
    /// @author ZZ
    /// @details This function respawns the character at its starting location.
    /// Often used with the Clean functions

    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    selfContext.lifecycle->respawn();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnCharacterXYZ( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacterXYZ( tmpx = "x", tmpy = "y", tmpdistance = "z", tmpturn = "turn" )
    /// @author ZZ
    /// @details This function spawns a character of the same type at a specific location, failing if x,y,z is invalid

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(float(state.x), float(state.y), float(state.distance));
    const std::shared_ptr<Object> child = spawnCharacterLikeSelf(selfContext,
                                                                 position,
                                                                 Facing(Ego::Math::clipBits<16>(state.turn)));
    if (child == nullptr)
    {
        logSelfCopySpawnFailure(selfContext);
        return false;
    }

    return publishCopiedChildState(selfContext, child, self);
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

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    const std::shared_ptr<Object> child = spawnCharacterAt(position,
                                                           ObjectProfileRef(static_cast<PRO_REF>(state.argument)),
                                                           selfContext.targetInfo->getTeamRef(),
                                                           Facing(Ego::Math::clipBits<16>(state.turn)));

    if (!child)
    {
        return false;
    }

    return publishCopiedChildState(selfContext, child, self);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableRespawn( script_state_t& state, ai_state_t& self )
{
    // EnableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button on

    if (!resolveSelfContext(self).isResolved()) return false;

    setModuleRespawnValid(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableRespawn( script_state_t& state, ai_state_t& self )
{
    // DisableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button off

    if (!resolveSelfContext(self).isResolved()) return false;

    setModuleRespawnValid(false);

    return true;
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
                                                           selfContext.targetInfo->getTeamRef(),
                                                           FACE_NORTH);

    if (child == nullptr)
    {
        logAttachedCharacterSpawnFailure(selfContext, state.argument);
        return false;
    }

    const uint8_t grip = Ego::Math::constrain<int>(state.distance,
                                                   ATTACH_INVENTORY,
                                                   ATTACH_RIGHT);
    return resolveSpawnAttachedCharacterPlacement(selfContext,
                                                  targetContext,
                                                  grip,
                                                  child,
                                                  self);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MorphToTarget( script_state_t& state, ai_state_t& self )
{
    // MorphToTarget()
    /// @author ZF
    /// @details This morphs the character into the target
    /// Also set size and keeps the previous AI type

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

    return true;
}
