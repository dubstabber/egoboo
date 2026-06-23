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

/// @file egolib/Entities/Object_attributes.cpp
/// @brief Attribute, perk, enchantment, temp-attribute, and profile-query Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/CharacterParticleOps.h"                          // DisplayMsg_printf / disaffirm_attached_particles
#include "egolib/Graphics/IBillboardSystem.hpp"        // Ego::Graphics::tryActiveBillboardSystem
#include "egolib/Logic/IPerkHandler.hpp"               // Ego::Perks::activePerkHandler + Perk
#include "egolib/Log/_Include.hpp"                     // Log::activeTarget
#include "egolib/game/Graphics/Billboard.hpp"         // Ego::Graphics::Billboard::Flags
#include "egolib/Physics/PhysicalConstants.hpp"  // Ego::Physics::CHR_INFINITE_WEIGHT / CHR_MAX_WEIGHT
#include "egolib/AI/LineOfSight.hpp" // line_of_sight_info_t
#include "egolib/Entities/Object_attributes_internal.h"

float Object::getBaseAttribute(const Ego::Attribute::AttributeType type) const
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    return _baseAttribute[type];
}

void Object::setBaseAttribute(const Ego::Attribute::AttributeType type, float value)
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    _baseAttribute[type] = value;
}

float Object::getAttribute(const Ego::Attribute::AttributeType type) const
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);

    float attributeValue = _baseAttribute[type];

    if (hasTempAttribute(type)) {
        const float tempAttributeValue = getTempAttributeValue(type);

        //Is this a SET type attribute or a cumulative ADD type attribute?
        if(isOverrideSetAttribute(type)) {
            return tempAttributeValue;
        }
        else {
            //Total value is base plus temp bonuses from enchants
            attributeValue += tempAttributeValue;
        }
    }

    switch(type) {

        //Wolverine perk gives +0.25 Life Regeneration while holding a Claw weapon
        case Ego::Attribute::LIFE_REGEN:
            if(hasPerk(Ego::Perks::WOLVERINE)) {
                const std::shared_ptr<Object>& leftHandItem = heldItem(*this, SLOT_LEFT);
                const std::shared_ptr<Object>& rightHandItem = heldItem(*this, SLOT_RIGHT);
                if( (leftHandItem && leftHandItem->getProfile()->getIDSZ(IDSZ_PARENT).equals('C','L','A','W'))
                 || (rightHandItem && rightHandItem->getProfile()->getIDSZ(IDSZ_PARENT).equals('C','L','A','W')))
                 {
                    attributeValue += 0.25f;
                 }
            }
        break;

        case Ego::Attribute::JUMP_POWER:
            //Special value for flying Objects
            if(getAttribute(Ego::Attribute::FLY_TO_HEIGHT) > 0.0f) {
                return Object::JUMPINFINITE;
            }

            //Athletics Perks gives +25% jump power
            if(hasPerk(Ego::Perks::ATHLETICS)) {
                attributeValue *= 1.25f;
            }

            //Every point of Might increases jump power by 1%
            attributeValue *= 1.0f + (getAttribute(Ego::Attribute::MIGHT) / 100.0f);
        break;

        //Limit lowest acceleration to zero
        case Ego::Attribute::ACCELERATION:
        {
            if(attributeValue < 0.0f) return 0.0f;
        }
        break;

        //Limit lowest base attribute to 1
        case Ego::Attribute::MIGHT:
        case Ego::Attribute::AGILITY:
        case Ego::Attribute::INTELLECT:
        {
            if(attributeValue < 1.0f) return 1.0f;
        }
        break;

        default:
            //nothing, keep default case to quench GCC warnings
        break;
    }

    return attributeValue;
}

void Object::increaseBaseAttribute(const Ego::Attribute::AttributeType type, float value)
{
    IDLIB_DEBUG_ASSERT(type < _baseAttribute.size() && type != Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES);
    _baseAttribute[type] = Ego::Math::constrain(_baseAttribute[type] + value, 0.0f, 255.0f);

    //Handle current life and mana increase as well
    if(type == Ego::Attribute::MAX_LIFE) {
        _currentLife += value;
    }
    else if(type == Ego::Attribute::MAX_MANA) {
        _currentMana += value;
    }
}

bool Object::hasPerk(Ego::Perks::PerkID perk) const
{
    if(perk == Ego::Perks::NR_OF_PERKS) return true;

    //@note ZF> We also have to check our profile in case we are polymorphed and gain new
    //          skills from our new form (e.g Lumpkin form allows gunplay)
    return _perks[perk] || getProfile()->beginsWithPerk(perk);
}

std::vector<Ego::Perks::PerkID> Object::getValidPerks() const
{
    //Build list of perks we can learn
    std::vector<Ego::Perks::PerkID> result;
    for(size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i)
    {
        const Ego::Perks::PerkID id = static_cast<Ego::Perks::PerkID>(i);

        //Can we learn this perk?
        if(!getProfile()->canLearnPerk(id)) {
            continue;
        }

        //Cannot learn the same perk twice
        if(hasPerk(id)) {
            continue;
        }

        //Do we fulfill the requirements for this perk?
        const Ego::Perks::Perk& perk = Ego::Perks::activePerkHandler().getPerk(id);
        if(perk.getRequirement() == Ego::Perks::NR_OF_PERKS || hasPerk(perk.getRequirement())) {
            result.push_back(id);
        }
    }

    return result;
}

void Object::addPerk(Ego::Perks::PerkID perk)
{
    if(perk == Ego::Perks::NR_OF_PERKS) return;
    _perks[perk] = true;
}

float Object::getLife() const
{
    return _currentLife;
}

float Object::getMana() const
{
    return _currentMana;
}

bool Object::isHurt() const
{
    return isAlive() && getLife() <= getAttribute(Ego::Attribute::MAX_LIFE) - 1.0f;
}

bool Object::hasNotFullMana() const
{
    return isAlive() && getMana() <= getAttribute(Ego::Attribute::MAX_MANA) - 1.0f;
}

std::shared_ptr<Ego::Enchantment> Object::addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile, ObjectRef ownerRef, ObjectRef spawnerRef)
{
    if (enchantProfile >= ENCHANTPROFILES_MAX) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid enchant profile ", enchantProfile, Log::EndOfEntry);
        return nullptr;
    }
    const std::shared_ptr<EnchantProfile> &enchantmentProfile = activeProfileSystem().getEnchantProfile(enchantProfile);

    if(!activeProfileSystem().isLoaded(spawnerProfile)) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid spawner object profile ", spawnerProfile, Log::EndOfEntry);
        return nullptr;
    }

    std::shared_ptr<Object> owner = activeModule().getObjectHandler()[ownerRef];
    std::shared_ptr<Object> spawner = activeModule().getObjectHandler()[spawnerRef];

    std::shared_ptr<Ego::Enchantment> enchant = std::make_shared<Ego::Enchantment>(enchantmentProfile, ObjectProfileRef(spawnerProfile), owner);
    enchant->applyEnchantment(getObjRef());

    //Succeeded to apply the enchantment to the target?
    if(!enchant->isTerminated() && spawner) {
        spawner->_lastEnchantSpawned = enchant;
        return enchant;
    }

    return nullptr;
}

void Object::removeEnchantsWithIDSZ(const IDSZ2& idsz)
{
    //Nothing to do?
    if(idsz == IDSZ2::None) return;

    //Remove all active enchants that have the corresponding IDSZ
    for(const std::shared_ptr<Ego::Enchantment> &enchant : _activeEnchants)
    {
        if(enchant->isTerminated()) continue;
        if(idsz == enchant->getProfile()->removedByIDSZ) {
            enchant->requestTerminate();
        }
    }
}

const std::forward_list<std::shared_ptr<Ego::Enchantment>>& Object::getActiveEnchants() const
{
    return _activeEnchants;
}

bool Object::hasActiveEnchants() const
{
    return !_activeEnchants.empty();
}

std::shared_ptr<Ego::Enchantment> Object::getFirstActiveEnchant() const
{
    return _activeEnchants.empty() ? nullptr : _activeEnchants.front();
}

void Object::addActiveEnchant(const std::shared_ptr<Ego::Enchantment>& enchant)
{
    _activeEnchants.push_front(enchant);
}

bool Object::disenchant()
{
    bool oneRemoved = false;

    for(const std::shared_ptr<Ego::Enchantment> &enchant : _activeEnchants) {
        if(enchant->isTerminated()) continue;
        enchant->requestTerminate();
        oneRemoved = true;
    }

    return oneRemoved;
}

bool Object::hasTempAttribute(const Ego::Attribute::AttributeType type) const
{
    return _tempAttribute.find(type) != _tempAttribute.end();
}

float Object::getTempAttributeValue(const Ego::Attribute::AttributeType type) const
{
    const auto result = _tempAttribute.find(type);
    return result == _tempAttribute.end() ? 0.0f : result->second;
}

void Object::setTempAttribute(const Ego::Attribute::AttributeType type, const float value)
{
    _tempAttribute[type] = value;
}

void Object::adjustTempAttribute(const Ego::Attribute::AttributeType type, const float delta)
{
    _tempAttribute[type] += delta;
}

void Object::clearTempAttribute(const Ego::Attribute::AttributeType type)
{
    _tempAttribute.erase(type);
}

bool Object::isFlying() const
{
    return getAttribute(Ego::Attribute::FLY_TO_HEIGHT) > 0.0f;
}

bool Object::canSeeKurses() const
{
    return getAttribute(Ego::Attribute::SENSE_KURSES) > 0.0f;
}

bool Object::canOpenStuff() const
{
    return getProfile()->canOpenStuff();
}

bool Object::isWeapon() const
{
    return getProfile()->isRangedWeapon() || getProfile()->hasIDSZ(IDSZ2('X', 'W', 'E', 'P'));
}

bool Object::hasTypeIDSZ(const IDSZ2& idsz) const
{
    return getProfile()->hasTypeIDSZ(idsz);
}

bool Object::hasAnyIDSZ(const IDSZ2& idsz) const
{
    return getProfile()->hasIDSZ(idsz);
}

bool Object::matchesSpecialIDSZ(const IDSZ2& idsz) const
{
    return getProfile()->getIDSZ(IDSZ_SPECIAL) == idsz;
}

bool Object::matchesVulnerabilityIDSZ(const IDSZ2& idsz) const
{
    return getProfile()->getIDSZ(IDSZ_VULNERABILITY) == idsz;
}

bool Object::isOnSameTeam(TEAM_REF teamRef) const
{
    return VALID_TEAM_RANGE(teamRef) && VALID_TEAM_RANGE(getTeamRef()) && getTeamRef() == teamRef;
}

bool Object::isHatedByTeam(TEAM_REF teamRef) const
{
    return VALID_TEAM_RANGE(teamRef) && VALID_TEAM_RANGE(getTeamRef()) && team_hates_team(teamRef, getTeamRef());
}

std::shared_ptr<Ego::Enchantment> Object::getLastEnchantmentSpawned() const
{
    return _lastEnchantSpawned.lock();
}

void Object::setMana(const float value)
{
    _currentMana = Ego::Math::constrain(_currentMana+value, 0.00f, getAttribute(Ego::Attribute::MAX_MANA));
}

void Object::setLife(const float value)
{
    _currentLife = Ego::Math::constrain(_currentLife+value, 0.01f, getAttribute(Ego::Attribute::MAX_LIFE));
}
