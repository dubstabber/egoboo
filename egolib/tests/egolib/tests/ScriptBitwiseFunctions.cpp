#include "gtest/gtest.h"

#include "egolib/Script/script.h"
#include "egolib/game/script_functions.h"
#include "idlib/exception/argument_out_of_bounds_error.hpp"

namespace
{

TEST(ScriptBitwiseFunctions, AlertBitHelpersMutateAndQueryAlertBits)
{
    script_state_t state;
    ai_state_t self;

    state.argument = 5;
    EXPECT_TRUE(scr_SetAlertBit(state, self));
    EXPECT_EQ(self.alert, 1u << 5);
    EXPECT_TRUE(scr_TestAlertBit(state, self));

    state.argument = 6;
    EXPECT_FALSE(scr_TestAlertBit(state, self));

    state.argument = 5;
    EXPECT_TRUE(scr_ClearAlertBit(state, self));
    EXPECT_EQ(self.alert, 0u);
}

TEST(ScriptBitwiseFunctions, AlertMaskHelpersMutateAndQueryAlertMasks)
{
    script_state_t state;
    ai_state_t self;

    state.argument = 0x0A;
    EXPECT_TRUE(scr_SetAlert(state, self));
    EXPECT_EQ(self.alert, 0x0Au);

    state.argument = 0x08;
    EXPECT_TRUE(scr_TestAlert(state, self));

    state.argument = 0x10;
    EXPECT_FALSE(scr_TestAlert(state, self));

    state.argument = 0x08;
    EXPECT_TRUE(scr_ClearAlert(state, self));
    EXPECT_EQ(self.alert, 0x02u);
}

TEST(ScriptBitwiseFunctions, StateBitHelpersMutateAndQueryStateBits)
{
    script_state_t state;
    ai_state_t self;

    state.y = 3;
    EXPECT_TRUE(scr_SetBit(state, self));
    EXPECT_EQ(state.x, 1 << 3);
    EXPECT_TRUE(scr_TestBit(state, self));

    state.y = 4;
    EXPECT_FALSE(scr_TestBit(state, self));

    state.y = 3;
    EXPECT_TRUE(scr_ClearBit(state, self));
    EXPECT_EQ(state.x, 0);
}

TEST(ScriptBitwiseFunctions, StateMaskHelpersMutateAndQueryStateMasks)
{
    script_state_t state;
    ai_state_t self;

    state.x = 0x05;
    state.y = 0x0A;
    EXPECT_TRUE(scr_SetBits(state, self));
    EXPECT_EQ(state.x, 0x0F);

    state.y = 0x08;
    EXPECT_TRUE(scr_TestBits(state, self));

    state.y = 0x10;
    EXPECT_FALSE(scr_TestBits(state, self));

    state.y = 0x05;
    EXPECT_TRUE(scr_ClearBits(state, self));
    EXPECT_EQ(state.x, 0x0A);
}

TEST(ScriptBitwiseFunctions, InvalidBitIndexesStillThrow)
{
    script_state_t state;
    ai_state_t self;

    state.argument = -1;
    EXPECT_THROW(scr_SetAlertBit(state, self), idlib::argument_out_of_bounds_error);

    state.argument = 32;
    EXPECT_THROW(scr_ClearAlertBit(state, self), idlib::argument_out_of_bounds_error);

    state.y = -1;
    EXPECT_THROW(scr_SetBit(state, self), idlib::argument_out_of_bounds_error);

    state.y = 32;
    EXPECT_THROW(scr_TestBit(state, self), idlib::argument_out_of_bounds_error);
}

} // namespace
