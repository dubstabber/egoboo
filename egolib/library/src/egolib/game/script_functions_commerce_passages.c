/// @file egolib/game/script_functions_commerce_passages.c
/// @brief Passage manipulation script functions (open/close/flash/shop passages, tile-in-passage queries)

#include "egolib/game/script_functions_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
struct PassageCompatibilityContext
{
    std::shared_ptr<Passage> passage;
    ObjectRef selfRef = ObjectRef::Invalid;
};

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
} // namespace

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
