#pragma once

#include "egolib/game/egoboo.h"

class ITargetInfo
{
public:
    virtual ~ITargetInfo() = default;

    virtual ObjectRef getObjRef() const = 0;

    virtual ObjectRef getHolderRef() const = 0;
    virtual slot_t getAttachmentSlot() const = 0;
    virtual PLA_REF getPlayerNumber() const = 0;

    virtual bool isAlive() const = 0;
    virtual bool isBeingHeld() const = 0;
    virtual bool isPlayer() const = 0;
    virtual Gender getGender() const = 0;
    virtual ModelAction getCurrentAnimation() const = 0;

    virtual bool isMount() const = 0;
    virtual bool isPlatform() const = 0;
    virtual bool isFlying() const = 0;
    virtual bool isHurt() const = 0;
    virtual bool hasNotFullMana() const = 0;
    virtual bool isAttacking() const = 0;
    virtual bool isNameKnown() const = 0;
    virtual bool isKursed() const = 0;
    virtual bool isEquipped() const = 0;
    virtual bool isOnWaterTile() const = 0;
    virtual bool isStealthed() const = 0;

    virtual bool canSeeInvisible() const = 0;
    virtual bool canSeeKurses() const = 0;
    virtual bool canOpenStuff() const = 0;
    virtual bool isWeapon() const = 0;

    virtual bool hasSkillIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool hasTypeIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool hasAnyIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool matchesSpecialIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool matchesVulnerabilityIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool wieldsItemIDSZ(const IDSZ2& idsz) const = 0;

    virtual bool isOnSameTeam(TEAM_REF teamRef) const = 0;
    virtual bool isHatedByTeam(TEAM_REF teamRef) const = 0;
    virtual TEAM_REF getTeamRef() const = 0;
    virtual TEAM_REF getBaseTeamRef() const = 0;
    virtual IDSZ2 getTypeIDSZ() const = 0;
    virtual IDSZ2 getHateIDSZ() const = 0;

    virtual uint16_t getAmmo() const = 0;
    virtual SKIN_T getSkin() const = 0;
    virtual bool canBeGrogged() const = 0;
    virtual bool canBeDazed() const = 0;
    virtual int16_t getGrogTimer() const = 0;
    virtual int16_t getDazeTimer() const = 0;
};
