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

/// @file egolib/Zeitgeist.cpp
/// @brief Implementation of the real-world special-time checks (relocated from game.c).

#include "egolib/Zeitgeist.hpp"

#include "egolib/Time/LocalTime.hpp"           // Ego::Time::LocalTime
#include "idlib/exception.hpp"                 // idlib::unhandled_switch_case_error

namespace Zeitgeist {

bool CheckTime(Time time) {
    Ego::Time::LocalTime localTime;
    switch (time)
    {
    // Halloween is from 31th october 31th (incl.) until the november 1st (incl.).
    case Time::Halloween:
        return ((10 == localTime.getMonth() + 1 && localTime.getDayOfMonth() >= 31) ||
                (11 == localTime.getMonth() + 1 && localTime.getDayOfMonth() <= 1));

    // Chrsitmas is from december 16th (incl.) until january 1st/newyear (excl.).
    case Time::Christmas:
        return (12 == localTime.getMonth() + 1 && localTime.getDayOfMonth() >= 16);

    // From 0:00 to 6:00 (spooky time!).
    case Time::Nighttime:
        return localTime.getHours() <= 6;

     // Its day whenever it's not night.
    case Time::Daytime:
        return localTime.getHours() > 6;

    // Unhandled check.
    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__);
    }
}

}
