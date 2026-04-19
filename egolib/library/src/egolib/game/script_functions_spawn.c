/// @file egolib/game/script_functions_spawn.c
/// @brief Character/particle spawning, destruction, and lifecycle management

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
IScriptable& scriptable(Object& object)
{
    return object;
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

void publishImmediatePoof(IScriptable& target)
{
    target.setAIPoofTime(worldUpdateCount());
}

std::shared_ptr<Object> heldObject(const Object& holder, slot_t slot)
{
    return tryObjectShared(holder.getHeldObject(slot));
}
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropWeapons( script_state_t& state, ai_state_t& self )
{
    // DropWeapons()
    /// @author ZZ
    /// @details This function drops the character's in-hand items.  It will also
    /// buck the rider if the character is a mount

    SCRIPT_FUNCTION_BEGIN();

    // This funtion drops the character's in hand items/riders
    const std::shared_ptr<Object> leftItem = heldObject(*pchr, SLOT_LEFT);
    if (leftItem)
    {
        leftItem->detatchFromHolder(true, true);
        if ( pchr->isMount() )
        {
            leftItem->setVelocity({leftItem->getVelocity().x(),
                                leftItem->getVelocity().y(),
                                Object::DISMOUNTZVEL});
            leftItem->setJumpTimer(Object::JUMPDELAY);
            leftItem->movePosition(0.0f, 0.0f, Object::DISMOUNTZVEL);
        }
    }

    const std::shared_ptr<Object> rightItem = heldObject(*pchr, SLOT_RIGHT);
    if (rightItem)
    {
        rightItem->detatchFromHolder(true, true);
        if ( pchr->isMount() )
        {
            rightItem->setVelocity({rightItem->getVelocity().x(),
                                 rightItem->getVelocity().y(),
                                 Object::DISMOUNTZVEL});
            rightItem->setJumpTimer(Object::JUMPDELAY);
            rightItem->movePosition(0.0f, 0.0f, Object::DISMOUNTZVEL);
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GoPoof( script_state_t& state, ai_state_t& self )
{
    // GoPoof()
    /// @author ZZ
    /// @details This function flags the character to be removed from the game entirely.
    /// This doesn't work on players

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
    if (!pchr->isPlayer())
    {
        returncode = true;
        self.poof_time = worldUpdateCount();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropKeys( script_state_t& state, ai_state_t& self )
{
    // DropKeys()
    /// @author ZZ
    /// @details This function drops all of the keys in the character's inventory.
    /// This does NOT drop keys in the character's hands.

    SCRIPT_FUNCTION_BEGIN();

    pchr->dropKeys();

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

    SCRIPT_FUNCTION_BEGIN();

	Ego::Vector3f pos = Ego::Vector3f(static_cast<float>(state.x), static_cast<float>(state.y), pchr->getPosZ());

    std::shared_ptr<Object> pchild = activeModule().spawnObject(pos, pchr->getProfileID(), pchr->getTeamRef(), 0, Facing(Ego::Math::clipBits<16>( state.turn )), "", ObjectRef::Invalid);
    returncode = pchild != nullptr;

    if ( !returncode )
    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object ", "`", pchr->getName(), "`", " failed to spawn a copy of itself", Log::EndOfEntry);
    }
    else
    {
        // was the child spawned in a "safe" spot?
        if (!pchild->hasSafePosition()) {
			EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object ", "`", pchr->getName(), "`", " failed to spawn a copy of itself (no safe location)", Log::EndOfEntry);
            pchild->requestTerminate();
        }
        else
        {
            self.child = pchild->getObjRef();

            Facing turn = pchr->getFacingZ() + ATK_BEHIND;
            pchild->setVelocity(pchild->getVelocity() +
                                Ego::Vector3f(std::cos(turn) * state.distance,
                                              std::sin(turn) * state.distance,
                                              0.0f));

            pchild->setKursed(pchr->isKursed());  /// @note BB@> inherit this from your spawner
            inheritSpawnScriptState(*pchild, self);

            pchild->setDismountTimer(Object::PHYS_DISMOUNT_TIME);
            pchild->setDismountObject(self.getSelf());
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnCharacter( script_state_t& state, ai_state_t& self )
{
    // RespawnCharacter()
    /// @author ZZ
    /// @details This function respawns the character at its starting location.
    /// Often used with the Clean functions

    SCRIPT_FUNCTION_BEGIN();

    pchr->respawn();

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

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        pchr->detatchFromHolder(true, true);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_CleanUp( script_state_t& state, ai_state_t& self )
{
    // CleanUp()
    /// @author ZZ
    /// @details This function tells all the dead characters on the team to clean
    /// themselves up.  Usually done by the boss creature every second or so

    SCRIPT_FUNCTION_BEGIN();

    for(const std::shared_ptr<Object> &listener : objectHandler().iterator())
    {
        if ( pchr->getTeam() != listener->getTeam() ) continue;

        if ( !listener->isAlive() )
        {
            IScriptable& scriptableListener = scriptable(*listener);
            scriptableListener.setAITimer(worldUpdateCount() + 2);  // Don't let it think too much...
        }

        publishCleanedUpState(*listener);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnParticle(tmpargument = "particle", tmpdistance = "character vertex", tmpx = "offset x", tmpy = "offset y" )
    /// @author ZZ
    /// @details This function spawns a particle, offset from the character's location

    SCRIPT_FUNCTION_BEGIN();

	ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    //If we are a mount, our rider is the owner of this particle
    if ( pchr->isMount() && objectHandler().exists( pchr->getHeldObject(SLOT_LEFT) ) )
    {
        ichr = pchr->getHeldObject(SLOT_LEFT);
    }

    std::shared_ptr<Ego::Particle> particle = EngineContext::get().particleHandler().spawnLocalParticle(pchr->getPosition(), 
                                                   Facing(uint16_t(pchr->getFacingZ())), 
                                                   ObjectProfileRef(pchr->getProfileID()),
                                                   LocalParticleProfileRef(state.argument), self.getSelf(),
                                                   state.distance, pchr->getTeamRef(), ichr, ParticleRef::Invalid, 0,
                                                   ObjectRef::Invalid );

    returncode = (particle != nullptr);
    if ( returncode )
    {
        // attach the particle
        particle->placeAtVertex(objectHandler()[self.getSelf()], state.distance);
        particle->attach(ObjectRef::Invalid);

		Ego::Vector3f tmp_pos = particle->getPosition();

        // Correct X, Y, Z spacing
        tmp_pos.z() += particle->getProfile()->getSpawnPositionOffsetZ().base;

        // Don't spawn in walls
        tmp_pos.x() += state.x;
        if (EMPTY_BIT_FIELD != particle->test_wall(tmp_pos))
        {
            tmp_pos.x() = particle->getPosX();

            tmp_pos.y() += state.y;
            if (EMPTY_BIT_FIELD != particle->test_wall(tmp_pos))
            {
                tmp_pos.y() = particle->getPosY();
            }
        }

        particle->setPosition(tmp_pos);
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

    SCRIPT_FUNCTION_BEGIN();

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

    SCRIPT_FUNCTION_BEGIN();

    reaffirm_attached_particles(self.getSelf());

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedParticle( tmpargument = "particle", tmpdistance = "vertex" )
    /// @author ZZ
    /// @details This function spawns a particle attached to the character

    SCRIPT_FUNCTION_BEGIN();

    //If we are a weapon, our holder is the owner of this particle
	ObjectRef iself = self.getSelf();
	ObjectRef iholder = chr_get_lowest_attachment(iself, true);
    if (objectHandler().exists(iholder))
    {
		iself = iholder;
    }

    returncode = nullptr != EngineContext::get().particleHandler().spawnLocalParticle(pchr->getPosition(), idlib::canonicalize(pchr->getFacingZ()), ObjectProfileRef(pchr->getProfileID()),
                                                                      LocalParticleProfileRef(state.argument), self.getSelf(),
                                                                      state.distance, pchr->getTeamRef(), iself, ParticleRef::Invalid, 0,
                                                                      ObjectRef::Invalid);
    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position

    SCRIPT_FUNCTION_BEGIN();

    ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    {
		Ego::Vector3f vtmp =
			Ego::Vector3f
            (
				Ego::Script::Interpreter::safeCast<float>(state.x),
				Ego::Script::Interpreter::safeCast<float>(state.y),
				Ego::Script::Interpreter::safeCast<float>(state.distance)
            );

        returncode = nullptr != EngineContext::get().particleHandler().spawnLocalParticle(vtmp, idlib::canonicalize(pchr->getFacingZ()), ObjectProfileRef(pchr->getProfileID()),
                                                                          LocalParticleProfileRef(state.argument),
                                                                          ObjectRef::Invalid, 0, pchr->getTeamRef(), ichr,
                                                                          ParticleRef::Invalid, 0, ObjectRef::Invalid);
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushValid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushValid()
    /// @author ZZ
    /// @details This function makes a character able to be crushed by closing doors
    /// and such

    SCRIPT_FUNCTION_BEGIN();

    pchr->setCanBeCrushed(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PoofTarget( script_state_t& state, ai_state_t& self )
{
    // PoofTarget()
    /// @author ZZ
    /// @details This function removes the target from the game, failing if the
    /// target is a player

    SCRIPT_FUNCTION_BEGIN();

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
            self.poof_time = worldUpdateCount() + 1;
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

    SCRIPT_FUNCTION_BEGIN();

    const std::shared_ptr<Object> selfObject = tryObjectShared(self.getSelf());
    if (selfObject == nullptr)
    {
        return false;
    }

    EngineContext::get().particleHandler().spawnPoof(selfObject);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildState( script_state_t& state, ai_state_t& self )
{
    // SetChildState( tmpargument = "state" )
    /// @author ZZ
    /// @details This function lets a character set the state of the last character it
    /// spawned

    SCRIPT_FUNCTION_BEGIN();

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

    SCRIPT_FUNCTION_BEGIN();

    ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    std::shared_ptr<Ego::Particle> particle = EngineContext::get().particleHandler().spawnLocalParticle(pchr->getPosition(), idlib::canonicalize(pchr->getFacingZ()), 
                                                                                        ObjectProfileRef(pchr->getProfileID()), LocalParticleProfileRef(state.argument), self.getSelf(),
                                                                                        state.distance, pchr->getTeamRef(), ichr, ParticleRef::Invalid, 0,
                                                                                        ObjectRef::Invalid);

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

    SCRIPT_FUNCTION_BEGIN();

	ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    returncode = nullptr != EngineContext::get().particleHandler().spawnLocalParticle(pchr->getPosition(), Facing(Ego::Math::clipBits<16>( state.turn )),
                                                                      ObjectProfileRef(pchr->getProfileID()), LocalParticleProfileRef(state.argument),
                                                                      self.getSelf(), state.distance, pchr->getTeamRef(), ichr, ParticleRef::Invalid,
                                                                      0, ObjectRef::Invalid);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedHolderParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedHolderParticle( tmpargument = "particle", tmpdistance = "vertex" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character's holder, or to the character if no holder

    SCRIPT_FUNCTION_BEGIN();

    ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    returncode = nullptr != EngineContext::get().particleHandler().spawnLocalParticle(pchr->getPosition(), idlib::canonicalize(pchr->getFacingZ()), ObjectProfileRef(pchr->getProfileID()),
                                                                      LocalParticleProfileRef(state.argument), ichr,
                                                                      state.distance, pchr->getTeamRef(), ichr, ParticleRef::Invalid, 0,
                                                                      ObjectRef::Invalid);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnCharacterXYZ( script_state_t& state, ai_state_t& self )
{
    // SpawnCharacterXYZ( tmpx = "x", tmpy = "y", tmpdistance = "z", tmpturn = "turn" )
    /// @author ZZ
    /// @details This function spawns a character of the same type at a specific location, failing if x,y,z is invalid

    SCRIPT_FUNCTION_BEGIN();

	Ego::Vector3f pos = Ego::Vector3f(float(state.x), float(state.y), float(state.distance));

    std::shared_ptr<Object> pchild = activeModule().spawnObject( pos, pchr->getProfileID(), pchr->getTeamRef(), 0, Facing(Ego::Math::clipBits<16>( state.turn )), "", ObjectRef::Invalid );
    if (pchild == nullptr)
    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object ", "`", pchr->getName(), "`", " failed to spawn a copy of itself", Log::EndOfEntry );
        returncode = false;
    }
    else
    {
        self.child = pchild->getObjRef();

        pchild->setKursed(pchr->isKursed());  /// @note BB@> inherit this from your spawner
        inheritSpawnScriptState(*pchild, self);

        pchild->setDismountTimer(Object::PHYS_DISMOUNT_TIME);
        pchild->setDismountObject(self.getSelf());
        returncode = true;
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

    SCRIPT_FUNCTION_BEGIN();

	auto pos =
		Ego::Vector3f
        (
			Ego::Script::Interpreter::safeCast<float>(state.x),
            Ego::Script::Interpreter::safeCast<float>(state.y),
            Ego::Script::Interpreter::safeCast<float>(state.distance)
        );

    const std::shared_ptr<Object> pchild = activeModule().spawnObject(pos, ObjectProfileRef(static_cast<PRO_REF>(state.argument)), pchr->getTeamRef(), 0, Facing(Ego::Math::clipBits<16>(state.turn)), "", ObjectRef::Invalid);

    if ( !pchild )
    {
        returncode = false;
    }
    else
    {
        self.child = pchild->getObjRef();

        pchild->setKursed(pchr->isKursed());  /// @note BB@> inherit this from your spawner
        inheritSpawnScriptState(*pchild, self);

        pchild->setDismountTimer(Object::PHYS_DISMOUNT_TIME);
        pchild->setDismountObject(self.getSelf());
        returncode = true;
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

    SCRIPT_FUNCTION_BEGIN();

    ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    {
		auto vtmp =
			Ego::Vector3f
            (
                Ego::Script::Interpreter::safeCast<float>(state.x),
                Ego::Script::Interpreter::safeCast<float>(state.y),
                Ego::Script::Interpreter::safeCast<float>(state.distance)
            );

        particle = EngineContext::get().particleHandler().spawnLocalParticle(vtmp, idlib::canonicalize(pchr->getFacingZ()), ObjectProfileRef(pchr->getProfileID()),
                                                             LocalParticleProfileRef(state.argument),
                                                             ObjectRef::Invalid, 0, pchr->getTeamRef(), ichr, ParticleRef::Invalid,
                                                             0, ObjectRef::Invalid);
    }

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

    SCRIPT_FUNCTION_BEGIN();

    pchr->dropAllItems();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_RespawnTarget( script_state_t& state, ai_state_t& self )
{
    // RespawnTarget()
    /// @author ZZ
    /// @details This function respawns the target at its current location

    SCRIPT_FUNCTION_BEGIN();

	Object *pself_target;
    SCRIPT_REQUIRE_TARGET( pself_target );
	Ego::Vector3f save_pos = pself_target->getPosition();
    pself_target->respawn();
    pself_target->setPosition(save_pos);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_NotAnItem( script_state_t& state, ai_state_t& self )
{
    // NotAnItem()
    /// @author ZZ
    /// @details This function makes the character a non-item character.
    /// Usage: Used for spells that summon creatures

    SCRIPT_FUNCTION_BEGIN();

    pchr->setItem(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildAmmo( script_state_t& state, ai_state_t& self )
{
    // SetChildAmmo( tmpargument = "none" )
    /// @author ZZ
    /// @details This function sets the ammo of the last character spawned by this character

    SCRIPT_FUNCTION_BEGIN();

    objectHandler().get(self.child)->setAmmo(Ego::Math::constrain( state.argument, 0, 0xFFFF ));

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IdentifyTarget( script_state_t& state, ai_state_t& self )
{
    // IdentifyTarget()
    /// @author ZZ
    /// @details This function reveals the target's name, ammo, and usage
    /// Proceeds if the target was unknown

    SCRIPT_FUNCTION_BEGIN();

    returncode = false;
	ObjectRef ichr = self.getTarget();
    if ( objectHandler().get(ichr)->getAmmoMax() != 0 )  objectHandler().get(ichr)->setAmmoKnown(true);


    returncode = !objectHandler().get(ichr)->isNameKnown();
    objectHandler().get(ichr)->setNameKnown(true);
    ppro->makeUsageKnown();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DropTargetKeys( script_state_t& state, ai_state_t& self )
{
    // DropTargetKeys()
    /// @author ZZ
    /// @details This function makes the Target drops keys in inventory (Not inhand)

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    pself_target->dropKeys();

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MakeCrushInvalid( script_state_t& state, ai_state_t& self )
{
    // MakeCrushInvalid()
    /// @author ZZ
    /// @details This function makes doors unable to close on this object

    SCRIPT_FUNCTION_BEGIN();

    pchr->setCanBeCrushed(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageTime( script_state_t& state, ai_state_t& self )
{
    // SetDamageTime( tmpargument = "time" )
    /// @author ZZ
    /// @details This function makes the character invincible for a little while

    SCRIPT_FUNCTION_BEGIN();

    IDamageable* selfDamageable = tryDamageable(self.getSelf());
    if (selfDamageable == nullptr)
    {
        return false;
    }

    selfDamageable->setDamageTimer(static_cast<uint8_t>(Ego::Math::constrain(state.argument, 0, 0xFFFF)));

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

    SCRIPT_FUNCTION_BEGIN();

	ObjectRef ichr = self.getSelf();
    if ( objectHandler().exists( pchr->getHolderRef() ) )
    {
        ichr = pchr->getHolderRef();
    }

    {
		Ego::Vector3f vtmp =
			Ego::Vector3f
            (
				float(state.x),
				float(state.y),
				float(state.distance)
            );

        particle = EngineContext::get().particleHandler().spawnLocalParticle(vtmp, idlib::canonicalize(pchr->getFacingZ()), ObjectProfileRef(pchr->getProfileID()),
                                                             LocalParticleProfileRef(state.argument),
                                                             ObjectRef::Invalid, 0, pchr->getTeamRef(), ichr, ParticleRef::Invalid,
                                                             0, ObjectRef::Invalid);
    }

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

    SCRIPT_FUNCTION_BEGIN();

    PIP_REF ipip = ppro->getParticlePoofProfile();
    if ( INVALID_PIP_REF == ipip) return false;
    const std::shared_ptr<ParticleProfile> &ppip = EngineContext::get().profileSystem().getParticleProfile(ipip);

    returncode = false;
    if (ppip != nullptr)
    {
        const float velOffsetBase = static_cast<float>(state.x);
        const float posOffsetBase = static_cast<float>(state.y);
        const float damage_rand = ppip->damage.length();

        Facing facing_z = pchr->getFacingZ();
        for (int cnt = 0; cnt < pchr->getProfile()->getParticlePoofAmount(); cnt++)
        {
            auto poofParticle = EngineContext::get().particleHandler().spawnParticle(pchr->getOldPosition(), facing_z, pchr->getProfile()->getSlotNumber(), ipip,
                                                                     ObjectRef::Invalid, GRIP_LAST, pchr->getTeamRef(), scriptable(*pchr).getAIOwner(), ParticleRef::Invalid, cnt);

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

            facing_z += Facing(pchr->getProfile()->getParticlePoofFacingAdd());
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

    SCRIPT_FUNCTION_BEGIN();

    activeModule().setRespawnValid(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableRespawn( script_state_t& state, ai_state_t& self )
{
    // DisableRespawn()
    /// @author ZF
    /// @details This function turns respawn with JUMP button off

    SCRIPT_FUNCTION_BEGIN();

    activeModule().setRespawnValid(false);

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
    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

	Ego::Vector3f pos = Ego::Vector3f(float(state.x), float(state.y), float(state.distance));

    std::shared_ptr<Object> pchild = activeModule().spawnObject(pos, ObjectProfileRef((PRO_REF)state.argument), pchr->getTeamRef(), 0, FACE_NORTH, "", ObjectRef::Invalid);
    returncode = pchild != nullptr;

    if ( !returncode )
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "object ", "`", pchr->getName(), "`", "/", "`", pchr->getProfile()->getClassName(), "`", " failed to spawn "
                                         "profile index ", state.argument, Log::EndOfEntry);
    }
    else
    {
        uint8_t grip = Ego::Math::constrain<int>(state.distance, ATTACH_INVENTORY, ATTACH_RIGHT);

        if ( grip == ATTACH_INVENTORY )
        {
            // Inventory character
            if ( Inventory::add_item( self.getTarget(), pchild->getObjRef(), pchr->getFirstFreeInventorySlot(), true ) )
            {
                publishGrabbedAlert(*pchild);  // Make spellbooks change
                pchild->setHolderRef(self.getTarget());  // Make grab work
                scr_run_chr_script( pchild->getObjRef() );  // Empty the grabbed messages

                pchild->setHolderRef(ObjectRef::Invalid);  // Fix grab

                //Set some AI values
                self.child = pchild->getObjRef();
                inheritSpawnScriptState(*pchild, self);
            }

            //No more room!
            else
            {
                pchild->requestTerminate();
            }
        }
        else if ( grip == ATTACH_LEFT || grip == ATTACH_RIGHT )
        {
            if ( !objectHandler().exists( pself_target->getHeldObject(static_cast<slot_t>(grip)) ) )
            {
                // Wielded character
                grip_offset_t grip_off = ( ATTACH_LEFT == grip ) ? GRIP_LEFT : GRIP_RIGHT;

                if(pchild->attachToObject(objectHandler()[self.getTarget()], grip_off))
                {
                    // Handle the "grabbed" messages
                    scr_run_chr_script( pchild->getObjRef() );
                }

                //Set some AI values
                self.child = pchild->getObjRef();
                inheritSpawnScriptState(*pchild, self);
            }

            //Grip is already used
            else
            {
                pchild->requestTerminate();
            }
        }
        else
        {
            // we have been given an invalid attachment point.
            // still allow the character to spawn if it is not in an invalid area

            //Set some AI values
            self.child = pchild->getObjRef();
            inheritSpawnScriptState(*pchild, self);
        }
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetToChild( script_state_t& state, ai_state_t& self )
{
    // SetTargetToChild()
    /// @author ZF
    /// @details This function sets the target to the character it spawned last (also called it's "child")

    SCRIPT_FUNCTION_BEGIN();

    if ( objectHandler().exists( self.child ) )
    {
        self.setTarget(self.child);
    }
    else
    {
        returncode = false;
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetDamageThreshold( script_state_t& state, ai_state_t& self )
{
    // SetDamageThreshold()
    /// @author ZF
    /// @details This sets the damage treshold for this character. Damage below the threshold is ignored

    SCRIPT_FUNCTION_BEGIN();

    int v = (int)state.argument;
    if ( v > 0 ) pchr->setDamageThreshold(v);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_MorphToTarget( script_state_t& state, ai_state_t& self )
{
    // MorphToTarget()
    /// @author ZF
    /// @details This morphs the character into the target
    /// Also set size and keeps the previous AI type

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    if ( !objectHandler().exists( self.getTarget() ) ) return false;

    pchr->polymorphObject(pself_target->getBaseModelRef(), pself_target->getSkin());

    // let the resizing take some time
    pchr->setTargetFat(pself_target->getFat());
    pchr->setResizeTimeRemaining(Object::SIZETIME);

    // change back to our original AI (keep our old AI script)
//    pself->type      = ProList.lst[pchr->basemodel_ref].iai;      //TODO: this no longer works (is it even needed?)

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetChildContent( script_state_t& state, ai_state_t& self )
{
    // SetChildContent( tmpargument = "content" )
    /// @author ZF
    /// @details This function lets a character set the content of the last character it
    /// spawned last

    SCRIPT_FUNCTION_BEGIN();

    returncode = trySetChildContent(self.child, state.argument);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableInvictus( script_state_t& state, ai_state_t& self )
{
    // EnableInvictus()
    /// @author ZF
    /// @details This function makes the character invulerable

    SCRIPT_FUNCTION_BEGIN();

    IDamageable* selfDamageable = tryDamageable(self.getSelf());
    if (selfDamageable == nullptr)
    {
        return false;
    }

    selfDamageable->setInvincible(true);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableInvictus( script_state_t& state, ai_state_t& self )
{
    // DisableInvictus()
    /// @author ZF
    /// @details This function makes the character not invulerable

    SCRIPT_FUNCTION_BEGIN();

    IDamageable* selfDamageable = tryDamageable(self.getSelf());
    if (selfDamageable == nullptr)
    {
        return false;
    }

    selfDamageable->setInvincible(false);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTargetSize( script_state_t& state, ai_state_t& self )
{
    // SetTargetSize( tmpargument = "percent" )
    /// @author ZF
    /// @details This changes the AI target's size

    Object * pself_target;

    SCRIPT_FUNCTION_BEGIN();

    SCRIPT_REQUIRE_TARGET( pself_target );

    pself_target->setTargetFat(pself_target->getTargetFat() * state.argument / 100.0f);
    pself_target->setResizeTimeRemaining(pself_target->getResizeTimeRemaining() + Object::SIZETIME);

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableStealth( script_state_t& state, ai_state_t& self )
{
    // EnableStealth()
    /// @author ZF
    /// @details Makes the object enter stealth mode. Returns true if it is now hidden from others.

    SCRIPT_FUNCTION_BEGIN();

    if(pchr->isStealthed()) {
        returncode = false;
    }
    else {
        returncode = pchr->activateStealth();
    }

    SCRIPT_FUNCTION_END();
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableStealth( script_state_t& state, ai_state_t& self )
{
    // DisableStealth()
    /// @author ZF
    /// @details Makes the object exit stealth mode. Returns true if it exited stealth mode.

    SCRIPT_FUNCTION_BEGIN();

    returncode = pchr->isStealthed();
    pchr->deactivateStealth();

    SCRIPT_FUNCTION_END();
}
