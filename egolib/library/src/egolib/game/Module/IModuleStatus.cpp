/// @file egolib/game/Module/IModuleStatus.cpp
/// @brief Ownership of the installed active module-status surface.

#include "egolib/game/Module/IModuleStatus.hpp"

#include <stdexcept>

namespace
{
IModuleStatus* g_activeModuleStatus = nullptr;
}

void installModuleStatus(IModuleStatus* status)
{
    g_activeModuleStatus = status;
}

void clearModuleStatus()
{
    g_activeModuleStatus = nullptr;
}

IModuleStatus* tryActiveModuleStatus()
{
    return g_activeModuleStatus;
}

IModuleStatus& activeModuleStatus()
{
    if (!g_activeModuleStatus)
    {
        throw std::logic_error("no active module status");
    }
    return *g_activeModuleStatus;
}
