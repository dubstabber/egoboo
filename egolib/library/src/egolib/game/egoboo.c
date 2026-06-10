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

/// @file egolib/game/egoboo.c
/// @brief Code for the main program process
/// @details

#include "egolib/game/egoboo.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/LegacyLocalStats.hpp"
#include "egolib/game/game.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/graphic.h"

//--------------------------------------------------------------------------------------------

// Legacy runtime aggregate; local-player status mirrors are synchronized via GameSessionContext.
static local_stats_t local_stats_storage;
bool timeron = false;  ///< Game timer displayed?
uint32_t timervalue = 0; ///< Timer time ( 50ths of a second )

local_stats_t* legacy_local_stats()
{
    return &local_stats_storage;
}

const local_stats_t* legacy_local_stats_const()
{
    return &local_stats_storage;
}

//--------------------------------------------------------------------------------------------

/**
 * @brief
 *  Download the data from the egoboo_config_t data structure to program variables.
 * @param cfg
 *  the configuration
 * @return
 *  @a true on success, @a false on failure
 */
bool config_download(egoboo_config_t& cfg);

/**
 * @brief
 *  Upload the data from program variables into an egoboo_config_t data structure.
 * @param cfg
 *  the configuration
 * @return
 *  @a true on success, @a false on failure
 * @note
 *  This function must be implemented by the user
 */
bool config_upload(egoboo_config_t& cfg);


bool config_synch(egoboo_config_t& cfg, bool fromFile,bool toFile)
{
    if (fromFile)
    {
        if (!Ego::Setup::download(cfg))   // Download 'setup.txt' into the Egoboo configuration.
        {
            return false;
        }
    }
    if (!config_download(cfg))      // Download the the program variables into the Egoboo configuration.
    {
        return false;
    }
    if (!config_upload(cfg))        // Upload the Egoboo configuration into the program variables.
    {
        return false;
    }
    if (toFile)
    {
        return Ego::Setup::upload(cfg);   // Upload the Egoboo configuration into 'setup.txt'.
    }
    return true;
}

bool config_download(egoboo_config_t& cfg)
{
    // Download configuration.
    EngineContext::get().particleHandler().download(cfg);
    AudioSystem::get().download(cfg);

    /// @todo Fix old-style download.
    CameraSystem::get().getCameraOptions().turnMode = cfg.camera_control.getValue();
    gfx_config_t::download(gfx, cfg);
    EngineContext::get().renderer().download(cfg);

    return true;
}

bool config_upload(egoboo_config_t& cfg)
{
    /// @todo Fix old-style upload.
    cfg.camera_control.setValue(CameraSystem::get().getCameraOptions().turnMode);

    // Upload configuration.
    AudioSystem::get().upload(cfg);
    EngineContext::get().particleHandler().upload(cfg);
    EngineContext::get().renderer().upload(cfg);

    return true;
}
