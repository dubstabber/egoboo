#pragma once

#include "egolib/game/egoboo.h"

class IItemInfo
{
public:
    virtual ~IItemInfo() = default;

    virtual bool hasTypeIDSZ(const IDSZ2& idsz) const = 0;
    virtual bool isRangedWeapon() const = 0;
    virtual bool isMeleeWeapon() const = 0;
    virtual bool isShield() const = 0;
};
