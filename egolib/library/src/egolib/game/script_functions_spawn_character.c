/// @file egolib/game/script_functions_spawn_character.c
/// @brief EgoScript dispatch entries that spawn, respawn, attach, or morph characters.

#include "egolib/game/script_functions_spawn_internal.h"
#include "egolib/FileFormats/SpawnFile/spawn_file.h"

namespace
{
struct SpawnAttachmentTargetContext
{
    ObjectRef ref = ObjectRef::Invalid;
    IInventoryHolder* inventory = nullptr;
};

struct SpawnedCharacterContext
{
    ObjectRef ref = ObjectRef::Invalid;
    const IPhysical* physical = nullptr;
    IAttachmentControl* attachment = nullptr;
    IScriptable* scriptable = nullptr;
    ICharacterState* characterState = nullptr;
    ILifecycleControl* lifecycle = nullptr;
    IMovementControl* movement = nullptr;

    bool isResolved() const
    {
        return ref != ObjectRef::Invalid &&
               physical != nullptr &&
               attachment != nullptr &&
               scriptable != nullptr &&
               characterState != nullptr &&
               lifecycle != nullptr &&
               movement != nullptr;
    }
};

SpawnedCharacterContext makeSpawnedCharacterContext(ObjectRef childRef)
{
    return SpawnedCharacterContext{
        childRef,
        tryPhysical(childRef),
        tryAttachmentControl(childRef),
        tryScriptable(childRef),
        tryCharacterState(childRef),
        tryLifecycleControl(childRef),
        tryMovementControl(childRef)
    };
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
    Log::activeTarget() << Log::Entry::create(Log::Level::Warning,
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
    Log::activeTarget() << Log::Entry::create(Log::Level::Warning,
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
    Log::activeTarget() << Log::Entry::create(Log::Level::Warning,
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

bool hasSafeSpawnPosition(const SpawnedCharacterContext& child)
{
    return child.physical != nullptr && child.physical->hasSafePosition();
}

void terminateSpawnedCharacter(const SpawnedCharacterContext& child)
{
    if (child.lifecycle != nullptr)
    {
        child.lifecycle->requestTerminate();
    }
}

bool attachSpawnedCharacterToGrip(const SpawnedCharacterContext& child,
                                  ObjectRef targetRef,
                                  grip_offset_t gripOffset)
{
    return child.attachment != nullptr && child.attachment->attachToObject(targetRef, gripOffset);
}

void setSpawnedCharacterHolder(const SpawnedCharacterContext& child, ObjectRef holderRef)
{
    if (child.attachment != nullptr)
    {
        child.attachment->setHolderRef(holderRef);
    }
}

void runSpawnedCharacterScript(const SpawnedCharacterContext& child)
{
    if (child.ref != ObjectRef::Invalid)
    {
        scr_run_chr_script(child.ref);
    }
}

void runSpawnedCharacterScriptAsHeld(const SpawnedCharacterContext& child, ObjectRef holderRef)
{
    setSpawnedCharacterHolder(child, holderRef);
    runSpawnedCharacterScript(child);
    setSpawnedCharacterHolder(child, ObjectRef::Invalid);
}

void publishSpawnChildState(const SpawnedCharacterContext& child,
                            bool inheritKurse,
                            ObjectRef dismountObjectRef,
                            ai_state_t& self)
{
    self.child = child.ref;

    child.characterState->setKursed(inheritKurse);

    inheritSpawnScriptState(*child.scriptable, self);
    publishSpawnDismount(*child.lifecycle, dismountObjectRef);
}

SpawnedCharacterContext spawnCharacterAt(const Ego::Vector3f& position,
                                         ObjectProfileRef profile,
                                         TEAM_REF teamRef,
                                         Facing facing)
{
    const ObjectRef childRef = moduleCommands().spawnObjectRef(position, profile, teamRef, 0, facing, "", ObjectRef::Invalid);
    return makeSpawnedCharacterContext(childRef);
}

void setModuleRespawnValid(bool valid)
{
    moduleCommands().setRespawnValid(valid);
}

SpawnedCharacterContext spawnCharacterLikeSelf(const SpawnSelfContext& selfContext,
                                               const Ego::Vector3f& position,
                                               Facing facing)
{
    return spawnCharacterAt(position,
                            selfContext.profileRef,
                            selfContext.targetInfo->getTeamRef(),
                            facing);
}

bool publishAttachedChildState(const SpawnedCharacterContext& child,
                               ai_state_t& self)
{
    if (!child.isResolved())
    {
        return false;
    }

    self.child = child.ref;
    inheritSpawnScriptState(*child.scriptable, self);
    return true;
}

bool publishCopiedChildState(const SpawnSelfContext& selfContext,
                             const SpawnedCharacterContext& child,
                             ai_state_t& self)
{
    if (!child.isResolved())
    {
        return false;
    }

    publishSpawnChildState(child, selfContext.targetInfo->isKursed(), selfContext.ref, self);
    return true;
}

bool finalizeSafeSelfCopySpawn(const SpawnSelfContext& selfContext,
                               SpawnedCharacterContext childContext,
                               ai_state_t& self,
                               int initialVelocity)
{
    if (!childContext.isResolved())
    {
        logSelfCopySpawnFailure(selfContext);
        return false;
    }

    if (!hasSafeSpawnPosition(childContext))
    {
        logUnsafeSelfCopySpawnFailure(selfContext);
        terminateSpawnedCharacter(childContext);
        return true;
    }

    const Facing turn = selfContext.physical->getFacingZ() + ATK_BEHIND;
    applySpawnVelocity(*childContext.movement, turn, initialVelocity);
    return publishCopiedChildState(selfContext, childContext, self);
}

bool tryAttachSpawnedInventoryChild(const SpawnSelfContext& selfContext,
                                    const SpawnAttachmentTargetContext& targetContext,
                                    SpawnedCharacterContext& child,
                                    ai_state_t& self)
{
    if (!child.isResolved())
    {
        return false;
    }

    if (!Inventory::add_item(targetContext.ref,
                             child.ref,
                             selfContext.inventory->getFirstFreeInventorySlot(),
                             true))
    {
        terminateSpawnedCharacter(child);
        return true;
    }

    publishGrabbedAlert(*child.scriptable);
    runSpawnedCharacterScriptAsHeld(child, targetContext.ref);
    return publishAttachedChildState(child, self);
}

bool tryAttachSpawnedGripChild(const SpawnAttachmentTargetContext& targetContext,
                               uint8_t grip,
                               SpawnedCharacterContext& child,
                               ai_state_t& self)
{
    if (!child.isResolved())
    {
        return false;
    }

    const slot_t slot = (grip == ATTACH_LEFT) ? SLOT_LEFT : SLOT_RIGHT;
    if (isLiveSpawnObjectRef(targetContext.inventory->getHeldObject(slot)))
    {
        terminateSpawnedCharacter(child);
        return true;
    }

    const grip_offset_t gripOffset = (grip == ATTACH_LEFT) ? GRIP_LEFT : GRIP_RIGHT;
    if (attachSpawnedCharacterToGrip(child, targetContext.ref, gripOffset))
    {
        runSpawnedCharacterScript(child);
    }

    return publishAttachedChildState(child, self);
}

bool resolveSpawnAttachedCharacterPlacement(const SpawnSelfContext& selfContext,
                                            const SpawnAttachmentTargetContext& targetContext,
                                            uint8_t grip,
                                            SpawnedCharacterContext& child,
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

    return publishAttachedChildState(child, self);
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const Ego::Vector3f position(static_cast<float>(state.x),
                                 static_cast<float>(state.y),
                                 selfContext.physical->getPosZ());
    SpawnedCharacterContext child = spawnCharacterLikeSelf(selfContext,
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    selfContext.lifecycle->respawn();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnCharacterXYZ( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacterXYZ( tmpx = "x", tmpy = "y", tmpdistance = "z", tmpturn = "turn" )
    /// @author ZZ
    /// @details This function spawns a character of the same type at a specific location, failing if x,y,z is invalid

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const Ego::Vector3f position(float(state.x), float(state.y), float(state.distance));
    SpawnedCharacterContext childContext = spawnCharacterLikeSelf(selfContext,
                                                                  position,
                                                                  Facing(Ego::Math::clipBits<16>(state.turn)));
    if (!childContext.isResolved())
    {
        logSelfCopySpawnFailure(selfContext);
        return false;
    }

    return publishCopiedChildState(selfContext, childContext, self);
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    SpawnedCharacterContext childContext = spawnCharacterAt(position,
                                                            ObjectProfileRef(static_cast<PRO_REF>(state.argument)),
                                                            selfContext.targetInfo->getTeamRef(),
                                                            Facing(Ego::Math::clipBits<16>(state.turn)));

    if (!childContext.isResolved())
    {
        return false;
    }

    return publishCopiedChildState(selfContext, childContext, self);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableRespawn( script_state_t& state, ai_state_t& self )
{
    // EnableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button on

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

    setModuleRespawnValid(true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableRespawn( script_state_t& state, ai_state_t& self )
{
    // DisableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button off

    if (!resolveSpawnSelfContext(self).isResolved()) return false;

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
    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    SpawnAttachmentTargetContext targetContext;
    if (!resolveSpawnAttachmentTarget(self, targetContext))
    {
        return false;
    }

    const Ego::Vector3f position(float(state.x), float(state.y), float(state.distance));
    SpawnedCharacterContext childContext = spawnCharacterAt(position,
                                                            ObjectProfileRef((PRO_REF)state.argument),
                                                            selfContext.targetInfo->getTeamRef(),
                                                            FACE_NORTH);

    if (!childContext.isResolved())
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
                                                  childContext,
                                                  self);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MorphToTarget( script_state_t& state, ai_state_t& self )
{
    // MorphToTarget()
    /// @author ZF
    /// @details This morphs the character into the target
    /// Also set size and keeps the previous AI type

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    IMorphControl* selfMorph = tryMorphControl(selfContext.ref);
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
