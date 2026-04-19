#pragma once

#include "egolib/game/egoboo.h"

class IWallet
{
public:
    virtual ~IWallet() = default;

    virtual void giveMoney(int amount) = 0;
    virtual uint16_t getMoney() const = 0;
    virtual void dropMoney(int amount) = 0;
};
