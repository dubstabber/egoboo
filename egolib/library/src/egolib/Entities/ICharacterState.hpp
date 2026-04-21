#pragma once

#include "egolib/game/egoboo.h"

class ICharacterState
{
public:
    virtual ~ICharacterState() = default;

    virtual float getLife() const = 0;
    virtual float getMana() const = 0;
    virtual float getAttribute(Ego::Attribute::AttributeType type) const = 0;
    virtual uint32_t getExperience() const = 0;
    virtual uint8_t getExperienceLevelIndex() const = 0;
    virtual uint16_t getReloadTimer() const = 0;

    virtual uint16_t getAmmoMax() const = 0;
    virtual uint16_t getAmmo() const = 0;
    virtual void setAmmo(uint16_t ammoCount) = 0;

    virtual bool costMana(int amount, ObjectRef killer) = 0;
    virtual void giveExperience(int amount, XPType xptype, bool overrideInvincibility) = 0;
    virtual void increaseBaseAttribute(Ego::Attribute::AttributeType type, float value) = 0;

    virtual int16_t getGrogTimer() const = 0;
    virtual void setGrogTimer(int16_t timer) = 0;
    virtual int16_t getDazeTimer() const = 0;
    virtual void setDazeTimer(int16_t timer) = 0;

    virtual bool isKursed() const = 0;
    virtual void setKursed(bool kursed) = 0;

    virtual void removeEnchantsWithIDSZ(const IDSZ2& idsz) = 0;
    virtual bool hasPerk(Ego::Perks::PerkID perk) const = 0;
    virtual void addPerk(Ego::Perks::PerkID perk) = 0;
};
