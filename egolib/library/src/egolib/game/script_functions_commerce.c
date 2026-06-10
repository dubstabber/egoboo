/// @file egolib/game/script_functions_commerce.c
/// @brief Passages, fog/water/tile manipulation, module beats/exports, pits, and money/armor transactions

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

struct PassageCompatibilityContext
{
    std::shared_ptr<Passage> passage;
    ObjectRef selfRef = ObjectRef::Invalid;
};

struct ModuleEffectsContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    GameModule* module = nullptr;
};

struct TargetEconomyCompatibilityContext
{
    IAppearanceProfile* targetAppearance = nullptr;
    IWallet* selfWallet = nullptr;
    IWallet* targetWallet = nullptr;
};

struct ArmorCostPolicy
{
    int requestedSkinCost = 0;
    int currentSkinRefund = 0;
    int netCost = 0;
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
    engine().pushGameState(std::make_shared<VictoryScreen>(nullptr, true));
}

bool resolvePassageCompatibilityContext(const ai_state_t& self,
                                        int passageId,
                                        PassageCompatibilityContext& context)
{
    context.selfRef = self.getSelf();
    context.passage = tryPassage(passageId);
    return context.passage != nullptr;
}

bool openResolvedPassage(const PassageCompatibilityContext& context)
{
    if (!context.passage)
    {
        return false;
    }

    context.passage->open();
    return true;
}

bool closeResolvedPassage(const PassageCompatibilityContext& context)
{
    return context.passage != nullptr && context.passage->close();
}

bool isResolvedPassageOpen(const PassageCompatibilityContext& context)
{
    return context.passage != nullptr && context.passage->isOpen();
}

void flashResolvedPassage(const PassageCompatibilityContext& context, uint8_t flashColor)
{
    if (context.passage)
    {
        context.passage->flashColor(flashColor);
    }
}

bool addResolvedShopPassage(const PassageCompatibilityContext& context)
{
    if (!context.passage)
    {
        return false;
    }

    context.passage->makeShop(context.selfRef);
    return true;
}

TargetEconomyCompatibilityContext makeTargetEconomyCompatibilityContext(const ai_state_t& self)
{
    TargetEconomyCompatibilityContext context;
    context.targetAppearance = tryAppearanceProfile(self.getTarget());
    context.selfWallet = tryWallet(self.getSelf());
    context.targetWallet = tryWallet(self.getTarget());
    return context;
}

bool setTargetArmorPrice(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr)
    {
        return false;
    }

    const int value = context.targetAppearance->getSkinCost(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    if (value <= 0)
    {
        state.x = 0;
        return false;
    }

    state.x = value;
    return true;
}

ArmorCostPolicy makeArmorCostPolicy(const IAppearanceProfile& appearance,
                                    const size_t requestedSkin)
{
    ArmorCostPolicy policy;
    policy.requestedSkinCost = appearance.getSkinCost(requestedSkin);
    policy.currentSkinRefund = appearance.getSkinCost(appearance.getSkin());
    policy.netCost = policy.requestedSkinCost - policy.currentSkinRefund;
    return policy;
}

bool changeTargetArmor(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr)
    {
        return false;
    }

    const int oldSkin = context.targetAppearance->getSkin();
    state.x = context.targetAppearance->setSkin(Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.argument = oldSkin;
    return true;
}

void clampTransferredMoney(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.selfWallet == nullptr || context.targetWallet == nullptr)
    {
        return;
    }

    if (state.argument < 0 && std::abs(state.argument) > context.targetWallet->getMoney())
    {
        state.argument = -context.targetWallet->getMoney();
    }
    if (state.argument > context.selfWallet->getMoney())
    {
        state.argument = context.selfWallet->getMoney();
    }
}

bool giveMoneyToTarget(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.selfWallet == nullptr || context.targetWallet == nullptr)
    {
        return false;
    }

    clampTransferredMoney(state, context);
    context.selfWallet->giveMoney(-state.argument);
    context.targetWallet->giveMoney(state.argument);
    return true;
}

bool chargeTargetArmor(script_state_t& state, const TargetEconomyCompatibilityContext& context)
{
    if (context.targetAppearance == nullptr || context.targetWallet == nullptr)
    {
        return false;
    }

    const ArmorCostPolicy armorCost = makeArmorCostPolicy(*context.targetAppearance,
                                                          Ego::Script::Interpreter::safeCast<size_t>(state.argument));
    state.y = armorCost.requestedSkinCost;

    if (armorCost.netCost > context.targetWallet->getMoney())
    {
        state.x = armorCost.netCost - context.targetWallet->getMoney();
        return false;
    }

    context.targetWallet->giveMoney(-armorCost.netCost);
    state.x = 0;
    return true;
}

bool dropMoney(const script_state_t& state, IWallet* targetWallet)
{
    if (targetWallet == nullptr)
    {
        return false;
    }

    targetWallet->dropMoney(state.argument);
    return true;
}
} // namespace

//--------------------------------------------------------------------------------------------
uint8_t scr_GetTargetArmorPrice( script_state_t& state, ai_state_t& self )
{
    // tmpx = GetTargetArmorPrice( tmpargument = "skin" )
    /// @author ZZ
    /// @details This function returns the cost of the desired skin upgrade, setting
    /// tmpx to the price

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return setTargetArmorPrice(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_OpenPassage( script_state_t& state, ai_state_t& self )
{
    // OpenPassage( tmpargument = "passage" )

    /// @author ZZ
    /// @details This function opens the passage specified by tmpargument, failing if the
    /// passage was already open.
    /// Passage areas are defined in passage.txt and set in spawn.txt for the given character

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           openResolvedPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ClosePassage( script_state_t& state, ai_state_t& self )
{
    // ClosePassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function closes the passage specified by tmpargument, proceeding
    /// if the passage isn't blocked.  Crushable characters within the passage
    /// are crushed.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           closeResolvedPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfPassageOpen( script_state_t& state, ai_state_t& self )
{
    // IfPassageOpen( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function proceeds if the given passage is valid and open to movement
    /// Used mostly by door characters to tell them when to run their open animation.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           isResolvedPassageOpen(passageContext);
}


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
uint8_t scr_DropMoney( script_state_t& state, ai_state_t& self )
{
    // DropMoney( tmpargument = "money" )
    /// @author ZZ
    /// @details This function drops a certain amount of money, if the character has that
    /// much

    if (!resolveSelfContext(self).isResolved()) return false;

    IWallet* selfWallet = tryWallet(self.getSelf());
    return dropMoney(state, selfWallet);
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
uint8_t scr_BreakPassage( script_state_t& state, ai_state_t& self )
{
    // BreakPassage( tmpargument = "passage", tmpturn = "tile type", tmpdistance = "number of frames", tmpx = "borken tile", tmpy = "tile fx bits" )

    /// @author ZZ
    /// @details This function makes the tiles fall away ( turns into damage terrain )
    /// This function causes the tiles of a passage to increment if stepped on.
    /// tmpx and tmpy are both set to the location of whoever broke the tile if
    /// the function passed.

    if (!resolveSelfContext(self).isResolved()) return false;

    return ::BreakPassage( state.y, state.x, state.distance, state.turn, ( PASS_REF )state.argument, &( state.x ), &( state.y ) );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FlashPassage( script_state_t& state, ai_state_t& self )
{
    // FlashPassage( tmpargument = "passage", tmpdistance = "color" )

    /// @author ZZ
    /// @details This function makes the given passage light or dark.
    /// Usage: For debug purposes

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    resolvePassageCompatibilityContext(self, state.argument, passageContext);
    flashResolvedPassage(passageContext, state.distance);

    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_FindTileInPassage( script_state_t& state, ai_state_t& self )
{
    // tmpx, tmpy = FindTileInPassage( tmpargument = "passage", tmpdistance = "tile type", tmpx, tmpy )

    /// @author ZZ
    /// @details This function finds all tiles of the specified type that lie within the
    /// given passage.  Call multiple times to find multiple tiles.  tmpx and
    /// tmpy will be set to the middle of the found tile if one is found, or
    /// both will be set to 0 if no tile is found.
    /// tmpx and tmpy are required and set on return

    if (!resolveSelfContext(self).isResolved()) return false;

    return ::FindTileInPassage( state.x, state.y, state.distance, static_cast<PASS_REF>(state.argument), &(state.x), &(state.y) );
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
uint8_t scr_DropTargetMoney( script_state_t& state, ai_state_t& self )
{
    // DropTargetMoney( tmpargument = "amount" )
    /// @author ZZ
    /// @details This function drops some of the target's money

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return dropMoney(state, targetContext.targetWallet);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_AddShopPassage( script_state_t& state, ai_state_t& self )
{
    // AddShopPassage( tmpargument = "passage" )
    /// @author ZZ
    /// @details This function makes a passage behave as a shop area, as long as the
    /// character is alive.

    if (!resolveSelfContext(self).isResolved()) return false;

    PassageCompatibilityContext passageContext;
    return resolvePassageCompatibilityContext(self, state.argument, passageContext) &&
           addResolvedShopPassage(passageContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetPayForArmor( script_state_t& state, ai_state_t& self )
{
    // tmpx, tmpy = TargetPayForArmor( tmpargument = "skin" )

    /// @author ZZ
    /// @details This function costs the Target the appropriate amount of money for the
    /// given armor type.  Passes if the character has enough, and fails if not.
    /// Does trade-in bonus automatically.  tmpy is always set to cost of requested
    /// skin tmpx is set to amount needed after trade-in ( 0 for pass ).

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return chargeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_ChangeTargetArmor( script_state_t& state, ai_state_t& self )
{
    // ChangeTargetArmor( tmpargument = "armor" )

    /// @author ZZ
    /// @details This function sets the target's armor type and returns the old type
    /// as tmpargument and the new type as tmpx

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return changeTargetArmor(state, targetContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveMoneyToTarget( script_state_t& state, ai_state_t& self )
{
    // GiveMoneyToTarget( tmpargument = "money" )
    /// @author ZZ
    /// @details This function increases the target's money, while decreasing the
    /// character's own money.  tmpargument is set to the amount transferred

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetEconomyCompatibilityContext targetContext = makeTargetEconomyCompatibilityContext(self);
    return giveMoneyToTarget(state, targetContext);
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
