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

/// @file egolib/game/game_wawalite.c
/// @brief Environment configuration — wawalite read/write/upload and lighting query

#include "egolib/game/game_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

void Upload::upload_light_data(const wawalite_data_t& data)
{
    // Upload the lighting data.
    light_nrm = data.light.light_d;
    light_a = data.light.light_a;

    if (idlib::euclidean_norm(light_nrm) > 0.0f)
    {
        float length = idlib::euclidean_norm(light_nrm);

        // Get the extra magnitude of the direct light.
        if (gfx.usefaredge)
        {
            // We are outside, do the direct light as sunlight.
            light_d = light_a * length;
            light_a = 0.0f;
            //light_a = Ego::Math::constrain( light_a, 0.0f, 1.0f );
        }
        else
        {
            // We are inside. take the lighting values at face value.
            //light_d = (1.0f - light_a) * fTmp;
            //light_d = Ego::Math::constrain(light_d, 0.0f, 1.0f);
            light_d = Ego::Math::constrain(length, 0.0f, 1.0f);
        }

        light_nrm *= 1.0f / length;
    }
    else
    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "directional light vector is 0", Log::EndOfEntry);
    }

    //make_lighttable( pdata->light_x, pdata->light_y, pdata->light_z, pdata->light_a );
    //make_lighttospek();
}

void Upload::upload_phys_data( const wawalite_physics_t& data )
{
    // upload the physics data
    Ego::Physics::g_environment.hillslide = data.hillslide;
    Ego::Physics::g_environment.slippyfriction = data.slippyfriction;
    Ego::Physics::g_environment.noslipfriction = data.noslipfriction;
    Ego::Physics::g_environment.airfriction = data.airfriction;
    Ego::Physics::g_environment.waterfriction = data.waterfriction;
    Ego::Physics::g_environment.gravity = data.gravity;
}

void Upload::upload_graphics_data( const wawalite_graphics_t& data )
{
    // Read extra data
    gfx.exploremode = data.exploremode;
    gfx.usefaredge  = data.usefaredge;
}

void Upload::upload_camera_data( const wawalite_camera_t& data )
{
    if (!CameraSystem::is_initialized())
    {
        return;
    }

    CameraSystem::get().getCameraOptions().swing     = data.swing;
    CameraSystem::get().getCameraOptions().swingRate = data.swing_rate;
    CameraSystem::get().getCameraOptions().swingAmp  = data.swing_amp;
}

//--------------------------------------------------------------------------------------------
void upload_wawalite(fog_instance_t& fog, WeatherState& weatherState, AnimatedTilesState& animatedTilesState)
{
    /// @author ZZ
    /// @details This function sets up water and lighting for the module
    Upload::upload_phys_data( wawalite_data.phys );
    Upload::upload_graphics_data( wawalite_data.graphics );
    Upload::upload_light_data( wawalite_data);                         // this statement depends on data from upload_graphics_data()
    Upload::upload_camera_data( wawalite_data.camera );
    fog.upload( wawalite_data.fog );
    weatherState.upload( wawalite_data.weather );
    animatedTilesState.upload(wawalite_data.animtile);
}


//--------------------------------------------------------------------------------------------
wawalite_data_t *read_wawalite_vfs()
{
    wawalite_data_t *data = wawalite_data_read("mp_data/wawalite.txt", &wawalite_data);
    if (!data)
    {
        return nullptr;
    }

    // Fix any out-of-bounds data.
    wawalite_limit(&wawalite_data);

    // Finish up any data that has to be calculated.
    wawalite_finalize(&wawalite_data);

    return &wawalite_data;
}

//--------------------------------------------------------------------------------------------
bool wawalite_finalize(wawalite_data_t *data)
{
    /// @author BB
    /// @details coerce all parameters to in-bounds values
    if (!data) return false;

    // No weather?
    if (data->weather.weather_name == "*NONE*")
    {
        data->weather.part_gpip = LocalParticleProfileRef::Invalid;
    }
    else
    {
        std::string weather_name = data->weather.weather_name;
        idlib::to_lower_in_situ(weather_name);

        // Compute load paths.
        std::string prt_file = "mp_data/weather_" + weather_name + ".txt";
        std::string prt_end_file = "mp_data/weather_" + weather_name + "_finish.txt";

        // Try to load the particle files. We need at least the first particle for weather to work.
        bool success = INVALID_PIP_REF != EngineContext::get().profileSystem().loadParticleProfile(prt_file.c_str(), (PIP_REF)PIP_WEATHER);
        EngineContext::get().profileSystem().loadParticleProfile(prt_end_file, (PIP_REF)PIP_WEATHER_FINISH);

        // Unknown weather parsed.
        if (!success)
        {
            if(weather_name != "none")
            {
				EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to load weather type ", "`", weather_name, "`", "/", "`", prt_file, "`", " from wawalite.txt", Log::EndOfEntry);
            }
            data->weather.part_gpip = LocalParticleProfileRef::Invalid;
            data->weather.weather_name = "*NONE*";
        }
    }

    int windspeed_count = 0;
    Ego::Physics::g_environment.windspeed = idlib::zero<Ego::Vector3f>();

    int waterspeed_count = 0;
    Ego::Physics::g_environment.waterspeed = idlib::zero<Ego::Vector3f>();

    wawalite_water_layer_t *ilayer = wawalite_data.water.layer + 0;
    if (wawalite_data.water.background_req)
    {
        // This is a bit complicated.
        // It is the best I can do at reverse engineering what I did in render_world_background().

        const float cam_height = 1500.0f;
        const float default_bg_repeat = 4.0f;

        windspeed_count++;
        Ego::Physics::g_environment.windspeed[kX] += -ilayer->tx_add[SS] * Info<float>::Grid::Size() / (wawalite_data.water.backgroundrepeat / default_bg_repeat) * (cam_height + 1.0f / ilayer->dist[XX]) / cam_height;
        Ego::Physics::g_environment.windspeed[kY] += -ilayer->tx_add[TT] * Info<float>::Grid::Size() / (wawalite_data.water.backgroundrepeat / default_bg_repeat) * (cam_height + 1.0f / ilayer->dist[YY]) / cam_height;
        Ego::Physics::g_environment.windspeed[kZ] += -0;
    }
    else
    {
        waterspeed_count++;
		Ego::Vector3f tmp(-ilayer->tx_add[SS] * Info<float>::Grid::Size(), -ilayer->tx_add[TT] * Info<float>::Grid::Size(), 0.0f);
        Ego::Physics::g_environment.waterspeed += tmp;
    }

    ilayer = wawalite_data.water.layer + 1;
    if ( wawalite_data.water.overlay_req )
    {
        windspeed_count++;

        Ego::Physics::g_environment.windspeed[kX] += -600 * ilayer->tx_add[SS] * Info<float>::Grid::Size() / wawalite_data.water.foregroundrepeat * 0.04f;
        Ego::Physics::g_environment.windspeed[kY] += -600 * ilayer->tx_add[TT] * Info<float>::Grid::Size() / wawalite_data.water.foregroundrepeat * 0.04f;
        Ego::Physics::g_environment.windspeed[kZ] += -0;
    }
    else
    {
        waterspeed_count++;

        Ego::Physics::g_environment.waterspeed[kX] += -ilayer->tx_add[SS] * Info<float>::Grid::Size();
        Ego::Physics::g_environment.waterspeed[kY] += -ilayer->tx_add[TT] * Info<float>::Grid::Size();
        Ego::Physics::g_environment.waterspeed[kZ] += -0;
    }

    if ( waterspeed_count > 1 )
    {
        Ego::Physics::g_environment.waterspeed *= 1.0f/(float)waterspeed_count;
    }

    if ( windspeed_count > 1 )
    {
        Ego::Physics::g_environment.windspeed *= 1.0f/(float)windspeed_count;
    }

    return true;
}

//--------------------------------------------------------------------------------------------
bool write_wawalite_vfs(const wawalite_data_t *data)
{
    /// @author BB
    /// @details Prepare and write the wawalite file

    if (!data) return false;

    return wawalite_data_write("mp_data/wawalite.txt",data);
}

//--------------------------------------------------------------------------------------------
uint8_t get_light( int light, float seedark_mag )
{
    // ZF> Why should Darkvision reveal invisible?
    // BB> This modification makes the character's light (i.e how much it glows)
    //     more visible to characters with darkvision. This does not affect an object's
    //     alpha, which is what makes somethig invisible. If the object is glowing AND invisible,
    //     darkvision should make it more visible

    // is this object NOT glowing?
    if ( light >= 0xFF ) return 0xFF;

    if ( seedark_mag != 1.0f )
    {
        light *= seedark_mag;
    }

    return Ego::Math::constrain( light, 0, 0xFE );
}
