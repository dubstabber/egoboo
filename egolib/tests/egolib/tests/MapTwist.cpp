#include "gtest/gtest.h"

#include "egolib/map_functions.h"

#include <cmath>

// Characterization tests for the pure twist <-> normal math in map_functions.c:
//   - cartman_calc_twist(dx, dy): clamp each tilt to [-7,8], bias +7, pack as
//     twist = (y<<4) + x. Flat ground (0,0) -> 0x77 (119).
//   - twist_to_normal(twist, v, slide): decode a twist byte to an (always unit)
//     surface normal. Constants CARTMAN_FIXNUM=4.125, CARTMAN_SLOPE=50 are baked
//     into the expected numbers below; the test does not reference them. All
//     cases use slide=1.0 (what mesh.c uses), so diff_xy=128.
// Pure math, no fixture/bootstrap.

namespace
{

float vectorLength(const Ego::Vector3f& v)
{
    return std::sqrt(v[kX] * v[kX] + v[kY] * v[kY] + v[kZ] * v[kZ]);
}

// ---------------------------------------------------------------------------
// cartman_calc_twist : clamp + bias + nibble-pack
// ---------------------------------------------------------------------------

TEST(MapCalcTwist, FlatGroundIs0x77)
{
    // (0+7)=7 each -> (7<<4)+7 = 119.
    EXPECT_EQ(cartman_calc_twist(0, 0), 119); // 0x77
}

TEST(MapCalcTwist, PlusXRaisesLowNibblePlusYRaisesHighNibble)
{
    EXPECT_EQ(cartman_calc_twist(1, 0), 120); // (7<<4)+8 = 0x78
    EXPECT_EQ(cartman_calc_twist(0, 1), 135); // (8<<4)+7 = 0x87
}

TEST(MapCalcTwist, ClampsToCornersZeroAnd255)
{
    EXPECT_EQ(cartman_calc_twist(-7, -7), 0);     // (0<<4)+0
    EXPECT_EQ(cartman_calc_twist(-100, -100), 0); // clamped to -7,-7
    EXPECT_EQ(cartman_calc_twist(8, 8), 255);     // (15<<4)+15 = 0xFF
    EXPECT_EQ(cartman_calc_twist(1000, 1000), 255);
}

TEST(MapCalcTwist, ClampRangeIsAsymmetricMinus7ToPlus8)
{
    // -8 clamps UP to -7 (->0); 9 clamps DOWN to 8 (->15). Pins the [-7,8] range.
    EXPECT_EQ(cartman_calc_twist(-8, 0), 112); // (7<<4)+0 = 0x70
    EXPECT_EQ(cartman_calc_twist(9, 0), 127);  // (7<<4)+15 = 0x7F
}

// ---------------------------------------------------------------------------
// twist_to_normal : decode to a (unit) surface normal
// ---------------------------------------------------------------------------

TEST(MapTwistToNormal, FlatTwistIsExactlyVertical)
{
    Ego::Vector3f n;
    const bool r = twist_to_normal(119, n, 1.0f); // 0x77: ix=iy=0 -> dx=dy=0
    EXPECT_TRUE(r);
    EXPECT_FLOAT_EQ(n[kX], 0.0f);
    EXPECT_FLOAT_EQ(n[kY], 0.0f);
    EXPECT_FLOAT_EQ(n[kZ], 1.0f);
}

TEST(MapTwistToNormal, RaisingLowNibbleTiltsTowardPositiveX)
{
    Ego::Vector3f n;
    twist_to_normal(120, n, 1.0f); // 0x78: ix=1 -> dx<0 -> nx>0
    EXPECT_GT(n[kX], 0.0f);
    EXPECT_FLOAT_EQ(n[kY], 0.0f);
    EXPECT_GT(n[kZ], 0.0f);
    EXPECT_LT(n[kZ], 1.0f);
    EXPECT_NEAR(n[kX], 0.094275f, 1.0e-3f);
    EXPECT_NEAR(vectorLength(n), 1.0f, 1.0e-5f);
}

TEST(MapTwistToNormal, LoweringLowNibbleTiltsTowardNegativeX)
{
    Ego::Vector3f n;
    twist_to_normal(118, n, 1.0f); // 0x76: ix=-1 -> dx>0 -> nx<0 (mirror of 0x78)
    EXPECT_LT(n[kX], 0.0f);
    EXPECT_FLOAT_EQ(n[kY], 0.0f);
    EXPECT_NEAR(n[kX], -0.094275f, 1.0e-3f);
    EXPECT_NEAR(vectorLength(n), 1.0f, 1.0e-5f);
}

TEST(MapTwistToNormal, RaisingHighNibbleTiltsTowardNegativeY)
{
    Ego::Vector3f n;
    twist_to_normal(135, n, 1.0f); // 0x87: iy=1 -> dy>0 -> ny<0 (sign asymmetry vs X)
    EXPECT_FLOAT_EQ(n[kX], 0.0f);
    EXPECT_LT(n[kY], 0.0f);
    EXPECT_NEAR(n[kY], -0.094275f, 1.0e-3f);
    EXPECT_NEAR(vectorLength(n), 1.0f, 1.0e-5f);
}

TEST(MapTwistToNormal, NormalIsAlwaysUnitLengthAndUpward)
{
    const uint8_t twists[] = {0, 118, 120, 135, 103, 255, 136, 119};
    for (uint8_t tw : twists)
    {
        Ego::Vector3f n;
        const bool r = twist_to_normal(tw, n, 1.0f);
        EXPECT_TRUE(r); // there is no failure path
        EXPECT_NEAR(vectorLength(n), 1.0f, 1.0e-5f);
        EXPECT_GE(n[kZ], 0.0f); // nz = sqrt(...) is never negative
    }
}

// ---------------------------------------------------------------------------
// round-trips tying the two functions
// ---------------------------------------------------------------------------

TEST(MapTwistRoundTrip, FlatTiltRoundTripsToVerticalNormal)
{
    Ego::Vector3f n;
    const uint8_t tw = cartman_calc_twist(0, 0);
    EXPECT_EQ(tw, 119);
    twist_to_normal(tw, n, 1.0f);
    EXPECT_FLOAT_EQ(n[kX], 0.0f);
    EXPECT_FLOAT_EQ(n[kY], 0.0f);
    EXPECT_FLOAT_EQ(n[kZ], 1.0f);
}

TEST(MapTwistRoundTrip, PlusXTiltRoundTripsToPositiveXNormal)
{
    Ego::Vector3f n;
    const uint8_t tw = cartman_calc_twist(1, 0);
    EXPECT_EQ(tw, 120);
    twist_to_normal(tw, n, 1.0f);
    EXPECT_GT(n[kX], 0.0f);
    EXPECT_FLOAT_EQ(n[kY], 0.0f);
}

} // namespace
