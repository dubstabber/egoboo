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

/// @file egolib/Entities/Object_attributes_behaviors.cpp
/// @brief Polymorph, stealth, team, skill, and money Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/CharacterParticleOps.h"                          // DisplayMsg_printf / disaffirm_attached_particles
#include "egolib/Graphics/IBillboardSystem.hpp"        // Ego::Graphics::tryActiveBillboardSystem
#include "egolib/Logic/IPerkHandler.hpp"               // Ego::Perks::activePerkHandler + Perk
#include "egolib/Log/_Include.hpp"                     // Log::activeTarget
#include "egolib/game/Graphics/Billboard.hpp"         // Ego::Graphics::Billboard::Flags
#include "egolib/Physics/PhysicalConstants.hpp"  // Ego::Physics::CHR_INFINITE_WEIGHT / CHR_MAX_WEIGHT
#include "egolib/AI/LineOfSight.hpp" // line_of_sight_info_t
#include "egolib/Entities/Object_attributes_internal.h"

namespace
{
IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

void publishStealthBillboardIfAvailable(ObjectRef objectRef, const std::string& text)
{
    if (auto* billboardSystem = Ego::Graphics::tryActiveBillboardSystem())
    {
        billboardSystem->makeBillboard(objectRef,
                                       text,
                                       Ego::Colour4f::white(),
                                       Ego::Colour4f::white(),
                                       2,
                                       Ego::Graphics::Billboard::Flags::All);
    }
}
}

void Object::polymorphObject(ObjectProfileRef profileID, const SKIN_T newSkin)
{
    if(!activeProfileSystem().isLoaded(profileID)) {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to polymorph object: target profile ", profileID, " does not exist", Log::EndOfEntry);
        return;
    }

    _profileID = profileID;
    _profile = activeProfileSystem().getProfile(_profileID);

    //Exit stealth if we change form
    deactivateStealth();

    //Get any items we are holding
    const std::shared_ptr<Object> &leftItem = heldItem(*this, SLOT_LEFT);
    const std::shared_ptr<Object> &rightItem = heldItem(*this, SLOT_RIGHT);

    // Drop left weapon if we have no left grip
    if ( leftItem && ( !_profile->isSlotValid(SLOT_LEFT) || _profile->isMount() ) )
    {
        leftItem->detachFromHolder(true, true);
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
        rightItem->detachFromHolder(true, true);
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
    chr_update_matrix(*this, true);

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

    publishStealthBillboardIfAvailable(getObjRef(), "Revealed!");
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
    std::vector<ObjectRef> nearbyObjectRefs;
    activeModule().getObjectHandler().findObjectRefs(getPosX(), getPosY(), WIDE, nearbyObjectRefs, false);
    for (const ObjectRef& objectRef : nearbyObjectRefs) {
        Object* object = activeModule().getObjectHandler().get(objectRef);
        if (object == nullptr) {
            continue;
        }

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
        if (line_of_sight_info_t::blocked(lineOfSightInfo, activeModule())) {
            continue;
        }

        //Camouflage Perk allows us to hide as long as enemies aren't directly looking at us
        if(hasPerk(Ego::Perks::CAMOUFLAGE) && !object->isFacingLocation(getPosX(), getPosY())) {
            continue;
        }

        //We can't stealth while an enemy is nearby
        if(isPlayer()) {
            publishStealthBillboardIfAvailable(getObjRef(), "Hide Failed!");
            audioSystem().playSound(getPosition(), audioSystem().getGlobalSound(GSND_STEALTH_END));
        }
        return false;
    }

    //All good, we are now stealthed!
    _stealth = true;
    setAlpha(0);
    publishStealthBillboardIfAvailable(getObjRef(), "Hidden!");
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
