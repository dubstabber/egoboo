#pragma once

#include "egolib/typedef.h"  // SKIN_T

#include <cstddef>

class IAppearanceProfile
{
public:
    virtual ~IAppearanceProfile() = default;

    /**
    * @brief
    *   Changes the skin of this Object to the specified skin number.
    *   This changes this Objects damage resistances and movement speed accordingly to the new
    *   armor of the skin.
    * @return
    *   true if the skin could be changed into the specified number or false if it fails
    **/
    virtual SKIN_T getSkin() const = 0;
    virtual bool setSkin(size_t skin) = 0;
    virtual uint16_t getSkinCost(size_t skin) const = 0;
    virtual bool isCurrentSkinDressy() const = 0;
    virtual bool hasIntellectDamageParticle() const = 0;
};
