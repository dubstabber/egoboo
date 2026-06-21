#pragma once

#include "egolib/Logic/Attribute.hpp"     // Ego::Attribute
#include "egolib/Logic/Perk.hpp"          // Ego::Perks
#include "egolib/IDSZ.hpp"                 // IDSZ2
#include "egolib/Profiles/_Include.hpp"    // XPType

class ICharacterState
{
public:
    virtual ~ICharacterState() = default;

    /**
    * @return
    *   current life remaining in float format
    **/
    virtual float getLife() const = 0;
    /**
    * @return
    *   Get current mana
    **/
    virtual float getMana() const = 0;
    /**
    * @brief
    *   Get total value for the specified attribute. Includes bonuses from Enchants, Perks
    *   and other active boni or penalties.
    **/
    virtual float getAttribute(Ego::Attribute::AttributeType type) const = 0;
    virtual uint32_t getExperience() const = 0;
    virtual uint8_t getExperienceLevelIndex() const = 0;
    virtual uint16_t getReloadTimer() const = 0;

    virtual uint16_t getAmmoMax() const = 0;
    virtual uint16_t getAmmo() const = 0;
    virtual void setAmmo(uint16_t ammoCount) = 0;

    /**
    * @brief
    *   This function takes mana from a character ( or gives mana ), and returns true if the character had enough to pay, or false
    *   otherwise. This can kill a character in hard mode.
    * @param amount
    *   How much mana to take (positive value) of give (negative value)
    * @param killer
    *   If characters have channeling they can use life instead of mana. This can actually kill them (ghosts that drain mana for example)
    * @return
    *   true if all the requested mana was successfully consumed by the Object
    **/
    virtual bool costMana(int amount, ObjectRef killer) = 0;
    /**
    * @brief
    *   Awards some experience points to this object, potentionally allowing it to reach another
    *   character level. This function handles additional experience gain modifiers such as
    *   XP bonus, roleplay or game difficulity.
    * @param xptype
    *   What kind of experience to give. Different classes gain experience differently depending
    *   on the kind of xp.
    * @param overrideInvincibility
    *   Invincible objects usually gain no experience (scenery objects such as a rock for example).
    *   Set this parameter to true to override this and give the experience anyways.
    **/
    virtual void giveExperience(int amount, XPType xptype, bool overrideInvincibility) = 0;
    /**
    * @brief
    *   Permanently increases or decreases an attribute of this Object
    **/
    virtual void increaseBaseAttribute(Ego::Attribute::AttributeType type, float value) = 0;

    virtual int16_t getGrogTimer() const = 0;
    virtual void setGrogTimer(int16_t timer) = 0;
    virtual int16_t getDazeTimer() const = 0;
    virtual void setDazeTimer(int16_t timer) = 0;

    virtual bool isKursed() const = 0;
    virtual void setKursed(bool kursed) = 0;

    virtual void removeEnchantsWithIDSZ(const IDSZ2& idsz) = 0;
    /**
    * @return
    *   true if this Object has mastered the specified perk. Returns always true for NR_OF_PERKS
    **/
    virtual bool hasPerk(Ego::Perks::PerkID perk) const = 0;
    /**
    * @brief
    *   permanently adds a new Perk to this character object
    **/
    virtual void addPerk(Ego::Perks::PerkID perk) = 0;
};
