/// @file egolib/game/script_functions_commerce_module.c
/// @brief Module environment script functions (water/fog/tile manipulation, beats, exports, pits, expansion IDSZ)

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/GUI/MessageLog.hpp"

namespace
{
GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

egoboo_config_t& config()
{
    return EngineContext::get().config();
}

struct ModuleEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    GameModule* module = nullptr;
};

ModuleEffectsContext makeModuleEffectsContext(const ai_state_t& self)
{
    ModuleEffectsContext context;
    context.selfRef = self.getSelf();
    context.module = gameSession().tryActiveModule();
    return context;
}

GameModule& compatibleModule(const ModuleEffectsContext& context)
{
    if (context.module != nullptr)
    {
        return *context.module;
    }

    return activeModule();
}

water_instance_t& moduleWater(const ModuleEffectsContext& context)
{
    return compatibleModule(context).getWater();
}

fog_instance_t& moduleFog(const ModuleEffectsContext&)
{
    return gameSession().fog();
}

void setModuleWaterLevel(const ModuleEffectsContext& context, int waterLevelTimesTen)
{
    moduleWater(context).set_douse_level(waterLevelTimesTen / 10.0f);
}

int getModuleWaterLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleWater(context)._douse_level * 10;
}

void setModuleFogTopLevel(const ModuleEffectsContext& context, int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog(context);
    const float delta = (Ego::Script::Interpreter::safeCast<float>(fogLevelTimesTen) / 10.0f) - fog._top;
    fog._top += delta;
    fog._distance += delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogTopLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleFog(context)._top * 10;
}

void setModuleFogColor(const ModuleEffectsContext& context, int red, int green, int blue)
{
    fog_instance_t& fog = moduleFog(context);
    fog._red = Ego::Math::constrain(red, 0, 0xFF);
    fog._grn = Ego::Math::constrain(green, 0, 0xFF);
    fog._blu = Ego::Math::constrain(blue, 0, 0xFF);
}

void setModuleFogBottomLevel(const ModuleEffectsContext& context, int fogLevelTimesTen)
{
    fog_instance_t& fog = moduleFog(context);
    const float delta = (fogLevelTimesTen / 10.0f) - fog._bottom;
    fog._bottom += delta;
    fog._distance -= delta;
    fog._on = config().graphic_fog_enable.getValue();
    if (fog._distance < 1.0f)
    {
        fog._on = false;
    }
}

int getModuleFogBottomLevelTimesTen(const ModuleEffectsContext& context)
{
    return moduleFog(context)._bottom * 10;
}

bool tryGetModuleTileTypeAtPosition(const ModuleEffectsContext& context,
                                    const Ego::Vector2f& position,
                                    uint16_t& tileType)
{
    return compatibleModule(context).tryGetTileTypeAtPosition(position, tileType);
}

bool setModuleTileTypeAtPosition(const ModuleEffectsContext& context,
                                 const Ego::Vector2f& position,
                                 uint16_t tileType)
{
    return compatibleModule(context).setTileTypeAtPosition(position, tileType);
}

void markActiveModuleBeaten(const ModuleEffectsContext& context)
{
    compatibleModule(context).beatModule();
}

void setActiveModuleExportValid(const ModuleEffectsContext& context, bool valid)
{
    compatibleModule(context).setExportValid(valid);
}

void enableActiveModulePitsKill(const ModuleEffectsContext& context)
{
    compatibleModule(context).enablePitsKill();
}

void enableActiveModulePitsTeleport(const ModuleEffectsContext& context, const Ego::Vector3f& location)
{
    compatibleModule(context).enablePitsTeleport(location);
}

bool tryAddActiveModuleIdsz(const ModuleEffectsContext& context, const IDSZ2& idsz)
{
    return ModuleProfile::moduleAddIDSZ(compatibleModule(context).getPath(), idsz);
}

bool setActorTileType(const ModuleEffectsContext& context, uint16_t tileType)
{
    Object* selfObject = tryObject(context.selfRef);
    return selfObject != nullptr &&
           compatibleModule(context).setTileType(selfObject->getTile(), tileType);
}

void configurePitFall(const ModuleEffectsContext& context, const Ego::Vector3f& location)
{
    if (compatibleModule(context).isInsidePitBounds(location.x(), location.y()))
    {
        enableActiveModulePitsTeleport(context, location);
        return;
    }

    enableActiveModulePitsKill(context);
}

void pushModuleEndVictoryScreen()
{
    // Victory is owned by the active in-game state: route through the controller seam so this
    // game-core TU does not depend on the concrete VictoryScreen (which lives in egolib-gamestates).
    // scr_EndModule (the only caller) runs inside the gameplay update loop, where the active state is
    // always the PlayingState, so the guard is effectively always taken in real play.
    if (auto controller = EngineContext::get().tryActivePlayingState())
    {
        controller->endModuleInVictory();
    }
}
} // namespace

//--------------------------------------------------------------------------------------------
uint8_t scr_AddIDSZ( script_state_t& state, ai_state_t& self )
{
    // AddIDSZ( tmpargument = "idsz" )
    /// @author ZZ
    /// @details This function slaps an expansion IDSZ onto the menu.txt file.
    /// Used to show completion of special quests for a given module

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    if (tryAddActiveModuleIdsz(moduleContext, Ego::Script::Interpreter::safeCast<IDSZ2>(state.argument)))
    {
        // invalidate any module list so that we will reload them
        //module_list_valid = false;
    }

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTile( script_state_t& state, ai_state_t& self )
{
    // ChangeTile( tmpargument = "tile type")
    /// @author ZZ
    /// @details This function changes the tile under the character to the new tile type,
    /// which is highly module dependent

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    return setActorTileType(moduleContext, state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // SetWaterLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function raises or lowers the water in the module

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleWaterLevel(moduleContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetWaterLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetWaterLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the current douse level for the water * 10.
    /// A waterlevel in wawalight of 85 would set tmpargument to 850

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleWaterLevelTimesTen(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogLevel( tmpargument = "level" )
    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10
    /// !!BAD!! DOESN'T WORK !!BAD!!

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogTopLevel(moduleContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogLevel()
    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleFogTopLevelTimesTen(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogTAD( script_state_t& state, ai_state_t& self )
{
    /// @author ZZ
    /// @details This function sets the color of the module's fog.
    /// TAD stands for <turn, argument, distance> == <red, green, blue>.
    /// Makes sense, huh?
    /// !!BAD!! DOESN'T WORK !!BAD!!

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogColor(moduleContext, state.turn, state.argument, state.distance);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // SetFogBottomLevel( tmpargument = "level" )

    /// @author ZZ
    /// @details This function sets the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setModuleFogBottomLevel(moduleContext, state.argument);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetFogBottomLevel( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetFogBottomLevel()

    /// @author ZZ
    /// @details This function sets tmpargument to the level of the module's fog.
    /// Values are * 10

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    state.argument = getModuleFogBottomLevelTimesTen(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GetTileXY( script_state_t& state, ai_state_t& self )
{
    // tmpargument = GetTileXY( tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function sets tmpargument to the tile type at the specified
    /// coordinates

    if (!resolveSelfContext(self).isResolved()) return false;

    uint16_t tileType = 0;
    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    if (!tryGetModuleTileTypeAtPosition(moduleContext,
                                        Ego::Vector2f(float(state.x), float(state.y)),
                                        tileType))
    {
        return false;
    }

    state.argument = tileType;
    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_SetTileXY( script_state_t& state, ai_state_t& self )
{
    // SetTileXY( tmpargument = "tile type", tmpx = "x", tmpy = "y" )
    /// @author ZZ
    /// @details This function changes the tile type at the specified coordinates

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    return setModuleTileTypeAtPosition(moduleContext,
                                       Ego::Vector2f(float(state.x), float(state.y)),
                                       state.argument);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BeatModule( script_state_t& state, ai_state_t& self )
{
    // BeatModule()
    /// @author ZZ
    /// @details This function displays the Module Ended message

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    markActiveModuleBeaten(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EndModule( script_state_t& state, ai_state_t& self )
{
    // EndModule()
    /// @author ZZ
    /// @details This function presses the Escape key

    if (!resolveSelfContext(self).isResolved()) return false;

    pushModuleEndVictoryScreen();

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_DisableExport( script_state_t& state, ai_state_t& self )
{
    // DisableExport()
    /// @author ZZ
    /// @details This function turns export off

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setActiveModuleExportValid(moduleContext, false);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_EnableExport( script_state_t& state, ai_state_t& self )
{
    // EnableExport()
    /// @author ZZ
    /// @details This function turns export on

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    setActiveModuleExportValid(moduleContext, true);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsKill( script_state_t& state, ai_state_t& self )
{
    // PitsKill()
    /// @author ZZ
    /// @details This function activates pit deaths for when characters fall below a
    /// certain altitude.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    enableActiveModulePitsKill(moduleContext);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_PitsFall( script_state_t& state, ai_state_t& self )
{
    // PitsFall( tmpx = "teleprt x", tmpy = "teleprt y", tmpdistance = "teleprt z" )
    /// @author ZF
    /// @details This function activates pit teleportation.

    if (!resolveSelfContext(self).isResolved()) return false;

    const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
    configurePitFall(moduleContext,
                     Ego::Vector3f(static_cast<float>(state.x),
                                   static_cast<float>(state.y),
                                   static_cast<float>(state.distance)));

    return true;
}
