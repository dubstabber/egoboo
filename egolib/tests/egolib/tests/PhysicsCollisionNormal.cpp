#include "gtest/gtest.h"

#include "egolib/game/physics.h"

// Characterization tests for the pure-math physics free functions that were not
// previously covered by PhysicsIntersection.cpp:
//   - phys_expand_oct_bb           (swept bounding-volume expansion)
//   - phys_estimate_collision_normal / phys_estimate_pressure_normal
//   - apos_t displacement accumulator (join / evaluate)
// These run with no fixture/bootstrap (pure math), mirroring PhysicsIntersection.cpp.

namespace
{

// Identical helper to PhysicsIntersection.cpp: build an axis-aligned octagonal box.
oct_bb_t makeAxisAlignedOctBox(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
{
    oct_bb_t box;
    box._empty = false;

    box._mins[OCT_X] = minX;
    box._maxs[OCT_X] = maxX;
    box._mins[OCT_Y] = minY;
    box._maxs[OCT_Y] = maxY;
    box._mins[OCT_Z] = minZ;
    box._maxs[OCT_Z] = maxZ;

    box._mins[OCT_XY] = minX + minY;
    box._maxs[OCT_XY] = maxX + maxY;
    box._mins[OCT_YX] = minY - maxX;
    box._maxs[OCT_YX] = maxY - minX;
    return box;
}

float vectorLength(const Ego::Vector3f& v)
{
    return std::sqrt(v[kX] * v[kX] + v[kY] * v[kY] + v[kZ] * v[kZ]);
}

// ---------------------------------------------------------------------------
// phys_expand_oct_bb : sweep an oct box along a velocity over [tmin, tmax].
// ---------------------------------------------------------------------------

TEST(PhysicsExpandOctBB, ZeroVelocityReturnsSourceUnchanged)
{
    const oct_bb_t src = makeAxisAlignedOctBox(0.0f, 2.0f, -1.0f, 3.0f, 0.0f, 5.0f);

    oct_bb_t dst;
    EXPECT_TRUE(phys_expand_oct_bb(src, Ego::Vector3f(), 0.0f, 1.0f, dst));

    EXPECT_FALSE(dst.isEmpty());
    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 2.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], -1.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 3.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 5.0f);
}

TEST(PhysicsExpandOctBB, PositiveXVelocityExtendsMaxAlongSweep)
{
    const oct_bb_t src = makeAxisAlignedOctBox(0.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f);

    oct_bb_t dst;
    // Sweep over the whole frame [0,1] at velocity (3,0,0): the box at t=0 unioned
    // with the box translated by (3,0,0) -> X grows from [0,2] to [0,5].
    EXPECT_TRUE(phys_expand_oct_bb(src, Ego::Vector3f(3.0f, 0.0f, 0.0f), 0.0f, 1.0f, dst));

    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 5.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 2.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 2.0f);
}

TEST(PhysicsExpandOctBB, NegativeXVelocityExtendsMinAlongSweep)
{
    const oct_bb_t src = makeAxisAlignedOctBox(0.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f);

    oct_bb_t dst;
    EXPECT_TRUE(phys_expand_oct_bb(src, Ego::Vector3f(-2.0f, 0.0f, 0.0f), 0.0f, 1.0f, dst));

    EXPECT_FLOAT_EQ(dst._mins[OCT_X], -2.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 2.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 2.0f);
}

TEST(PhysicsExpandOctBB, NonZeroTminSweepsBetweenTimesNotFromOrigin)
{
    const oct_bb_t src = makeAxisAlignedOctBox(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    oct_bb_t dst;
    // With tmin=1, tmax=2 and velocity (0,4,0), BOTH endpoints are translated:
    // box at t=1 is Y=[4,5], box at t=2 is Y=[8,9] -> union Y=[4,9]. The original
    // t=0 position is NOT included.
    EXPECT_TRUE(phys_expand_oct_bb(src, Ego::Vector3f(0.0f, 4.0f, 0.0f), 1.0f, 2.0f, dst));

    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], 4.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 9.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 1.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 1.0f);
}

// ---------------------------------------------------------------------------
// phys_estimate_collision_normal / phys_estimate_pressure_normal
// Characterize the contract (return value, unit normal, positive depth,
// dominant axis, contained->pressure delegation) without over-pinning floats.
// exponent == 1.0f keeps the normal a straight normalized 1/depth vector.
// ---------------------------------------------------------------------------

TEST(PhysicsCollisionNormal, PartiallyOverlappingBoxesYieldUnitNormalAlongShallowAxis)
{
    // a and b overlap but neither contains the other; the overlap is thinnest along
    // X (depth 1) and thicker along Y/Z (depth 3), and the centres differ on every
    // axis so the direct collision-depth path is taken.
    const oct_bb_t a = makeAxisAlignedOctBox(0.0f, 4.0f, 0.0f, 4.0f, 0.0f, 4.0f);
    const oct_bb_t b = makeAxisAlignedOctBox(3.0f, 7.0f, 1.0f, 5.0f, 1.0f, 5.0f);

    oct_vec_v2_t odepth;
    Ego::Vector3f nrm;
    float depth = -1.0f;

    EXPECT_TRUE(phys_estimate_collision_normal(a, b, 1.0f, odepth, nrm, depth));
    EXPECT_GT(depth, 0.0f);
    EXPECT_NEAR(vectorLength(nrm), 1.0f, 1.0e-4f);

    // The shallow (X) axis dominates the separation normal.
    EXPECT_GT(std::abs(nrm[kX]), std::abs(nrm[kY]));
    EXPECT_GT(std::abs(nrm[kX]), std::abs(nrm[kZ]));
    // b's centre is on the +X side of a's centre, so the normal points along +X.
    EXPECT_GT(nrm[kX], 0.0f);
}

TEST(PhysicsCollisionNormal, WarpExponentStillProducesValidUnitNormal)
{
    // exponent != 1 routes through phys_warp_normal (the cylinder-warp branch that
    // exponent==1 skips). Characterize that the warped path still yields a valid
    // unit normal and a positive depth for the same partially-overlapping pair.
    const oct_bb_t a = makeAxisAlignedOctBox(0.0f, 4.0f, 0.0f, 4.0f, 0.0f, 4.0f);
    const oct_bb_t b = makeAxisAlignedOctBox(3.0f, 7.0f, 1.0f, 5.0f, 1.0f, 5.0f);

    oct_vec_v2_t odepth;
    Ego::Vector3f nrm;
    float depth = -1.0f;

    EXPECT_TRUE(phys_estimate_collision_normal(a, b, 2.0f, odepth, nrm, depth));
    EXPECT_GT(depth, 0.0f);
    EXPECT_NEAR(vectorLength(nrm), 1.0f, 1.0e-4f);
}

TEST(PhysicsCollisionNormal, FullySeparatedBoxesReturnFalse)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f);
    const oct_bb_t b = makeAxisAlignedOctBox(5.0f, 7.0f, 0.0f, 2.0f, 0.0f, 2.0f);

    oct_vec_v2_t odepth;
    Ego::Vector3f nrm;
    float depth = -1.0f;

    // No overlap -> pressure path detects the negative gap and reports failure.
    EXPECT_FALSE(phys_estimate_collision_normal(a, b, 1.0f, odepth, nrm, depth));
}

TEST(PhysicsCollisionNormal, ContainedBoxDelegatesToPressureNormal)
{
    // a fully contains b -> collision_normal must take the pressure branch and
    // therefore agree exactly with a direct phys_estimate_pressure_normal call.
    const oct_bb_t a = makeAxisAlignedOctBox(0.0f, 10.0f, 0.0f, 10.0f, 0.0f, 10.0f);
    const oct_bb_t b = makeAxisAlignedOctBox(1.0f, 3.0f, 4.0f, 6.0f, 4.0f, 6.0f);

    oct_vec_v2_t odepthCollision;
    Ego::Vector3f nrmCollision;
    float depthCollision = -1.0f;
    const bool collisionResult =
        phys_estimate_collision_normal(a, b, 1.0f, odepthCollision, nrmCollision, depthCollision);

    oct_vec_v2_t odepthPressure;
    Ego::Vector3f nrmPressure;
    float depthPressure = -1.0f;
    const bool pressureResult =
        phys_estimate_pressure_normal(a, b, 1.0f, odepthPressure, nrmPressure, depthPressure);

    EXPECT_TRUE(collisionResult);
    EXPECT_EQ(collisionResult, pressureResult);
    EXPECT_GT(depthCollision, 0.0f);
    EXPECT_NEAR(vectorLength(nrmCollision), 1.0f, 1.0e-4f);

    // Delegation: contained-box collision normal == pressure normal.
    EXPECT_FLOAT_EQ(nrmCollision[kX], nrmPressure[kX]);
    EXPECT_FLOAT_EQ(nrmCollision[kY], nrmPressure[kY]);
    EXPECT_FLOAT_EQ(nrmCollision[kZ], nrmPressure[kZ]);
    EXPECT_FLOAT_EQ(depthCollision, depthPressure);
}

// ---------------------------------------------------------------------------
// apos_t : directional displacement accumulator.
// "evaluate" returns maxs + mins (the extremes), NOT the running sum -- opposing
// pushes are taken as their largest magnitude per direction, not added up.
// ---------------------------------------------------------------------------

TEST(PhysicsApos, EmptyAccumulatorEvaluatesToZero)
{
    apos_t accumulator;
    Ego::Vector3f result(1.0f, 1.0f, 1.0f);
    apos_t::evaluate(accumulator, result);

    EXPECT_FLOAT_EQ(result[kX], 0.0f);
    EXPECT_FLOAT_EQ(result[kY], 0.0f);
    EXPECT_FLOAT_EQ(result[kZ], 0.0f);
}

TEST(PhysicsApos, JoinTakesDirectionalExtremaNotSum)
{
    apos_t accumulator;
    accumulator.join(3.0f, 0); // +X
    accumulator.join(1.0f, 0); // +X again -- does NOT add to 4

    Ego::Vector3f afterPositives(0.0f, 0.0f, 0.0f);
    apos_t::evaluate(accumulator, afterPositives);
    EXPECT_FLOAT_EQ(afterPositives[kX], 3.0f); // max push, not 4

    accumulator.join(-2.0f, 0); // opposing push
    Ego::Vector3f afterNegative(0.0f, 0.0f, 0.0f);
    apos_t::evaluate(accumulator, afterNegative);
    EXPECT_FLOAT_EQ(afterNegative[kX], 1.0f); // maxs(3) + mins(-2)

    // The running sum field still accumulates every push (3 + 1 - 2 = 2).
    EXPECT_FLOAT_EQ(accumulator.sum[kX], 2.0f);
}

TEST(PhysicsApos, JoinVectorSplitsExtremaByComponentSign)
{
    apos_t accumulator;
    accumulator.join(Ego::Vector3f(3.0f, -2.0f, 0.0f));

    Ego::Vector3f firstResult(0.0f, 0.0f, 0.0f);
    apos_t::evaluate(accumulator, firstResult);
    EXPECT_FLOAT_EQ(firstResult[kX], 3.0f);
    EXPECT_FLOAT_EQ(firstResult[kY], -2.0f);
    EXPECT_FLOAT_EQ(firstResult[kZ], 0.0f);

    accumulator.join(Ego::Vector3f(-1.0f, 5.0f, 0.0f));
    Ego::Vector3f secondResult(0.0f, 0.0f, 0.0f);
    apos_t::evaluate(accumulator, secondResult);
    EXPECT_FLOAT_EQ(secondResult[kX], 2.0f);  // maxs(3) + mins(-1)
    EXPECT_FLOAT_EQ(secondResult[kY], 3.0f);  // maxs(5) + mins(-2)
    EXPECT_FLOAT_EQ(secondResult[kZ], 0.0f);
}

TEST(PhysicsApos, JoinAtOutOfBoundsIndexThrows)
{
    apos_t accumulator;
    EXPECT_THROW(accumulator.join(1.0f, 3), std::runtime_error);
}

} // namespace
