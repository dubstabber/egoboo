/// @file egolib/game/mesh_bridge.c
/// @brief Water-aware getElevation through the active module-environment seam.

#include "egolib/game/mesh.h"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Water.hpp"

float ego_mesh_t::getElevation(const Ego::Vector2f& p, bool waterwalk) const
{
    const float floorElevation = getElevation(p);
    water_instance_t& water = activeModuleEnvironment().water();

    if (waterwalk && water._surface_level > floorElevation && water._is_water) {
        if (0 != test_fx(getTileIndex(p), MAPFX_WATER)) {
            return water._surface_level;
        }
    }
    return floorElevation;
}
