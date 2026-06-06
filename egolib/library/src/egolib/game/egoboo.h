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

/// @file egolib/game/egoboo.h
///
/// @details Disgusting, hairy, way too monolithic header file for the whole darn
///          project.  In severe need of cleaning up.  Venture here with extreme
///          caution, and bring one of those canaries with you to make sure you
///          don't run out of oxygen.

#pragma once

// Precise includes for egoboo.h's own (thin) body, so it no longer depends on
// the egolib.h uber-header for its own declarations:
#include "egolib/typedef.h"       // egolib_rv (for the gfx_rv alias)
#include "egolib/egoboo_setup.h"  // egoboo_config_t (for config_synch)
#include <cstdint>                // uint32_t (for timervalue)

// --- uber-header teardown in progress ---
// egolib.h is included by default so the tree stays green during the migration,
// but defining EGOBOO_NO_UBER_INCLUDE suppresses it to verify that a consumer
// header is self-contained (carries its own precise includes). Once every
// game/Entities header is self-contained, this include and guard are removed.
#ifndef EGOBOO_NO_UBER_INCLUDE
#include "egolib/egolib.h"
#endif

/**
* @todo
*	Remove this.
*/
typedef egolib_rv gfx_rv;
#define gfx_error rv_error
#define gfx_fail rv_fail
#define gfx_success rv_success

//--------------------------------------------------------------------------------------------

#define NOSPARKLE 255 ///< Dont sparkle icons
#define SPELLBOOK 127 ///< The spellbook model
#define SEEINVISIBLE 128 ///< Cutoff for invisible characters

/// Messaging stuff
#define DAMAGERAISE 25 ///< Tolerance for damage tiles

#define ONESECOND 50 ///< How many game loop updates represent 1 second (50 UPS = 1 second)

#define WRAP_TOLERANCE 90    ///< Status bar

#define INVISIBLE 20 ///< The character can't be detected

//--------------------------------------------------------------------------------------------
// Timers
// HUD
extern bool timeron;  ///< Game timer displayed?
extern uint32_t timervalue; ///< Timer time ( 50ths of a second )

//--------------------------------------------------------------------------------------------

/**
 * @brief
 * @param sync_from_file
 *  see remarks
 * @remark
 *  If @a fromfile is @a true, the values from <tt>"setup.txt"</tt> are downloaded
 *  into the egoboo_config_t data structure, otherwise not. Next, the data from the program
 *  variables are downloaded into the egoboo_config_t data structure and the program variables
 *  are uploaded into the egoboo_config_data_t data structure. Finally, if @a tofile
 *  is @a true, the values from the egoboo_config_data_t are uploaded into <tt>"setup.txt"</tt>.
 */
bool config_synch(egoboo_config_t& cfg, bool fromfile, bool tofile);
