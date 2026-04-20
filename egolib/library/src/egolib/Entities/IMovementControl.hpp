#pragma once

#include "egolib/game/egoboo.h"

enum turn_mode_t : uint8_t;
enum LatchButton : uint8_t;

class IMovementControl
{
public:
    virtual ~IMovementControl() = default;

    virtual float getFat() const = 0;

    virtual const Ego::Vector3f& getVelocity() const = 0;
    virtual void setVelocity(const Ego::Vector3f& velocity) = 0;
    virtual void setJumpTimer(uint8_t timer) = 0;
    virtual void movePosition(float x, float y, float z) = 0;
    virtual void setTurnMode(turn_mode_t mode) = 0;
    virtual void setBumpHeight(float height) = 0;
    virtual void setBumpWidth(float width) = 0;
    virtual void setLatchButton(LatchButton latchButton, bool pressed) = 0;
    virtual bool teleport(const Ego::Vector3f& position, Facing facing_z) = 0;
    virtual void setReloadTimer(uint16_t timer) = 0;
    virtual void setShadowSize(uint32_t shadowSize) = 0;
    virtual void setSavedShadowSize(uint32_t shadowSize) = 0;
    virtual void setFlyHeight(float height) = 0;
};
