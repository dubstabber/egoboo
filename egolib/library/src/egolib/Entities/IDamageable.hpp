#pragma once

#include "egolib/Logic/Damage.hpp"  // DamageType
#include "egolib/_math.h"           // Facing

#include <memory>

class Object;

class IDamageable
{
public:
    virtual ~IDamageable() = default;

    /**
	 * @brief Get the unique object reference of this object.
     * @return the unique object reference of this object
     */
    virtual ObjectRef getObjRef() const = 0;
    /**
    * @brief
    *   Returns true if this Object has not been killed by anything
    **/
    virtual bool isAlive() const = 0;
    virtual bool isInvincible() const = 0;
    virtual void setInvincible(bool invincible) = 0;

    virtual uint8_t getDamageTimer() const = 0;
    virtual void setDamageTimer(uint8_t timer) = 0;

    virtual DamageType getDamageTargetType() const = 0;
    virtual DamageType getReaffirmDamageType() const = 0;
    /**
    * @brief
    *   Gets how resistant this Object is to a specific type of damage (ZAP, FIRE, POKE, etc.)
    *   For positive Defence, damage reduction =((defence)*0.06)/(1+0.06*(defence))
    *   For negative Defence, it is damage increase = 1-0.94^(defence).
    * @param type
    *   What kind of damage resistance to retrieve
    * @param includeArmor
    *   true if Defence should be included in damage reduction calculation
    * @return
    *   A floating point value representing the damage reduction (0.0f = no reduction, 1.0f = no damage, -1.0f = double damage)
    *   I.e a return value of 0.05f would mean damage reduction of 5%.
    **/
    virtual float getDamageReduction(DamageType type, bool includeArmor = true) const = 0;

    /**
    * @brief
    *   This function calculates and applies damage to a character.  It also
    *   sets alerts and begins actions.  Blocking and frame invincibility are done here too.
    *
    * @param direction
    *   Direction is ATK_FRONT if the attack is coming head on, ATK_RIGHT if from the right,
    *   ATK_BEHIND if from the back, ATK_LEFT if from the left.
    *
    * @param damage
    *   is a random range of damage to deal
    *
    * @param damageType
    *   indicates what kind of damage this is (ZAP, CRUSH, FIRE, etc.) which is again
    *   affected by resistances immunities, etc.
    *
    * @param team
    *   which team is dealing the damage
    *
    * @param attacker
    *   The Object which is dealing the damage to this Object
    *
    * @param effects
    *   is a BIT_FIELD of various flags which affect how we determine damage.
    *
    * @param ignore_invictus
    *   if this is true, then we allow damaging this object even though it is normally immune to damage.
    **/
    virtual int damage(Facing direction, IPair damage, DamageType damageType, TEAM_REF attackerTeam,
                       const std::shared_ptr<Object>& attacker, bool ignoreArmour, bool setDamageTime, bool ignoreInvictus) = 0;
    /**
     * @brief
     *  This function gives some purelife points to the target, ignoring any resistances and so forth.
     * @param healer
     *  the healer
     * @param amount
     *  the amount to heal the character
     */
    virtual bool heal(const std::shared_ptr<Object>& healer, UFP8_T amount, bool ignoreInvincibility) = 0;
    /**
    * @author BB
    * @details Handle a character death. Set various states, disconnect it from the world, etc.
    **/
    virtual void kill(const std::shared_ptr<Object>& originalKiller, bool ignoreInvincibility) = 0;
};
