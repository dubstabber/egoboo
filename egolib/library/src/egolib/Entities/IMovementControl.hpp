#pragma once

#include "egolib/_math.h"  // Facing, Vector2f, Vector3f
#include <cstdint>         // uint8_t (enum forward declarations below)

enum turn_mode_t : uint8_t;
enum LatchButton : uint8_t;

class IMovementControl
{
public:
    virtual ~IMovementControl() = default;

    virtual float getFat() const = 0;

    virtual const Ego::Vector3f& getVelocity() const = 0;
    virtual void setVelocity(const Ego::Vector3f& velocity) = 0;
    virtual const Ego::Vector2f& getDesiredVelocity() const = 0;
    virtual void setDesiredVelocity(const Ego::Vector2f& velocity) = 0;
    virtual void setJumpTimer(uint8_t timer) = 0;
    /**
    * @brief Translate the current X, Y, Z position of this object by the specified values
    **/
    virtual void movePosition(float x, float y, float z) = 0;
    virtual void setTurnMode(turn_mode_t mode) = 0;
    /**
    * @brief Set the (base) height of a character.
    * @param chr the character
    * @param height the new height
    * @remark The (base) height influences the character size.
    **/
    virtual void setBumpHeight(float height) = 0;
    /**
    * @brief Set the (base) width of a character.
    * @param chr the character
    * @param width the new width
    * @remark Also modifies the shadow size.
    **/
    virtual void setBumpWidth(float width) = 0;
    /**
    * @brief Set or unset a latch button. This triggers in game character commands such as attacking, grabbing items or jumping
    * @param latchButton
    *   Which button to set
    * @param pressed
    *   true if this button should be active or false if not
    * @see enum LatchButton
    **/
    virtual void setLatchButton(LatchButton latchButton, bool pressed) = 0;
    /**
    * @brief
    *   Tries to teleport this Object to the specified location if it is valid
    * @result
    *   Success returns true, failure returns false;
    **/
    virtual bool teleport(const Ego::Vector3f& position, Facing facing_z) = 0;
    virtual void setReloadTimer(uint16_t timer) = 0;
    virtual void setShadowSize(uint32_t shadowSize) = 0;
    virtual void setSavedShadowSize(uint32_t shadowSize) = 0;
    virtual void setFlyHeight(float height) = 0;
};
