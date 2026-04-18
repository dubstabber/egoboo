#pragma once

#include "egolib/game/egoboo.h"

#include <memory>

class Object;

class IDamageable
{
public:
    virtual ~IDamageable() = default;

    virtual ObjectRef getObjRef() const = 0;
    virtual bool isAlive() const = 0;
    virtual bool isInvincible() const = 0;

    virtual uint8_t getDamageTimer() const = 0;
    virtual void setDamageTimer(uint8_t timer) = 0;

    virtual DamageType getDamageTargetType() const = 0;
    virtual DamageType getReaffirmDamageType() const = 0;
    virtual float getDamageReduction(DamageType type, bool includeArmor = true) const = 0;

    virtual int damage(Facing direction, IPair damage, DamageType damageType, TEAM_REF attackerTeam,
                       const std::shared_ptr<Object>& attacker, bool ignoreArmour, bool setDamageTime, bool ignoreInvictus) = 0;
    virtual bool heal(const std::shared_ptr<Object>& healer, UFP8_T amount, bool ignoreInvincibility) = 0;
    virtual void kill(const std::shared_ptr<Object>& originalKiller, bool ignoreInvincibility) = 0;
};
