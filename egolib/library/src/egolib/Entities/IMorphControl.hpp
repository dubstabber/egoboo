#pragma once

#include "egolib/game/egoboo.h"

class IMorphControl
{
public:
    virtual ~IMorphControl() = default;

    virtual ObjectProfileRef getBaseModelRef() const = 0;
    virtual SKIN_T getSkin() const = 0;
    virtual float getFat() const = 0;
    virtual float getTargetFat() const = 0;
    virtual void setTargetFat(float fat) = 0;
    virtual int16_t getResizeTimeRemaining() const = 0;
    virtual void setResizeTimeRemaining(int16_t remaining) = 0;
    virtual void polymorphObject(ObjectProfileRef profileID, SKIN_T skin) = 0;
};
