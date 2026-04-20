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

/// @file egolib/game/Entities/Object_lifecycle.cpp
/// @brief Lifecycle-oriented Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}
}

Object::Object(ObjectProfileRef proRef, ObjectRef objRef) :
    ai(),
    gender(Gender::Male),
    experience(0),
    experiencelevel(0),
    ammomax(0),
    ammo(0),
    holdingwhich(),
    equipment(),
    team(Team::TEAM_NULL),
    team_base(Team::TEAM_NULL),
    fat_stt(0.0f),
    fat(0.0f),
    fat_goto(0.0f),
    fat_goto_time(0),

    jump_timer(JUMPDELAY),
    jumpnumber(0),
    jumpready(false),

    attachedto(),
    inwhich_slot(SLOT_LEFT),
    inwhich_inventory(),
    platform(false),
    canuseplatforms(false),
    holdingweight(0),
    damagetarget_damagetype(DamageType::DAMAGE_SLASH),
    reaffirm_damagetype(DamageType::DAMAGE_SLASH),
    damage_threshold(0),
    is_which_player(INVALID_PLA_REF),
    islocalplayer(false),
    invictus(false),
    iskursed(false),
    nameknown(false),
    ammoknown(false),
    hitready(true),
    isequipped(false),
    isitem(false),
    isshopitem(false),
    canbecrushed(false),

    // Misc timers
    grog_timer(0),
    daze_timer(0),
    bore_timer(0),
    careful_timer(CAREFULTIME),
    reload_timer(0),
    damage_timer(0),

    draw_icon(false),
    sparkle(NOSPARKLE),
    shadow_size_stt(0.0f),
    shadow_size(0),
    shadow_size_save(0),
    is_overlay(false),
    skin(0),
    skin_stt(0),
    basemodel_ref(proRef),

    bump_stt(),
    bump(),
    bump_save(),
    bump_1(),
    chr_max_cv(),
    chr_min_cv(),
    slot_cv(),

    stoppedby(0),

    ori(),
    ori_old(),
    bumplist_next(),

    turnmode(TURNMODE_VELOCITY),

    inwater(false),
    dismount_timer(0),  /// @note ZF@> If this is != 0 then scorpion claws and riders are dropped at spawn (non-item objects)
    dismount_object(),

    _terminateRequested(false),
    _objRef(objRef),
    _profileID(proRef),
    _profile(EngineContext::get().profileSystem().getProfile(_profileID)),
    _showStatus(false),
    _isAlive(true),
    _name("*NONE*"),

    _currentLife(0.0f),
    _currentMana(0.0f),
    _baseAttribute(),
    _tempAttribute(),

    _inventory(),
    _money(0),
    _perks(),
    _levelUpSeed(Random::next(std::numeric_limits<uint32_t>::max())),

    // Graphics
    inst(*this),

    // Physics
    _objectPhysics(*this),

    // Input commands
    _inputLatchesPressed(),

    // Non-persistent variables
    _hasBeenKilled(false),
    _reallyDuration(0),
    _stealth(false),
    _stealthTimer(0),
    _observationTimer((objRef.get() % ONESECOND) + worldUpdateCount()), // spread observations so all characters don't happen at the same time

    // Enchants
    _activeEnchants(),
    _lastEnchantSpawned()
{
    holdingwhich.fill(ObjectRef::Invalid);
    _baseAttribute.fill(0.0f);
    equipment.fill(ObjectRef::Invalid);

    ori.map_twist_facing_y = orientation_t::MAP_TURN_OFFSET;
    ori.map_twist_facing_x = orientation_t::MAP_TURN_OFFSET;

    for (size_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i) {
        const idlib::interval<float>& baseRange = _profile->getAttributeBase(static_cast<Ego::Attribute::AttributeType>(i));
        _baseAttribute[i] = Random::next(baseRange);
    }

    resetBoredTimer();
}

Object::~Object()
{
    // Detach the character from the active game.
    if (GameModule* module = tryActiveModule()) {
        removeFromGame(this);

        for (const std::shared_ptr<Object>& pitem : _inventory.iterate()) {
            pitem->requestTerminate();
        }

        if (isAlive() && !getProfile()->isInvincible() && VALID_TEAM_RANGE(team_base)) {
            getMutableTeam(team_base).decreaseMorale();
        }

        clearTeamLeadershipIfSelf(team);

        disaffirm_attached_particles(getObjRef());
    }
}

void Object::requestTerminate()
{
    activeModule().getObjectHandler().remove(getObjRef());
}

void Object::removeFromGame(Object* obj)
{
    auto objRef = obj->getObjRef();

    obj->sparkle = NOSPARKLE;

    obj->team = obj->team_base;
    if (VALID_TEAM_RANGE(obj->team)) {
        obj->getMutableTeam(obj->team).decreaseMorale();
        obj->clearTeamLeadershipIfSelf(obj->team);
    }

    activeModule().removeShopOwner(objRef);

    if (activeModule().getObjectHandler().exists(obj->attachedto)) {
        obj->detatchFromHolder(true, false);
    }

    const std::shared_ptr<Object>& leftItem = obj->getLeftHandItem();
    if (leftItem && leftItem->isItem()) {
        leftItem->detatchFromHolder(true, false);
    }

    const std::shared_ptr<Object>& rightItem = obj->getRightHandItem();
    if (rightItem && rightItem->isItem()) {
        rightItem->detatchFromHolder(true, false);
    }

    audioSystem().stopObjectLoopingSounds(objRef);
}

void Object::respawn()
{
    if (isAlive()) {
        return;
    }

    const std::shared_ptr<ObjectProfile>& profile = getProfile();

    EngineContext::get().particleHandler().spawnPoof(this->toSharedPointer());
    disaffirm_attached_particles(getObjRef());

    for (std::shared_ptr<Object>& object : activeModule().getObjectHandler().iterator()) {
        if (object && object->getAttachedPlatformRef() == getObjRef()) {
            object->detachFromPlatform();
        }
    }

    _isAlive = true;
    resetBoredTimer();
    resetInputCommands();
    careful_timer = CAREFULTIME;
    _currentLife = getAttribute(Ego::Attribute::MAX_LIFE);
    _currentMana = getAttribute(Ego::Attribute::MAX_MANA);
    setPosition(getSpawnPosition());
    setVelocity(idlib::zero<Ego::Vector3f>());
    team = team_base;
    canbecrushed = false;
    ori.map_twist_facing_y = orientation_t::MAP_TURN_OFFSET;
    ori.map_twist_facing_x = orientation_t::MAP_TURN_OFFSET;
    claimTeamLeadershipIfUnset(team);
    if (!isInvincible()) {
        getMutableTeam().increaseMorale();
    }

    inst.startAnimation(ACTION_DA, true, true);

    fat_stt = profile->getSize();
    shadow_size_stt = profile->getShadowSize();
    bump_stt.size = profile->getBumpSize();
    bump_stt.size_big = profile->getBumpSizeBig();
    bump_stt.height = profile->getBumpHeight();

    shadow_size_save = shadow_size_stt;
    bump_save.size = bump_stt.size;
    bump_save.size_big = bump_stt.size_big;
    bump_save.height = bump_stt.height;

    recalculateCollisionSize();

    platform = profile->isPlatform();
    canuseplatforms = profile->canUsePlatforms();
    _baseAttribute[Ego::Attribute::FLY_TO_HEIGHT] = profile->getFlyHeight();
    phys.bumpdampen = profile->getBumpDampen();

    ai.alert = ALERTIF_CLEANEDUP;
    ai.setTarget(getObjRef());
    ai.timer = 0;

    grog_timer = 0;
    daze_timer = 0;

    for (const std::shared_ptr<Object>& pitem : _inventory.iterate()) {
        if (pitem->isequipped) {
            pitem->isequipped = false;
            SET_BIT(ai.alert, ALERTIF_PUTAWAY);
        }
    }

    inst.setObjectProfile(getProfile());
    chr_update_matrix(this, true);

    if (!isHidden()) {
        reaffirm_attached_particles(getObjRef());
    }
}

void Object::respawnInPlace()
{
    const Ego::Vector3f savedPosition = getPosition();
    respawn();
    setPosition(savedPosition);
}

size_t Object::getInventoryMaxItems() const
{
    return _inventory.getMaxItems();
}

size_t Object::getFirstFreeInventorySlot() const
{
    return _inventory.getFirstFreeSlotNumber();
}

ObjectRef Object::getInventoryItemRef(size_t slotNumber) const
{
    return _inventory.getItemID(slotNumber);
}

std::shared_ptr<Object> Object::getInventoryItem(size_t slotNumber) const
{
    return _inventory.getItem(slotNumber);
}

std::vector<ObjectRef> Object::getInventoryItemRefs() const
{
    std::vector<ObjectRef> refs;
    for (const std::shared_ptr<Object>& item : _inventory.iterate())
    {
        refs.push_back(item ? item->getObjRef() : ObjectRef::Invalid);
    }
    return refs;
}

std::vector<std::shared_ptr<Object>> Object::getInventoryItems() const
{
    return _inventory.iterate();
}

void Object::setInventoryItem(size_t slotNumber, const std::shared_ptr<Object>& item)
{
    _inventory.setItem(slotNumber, item);
}

bool Object::removeInventoryItem(const std::shared_ptr<Object>& item, bool ignoreKurse)
{
    return _inventory.removeItem(item, ignoreKurse);
}

const std::shared_ptr<Object>& Object::toSharedPointer() const
{
    return activeModule().getObjectHandler()[getObjRef()];
}

void Object::setName(const std::string& name)
{
    _name = name;
}

const std::shared_ptr<ObjectProfile>& Object::getProfile() const
{
    return _profile;
}

void Object::resetInputCommands()
{
    _objectPhysics.setDesiredVelocity(idlib::zero<Ego::Vector2f>());
    _inputLatchesPressed.reset();
}

bool Object::canCollide() const
{
    if (isTerminated()) {
        return false;
    }

    if (isHidden()) {
        return false;
    }

    if (isBeingHeld()) {
        return false;
    }

    if (oct_bb_t::empty(chr_max_cv)) {
        return false;
    }

    return true;
}

uint16_t Object::getMoney() const
{
    return _money;
}

void Object::resetBoredTimer()
{
    bore_timer = Random::next<uint16_t>(250, 800);
}
