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

/// @file egolib/Entities/Object_accessors.cpp
/// @brief De-inlined trivial accessors and inst.* forwarders for Object.

#include "egolib/Entities/Object_internal.h"
#include "egolib/Math/Random.hpp"  // randomizeLevelUpSeed uses Random::next

// ---------------------------------------------------------------------------
// IRenderable / IVisualControl / IAnimationControl override wrappers (20 total)
// ---------------------------------------------------------------------------

uint8_t Object::getAlpha() const { return inst.getAlpha(); }

uint8_t Object::getLight() const { return inst.getLight(); }

uint8_t Object::getSheen() const { return inst.getSheen(); }

SFP8_T Object::getUOffset() const { return inst.getUOffset(); }

SFP8_T Object::getVOffset() const { return inst.getVOffset(); }

bool Object::hasModelDescriptor() const { return inst.getModelDescriptor() != nullptr; }

const std::shared_ptr<Ego::ModelDescriptor>& Object::getModelDescriptor() const { return inst.getModelDescriptor(); }

uint8_t Object::getReflectionAlpha() const { return inst.getReflectionAlpha(); }

void Object::getTint(GLXvector4f tint, bool reflection, int type) const { inst.getTint(tint, reflection, type); }

ModelAction Object::resolveModelAction(int actionIndex) const
{
    return getProfile()->getModel()->getAction(actionIndex);
}

bool Object::startAnimation(ModelAction action, bool actionReady, bool overrideAction)
{
    return inst.startAnimation(action, actionReady, overrideAction);
}

void Object::setActionKeep(bool val) { inst.setActionKeep(val); }

ModelAction Object::getCurrentAnimation() const { return inst.getCurrentAnimation(); }

void Object::removeInterpolation() { inst.removeInterpolation(); }

const Ego::Matrix4f4f& Object::getMatrix() const { return inst.getMatrix(); }

const Ego::Matrix4f4f& Object::getReflectionMatrix() const { return inst.getReflectionMatrix(); }

const GLvertex& Object::getVertex(size_t index) const { return inst.getVertex(index); }

size_t Object::getVertexCount() const { return inst.getVertexCount(); }

void Object::flash(uint8_t value) { inst.flash(value); }

void Object::flashVariableHeight(uint8_t valueLow, int16_t low, uint8_t valueHigh, int16_t high)
{
    inst.flashVariableHeight(valueLow, low, valueHigh, high);
}

int Object::getAmbientColour() const { return inst.getAmbientColour(); }

// ---------------------------------------------------------------------------
// IAppearanceProfile / IProfiled overrides
// ---------------------------------------------------------------------------

bool Object::isPhongMapped() const { return getProfile()->isPhongMapped(); }

bool Object::hasReflection() const { return getProfile()->hasReflection(); }

bool Object::isDontCullBackfaces() const { return getProfile()->isDontCullBackfaces(); }

// ---------------------------------------------------------------------------
// Position / velocity overrides (Collidable / PhysicsData pass-throughs)
// ---------------------------------------------------------------------------

float Object::getPosZ() const { return Ego::Physics::Collidable::getPosZ(); }

const Ego::Vector3f& Object::getPosition() const { return Ego::Physics::Collidable::getPosition(); }

float Object::getFloorElevation() const { return _objectPhysics.getGroundElevation(); }

const Ego::Vector3f& Object::getVelocity() const { return PhysicsData::getVelocity(); }

void Object::setVelocity(const Ego::Vector3f& velocity) { PhysicsData::setVelocity(velocity); }

const Ego::Vector3f& Object::getSpawnPosition() const { return Ego::Physics::Collidable::getSpawnPosition(); }

float Object::getPosX() const { return Ego::Physics::Collidable::getPosX(); }

float Object::getPosY() const { return Ego::Physics::Collidable::getPosY(); }

// ---------------------------------------------------------------------------
// Identity / team
// ---------------------------------------------------------------------------

ObjectRef Object::getObjRef() const { return _objRef; }

TEAM_REF Object::getTeamRef() const { return team; }

void Object::setTeamRef(TEAM_REF teamRef) { team = teamRef; }

TEAM_REF Object::getBaseTeamRef() const { return team_base; }

void Object::setBaseTeamRef(TEAM_REF teamRef) { team_base = teamRef; }

IDSZ2 Object::getTypeIDSZ() const { return getProfile()->getIDSZ(IDSZ_TYPE); }

IDSZ2 Object::getHateIDSZ() const { return getProfile()->getIDSZ(IDSZ_HATE); }

bool Object::isItem() const { return isitem; }

// ---------------------------------------------------------------------------
// Holder / platform / attachment
// ---------------------------------------------------------------------------

ObjectRef Object::getHolderRef() const { return attachedto; }

void Object::setHolderRef(ObjectRef holderRef) { attachedto = holderRef; }

ObjectRef Object::getAttachedPlatformRef() const { return onwhichplatform_ref; }

slot_t Object::getAttachmentSlot() const { return inwhich_slot; }

void Object::setAttachmentSlot(slot_t slot) { inwhich_slot = slot; }

ObjectRef Object::getInventoryHolderRef() const { return inwhich_inventory; }

void Object::setInventoryHolderRef(ObjectRef holderRef) { inwhich_inventory = holderRef; }

bool Object::isPlatform() const { return platform; }

void Object::setPlatform(bool platformState) { platform = platformState; }

bool Object::canUsePlatforms() const { return canuseplatforms; }

void Object::setCanUsePlatforms(bool enabled) { canuseplatforms = enabled; }

int Object::getHoldingWeight() const { return holdingweight; }

void Object::setHoldingWeight(int weight) { holdingweight = weight; }

void Object::adjustHoldingWeight(int delta) { holdingweight += delta; }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool Object::isTerminated() const { return _terminateRequested; }

void Object::markTerminateRequested() { _terminateRequested = true; }

bool Object::isMount() const { return getProfile()->isMount(); }

// ---------------------------------------------------------------------------
// Player / alive / visibility flags
// ---------------------------------------------------------------------------

bool Object::isPlayer() const { return islocalplayer; }

PLA_REF Object::getPlayerNumber() const { return is_which_player; }

void Object::setPlayerNumber(PLA_REF playerNumber) { is_which_player = playerNumber; }

void Object::setLocalPlayer(bool localPlayer) { islocalplayer = localPlayer; }

bool Object::isAlive() const { return _isAlive; }

bool Object::isNameKnown() const { return nameknown; }

void Object::setNameKnown(bool known) { nameknown = known; }

bool Object::isAmmoKnown() const { return ammoknown; }

void Object::setAmmoKnown(bool known) { ammoknown = known; }

bool Object::isInvincible() const { return invictus; }

void Object::setInvincible(bool invincible) { invictus = invincible; }

bool Object::isKursed() const { return iskursed; }

void Object::setKursed(bool kursed) { iskursed = kursed; }

bool Object::isHitReady() const { return hitready; }

void Object::setHitReady(bool ready) { hitready = ready; }

bool Object::isEquipped() const { return isequipped; }

void Object::setEquipped(bool equipped) { isequipped = equipped; }

void Object::setItem(bool item) { isitem = item; }

bool Object::isShopItem() const { return isshopitem; }

void Object::setShopItem(bool shopItem) { isshopitem = shopItem; }

bool Object::canBeCrushed() const { return canbecrushed; }

void Object::setCanBeCrushed(bool crushable) { canbecrushed = crushable; }

uint8_t Object::getSparkle() const { return sparkle; }

void Object::setSparkle(uint8_t sparkleValue) { sparkle = sparkleValue; }

// ---------------------------------------------------------------------------
// Held objects / equipment
// ---------------------------------------------------------------------------

ObjectRef Object::getHeldObject(slot_t slot) const { return holdingwhich[slot]; }

void Object::setHeldObject(slot_t slot, ObjectRef objectRef) { holdingwhich[slot] = objectRef; }

ObjectRef Object::getEquipment(inventory_t slot) const { return equipment[slot]; }

void Object::setEquipment(inventory_t slot, ObjectRef objectRef) { equipment[slot] = objectRef; }

// ---------------------------------------------------------------------------
// Size / fat
// ---------------------------------------------------------------------------

float Object::getBaseFat() const { return fat_stt; }

void Object::setBaseFat(float fat) { fat_stt = fat; }

float Object::getFat() const { return fat; }

void Object::setFatRaw(float currentFat) { fat = currentFat; }

float Object::getTargetFat() const { return fat_goto; }

void Object::setTargetFat(float fat) { fat_goto = fat; }

int16_t Object::getResizeTimeRemaining() const { return fat_goto_time; }

void Object::setResizeTimeRemaining(int16_t remaining) { fat_goto_time = remaining; }

// ---------------------------------------------------------------------------
// Collision volumes / bump
// ---------------------------------------------------------------------------

const bumper_t& Object::getInitialBump() const { return bump_stt; }

void Object::setInitialBump(const bumper_t& baseBump) { bump_stt = baseBump; }

const bumper_t& Object::getCurrentBump() const { return bump; }

void Object::setCurrentBump(const bumper_t& currentBump) { bump = currentBump; }

const bumper_t& Object::getSavedBump() const { return bump_save; }

void Object::setSavedBump(const bumper_t& savedBump) { bump_save = savedBump; }

void Object::initializeBaseBump(const bumper_t& baseBump)
{
    setInitialBump(baseBump);
    setSavedBump(baseBump);
}

const bumper_t& Object::getLooseBump() const { return bump_1; }

void Object::setLooseBump(const bumper_t& looseBump) { bump_1 = looseBump; }

const oct_bb_t& Object::getMinCollisionVolume() const { return chr_min_cv; }

void Object::setMinCollisionVolume(const oct_bb_t& minCollisionVolume) { chr_min_cv = minCollisionVolume; }

const oct_bb_t& Object::getMaxCollisionVolume() const { return chr_max_cv; }

void Object::setMaxCollisionVolume(const oct_bb_t& maxCollisionVolume) { chr_max_cv = maxCollisionVolume; }

const oct_bb_t& Object::getSlotCollisionVolume(slot_t slot) const { return slot_cv[slot]; }

void Object::setSlotCollisionVolume(slot_t slot, const oct_bb_t& slotCollisionVolume) { slot_cv[slot] = slotCollisionVolume; }

void Object::setCollisionVolumes(const oct_bb_t& minCollisionVolume,
                                 const oct_bb_t& maxCollisionVolume,
                                 const std::array<oct_bb_t, SLOT_COUNT>& slotCollisionVolumes)
{
    chr_min_cv = minCollisionVolume;
    chr_max_cv = maxCollisionVolume;
    slot_cv = slotCollisionVolumes;
}

// ---------------------------------------------------------------------------
// Physics / axis-aligned box
// ---------------------------------------------------------------------------

const Ego::AxisAlignedBox2f& Object::getAxisAlignedBox2D() const { return _objectPhysics.getAxisAlignedBox2D(); }

uint32_t Object::getPhysicsWeight() const { return phys.weight; }

// ---------------------------------------------------------------------------
// Stats / mana
// ---------------------------------------------------------------------------

float Object::getMaxMana() const { return getAttribute(Ego::Attribute::MAX_MANA); }

bool Object::getShowStatus() const { return _showStatus; }

void Object::setShowStatus(const bool val) { _showStatus = val; }

// ---------------------------------------------------------------------------
// Experience / level
// ---------------------------------------------------------------------------

uint8_t Object::getExperienceLevel() const { return experiencelevel + 1; }

uint8_t Object::getExperienceLevelIndex() const { return experiencelevel; }

void Object::setExperienceLevelIndex(uint8_t levelIndex) { experiencelevel = levelIndex; }

Gender Object::getGender() const { return gender; }

void Object::setGender(Gender objectGender) { gender = objectGender; }

uint32_t Object::getExperience() const { return experience; }

void Object::setExperience(uint32_t value) { experience = value; }

// ---------------------------------------------------------------------------
// Attribute shift overrides
// ---------------------------------------------------------------------------

void Object::setRedShift(int value)
{
    setBaseAttribute(Ego::Attribute::RED_SHIFT, value);
}

void Object::setGreenShift(int value)
{
    setBaseAttribute(Ego::Attribute::GREEN_SHIFT, value);
}

void Object::setBlueShift(int value)
{
    setBaseAttribute(Ego::Attribute::BLUE_SHIFT, value);
}

void Object::setFlyHeight(float height)
{
    setBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT, height < 0.0f ? 0.0f : height);
}

// ---------------------------------------------------------------------------
// Ammo
// ---------------------------------------------------------------------------

uint16_t Object::getAmmoMax() const { return ammomax; }

void Object::setAmmoMax(uint16_t maxAmmo) { ammomax = maxAmmo; }

uint16_t Object::getAmmo() const { return ammo; }

void Object::setAmmo(uint16_t ammoCount) { ammo = ammoCount; }

// ---------------------------------------------------------------------------
// Visibility / detection
// ---------------------------------------------------------------------------

bool Object::canSeeInvisible() const { return getAttribute(Ego::Attribute::SEE_INVISIBLE) > 0.0f; }

// ---------------------------------------------------------------------------
// Level up / rally
// ---------------------------------------------------------------------------

uint32_t Object::getRallyDuration() const { return _reallyDuration; }

uint32_t Object::getLevelUpSeed() const { return _levelUpSeed; }

void Object::randomizeLevelUpSeed()
{
    _levelUpSeed = Random::next(Random::next<uint32_t>(std::numeric_limits<uint32_t>::max()));
}

// ---------------------------------------------------------------------------
// Skin / model
// ---------------------------------------------------------------------------

SKIN_T Object::getSkin() const { return skin; }

SKIN_T Object::getBaseSkin() const { return skin_stt; }

void Object::setBaseSkin(SKIN_T skinNumber) { skin_stt = skinNumber; }

ObjectProfileRef Object::getBaseModelRef() const { return basemodel_ref; }

void Object::setBaseModelRef(ObjectProfileRef profileRef) { basemodel_ref = profileRef; }

bool Object::isOverlay() const { return is_overlay; }

void Object::setOverlay(bool overlayState) { is_overlay = overlayState; }

float Object::getBaseShadowSize() const { return shadow_size_stt; }

void Object::setBaseShadowSize(float shadowSize) { shadow_size_stt = shadowSize; }

uint32_t Object::getShadowSize() const { return shadow_size; }

void Object::setShadowSize(uint32_t shadowSize) { shadow_size = shadowSize; }

uint32_t Object::getSavedShadowSize() const { return shadow_size_save; }

void Object::setSavedShadowSize(uint32_t shadowSize) { shadow_size_save = shadowSize; }

// ---------------------------------------------------------------------------
// Profile ID
// ---------------------------------------------------------------------------

ObjectProfileRef Object::getProfileID() const { return _profileID; }

// ---------------------------------------------------------------------------
// Damage type / threshold
// ---------------------------------------------------------------------------

DamageType Object::getDamageTargetType() const { return damagetarget_damagetype; }

void Object::setDamageTargetType(DamageType damageType) { damagetarget_damagetype = damageType; }

DamageType Object::getReaffirmDamageType() const { return reaffirm_damagetype; }

void Object::setReaffirmDamageType(DamageType damageType) { reaffirm_damagetype = damageType; }

SFP8_T Object::getDamageThreshold() const { return damage_threshold; }

void Object::setDamageThreshold(SFP8_T threshold) { damage_threshold = threshold; }

// ---------------------------------------------------------------------------
// Weapon / shield type
// ---------------------------------------------------------------------------

bool Object::isRangedWeapon() const { return getProfile() && getProfile()->isRangedWeapon(); }

bool Object::isMeleeWeapon() const
{
    return getProfile() && !getProfile()->isRangedWeapon() && getProfile()->getWeaponAction() != ACTION_PA;
}

bool Object::isShield() const { return getProfile() && getProfile()->getWeaponAction() == ACTION_PA; }

// ---------------------------------------------------------------------------
// Grog / daze / misc timers
// ---------------------------------------------------------------------------

bool Object::canBeGrogged() const { return getProfile() && getProfile()->canBeGrogged(); }

bool Object::canBeDazed() const { return getProfile() && getProfile()->canBeDazed(); }

int16_t Object::getGrogTimer() const { return grog_timer; }

void Object::setGrogTimer(int16_t timer) { grog_timer = timer; }

int16_t Object::getDazeTimer() const { return daze_timer; }

void Object::setDazeTimer(int16_t timer) { daze_timer = timer; }

int16_t Object::getBoredTimer() const { return bore_timer; }

void Object::setBoredTimer(int16_t timer) { bore_timer = timer; }

uint8_t Object::getCarefulTimer() const { return careful_timer; }

void Object::setCarefulTimer(uint8_t timer) { careful_timer = timer; }

uint16_t Object::getReloadTimer() const { return reload_timer; }

void Object::setReloadTimer(uint16_t timer) { reload_timer = timer; }

uint8_t Object::getDamageTimer() const { return damage_timer; }

void Object::setDamageTimer(uint8_t timer) { damage_timer = timer; }

bool Object::shouldDrawIcon() const { return draw_icon; }

void Object::setDrawIcon(bool drawIcon) { draw_icon = drawIcon; }

bool Object::isInWater() const { return inwater; }

void Object::setInWater(bool inWater) { inwater = inWater; }

int Object::getDismountTimer() const { return dismount_timer; }

void Object::setDismountTimer(int timer) { dismount_timer = timer; }

ObjectRef Object::getDismountObject() const { return dismount_object; }

void Object::setDismountObject(ObjectRef objectRef) { dismount_object = objectRef; }

// ---------------------------------------------------------------------------
// Input / jump / movement
// ---------------------------------------------------------------------------

bool Object::isAnyLatchButtonPressed() { return _inputLatchesPressed.any(); }

uint8_t Object::getJumpTimer() const { return jump_timer; }

void Object::setJumpTimer(uint8_t timer) { jump_timer = timer; }

uint8_t Object::getJumpNumber() const { return jumpnumber; }

void Object::setJumpNumber(uint8_t count) { jumpnumber = count; }

bool Object::isJumpReady() const { return jumpready; }

void Object::setJumpReady(bool ready) { jumpready = ready; }

uint8_t Object::getStoppedByMask() const { return stoppedby; }

void Object::setStoppedByMask(uint8_t mask) { stoppedby = mask; }

ObjectRef Object::getBumpListNext() const { return bumplist_next; }

void Object::setBumpListNext(ObjectRef objectRef) { bumplist_next = objectRef; }

turn_mode_t Object::getTurnMode() const { return turnmode; }

void Object::setTurnMode(turn_mode_t mode) { turnmode = mode; }

Facing Object::getFacingZ() const { return ori.facing_z; }

void Object::setFacingZ(Facing facing) { ori.facing_z = facing; }

Facing Object::getMapTwistFacingX() const { return ori.map_twist_facing_x; }

void Object::setMapTwistFacingX(Facing facing) { ori.map_twist_facing_x = facing; }

Facing Object::getMapTwistFacingY() const { return ori.map_twist_facing_y; }

void Object::setMapTwistFacingY(Facing facing) { ori.map_twist_facing_y = facing; }

Facing Object::getPreviousFacingZ() const { return ori_old.facing_z; }

void Object::setPreviousFacingZ(Facing facing) { ori_old.facing_z = facing; }
