#pragma once

#include <cstdint>  // uint8_t, int16_t

class IVisualControl
{
public:
    virtual ~IVisualControl() = default;

    virtual void setRedShift(int value) = 0;
    virtual void setGreenShift(int value) = 0;
    virtual void setBlueShift(int value) = 0;
    /**
    * @brief Sets the transparency for this Object
    * @param light Transparency level between 0 (fully transparent) and 255 (fully opaque)
    */
    virtual void setLight(int value) = 0;
    /**
    * @brief Sets the transparency for this Object
    * @param alpha Transparency level between 0 (fully transparent) and 255 (fully opaque)
    **/
    virtual void setAlpha(int value) = 0;
    virtual void setNameKnown(bool known) = 0;
    virtual void setAmmoKnown(bool known) = 0;
    virtual void setSparkle(uint8_t value) = 0;
    virtual void flash(uint8_t value) = 0;
    virtual void flashVariableHeight(uint8_t valueLow, int16_t low, uint8_t valueHigh, int16_t high) = 0;
};
