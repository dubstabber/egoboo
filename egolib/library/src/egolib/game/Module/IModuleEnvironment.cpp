/// @file egolib/game/Module/IModuleEnvironment.cpp
/// @brief Ownership of the installed active module environment.

#include "egolib/game/Module/IModuleEnvironment.hpp"

#include <stdexcept>

namespace
{
IModuleEnvironment* g_activeModuleEnvironment = nullptr;
}

void installModuleEnvironment(IModuleEnvironment* environment)
{
    g_activeModuleEnvironment = environment;
}

void clearModuleEnvironment()
{
    g_activeModuleEnvironment = nullptr;
}

IModuleEnvironment* tryActiveModuleEnvironment()
{
    return g_activeModuleEnvironment;
}

IModuleEnvironment& activeModuleEnvironment()
{
    if (!g_activeModuleEnvironment)
    {
        throw std::logic_error("no active module environment");
    }
    return *g_activeModuleEnvironment;
}
