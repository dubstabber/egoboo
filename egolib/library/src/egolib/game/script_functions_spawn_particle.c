/// @file egolib/game/script_functions_spawn_particle.c
/// @brief EgoScript dispatch entries that spawn or affirm particles.

#include "egolib/game/script_functions_spawn_internal.h"

#define GAME_ENTITIES_PRIVATE 1
#include "egolib/Entities/Particle.hpp"
#undef GAME_ENTITIES_PRIVATE

namespace
{
ObjectRef resolveHolderOrSelfRef(const ITargetInfo& object)
{
    const ObjectRef holderRef = object.getHolderRef();
    return isLiveSpawnObjectRef(holderRef) ? holderRef : object.getObjRef();
}

ObjectRef resolveSpawnParticleOwnerRef(const SpawnSelfContext& selfContext)
{
    ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    if (selfContext.targetInfo->isMount())
    {
        const ObjectRef riderRef = selfContext.inventory->getHeldObject(SLOT_LEFT);
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
    if (!isLiveSpawnObjectRef(selfRef))
    {
        return false;
    }

    activeParticleHandler().spawnPoof(selfRef);
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
    return activeParticleHandler().spawnLocalParticle(position,
                                                      facing,
                                                      selfContext.profileRef,
                                                      profile,
                                                      attachedObjectRef,
                                                      distance,
                                                      selfContext.targetInfo->getTeamRef(),
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
    if (particle == nullptr || !isLiveSpawnObjectRef(selfRef))
    {
        return false;
    }

    particle->placeAtVertex(selfRef, vertex);
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveSpawnParticleOwnerRef(selfContext);
    const std::shared_ptr<Ego::Particle> particle =
        spawnLocalParticleForSelf(selfContext,
                                  selfContext.physical->getPosition(),
                                  Facing(uint16_t(selfContext.physical->getFacingZ())),
                                  LocalParticleProfileRef(state.argument),
                                  selfContext.ref,
                                  state.distance,
                                  ownerRef);

    if (particle == nullptr)
    {
        return false;
    }

    return tryAttachParticleToResolvedSelf(particle,
                                           selfContext.ref,
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    disaffirm_attached_particles(selfContext.ref);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ReaffirmCharacter( script_state_t& state, ai_state_t& self )
{
    // ReaffirmCharacter()
    /// @author ZZ
    /// @details This function makes sure it has all of its reaffirmation particles
    /// attached to it. Used to make the torch light again

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    reaffirm_attached_particles(selfContext.ref);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedParticle( tmpargument = "particle", tmpdistance = "vertex" )
    /// @author ZZ
    /// @details This function spawns a particle attached to the character

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveLowestAttachmentOrSelfRef(selfContext.ref);
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.physical->getPosition(),
                                                idlib::canonicalize(selfContext.physical->getFacingZ()),
                                                LocalParticleProfileRef(state.argument),
                                                selfContext.ref,
                                                state.distance,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnExactParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnExactParticle( tmpargument = "particle", tmpx = "x", tmpy = "y", tmpdistance = "z" )
    /// @author ZZ
    /// @details This function spawns a particle at a specific x, y, z position

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);

    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                position,
                                                idlib::canonicalize(selfContext.physical->getFacingZ()),
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    return spawnPoofForSelf(selfContext.ref);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedSizedParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedSizedParticle( tmpargument = "particle", tmpdistance = "vertex", tmpturn = "size" )
    /// @author ZZ
    /// @details This function spawns a particle of the specific size attached to the
    /// character. For spell charging effects

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    const std::shared_ptr<Ego::Particle> particle =
        spawnLocalParticleForSelf(selfContext,
                                  selfContext.physical->getPosition(),
                                  idlib::canonicalize(selfContext.physical->getFacingZ()),
                                  LocalParticleProfileRef(state.argument),
                                  selfContext.ref,
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.physical->getPosition(),
                                                Facing(Ego::Math::clipBits<16>(state.turn)),
                                                LocalParticleProfileRef(state.argument),
                                                selfContext.ref,
                                                state.distance,
                                                ownerRef);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SpawnAttachedHolderParticle( script_state_t& state, ai_state_t& self )
{
    // SpawnAttachedHolderParticle( tmpargument = "particle", tmpdistance = "vertex" )

    /// @author ZZ
    /// @details This function spawns a particle attached to the character's holder, or to the character if no holder

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    return nullptr != spawnLocalParticleForSelf(selfContext,
                                                selfContext.physical->getPosition(),
                                                idlib::canonicalize(selfContext.physical->getFacingZ()),
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    const Ego::Vector3f position(Ego::Script::Interpreter::safeCast<float>(state.x),
                                 Ego::Script::Interpreter::safeCast<float>(state.y),
                                 Ego::Script::Interpreter::safeCast<float>(state.distance));
    particle = spawnLocalParticleForSelf(selfContext,
                                         position,
                                         idlib::canonicalize(selfContext.physical->getFacingZ()),
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;
    const ObjectRef ownerRef = resolveHolderOrSelfRef(*selfContext.targetInfo);
    const Ego::Vector3f position(float(state.x),
                                 float(state.y),
                                 float(state.distance));
    particle = spawnLocalParticleForSelf(selfContext,
                                         position,
                                         idlib::canonicalize(selfContext.physical->getFacingZ()),
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

    const SpawnSelfContext selfContext = resolveSpawnSelfContext(self);
    if (!selfContext.isResolved()) return false;

    PIP_REF ipip = selfContext.profile->getParticlePoofProfile();
    if ( INVALID_PIP_REF == ipip) return false;
    const std::shared_ptr<ParticleProfile> &ppip = activeProfileSystem().getParticleProfile(ipip);

    bool spawnedPoof = false;
    if (ppip != nullptr)
    {
        const float velOffsetBase = static_cast<float>(state.x);
        const float posOffsetBase = static_cast<float>(state.y);
        const float damage_rand = ppip->damage.length();

        Facing facing_z = selfContext.physical->getFacingZ();
        for (int cnt = 0; cnt < selfContext.profile->getParticlePoofAmount(); cnt++)
        {
            auto poofParticle = activeParticleHandler().spawnParticle(selfContext.oldPosition,
                                                                      facing_z,
                                                                      selfContext.profile->getSlotNumber(),
                                                                      ipip,
                                                                      ObjectRef::Invalid,
                                                                      GRIP_LAST,
                                                                      selfContext.targetInfo->getTeamRef(),
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
