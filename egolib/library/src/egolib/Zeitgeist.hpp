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

/// @file egolib/Zeitgeist.hpp
/// @brief Real-world time hooks ("is it Halloween / Christmas / nighttime?"). Relocated down
///        from game.h so lower-layer code (e.g. the audio system's seasonal-theme selection)
///        can query special times without dragging game.h's heavy conduit. This header and
///        its implementation depend only on lower-layer Ego::Time, so they are game-free.

#pragma once

/**
 * Zeitgeist connects the game and the real world including. Functionality like audio
 * and video communication and social networking support might be integrated here.
 */
namespace Zeitgeist {

// An enumeration of special times.
enum class Time {
    Halloween,       // Halloween.
    Christmas,       // Christmas.
    Nighttime,       // Nighttime.
    Daytime,         // Daytime.
};

/// @brief Get whether the current real-world local time matches @a time.
/// @param time the time
/// @return @a true if the current local time falls within @a time
bool CheckTime(Time time);

}
