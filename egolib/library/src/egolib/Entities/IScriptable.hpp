#pragma once

#include "egolib/game/egoboo.h"

class IScriptable
{
public:
    virtual ~IScriptable() = default;

    virtual ObjectRef getObjRef() const = 0;

    virtual BIT_FIELD getAIAlertBits() const = 0;
    virtual void setAIAlertBits(BIT_FIELD bits) = 0;
    virtual void addAIAlertBits(BIT_FIELD bits) = 0;
    virtual void clearAIAlertBits(BIT_FIELD bits) = 0;
    virtual bool hasAnyAIAlertBits(BIT_FIELD bits) const = 0;

    virtual int getAIStateValue() const = 0;
    virtual void setAIStateValue(int value) = 0;
    virtual int getAIContent() const = 0;
    virtual void setAIContent(int value) = 0;
    virtual int getAIPassage() const = 0;
    virtual void setAIPassage(int value) = 0;

    virtual uint32_t getAITimer() const = 0;
    virtual void setAITimer(uint32_t timer) = 0;
    virtual int32_t getAIPoofTime() const = 0;
    virtual void setAIPoofTime(int32_t time) = 0;

    virtual ObjectRef getAIOwner() const = 0;
    virtual void setAIOwner(ObjectRef objectRef) = 0;
    virtual ObjectRef getAIChild() const = 0;
    virtual void setAIChild(ObjectRef objectRef) = 0;
    virtual ObjectRef getAITarget() const = 0;
    virtual void setAITarget(ObjectRef objectRef) = 0;
    virtual ObjectRef getAILastAttacker() const = 0;
    virtual void setAILastAttacker(ObjectRef objectRef) = 0;
    virtual ObjectRef getAIBumped() const = 0;
    virtual ObjectRef getAILastItemUsed() const = 0;
    virtual void setAILastItemUsed(ObjectRef objectRef) = 0;
    virtual ObjectRef getAILastHit() const = 0;
    virtual void setAILastHit(ObjectRef objectRef) = 0;

    virtual DamageType getAILastDamageType() const = 0;
    virtual void setAILastDamageType(DamageType damageType) = 0;
    virtual Facing getAILastDirection() const = 0;
    virtual void setAILastDirection(Facing direction) = 0;
    virtual float getAIMaxSpeed() const = 0;
    virtual void setAIMaxSpeed(float speed) = 0;

    virtual bool addAIOrder(uint32_t value, uint16_t counter) = 0;
    virtual bool markAIChanged() = 0;
    virtual bool recordAIBump(ObjectRef objectRef) = 0;
    virtual void resetAIState() = 0;
    virtual void spawnAIState(uint16_t rank) = 0;
};
