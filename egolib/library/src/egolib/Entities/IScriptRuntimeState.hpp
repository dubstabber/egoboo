#pragma once

struct ai_state_t;

/// @brief Narrow role exposing the object-owned EgoScript runtime state.
///
/// The VM consumes this role instead of depending on the concrete Object type. The state remains
/// owned by the implementing entity and is valid only for that entity's lifetime.
class IScriptRuntimeState
{
public:
    virtual ~IScriptRuntimeState() = default;

    virtual ai_state_t& scriptRuntimeState() noexcept = 0;
    virtual const ai_state_t& scriptRuntimeState() const noexcept = 0;
};
