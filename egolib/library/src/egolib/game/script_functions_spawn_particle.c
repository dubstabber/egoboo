/// @file egolib/game/script_functions_spawn_particle.c
/// @brief EgoScript dispatch entries that spawn or affirm particles.

#include "egolib/game/script_functions_spawn_internal.h"

namespace
{
const ITargetInfo& targetInfo(const Object& object)
{
    return object;
}

const IInventoryHolder& inventoryHolder(const Object& object)
{
    return object;
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

ObjectRef resolveHolderOrSelfRef(const ITargetInfo& object)
{
    const ObjectRef holderRef = object.getHolderRef();
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
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnParticle(tmpargument = "particle", tmpdistance = "character vertex", tmpx = "offset x", tmpy = "offset y" )
    /// @author ZZ
    /// @details This function spawns a particle, offset from the character's location

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

    if (particle == nullptr)
    {
        return false;
    }

    return tryAttachParticleToResolvedSelf(particle,
                                           self.getSelf(),
                                           state.distance,
                                           state.x,
                                           state.y);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisaffirmCharacter( script_state_t& state, ai_state_t& self )
{
    // DisaffirmCharacter()
    /// @author ZZ
    /// @details This function removes all the attached particles from a character
    /// ( stuck arrows, flames, etc )

    if (!resolveSelfContext(self).isResolved()) return false;

    disaffirm_attached_particles(self.getSelf());

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ReaffirmCharacter( script_state_t& state, ai_state_t& self )
{
    // ReaffirmCharacter()
    /// @author ZZ
    /// @details This function makes sure it has all of its reaffirmation particles
    /// attached to it. Used to make the torch light again

    if (!resolveSelfContext(self).isResolved()) return false;

    reaffirm_attached_particles(self.getSelf());

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedParticle( tmpargument = "particle", tmpdistance = "vertex" )
    /// @author ZZ
    /// @details This function spawns a particle attached to the character

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveLowestAttachmentOrSelfRef(self.getSelf());
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.object.getPosition(),
                                                idlib::canonicalize(selfContext.object.getFacingZ()),
                                                LocalParticleProfileRef(state.argument),
                                                self.getSelf(),
                                                state.distance,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);

    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                position,
                                                idlib::canonicalize(selfContext.object.getFacingZ()),
                                                LocalParticleProfileRef(state.argument),
                                                ObjectRef::Invalid,
                                                0,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnPoof( script_state_t& state, ai_state_t& self )
{
    // SpawnPoof
    /// @author ZZ
    /// @details This function makes a lovely little poof at the character's location.
    /// The poof form and particle types are set in data.txt

    if (!resolveSelfContext(self).isResolved()) return false;

    return spawnPoofForSelf(self.getSelf());
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedSizedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedSizedParticle( tmpargument = "particle", tmpdistance = "vertex", tmpturn = "size" )
    /// @author ZZ
    /// @details This function spawns a particle of the specific size attached to the
    /// character. For spell charging effects

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

    if (particle == nullptr)
    {
        return false;
    }

    particle->setSize(state.turn);
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedFacedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedFacedParticle(  tmpargument = "particle", tmpdistance = "vertex", tmpturn = "turn" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character, facing the
    /// same direction given by tmpturn

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.object.getPosition(),
                                                Facing(Ego::Math::clipBits<16>(state.turn)),
                                                LocalParticleProfileRef(state.argument),
                                                self.getSelf(),
                                                state.distance,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedHolderParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedHolderParticle( tmpargument = "particle", tmpdistance = "vertex" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character's holder, or to the character if no holder

    if (!resolveSelfContext(self).isResolved()) return false;

    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);
    const ObjectRef ownerRef = resolveHolderOrSelfRef(selfContext.object);
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.object.getPosition(),
                                                idlib::canonicalize(selfContext.object.getFacingZ()),
                                                LocalParticleProfileRef(state.argument),
                                                ownerRef,
                                                state.distance,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactChaseParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactChaseParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position,
    /// that will home in on the character's target

    std::shared_ptr<Ego::Particle> particle;

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

    if (particle == nullptr)
    {
        return false;
    }

    particle->setTarget(self.getTarget());
    return true;
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

    if (particle == nullptr)
    {
        return false;
    }

    particle->endspawn_characterstate = state.turn;
    return true;
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

    if (!resolveSelfContext(self).isResolved()) return false;
    const SpawnSelfContext selfContext = makeSpawnSelfContext(self);

    PIP_REF ipip = selfContext.profile->getParticlePoofProfile();
    if ( INVALID_PIP_REF == ipip) return false;
    const std::shared_ptr<ParticleProfile> &ppip = EngineContext::get().profileSystem().getParticleProfile(ipip);

    bool spawnedPoof = false;
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
                spawnedPoof = true;
            }

            facing_z += Facing(selfContext.profile->getParticlePoofFacingAdd());
        }
    }

    return spawnedPoof;
}
