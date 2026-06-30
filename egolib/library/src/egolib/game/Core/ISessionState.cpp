/// @file egolib/game/Core/ISessionState.cpp
/// @brief Ownership of the installed active session-state surface.

#include "egolib/game/Core/ISessionState.hpp"

#include <stdexcept>

namespace
{
ISessionState* g_activeSessionState = nullptr;
ISessionStatePublisher* g_activeSessionStatePublisher = nullptr;
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

void installSessionStatePublisher(ISessionStatePublisher* publisher)
{
    g_activeSessionStatePublisher = publisher;
}

void clearSessionStatePublisher()
{
    g_activeSessionStatePublisher = nullptr;
}

ISessionStatePublisher* tryActiveSessionStatePublisher()
{
    return g_activeSessionStatePublisher;
}

ISessionStatePublisher& activeSessionStatePublisher()
{
    if (!g_activeSessionStatePublisher)
    {
        throw std::logic_error("no active session-state publisher");
    }
    return *g_activeSessionStatePublisher;
}
