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

/// @file egolib/game/Entities/Object_attributes.cpp
/// @brief Attributes, team, stealth, and enchantment Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

const std::shared_ptr<Object>& heldItem(const Object& object, slot_t slot)
{
    return GameSessionContext::get().activeModule().getObjectHandler()[object.getHeldObject(slot)];
}
}

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
        const Ego::Perks::Perk& perk = EngineContext::get().perkHandler().getPerk(id);
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

std::shared_ptr<Ego::Enchantment> Object::addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile, const std::shared_ptr<Object>& owner, const std::shared_ptr<Object> &spawner)
{
    if (enchantProfile >= ENCHANTPROFILES_MAX) {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid enchant profile ", enchantProfile, Log::EndOfEntry);
        return nullptr;
    }
    const std::shared_ptr<EnchantProfile> &enchantmentProfile = EngineContext::get().profileSystem().getEnchantProfile(enchantProfile);

    if(!EngineContext::get().profileSystem().isLoaded(spawnerProfile)) {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to add enchant with invalid spawner object profile ", spawnerProfile, Log::EndOfEntry);
        return nullptr;
    }

    std::shared_ptr<Ego::Enchantment> enchant = std::make_shared<Ego::Enchantment>(enchantmentProfile, ObjectProfileRef(spawnerProfile), owner);
    enchant->applyEnchantment(this->toSharedPointer());

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

void Object::polymorphObject(ObjectProfileRef profileID, const SKIN_T newSkin)
{
    if(!EngineContext::get().profileSystem().isLoaded(profileID)) {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to polymorph object: target profile ", profileID, " does not exist", Log::EndOfEntry);
        return;
    }

    _profileID = profileID;
    _profile = EngineContext::get().profileSystem().getProfile(_profileID);

    //Exit stealth if we change form
    deactivateStealth();

    //Get any items we are holding
    const std::shared_ptr<Object> &leftItem = heldItem(*this, SLOT_LEFT);
    const std::shared_ptr<Object> &rightItem = heldItem(*this, SLOT_RIGHT);

    // Drop left weapon if we have no left grip
    if ( leftItem && ( !_profile->isSlotValid(SLOT_LEFT) || _profile->isMount() ) )
    {
        leftItem->detatchFromHolder(true, true);
        leftItem->detachFromPlatform();

        if ( isMount() )
        {
            leftItem->setVelocity({leftItem->getVelocity().x(),
                                   leftItem->getVelocity().y(),
                                   DISMOUNTZVEL});
            leftItem->jump_timer = JUMPDELAY;
            leftItem->movePosition(0.0f, 0.0f, DISMOUNTZVEL);
        }
    }

    // Drop right weapon if we have no right grip
    if ( rightItem && !_profile->isSlotValid(SLOT_RIGHT) )
    {
        rightItem->detatchFromHolder(true, true);
        rightItem->detachFromPlatform();

        if ( isMount() )
        {
            rightItem->setVelocity({rightItem->getVelocity().x(),
                                    rightItem->getVelocity().y(),
                                    DISMOUNTZVEL});
            rightItem->jump_timer = JUMPDELAY;
            rightItem->movePosition(0.0f, 0.0f, DISMOUNTZVEL);
        }
    }

    // Stuff that must be set
    stoppedby = _profile->getStoppedByMask();

    // Ammo
    ammomax = _profile->getMaxAmmo();
    ammo    = _profile->getAmmo();

    // Gender
    switch (_profile->getGender()) {
        case GenderProfile::Female:
            gender = Gender::Female;
            break;
        case GenderProfile::Male:
            gender = Gender::Male;
            break;
        case GenderProfile::Neuter:
            gender = Gender::Neuter;
            break;
        case GenderProfile::Random:
            /// @todo Random means, retain current gender, which is not intuitive.
            //gender = Random::getRandomElement(std::vector<Gender>{Gender::Female, Gender::Male, Gender::Neuter});
            break;
    };

    // AI stuff
    ai.state = 0;
    ai.timer          = 0;
    turnmode          = TURNMODE_VELOCITY;
    resetInputCommands();

    // Flags
    platform        = _profile->isPlatform();
    canuseplatforms = _profile->canUsePlatforms();
    isitem          = _profile->isItem();
    invictus        = _profile->isInvincible();
    jump_timer      = JUMPDELAY;
    reaffirm_damagetype = _profile->getReaffirmDamageType();

    //Physics
    phys.bumpdampen = _profile->getBumpDampen();

    if (CAP_INFINITE_WEIGHT == _profile->getWeight())
    {
        phys.weight = Ego::Physics::CHR_INFINITE_WEIGHT;
    }
    else
    {
        phys.weight = std::min<uint32_t>(_profile->getWeight() * fat * fat * fat, Ego::Physics::CHR_MAX_WEIGHT);
    }

    // Character size and bumping
    // set the character size so that the new model is the same size as the old model
    // the model will then morph its size to the correct size over time
    {
        float oldFat = fat;
        float newFat;

        if ( 0.0f == bump.size ) {
            newFat = _profile->getSize();
        }
        else {
            newFat = ( _profile->getBumpSize() * _profile->getSize() ) / bump.size;
        }

        // Spellbooks should stay the same size, even if their spell effect cause changes in size
        if (getProfileID() == ObjectProfileRef(SPELLBOOK)) newFat = oldFat = 1.00f;

        // copy all the cap size info over, as normal
        fat_stt           = _profile->getSize();
        shadow_size_stt   = _profile->getShadowSize();
        bump_stt.size     = _profile->getBumpSize();
        bump_stt.size_big = _profile->getBumpSizeBig();
        bump_stt.height   = _profile->getBumpHeight();

        //Initialize model size and collision box
        fat                = fat_stt;
        shadow_size_save   = shadow_size_stt;
        bump_save.size     = bump_stt.size;
        bump_save.size_big = bump_stt.size_big;
        bump_save.height   = bump_stt.height;
        recalculateCollisionSize();

        // make the model's size congruent
        if (0.0f != newFat && newFat != oldFat)
        {
            setFat(newFat);
            fat_goto      = oldFat;
            fat_goto_time = SIZETIME;
        }
        else
        {
            setFat(oldFat);
            fat_goto      = oldFat;
            fat_goto_time = 0;
        }
    }

    //Remove attached particles before changing our model
    disaffirm_attached_particles(getObjRef());

    //Actually change the model
    inst.setObjectProfile(getProfile());
    chr_update_matrix(this, true);

    // Set the skin after changing the model in ObjectGraphics::setProfile()
    setSkin(newSkin);

    // Must set the wepon grip AFTER the model is changed in ObjectGraphics::setProfile()
    if (isBeingHeld())
    {
        set_weapongrip(getObjRef(), attachedto, slot_to_grip_offset(inwhich_slot) );
    }

    if (leftItem)
    {
        IDLIB_DEBUG_ASSERT(leftItem->attachedto == getObjRef());
        set_weapongrip(leftItem->getObjRef(), getObjRef(), GRIP_LEFT);
    }

    if (rightItem)
    {
        IDLIB_DEBUG_ASSERT(rightItem->attachedto == getObjRef());
        set_weapongrip(rightItem->getObjRef(), getObjRef(), GRIP_RIGHT);
    }

    /// @note ZF@> disabled so that books dont burn when dropped
    //reaffirm_attached_particles( ichr );

    ai_state_t::set_changed(ai);
}

bool Object::isStealthed() const
{
    return _stealth;
}

void Object::deactivateStealth()
{
    //Not in stealth?
    if(!isStealthed()) return;

    //Reset stealth timer
    _stealthTimer = std::max<uint16_t>(_stealthTimer, ONESECOND);
    _stealth = false;

    GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Revealed!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
    audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_STEALTH_END));
    setAlpha(0xFF);
}

bool Object::activateStealth()
{
    //Already in stealth?
    if(isStealthed()) return true;

    //Not allowed to stealth yet?
    if(_stealthTimer > 0) {
        return false;
    }

    //Limit stealth atttempts to once per second
    _stealthTimer = ONESECOND;

    //Do they have the required stealth Perk?
    if(!hasPerk(Ego::Perks::STEALTH)) {
        if(isPlayer()) {
            DisplayMsg_printf("%s does not know how to stealth...", getName().c_str());
        }
        return false;
    }

    //Setup line of sight data
    line_of_sight_info_t lineOfSightInfo;
    lineOfSightInfo.x1 = getPosX();
    lineOfSightInfo.y1 = getPosY();
    lineOfSightInfo.z1 = getPosZ() + std::max(1.0f, bump.height);

    //Check if there are any nearby Objects disrupting our stealth attempt
    std::vector<std::shared_ptr<Object>> nearbyObjects = activeModule().getObjectHandler().findObjects(getPosX(), getPosY(), WIDE, false);
    for(const std::shared_ptr<Object> &object : nearbyObjects) {
        //Valid objects only
        if(object->isTerminated() || !object->isAlive() || object->isBeingHeld()) continue;

        //Skip scenery objects
        if (object->isScenery()) {
            continue;
        }

        //Ignore objects that are doing the sleep animation
        if(object->getCurrentAnimation() == ACTION_MK) {
            continue;
        }

        //Do they consider us an enemy?
        if(!object->getTeam().hatesTeam(getTeam())) {
            continue;
        }

        //Can they see us?
        lineOfSightInfo.x0         = object->getPosX();
        lineOfSightInfo.y0         = object->getPosY();
        lineOfSightInfo.z0         = object->getPosZ() + std::max(1.0f, object->bump.height);
        lineOfSightInfo.stopped_by = object->stoppedby;
        if (line_of_sight_info_t::blocked(lineOfSightInfo, activeModule().getMeshPointer())) {
            continue;
        }

        //Camouflage Perk allows us to hide as long as enemies aren't directly looking at us
        if(hasPerk(Ego::Perks::CAMOUFLAGE) && !object->isFacingLocation(getPosX(), getPosY())) {
            continue;
        }

        //We can't stealth while an enemy is nearby
        if(isPlayer()) {
            GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Hide Failed!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
            audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_STEALTH_END));
        }
        return false;
    }

    //All good, we are now stealthed!
    _stealth = true;
    setAlpha(0);
    GFX::get().getBillboardSystem().makeBillboard(getObjRef(), "Hidden!", Ego::Colour4f::white(), Ego::Colour4f::white(), 2, Ego::Graphics::Billboard::Flags::All);
    audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_STEALTH));

    return true;
}

void Object::setTeam(TEAM_REF team_new, bool permanent)
{
    if(!VALID_TEAM_RANGE(team_new)) {
        team_new = static_cast<TEAM_REF>(Team::TEAM_NULL);
    }

    //No change?
    if(getTeamRef() == team_new) {
        return;
    }

    // do we count this character as being on a team?
    const bool canHaveTeam = !isItem() && isAlive() && !isInvincible();
    const TEAM_REF oldTeam = this->team;

    // take the character off of its old team
    if ( VALID_TEAM_RANGE(oldTeam) )
    {
        // remove the character from the old team
        if ( canHaveTeam )
        {
            getMutableTeam(oldTeam).decreaseMorale();
        }

        clearTeamLeadershipIfSelf(oldTeam);
    }

    // place the character onto its new team
    this->team = team_new;

    // switch the base team only if required
    if (permanent) {
        team_base = this->team;
    }

    // add the character to the new team
    if (canHaveTeam) {
        getMutableTeam().increaseMorale();
        claimTeamLeadershipIfUnset(this->team);
    }

    if(permanent) {
        //Set the team of our mount as well
        const std::shared_ptr<Object>& holder = activeModule().getObjectHandler()[getHolderRef()];
        if (holder && holder->isMount()) {
            holder->setTeam(team_new, false);
        }

        //Switch team of whatever we are holding as well
        if (const std::shared_ptr<Object>& leftHandItem = heldItem(*this, SLOT_LEFT)) {
            leftHandItem->setTeam(team_new, false);
        }
        if (const std::shared_ptr<Object>& rightHandItem = heldItem(*this, SLOT_RIGHT)) {
            rightHandItem->setTeam(team_new, false);
        }
    }
}

bool Object::hasSkillIDSZ(const IDSZ2& whichskill) const
{
    if (isTerminated()) return false;

    //Any [NONE] IDSZ returns always "true"
    if ( IDSZ2::None == whichskill ) return true;

    // First check the character Skill ID matches
    if ( getProfile()->getIDSZ(IDSZ_SKILL) == whichskill ) {
        return true;
    }

    // Then check if any Perk matches
    switch(whichskill.toUint32())
    {
        case IDSZ2::caseLabel('P', 'O', 'I', 'S'):
            return hasPerk(Ego::Perks::POISONRY);

        case IDSZ2::caseLabel('C', 'K', 'U', 'R'):
            return hasPerk(Ego::Perks::SENSE_KURSES);

        case IDSZ2::caseLabel('D', 'A', 'R', 'K'):
            return hasPerk(Ego::Perks::NIGHT_VISION) || hasPerk(Ego::Perks::PERCEPTIVE);

        case IDSZ2::caseLabel('A', 'W', 'E', 'P'):
            return hasPerk(Ego::Perks::WEAPON_PROFICIENCY);

        case IDSZ2::caseLabel('W', 'M', 'A', 'G'):
            return hasPerk(Ego::Perks::ARCANE_MAGIC);

        case IDSZ2::caseLabel('D', 'M', 'A', 'G'):
        case IDSZ2::caseLabel('H', 'M', 'A', 'G'):
            return hasPerk(Ego::Perks::DIVINE_MAGIC);

        case IDSZ2::caseLabel('D', 'I', 'S', 'A'):
            return hasPerk(Ego::Perks::TRAP_LORE);

        case IDSZ2::caseLabel('F', 'I', 'N', 'D'):
            return hasPerk(Ego::Perks::PERCEPTIVE);

        case IDSZ2::caseLabel('T', 'E', 'C', 'H'):
            return hasPerk(Ego::Perks::USE_TECHNOLOGICAL_ITEMS);

        case IDSZ2::caseLabel('S', 'T', 'A', 'B'):
            return hasPerk(Ego::Perks::BACKSTAB);

        case IDSZ2::caseLabel('R', 'E', 'A', 'D'):
            return hasPerk(Ego::Perks::LITERACY);

        case IDSZ2::caseLabel('W', 'A', 'N', 'D'):
            return hasPerk(Ego::Perks::THAUMATURGY);

        case IDSZ2::caseLabel('J', 'O', 'U', 'S'):
            return hasPerk(Ego::Perks::JOUSTING);

        case IDSZ2::caseLabel('T', 'E', 'L', 'E'):
            return hasPerk(Ego::Perks::TELEPORT_MASTERY);

        case IDSZ2::caseLabel('G', 'N', 'O', 'M'):
            return hasPerk(Ego::Perks::READ_GNOMISH);
    }

    //Skill not found
    return false;
}

void Object::giveMoney(int amount)
{
    _money = Ego::Math::constrain<int>(static_cast<int>(_money) + amount, 0, MAXMONEY);
}
