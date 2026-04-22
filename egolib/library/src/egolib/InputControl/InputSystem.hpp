#pragma once

#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Math/_Include.hpp"
#include "egolib/InputControl/ModifierKeys.hpp"

namespace Ego {
namespace Input {

class InputSystem : public IInputSystem, public idlib::singleton<InputSystem>
{
public:
    static constexpr float MOUSE_SENSITIVITY = 12.0f; //TODO: make configurable in settings.txt
    using MouseButton = IInputSystem::MouseButton;

    /// @brief Construct this input system.
    InputSystem();

    /// @brief Destruct this input system.
    virtual ~InputSystem();

    void update() override;
    const Vector2f& getMouseMovement() const override;

    /// @brief Get if the mouse button is down.
    /// @param button the mouse button
    /// @return @a true if the mouse button is down, @a false otherwise
    bool isMouseButtonDown(MouseButton button) const override;

    /// @brief Get if a keyboar key is down.
    /// @param key the keyboard key
    /// @return @a true if the keyboard key is down, @a false otherwise
    bool isKeyDown(SDL_Keycode key) const override;

    /// @brief Get the modifier keys state.
    /// @return the modifier key state
    ModifierKeys getModifierKeys() const override;

private:

    /// @brief The mouse movement.
    /// @remark The mouse movement is the displacement of the mouse since the update before the last update and the last update.
    Vector2f mouseMovement;

    /// @brief The state of the mouse buttons at the last update.
    std::array<bool, IInputSystem::NR_OF_MOUSE_BUTTONS> mouseButtonDown;

    /// @brief The state of the modifiers keyboard keys at the last update.
    ModifierKeys modifierKeys;
};

} // namespace Input
} // namespace Ego
