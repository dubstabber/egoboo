#pragma once

#include "egolib/_math.h"                       // float_t
#include "egolib/Logic/Gender.hpp"              // Gender
#include "egolib/IDSZ.hpp"                       // IDSZ2
#include "egolib/Graphics/ModelDescriptor.hpp"  // ModelAction
#include "egolib/Logic/ObjectSlot.hpp"          // slot_t

#include <string>

class ITargetInfo
{
public:
    virtual ~ITargetInfo() = default;

    virtual ObjectRef getObjRef() const = 0;
    virtual std::string getDisplayName() const = 0;

    virtual ObjectRef getHolderRef() const = 0;
    virtual slot_t getAttachmentSlot() const = 0;
    virtual PLA_REF getPlayerNumber() const = 0;

    /**
    * @brief
    *   This function returns true if this Object is being held by another Object
    * @return
    *   true if held by another existing Object that is not marked for removal
    **/
    virtual bool isBeingHeld() const = 0;
    virtual bool isPlayer() const = 0;
    virtual Gender getGender() const = 0;
    virtual ModelAction getCurrentAnimation() const = 0;

    /**
     * @return true if this Object is mountable by other Objects
     */
    virtual bool isMount() const = 0;
    virtual bool isPlatform() const = 0;
    /**
    * @return
    *   true if this Object is currently levitating above the ground
    **/
    virtual bool isFlying() const = 0;
    virtual bool isHurt() const = 0;
    virtual bool hasNotFullMana() const = 0;
    /**
    * @return true if this Object is currently doing an attack animation
    **/
    virtual bool isAttacking() const = 0;
    virtual bool isNameKnown() const = 0;
    virtual bool isKursed() const = 0;
    virtual bool isEquipped() const = 0;
    /**
    * @brief
    *   This function returns true if the character is on a water tile
    * @return
    *   true if it is on a water tile
    **/
    virtual bool isOnWaterTile() const = 0;
    /**
    * @return
    *   true if this Object is actively trying to hide from others
    **/
    virtual bool isStealthed() const = 0;

    /**
    * @return
    *   true if this Object can detect and see invisible objects
    **/
    virtual bool canSeeInvisible() const = 0;
    virtual bool canSeeKurses() const = 0;
    virtual bool canOpenStuff() const = 0;
    virtual bool isWeapon() const = 0;

    /**
    * @brief
    *   checks if the object has a matching skill IDSZ. This function also maps between the old skill IDSZ
    *   system and the new Perk system.
    * @param whichskill
    *   The IDSZ of the skill to check. An IDSZ of [NONE] always matches true.
    * @return
    *   true if the Object has the matching skill IDSZ of a perk that matches the skill IDSZ
    **/
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
