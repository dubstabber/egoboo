#pragma once

#include "egolib/game/egoboo.h"

class ILifecycleControl
{
public:
    virtual ~ILifecycleControl() = default;

    virtual bool detachFromHolder(bool ignoreKurse, bool doShop) = 0;
    virtual void dropKeys() = 0;
    virtual void dropAllItems() = 0;
    virtual void respawn() = 0;
    virtual void respawnInPlace() = 0;
    virtual void setItem(bool item) = 0;
    virtual void setCanBeCrushed(bool crushable) = 0;
    virtual void setDamageThreshold(SFP8_T threshold) = 0;
    virtual bool activateStealth() = 0;
    virtual void deactivateStealth() = 0;
};
