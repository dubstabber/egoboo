#pragma once

#include "idlib/idlib.hpp"
#include "egolib/InputControl/ModifierKeys.hpp"
#include "egolib/integrations/math.hpp"

namespace Ego {
namespace Input {

class IInputSystem
{
public:
    enum MouseButton : uint8_t
    {
        LEFT,
        MIDDLE,
        RIGHT,
        X1,
        X2,
        NR_OF_MOUSE_BUTTONS
    };

    virtual ~IInputSystem() = default;

    virtual void update() = 0;
    virtual const Vector2f& getMouseMovement() const = 0;
    virtual bool isMouseButtonDown(MouseButton button) const = 0;
    virtual bool isKeyDown(SDL_Keycode key) const = 0;
    virtual ModifierKeys getModifierKeys() const = 0;
};

} // namespace Input
} // namespace Ego
