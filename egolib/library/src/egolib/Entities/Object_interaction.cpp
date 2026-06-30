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

/// @file egolib/game/Entities/Object_interaction.cpp
/// @brief Holder, inventory, and latch-input Object implementation.

#include "egolib/Entities/Object_internal.h"
#include "egolib/game/CharacterParticleOps.h"             // chr_do_latch_attack
#include "egolib/game/Shop.hpp"           // Shop::drop
#include "egolib/Audio/IAudioSystem.hpp"

namespace
{
IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

auto& objectHandler()
{
    return worldObjectHandler();
}

bool isKeyIDSZ(const IDSZ2& value)
{
    const uint32_t keyStart = IDSZ2('K', 'E', 'Y', 'A').toUint32();
    const uint32_t keyEnd = IDSZ2('K', 'E', 'Y', 'Z').toUint32();
    const uint32_t encoded = value.toUint32();
    return encoded >= keyStart && encoded <= keyEnd;
}
}

bool Object::detachFromHolder(const bool ignoreKurse, const bool doShop)
{
    ObjectRef holder = getHolderRef();
    Object* pholder = objectHandler().get(holder);
    if (pholder == nullptr) {
        return false;
    }

    if (!ignoreKurse && iskursed && pholder->isAlive() && isitem) {
        SET_BIT(ai.alert, ALERTIF_NOTDROPPED);
        return false;
    }

    dismount_timer = PHYS_DISMOUNT_TIME;
    dismount_object = holder;

    uint16_t hand = getAttachmentSlot();

    setHolderRef(ObjectRef::Invalid);
    if (pholder->holdingwhich[SLOT_LEFT] == getObjRef()) {
        pholder->holdingwhich[SLOT_LEFT] = ObjectRef::Invalid;
    }

    if (pholder->holdingwhich[SLOT_RIGHT] == getObjRef()) {
        pholder->holdingwhich[SLOT_RIGHT] = ObjectRef::Invalid;
    }

    if (isAlive()) {
        inst.playAction(static_cast<ModelAction>(ACTION_JB + hand), false);
    } else if (inst.getCurrentAnimation() < ACTION_KA || inst.getCurrentAnimation() > ACTION_KD) {
        const ModelAction action = getProfile()->getModel()->randomizeAction(ACTION_KA);
        inst.playAction(action, false);
        inst.setActionKeep(true);
    }

    if (chr_matrix_valid(this)) {
        setPosition(mat_getTranslate(inst.getMatrix()));
    } else {
        setPosition(pholder->getPosition());
    }

    if (EMPTY_BIT_FIELD != Collidable::test_wall()) {
        Ego::Vector3f pos_tmp = pholder->getPosition();
        pos_tmp[kZ] = getPosZ();

        setPosition(pos_tmp);
    }

    bool inshop = false;
    if (doShop) {
        inshop = Shop::drop(holder, getObjRef());
    }

    hitready = true;
    if (inshop) {
        setVelocity(Ego::Vector3f(0.0f, 0.0f, getVelocity().z()));
    } else {
        setVelocity(Ego::Vector3f(pholder->getVelocity().x(),
                                  pholder->getVelocity().y(),
                                  getVelocity().z()));
    }

    if (bump.size > 0) {
        Ego::Radians angle = FacingToRadian(Facing(ori.facing_z) + ATK_BEHIND);
        setVelocity(getVelocity() + Ego::Vector3f(std::cos(angle) * DROPXYVEL * 0.5f,
                                                  std::sin(angle) * DROPXYVEL * 0.5f,
                                                  0.0f));
    }

    setVelocity({getVelocity().x(), getVelocity().y(), DROPZVEL});

    inst.setActionLooped(false);

    if (pholder->isMount()) {
        pholder->team = pholder->team_base;
        pholder->addAIAlertBits(ALERTIF_DROPPED);
    }

    team = team_base;
    SET_BIT(ai.alert, ALERTIF_DROPPED);

    if (isitem && pholder->getProfile()->transferBlending()) {
        setAlpha(getProfile()->getAlpha());
        setLight(getProfile()->getLight());
    }

    ori.map_twist_facing_y = orientation_t::MAP_TURN_OFFSET;
    ori.map_twist_facing_x = orientation_t::MAP_TURN_OFFSET;

    if (!isAlive()) {
        const ModelAction action = getProfile()->getModel()->randomizeAction(ACTION_KA);
        inst.playAction(action, false);
        inst.setActionKeep(true);
    } else {
        inst.playAction(ACTION_JA, true);
        inst.setActionKeep(false);
    }

    chr_update_matrix(*this, true);

    return true;
}

void Object::resetAlpha()
{
    Object* mount = objectHandler().get(getHolderRef());
    if (mount == nullptr) {
        return;
    }

    if (isItem() && mount->getProfile()->transferBlending()) {
        setAlpha(getProfile()->getAlpha());
        setLight(getProfile()->getLight());
    }
}

int Object::getPrice() const
{
    uint16_t iskin;
    float price;

    ObjectProfileRef slotNumber = ObjectProfileRef::Invalid;
    if (_profileID == ObjectProfileRef(SPELLBOOK)) {
        slotNumber = basemodel_ref;
        iskin = 0;
    } else {
        slotNumber = _profileID;
        iskin = skin;
    }

    std::shared_ptr<ObjectProfile> profile = activeProfileSystem().getProfile(slotNumber);
    if (!profile) {
        return 0;
    }

    price = profile->getSkinInfo(iskin).cost;

    if (!isshopitem) price *= 0.5f;

    if (profile->isStackable()) {
        price *= ammo;
    } else if (profile->isRangedWeapon() && ammo < ammomax) {
        if (0 != ammomax) {
            price *= static_cast<float>(ammo) / static_cast<float>(ammomax);
        }
    }

    return static_cast<int>(price);
}

bool Object::isBeingHeld() const
{
    if (isInsideInventory()) {
        return true;
    }

    Object* holder = objectHandler().get(getHolderRef());
    if (holder != nullptr && !holder->isTerminated()) {
        return true;
    }

    return false;
}

bool Object::isInsideInventory() const
{
    if (getInventoryHolderRef() == ObjectRef::Invalid) {
        return false;
    }

    Object* holder = objectHandler().get(getInventoryHolderRef());
    if (holder == nullptr || holder->isTerminated()) {
        return false;
    }

    return true;
}

bool Object::wieldsItemIDSZ(const IDSZ2& idsz) const
{
    Object* leftHandItem = objectHandler().get(getHeldObject(SLOT_LEFT));
    if (leftHandItem != nullptr && leftHandItem->getProfile()->hasTypeIDSZ(idsz)) {
        return true;
    }

    Object* rightHandItem = objectHandler().get(getHeldObject(SLOT_RIGHT));
    return rightHandItem != nullptr && rightHandItem->getProfile()->hasTypeIDSZ(idsz);
}

void Object::dropMoney(int amount)
{
    static constexpr std::array<int, PIP_MONEY_COUNT> vals = {2000, 1000, 500, 200, 100, 25, 5, 1};
    static constexpr std::array<PIP_REF, PIP_MONEY_COUNT> pips =
    {
        PIP_GEM2000, PIP_GEM1000, PIP_GEM500, PIP_GEM200,
        PIP_COIN100, PIP_COIN25, PIP_COIN5, PIP_COIN1
    };
    static_assert(vals.size() == pips.size(), "Number of money particles != list size for value of money particles");

    amount = Ego::Math::constrain<int>(amount, 0, getMoney());

    if (getPosZ() <= (PitKillDepth / 2) || amount <= 0) {
        return;
    }

    giveMoney(-amount);

    Ego::Vector3f pos = getPosition();
    pos.z() += (chr_min_cv._maxs[OCT_Z] + chr_min_cv._mins[OCT_Z]) * 0.5f;

    damage_timer = DAMAGETIME;

    for (size_t cnt = 0; amount > 0 && cnt < vals.size(); cnt++) {
        int count = amount / vals[cnt];
        amount -= count * vals[cnt];

        for (size_t i = 0; i < count; i++) {
            activeParticleHandler().spawnGlobalParticle(pos, ATK_FRONT, LocalParticleProfileRef(pips[cnt]), i);
        }
    }
}

void Object::dropKeys()
{
    if (getPosZ() <= (PitKillDepth / 2)) return;

    const std::vector<ObjectRef> inventoryItemRefs = getInventoryItemRefs();

    for (const ObjectRef& keyRef : inventoryItemRefs) {
        Object* pkey = objectHandler().get(keyRef);
        if (pkey == nullptr || pkey->isTerminated()) {
            continue;
        }

        const IDSZ2& idsz_parent = pkey->getProfile()->getIDSZ(IDSZ_PARENT);
        const IDSZ2& idsz_type = pkey->getProfile()->getIDSZ(IDSZ_TYPE);

        if (!isKeyIDSZ(idsz_parent) && !isKeyIDSZ(idsz_type)) continue;

        Facing direction = Facing::random();
        Facing turn = direction;

        removeInventoryItemRef(keyRef, true);

        pkey->setDismountTimer(PHYS_DISMOUNT_TIME);
        pkey->setDismountObject(getObjRef());
        pkey->onwhichplatform_ref = onwhichplatform_ref;
        pkey->onwhichplatform_update = onwhichplatform_update;

        pkey->hitready = true;
        pkey->isequipped = false;
        pkey->ori.facing_z = idlib::canonicalize(direction + ATK_BEHIND);
        pkey->team = pkey->team_base;

        pkey->setVelocity(pkey->getVelocity() +
                          Ego::Vector3f(std::cos(turn) * DROPXYVEL,
                                        std::sin(turn) * DROPXYVEL,
                                        DROPZVEL));

        pkey->addAIAlertBits(ALERTIF_DROPPED);
        pkey->setPosition(getPosition());
    }
}

void Object::dropAllItems()
{
    Object* leftItem = objectHandler().get(getHeldObject(SLOT_LEFT));
    if (leftItem != nullptr) {
        leftItem->detachFromHolder(true, false);
    }
    Object* rightItem = objectHandler().get(getHeldObject(SLOT_RIGHT));
    if (rightItem != nullptr) {
        rightItem->detachFromHolder(true, false);
    }

    const std::vector<ObjectRef> inventoryItemRefs = getInventoryItemRefs();
    uint8_t pack_count = 0;
    for (const ObjectRef& itemRef : inventoryItemRefs) {
        Object* item = objectHandler().get(itemRef);
        if (item != nullptr && !item->isTerminated()) {
            ++pack_count;
        }
    }
    if (pack_count == 0) {
        return;
    }

    const FACING_T diradd = (std::numeric_limits<FACING_T>::max() / 2) / pack_count;

    Facing direction = ori.facing_z + ATK_BEHIND - Facing(diradd * (pack_count / 2));
    for (const ObjectRef& itemRef : inventoryItemRefs) {
        Object* pitem = objectHandler().get(itemRef);
        if (pitem == nullptr || pitem->isTerminated()) {
            continue;
        }

        removeInventoryItemRef(itemRef, true);

        pitem->setDismountTimer(PHYS_DISMOUNT_TIME);
        pitem->setDismountObject(getObjRef());
        pitem->onwhichplatform_ref = onwhichplatform_ref;
        pitem->onwhichplatform_update = onwhichplatform_update;

        pitem->hitready = true;
        pitem->ori.facing_z = idlib::canonicalize(direction + ATK_BEHIND);
        pitem->team = pitem->team_base;

        pitem->setVelocity(pitem->getVelocity() +
                           Ego::Vector3f(std::cos(direction) * DROPXYVEL,
                                         std::sin(direction) * DROPXYVEL,
                                         DROPZVEL));

        pitem->addAIAlertBits(ALERTIF_DROPPED);
        pitem->setPosition(getPosition());

        direction += Facing(diradd);
    }
}

void Object::setLatchButton(const LatchButton latchButton, const bool pressed)
{
    _inputLatchesPressed[latchButton] = pressed;
}

void Object::updateLatchButtons()
{
    auto ichr = getObjRef();

    if (!isAlive() || _inputLatchesPressed.none()) {
        return;
    }

    const std::shared_ptr<ObjectProfile>& profile = getProfile();

    if (_inputLatchesPressed[LATCHBUTTON_JUMP] && 0 == jump_timer) {
        if (isBeingHeld()) {
            detachFromHolder(true, true);
            detachFromPlatform();

            jump_timer = Object::JUMPDELAY;
            if (isFlying()) {
                setVelocity(getVelocity() + Ego::Vector3f(0.0f, 0.0f, Object::DISMOUNTZVEL / 3.0f));
            } else {
                setVelocity(getVelocity() + Ego::Vector3f(0.0f, 0.0f, Object::DISMOUNTZVEL));
            }

            setPosition(getPosX(), getPosY(), getPosZ() + getVelocity().z());

            if (getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) != Object::JUMPINFINITE && 0 != jumpnumber) {
                jumpnumber--;
            }

            audioSystem().playSound(getPosition(), profile->getJumpSound());
        } else if (0 != jumpnumber && !isFlying()) {
            if (1 != getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) || jumpready) {
                if (!hasPerk(Ego::Perks::STALKER)) {
                    deactivateStealth();
                }

                float jumpPower = getAttribute(Ego::Attribute::JUMP_POWER);
                hitready = true;
                jump_timer = Object::JUMPDELAY;

                if (isSubmerged() || floorIsSlippy()) {
                    jump_timer *= hasPerk(Ego::Perks::ATHLETICS) ? 2 : 4;
                    jumpPower *= 0.5f;
                }

                setVelocity(getVelocity() + Ego::Vector3f(0.0f, 0.0f, jumpPower));
                jumpready = false;

                if (getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) != Object::JUMPINFINITE) {
                    jumpnumber--;
                }

                if (inst.canBeInterrupted()) {
                    inst.playAction(ACTION_JA, true);
                }

                audioSystem().playSound(getPosition(), profile->getJumpSound());
            }
        }
    }
    if (_inputLatchesPressed[LATCHBUTTON_PACKLEFT] && inst.canBeInterrupted() && 0 == reload_timer) {
        reload_timer = Inventory::PACKDELAY;
        Inventory::swap_item(ichr, getFirstFreeInventorySlot(), SLOT_LEFT, false);
    }
    if (_inputLatchesPressed[LATCHBUTTON_PACKRIGHT] && inst.canBeInterrupted() && 0 == reload_timer) {
        reload_timer = Inventory::PACKDELAY;
        Inventory::swap_item(ichr, getFirstFreeInventorySlot(), SLOT_RIGHT, false);
    }

    if (_inputLatchesPressed[LATCHBUTTON_ALTLEFT] && inst.canBeInterrupted() && 0 == reload_timer) {
        reload_timer = GRABDELAY;
        if (objectHandler().get(getHeldObject(SLOT_LEFT)) == nullptr) {
            if (!getProfile()->getModel()->isActionValid(ACTION_ME)) {
                grabStuff(GRIP_LEFT, false);
            } else {
                inst.playAction(ACTION_ME, false);
            }
        } else {
            inst.playAction(ACTION_MA, false);
        }
    }

    if (_inputLatchesPressed[LATCHBUTTON_ALTRIGHT] && inst.canBeInterrupted() && 0 == reload_timer) {
        reload_timer = GRABDELAY;
        if (objectHandler().get(getHeldObject(SLOT_RIGHT)) == nullptr) {
            if (!getProfile()->getModel()->isActionValid(ACTION_MF)) {
                grabStuff(GRIP_RIGHT, false);
            } else {
                inst.playAction(ACTION_MF, false);
            }
        } else {
            inst.playAction(ACTION_MB, false);
        }
    }

    bool attack_handled = false;
    if (!attack_handled && _inputLatchesPressed[LATCHBUTTON_LEFT] && 0 == reload_timer) {
        attack_handled = chr_do_latch_attack(this, SLOT_LEFT);
    }
    if (!attack_handled && _inputLatchesPressed[LATCHBUTTON_RIGHT] && 0 == reload_timer) {
        chr_do_latch_attack(this, SLOT_RIGHT);
    }
}
