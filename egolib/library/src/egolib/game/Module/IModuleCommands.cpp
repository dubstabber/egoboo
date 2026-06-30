/// @file egolib/game/Module/IModuleCommands.cpp
/// @brief Ownership of the installed active module command surface.

#include "egolib/game/Module/IModuleCommands.hpp"

#include <stdexcept>

namespace
{
IModuleCommands* g_activeModuleCommands = nullptr;
}

void installModuleCommands(IModuleCommands* commands)
{
    g_activeModuleCommands = commands;
}

void clearModuleCommands()
{
    g_activeModuleCommands = nullptr;
}

IModuleCommands* tryActiveModuleCommands()
{
    return g_activeModuleCommands;
}

IModuleCommands& activeModuleCommands()
{
    if (!g_activeModuleCommands)
    {
        throw std::logic_error("no active module commands");
    }
    return *g_activeModuleCommands;
}
