#include "gtest/gtest.h"

#include "egolib/game/Physics/particle_collision.h"

// Characterization tests for get_recoil_factors(wta, wtb, *recoil_a, *recoil_b)
// in particle_collision.c: distribute a collision recoil between two masses.
// Pure arithmetic, no fixture/bootstrap; the symbol links from the egolib lib.
//
// Pinned behaviour (branch order is load-bearing -- first match wins):
//   * a weight >= 2^32 (the float value of CHR_INFINITE_WEIGHT) OR any negative
//     weight is treated as "infinite" (routes through the wta<0 / wtb<0 tests)
//   * both infinite, or wta==wtb (incl. 0==0): 0.5 / 0.5
//   * exactly one massless (==0): that SAME object takes ALL the recoil
//   * general finite both-nonzero: recoil_a = wtb/(wta+wtb), recoil_b = wta/(...)
//     (CROSS-assigned: the heavier object recoils less; the two sum to 1)
//   * a NULL out-param is rebound to a local, so nullptr is safe.

namespace
{

// (float)CHR_INFINITE_WEIGHT == (float)UINT32_MAX rounds to 2^32; the impl
// compares wt >= (float)CHR_INFINITE_WEIGHT, so 2^32 triggers the infinite branch.
constexpr float kInfiniteWeight = 4294967296.0f; // 2^32

TEST(GetRecoilFactors, FiniteSplitIsCrossAssignedAndSumsToOne)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(1.0f, 3.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.75f); // wtb/(wta+wtb) = 3/4
    EXPECT_FLOAT_EQ(rb, 0.25f); // wta/(wta+wtb) = 1/4
    EXPECT_FLOAT_EQ(ra + rb, 1.0f);
}

TEST(GetRecoilFactors, FiniteSplitScalesWithRatio)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(10.0f, 30.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.75f); // 30/40
    EXPECT_FLOAT_EQ(rb, 0.25f); // 10/40
}

TEST(GetRecoilFactors, NonDyadicSplitReproducesFloatDivision)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(1.0f, 2.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 2.0f / 3.0f);
    EXPECT_FLOAT_EQ(rb, 1.0f / 3.0f);
    EXPECT_NEAR(ra + rb, 1.0f, 1.0e-6f);
}

TEST(GetRecoilFactors, EqualFiniteMassesSplitHalfHalf)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(7.0f, 7.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.5f);
    EXPECT_FLOAT_EQ(rb, 0.5f);
}

TEST(GetRecoilFactors, BothZeroTakesEqualBranchNotZeroBranch)
{
    // wta==wtb (0==0) is tested before the zero/infinite branches -> 0.5/0.5,
    // and no division by zero occurs.
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(0.0f, 0.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.5f);
    EXPECT_FLOAT_EQ(rb, 0.5f);
}

TEST(GetRecoilFactors, MasslessObjectTakesAllItsOwnRecoil)
{
    // Counterintuitive but is the actual behaviour.
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(0.0f, 5.0f, &ra, &rb); // A massless
    EXPECT_FLOAT_EQ(ra, 1.0f);
    EXPECT_FLOAT_EQ(rb, 0.0f);

    get_recoil_factors(5.0f, 0.0f, &ra, &rb); // B massless
    EXPECT_FLOAT_EQ(ra, 0.0f);
    EXPECT_FLOAT_EQ(rb, 1.0f);
}

TEST(GetRecoilFactors, InfiniteMassGetsNoRecoilFiniteGetsAll)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(kInfiniteWeight, 10.0f, &ra, &rb); // A infinite
    EXPECT_FLOAT_EQ(ra, 0.0f);
    EXPECT_FLOAT_EQ(rb, 1.0f);

    get_recoil_factors(10.0f, kInfiniteWeight, &ra, &rb); // B infinite
    EXPECT_FLOAT_EQ(ra, 1.0f);
    EXPECT_FLOAT_EQ(rb, 0.0f);
}

TEST(GetRecoilFactors, BothInfiniteSplitHalfHalf)
{
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(kInfiniteWeight, kInfiniteWeight, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.5f);
    EXPECT_FLOAT_EQ(rb, 0.5f);
}

TEST(GetRecoilFactors, NegativeWeightBehavesLikeInfinite)
{
    // -1 is not normalized (it's < 2^32) but falls straight into the wta<0 test.
    float ra = -9.0f, rb = -9.0f;
    get_recoil_factors(-1.0f, 4.0f, &ra, &rb);
    EXPECT_FLOAT_EQ(ra, 0.0f);
    EXPECT_FLOAT_EQ(rb, 1.0f);
}

TEST(GetRecoilFactors, NullOutputPointersAreSafe)
{
    get_recoil_factors(2.0f, 3.0f, nullptr, nullptr); // rebound to locals -> no crash
    SUCCEED();

    // A NULL on one side still writes the other.
    float rb = -9.0f;
    get_recoil_factors(1.0f, 1.0f, nullptr, &rb);
    EXPECT_FLOAT_EQ(rb, 0.5f);
}

} // namespace
