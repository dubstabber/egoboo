#pragma once

#include "egolib/game/egoboo.h"

class IAnimationControl
{
public:
    virtual ~IAnimationControl() = default;

    virtual ModelAction resolveModelAction(int actionIndex) const = 0;
    virtual bool startAnimation(ModelAction action, bool actionReady, bool overrideAction) = 0;
    virtual void setActionKeep(bool value) = 0;
    virtual void removeInterpolation() = 0;
};
