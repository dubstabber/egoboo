/// @file egolib/game/Core/ISessionState.cpp
/// @brief Ownership of the installed active session-state surface.

#include "egolib/game/Core/ISessionState.hpp"

#include <stdexcept>

namespace
{
ISessionState* g_activeSessionState = nullptr;
}

void installSessionState(ISessionState* state)
{
    g_activeSessionState = state;
}

void clearSessionState()
{
    g_activeSessionState = nullptr;
}

ISessionState* tryActiveSessionState()
{
    return g_activeSessionState;
}

ISessionState& activeSessionState()
{
    if (!g_activeSessionState)
    {
        throw std::logic_error("no active session state");
    }
    return *g_activeSessionState;
}
