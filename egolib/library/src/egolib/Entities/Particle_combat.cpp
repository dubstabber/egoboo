//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/Entities/Particle_combat.cpp
/// @brief Particle damage application and destruction helpers.

#include "egolib/Entities/Particle_internal.h"

namespace Ego
{

void Particle::updateAttachedDamage()
{
    // this is often set to zero when the particle hits something
    int max_damage = std::abs(damage.base) + std::abs(damage.rand);

    // wait until the right time
    uint32_t update_count = worldUpdateCount() + _particleID.get();
    if (0 != (update_count & 31)) return;

    // we must be attached to something
    if (!isAttached()) return;

    const std::shared_ptr<Object> &attachedObject = getAttachedObject();
    IDamageable& damageable = *attachedObject;
    const IDamageable& constDamageable = *attachedObject;

    // find out who is holding the owner of this object
    ObjectRef iholder = chr_get_lowest_attachment(attachedObject->getObjRef(), true);
    if (ObjectRef::Invalid == iholder) iholder = attachedObject->getObjRef();

    // do nothing if you are attached to your owner
    if ((ObjectRef::Invalid != owner_ref) && (iholder == owner_ref || attachedObject->getObjRef() == owner_ref)) return;

    //---- only do damage in certain cases:

    // 1) the particle has the DAMFX_ARRO bit
    bool skewered_by_arrow = getProfile()->hasBit(DAMFX_ARRO);

    // 2) the character is vulnerable to this damage type
    bool has_vulnie = (attachedObject->getProfile()->getIDSZ(IDSZ_VULNERABILITY) == ProfileSystem::get().getProfile(_spawnerProfile)->getIDSZ(IDSZ_TYPE) ||
                       attachedObject->getProfile()->getIDSZ(IDSZ_VULNERABILITY) == ProfileSystem::get().getProfile(_spawnerProfile)->getIDSZ(IDSZ_PARENT));

    // 3) the character is "lit on fire" by the particle damage type
    bool is_immolated_by = (damagetype < DAMAGE_COUNT && constDamageable.getReaffirmDamageType() == damagetype);

    // 4) the character has no protection to the particle
    bool no_protection_from = (0 != max_damage) && (damagetype < DAMAGE_COUNT) && (0.0f <= constDamageable.getDamageReduction(damagetype));

    if (!skewered_by_arrow && !has_vulnie && !is_immolated_by && !no_protection_from)
    {
        return;
    }

    IPair local_damage;
    if (has_vulnie || is_immolated_by)
    {
        // the damage is the maximum damage over and over again until the particle dies
        local_damage = range_to_pair(getProfile()->damage);
    }
    else if (no_protection_from)
    {
        // take a portion of whatever damage remains
        local_damage = damage;
    }
    else
    {
        local_damage = range_to_pair(getProfile()->damage);

        local_damage.base /= 2;
        local_damage.rand /= 2;

        // distribute 1/2 of the maximum damage over the particle's lifetime
        if (!is_eternal)
        {
            // how many 32 update cycles will this particle live through?
            int cycles = lifetime_total / 32;

            if (cycles > 1)
            {
                local_damage.base /= cycles;
                local_damage.rand /= cycles;
            }
        }
    }

    //---- special effects
    if (getProfile()->allowpush && 0 == getProfile()->getSpawnVelocityOffsetXY().base)
    {
        // Make character limp
        attachedObject->setVelocity
        ({
            attachedObject->getVelocity().x() * 0.5f,
            attachedObject->getVelocity().y() * 0.5f,
            attachedObject->getVelocity().z()
        });
    }

    //---- do the damage
    int actual_damage = damageable.damage(ATK_BEHIND, local_damage, static_cast<DamageType>(damagetype), team,
                                          activeModule().getObjectHandler()[owner_ref], getProfile()->hasBit(DAMFX_ARMO),
                                          !getProfile()->hasBit(DAMFX_TIME), false);

    // adjust any remaining particle damage
    if (damage.base > 0)
    {
        damage.base -= actual_damage;
        damage.base = std::max(0, damage.base);

        // properly scale the random amount
        // @todo The interval class ensures o.length() is non-negative.
        // However, what if o.lower() is zero?
        damage.rand = getProfile()->damage.length() * damage.base / getProfile()->damage.lower();
    }
}

void Particle::destroy()
{
    if(_particleID == ParticleRef::Invalid) {
        throw std::logic_error("tried to destroy() Particle that was already destroyed");
    }

    if(!isTerminated()) {
        throw std::logic_error("tried to destroy() Particle that was not terminated");
    }

    //This is no longer a valid particle
    _particleID = ParticleRef::Invalid;

    // Spawn new particles if time for old one is up
    if (getProfile()->endspawn._amount > 0 && LocalParticleProfileRef::Invalid != getProfile()->endspawn._lpip)
    {
        Facing facingAdd = this->facing;
        for (size_t tnc = 0; tnc < getProfile()->endspawn._amount; tnc++)
        {
            if(_spawnerProfile == ObjectProfileRef::Invalid)
            {
                //Global particle
                ParticleHandler::get().spawnGlobalParticle(getOldPosition(), facingAdd, getProfile()->endspawn._lpip, tnc);
            }
            else
            {
                //Local particle
                ParticleHandler::get().spawnLocalParticle(getOldPosition(), facingAdd, ObjectProfileRef(_spawnerProfile), getProfile()->endspawn._lpip,
                                                          ObjectRef::Invalid, GRIP_LAST, team, owner_ref, _particleID, tnc, _target);
            }

            facingAdd += Facing(getProfile()->endspawn._facingAdd);
        }
    }

    //Spawn an Object on particle end? (happens through a special script function)
    if (SPAWNNOCHARACTER != endspawn_characterstate)
    {
        std::shared_ptr<Object> child = activeModule().spawnObject(getPosition(), _spawnerProfile, team, 0, facing, "", ObjectRef::Invalid);
        if (child)
        {
            child->setAIStateValue(endspawn_characterstate);
            child->setAIOwner(owner_ref);
        }
    }

    // Play end sound
    playSound(getProfile()->end_sound);
}

} // namespace Ego
