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

/// @file egolib/game/Logic/LevelUp.hpp
/// @brief GUI-free character level-up computation, extracted from
///        Ego::GUI::LevelUpWindow::doLevelUp.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "egolib/Logic/Attribute.hpp"

// Forward declarations.
class Object;

namespace Ego
{

class Player;

namespace Perks
{
class Perk;
} // namespace Perks

/// @brief Result of applying a level-up: what changed and what each attribute read as
///        immediately before its own increase was applied (the value the GUI displays).
struct LevelUpReport
{
    std::array<float, Attribute::NR_OF_PRIMARY_ATTRIBUTES> increase;        ///< Signed deltas, incl. perk +1 and flat bonuses.
    std::array<float, Attribute::NR_OF_PRIMARY_ATTRIBUTES> displayedValue;  ///< getAttribute(i) sampled just before increase[i] is applied.
};

/// @brief Applies a level-up to @a character for the chosen @a selectedPerk.
///
/// Seeds the global Random from the character's stored level-up seed, draws the
/// attribute gains, grants the perk (+flat bonuses), bumps the level, raises
/// ALERTIF_LEVELUP, clears the player's level-up indicator, re-randomizes the
/// seed, applies size growth, then applies the base-attribute increases.
///
/// @param character the character leveling up
/// @param selectedPerk the perk chosen for this level up
/// @param playerList the active session's player list, indexed by @a character's
///        player number (see Object::getPlayerNumber()); the caller is responsible
///        for providing the correct list (typically activeSessionState().playerList())
LevelUpReport applyCharacterLevelUp(Object& character,
                                     const Perks::Perk& selectedPerk,
                                     const std::vector<std::shared_ptr<Player>>& playerList);

} // namespace Ego
