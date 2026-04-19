#pragma once

#include "egolib/game/egoboo.h"

#include <cstddef>

class IAppearanceProfile
{
public:
    virtual ~IAppearanceProfile() = default;

    virtual SKIN_T getSkin() const = 0;
    virtual bool setSkin(size_t skin) = 0;
    virtual uint16_t getSkinCost(size_t skin) const = 0;
    virtual bool isCurrentSkinDressy() const = 0;
    virtual bool hasIntellectDamageParticle() const = 0;
};
