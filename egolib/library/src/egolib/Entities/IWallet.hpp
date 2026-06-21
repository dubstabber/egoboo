#pragma once

#include <cstdint>  // uint16_t

class IWallet
{
public:
    virtual ~IWallet() = default;

    /**
    * @brief
    *   Modifies the amount of money this character has. This method
    *   ensures the resulting amount is not negative and not above the
    *   maximum amount.
    * @param amount
    *   The amount to add or subtract (if negative)
    **/
    virtual void giveMoney(int amount) = 0;
    /**
    * @return
    *   The amount of money (zennies) this Object currently has
    **/
    virtual uint16_t getMoney() const = 0;
    /**
    * @brief
    *   This function will make the Object drop the specified amount of money.
    *   Dropping money will spawn money particles around the Object
    * @param amount
    *   The amount of money to be dropped. If this is more than the max money,
    *   then all available money will be dropped
    **/
    virtual void dropMoney(int amount) = 0;
};
