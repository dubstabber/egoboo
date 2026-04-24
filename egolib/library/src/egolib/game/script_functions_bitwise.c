/// @file egolib/game/script_functions_bitwise.c
/// @brief Bitwise alert and flag manipulation functions

#include "egolib/game/script_functions_internal.h"

/// @defgroup _bitwise_functions_ Bitwise Scripting Functions
/// @details These functions may be necessary to export the bitwise functions for handling alerts to
///  scripting languages where there is no support for bitwise operators (Lua, tcl, ...)

//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_SetAlertBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Sets the bit in the 32-bit integer self.alert indexed by state.argument

    int i = getBitIndex<0,31>(state.argument);
    SET_BIT( self.alert, 1 << i );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_ClearAlertBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Clears the bit in the 32-bit integer self.alert indexed by state.argument

    int i = getBitIndex<0, 31>(state.argument);
    UNSET_BIT( self.alert, 1 << i );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_TestAlertBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Tests to see if the the bit in the 32-bit integer self.alert indexed by state.argument is non-zero

    int i = getBitIndex<0,31>(state.argument);
    return HAS_SOME_BITS( self.alert,  1 << i );
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_SetAlert( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Sets one or more bits of the self.alert variable given by the bitmask in tmpargument

    SET_BIT( self.alert, Ego::Script::Interpreter::safeCast<int>(state.argument));
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_ClearAlert( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Clears one or more bits of the self.alert variable given by the bitmask in tmpargument

    UNSET_BIT( self.alert, Ego::Script::Interpreter::safeCast<int>(state.argument));
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_TestAlert( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Tests one or more bits of the self.alert variable given by the bitmask in tmpargument

    return HAS_SOME_BITS( self.alert, Ego::Script::Interpreter::safeCast<int>(state.argument) );
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_SetBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Sets the bit in the 32-bit tmpx variable with the offset given in tmpy

    int i = getBitIndex<0, 31>(state.y);
    SET_BIT( state.x, 1 << i );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_ClearBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Clears the bit in the 32-bit tmpx variable with the offset given in tmpy

    int i = getBitIndex<0, 31>(state.y);
    UNSET_BIT( state.x, 1 << i );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_TestBit( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Tests the bit in the 32-bit tmpx variable with the offset given in tmpy

    int i = getBitIndex<0, 31>(state.y);
    return HAS_SOME_BITS( state.x, 1 << i );
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_SetBits( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Adds the bits in the 32-bit tmpx based on the bitmask in tmpy

    SET_BIT( state.x, (int)state.y );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_ClearBits( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Clears the bits in the 32-bit tmpx based on the bitmask in tmpy

    UNSET_BIT( state.x, (int)state.y );
    return true;
}


//--------------------------------------------------------------------------------------------

/// @ingroup _bitwise_functions_
uint8_t scr_TestBits( script_state_t& state, ai_state_t& self )
{
    /// @author BB
    /// @details Tests the bits in the 32-bit tmpx based on the bitmask in tmpy

    return HAS_SOME_BITS( state.x, (int)state.y );
}
