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

/// @brief Install the active input system.
/// @throw std::logic_error if an input system is already installed.
void installActiveInputSystem(IInputSystem& inputSystem);

/// @brief Clear the installed active input system.
void clearActiveInputSystem();

/// @brief The installed active input system, or @a nullptr if none is installed.
IInputSystem* tryActiveInputSystem();

/// @brief The active input system.
/// @throw std::logic_error if no input system is installed.
IInputSystem& activeInputSystem();

} // namespace Input
} // namespace Ego
