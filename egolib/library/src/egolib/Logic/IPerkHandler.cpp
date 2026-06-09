//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/Logic/IPerkHandler.cpp
/// @brief Ownership of the installed active perk handler.

#include "egolib/Logic/IPerkHandler.hpp"

#include <stdexcept>

namespace Ego
{
namespace Perks
{

namespace
{
IPerkHandler* g_activePerkHandler = nullptr;
}

void installActivePerkHandler(IPerkHandler& perkHandler)
{
    if (g_activePerkHandler)
    {
        throw std::logic_error("perk handler already installed");
    }
    g_activePerkHandler = &perkHandler;
}

void clearActivePerkHandler()
{
    g_activePerkHandler = nullptr;
}

IPerkHandler* tryActivePerkHandler()
{
    return g_activePerkHandler;
}

IPerkHandler& activePerkHandler()
{
    if (!g_activePerkHandler)
    {
        throw std::logic_error("no active perk handler");
    }
    return *g_activePerkHandler;
}

} // namespace Perks
} // namespace Ego
