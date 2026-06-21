#pragma once

#include "egolib/typedef.h"  // ObjectProfileRef, SKIN_T

class IMorphControl
{
public:
    virtual ~IMorphControl() = default;

    virtual ObjectProfileRef getBaseModelRef() const = 0;
    virtual void setBaseModelRef(ObjectProfileRef profileRef) = 0;
    virtual SKIN_T getSkin() const = 0;
    virtual float getFat() const = 0;
    virtual float getTargetFat() const = 0;
    virtual void setTargetFat(float fat) = 0;
    virtual int16_t getResizeTimeRemaining() const = 0;
    virtual void setResizeTimeRemaining(int16_t remaining) = 0;
    /**
    * @brief
    *   Changes this Object into a different type. This effect is reversible (base profile is not changed)
    **/
    virtual void polymorphObject(ObjectProfileRef profileID, SKIN_T skin) = 0;
};
