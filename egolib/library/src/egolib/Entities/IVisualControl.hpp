#pragma once

#include "egolib/game/egoboo.h"

class IVisualControl
{
public:
    virtual ~IVisualControl() = default;

    virtual void setRedShift(int value) = 0;
    virtual void setGreenShift(int value) = 0;
    virtual void setBlueShift(int value) = 0;
    virtual void setLight(int value) = 0;
    virtual void setAlpha(int value) = 0;
    virtual void setNameKnown(bool known) = 0;
    virtual void setAmmoKnown(bool known) = 0;
    virtual void setSparkle(uint8_t value) = 0;
};
