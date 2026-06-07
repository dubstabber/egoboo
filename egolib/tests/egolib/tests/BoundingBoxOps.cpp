#include "gtest/gtest.h"

#include "egolib/bbox.h"

// Characterization tests for the pure oct_bb_t operations in bbox.c that were
// not previously covered by BoundingBox.cpp (which only exercises validate/cut/
// downgrade): the two static contains() overloads, the join() family (vector,
// box, restricted-index, and the static src1/src2/dst form), intersection,
// interpolate, self_grow, and the to_points/points_to_oct_bb round-trip.
//
// An oct_bb_t carries 5 axes (OCT_X,Y,Z,XY,YX). A self-consistent axis-aligned
// box has OCT_XY = [minX+minY, maxX+maxY] and OCT_YX = [minY-maxX, maxY-minX].
// Emptiness is recomputed by empty_raw (true iff any mins[i] > maxs[i], STRICT),
// except the restricted-index join which validates via empty_index_raw (>=).
// Pure math, no fixture/bootstrap -- mirrors PhysicsCollisionNormal.cpp.

namespace
{

// Same helper as PhysicsCollisionNormal.cpp: a self-consistent axis-aligned box.
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

oct_vec_v2_t pointVec(float x, float y, float z)
{
    return oct_vec_v2_t(Ego::Vector3f(x, y, z)); // derives XY=x+y, YX=y-x
}

// ---------------------------------------------------------------------------
// contains(self, point)
// ---------------------------------------------------------------------------

TEST(OctBBContainsPoint, InteriorPointIsContained)
{
    const oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    EXPECT_TRUE(oct_bb_t::contains(box, pointVec(2, 2, 2)));
}

TEST(OctBBContainsPoint, BoundaryIsInclusive)
{
    // The corner (4,4,4): XY=8 == maxXY, YX=0 within [-4,4]. Rejection is strict
    // (< and >), so equal-to-bound passes on every axis.
    const oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    EXPECT_TRUE(oct_bb_t::contains(box, pointVec(4, 4, 4)));
}

TEST(OctBBContainsPoint, OutsideDiamondButInsideAabbIsRejected)
{
    // Cut the top-right corner off by tightening maxXY to 6, making a true
    // octagon. The point (4,4) is inside the X/Y AABB but has XY=8 > 6, so the
    // diamond (XY) axis rejects it.
    oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    box._maxs[OCT_XY] = 6.0f;
    EXPECT_FALSE(oct_bb_t::contains(box, pointVec(4, 4, 2)));
}

TEST(OctBBContainsPoint, EmptyBoxContainsNothing)
{
    const oct_bb_t empty; // default: _empty=true, all bounds 0
    EXPECT_FALSE(oct_bb_t::contains(empty, pointVec(0, 0, 0)));
}

// ---------------------------------------------------------------------------
// contains(self, other)
// ---------------------------------------------------------------------------

TEST(OctBBContainsBox, EmptyOtherIsAlwaysContainedEmptySelfContainsNothing)
{
    const oct_bb_t nonEmpty = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    const oct_bb_t empty;

    EXPECT_TRUE(oct_bb_t::contains(nonEmpty, empty));   // empty other -> true
    EXPECT_FALSE(oct_bb_t::contains(empty, nonEmpty));  // empty self, non-empty other -> false
    EXPECT_TRUE(oct_bb_t::contains(nonEmpty, nonEmpty)); // self-containment
}

TEST(OctBBContainsBox, StrictSubsetContainedSupersetNot)
{
    const oct_bb_t outer = makeAxisAlignedOctBox(0, 10, 0, 10, 0, 10);
    const oct_bb_t inner = makeAxisAlignedOctBox(2, 8, 2, 8, 2, 8);

    EXPECT_TRUE(oct_bb_t::contains(outer, inner));
    EXPECT_FALSE(oct_bb_t::contains(inner, outer));
}

// ---------------------------------------------------------------------------
// join
// ---------------------------------------------------------------------------

TEST(OctBBJoin, JoinVectorExpandsMaxsKeepsMins)
{
    oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4); // XY[0,8] YX[-4,4]
    box.join(pointVec(6, 6, 6)); // vec X=6 Y=6 Z=6 XY=12 YX=0

    EXPECT_FALSE(box.isEmpty());
    EXPECT_FLOAT_EQ(box._mins[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(box._maxs[OCT_X], 6.0f);
    EXPECT_FLOAT_EQ(box._maxs[OCT_XY], 12.0f); // max(8,12)
    EXPECT_FLOAT_EQ(box._maxs[OCT_YX], 4.0f);  // max(4,0)
}

TEST(OctBBJoin, StaticJoinIsPerAxisUnion)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4); // XY[0,8] YX[-4,4]
    const oct_bb_t b = makeAxisAlignedOctBox(2, 8, -2, 2, 1, 5); // XY[0,10] YX[-10,0]
    oct_bb_t dst;
    oct_bb_t::join(a, b, dst);

    EXPECT_FALSE(dst.isEmpty());
    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 0.0f);   EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 8.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], -2.0f);  EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 4.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 0.0f);   EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 5.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_XY], 0.0f);  EXPECT_FLOAT_EQ(dst._maxs[OCT_XY], 10.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_YX], -10.0f); EXPECT_FLOAT_EQ(dst._maxs[OCT_YX], 4.0f);
}

TEST(OctBBJoin, RestrictedIndexJoinTouchesOnlyOneAxis)
{
    oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    const oct_bb_t before = box;
    const oct_bb_t other = makeAxisAlignedOctBox(-5, 9, 0, 4, 0, 4); // X[-5,9]

    box.join(other, OCT_X);

    EXPECT_FLOAT_EQ(box._mins[OCT_X], -5.0f); // min(0,-5)
    EXPECT_FLOAT_EQ(box._maxs[OCT_X], 9.0f);  // max(4,9)
    // Every other axis is untouched.
    EXPECT_FLOAT_EQ(box._mins[OCT_Y], before._mins[OCT_Y]);
    EXPECT_FLOAT_EQ(box._maxs[OCT_Y], before._maxs[OCT_Y]);
    EXPECT_FLOAT_EQ(box._maxs[OCT_XY], before._maxs[OCT_XY]);
    EXPECT_FLOAT_EQ(box._mins[OCT_YX], before._mins[OCT_YX]);
    EXPECT_FALSE(box.isEmpty());
}

TEST(OctBBJoin, RestrictedIndexJoinThrowsOnOutOfRange)
{
    oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    const oct_bb_t other = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4);
    EXPECT_THROW(box.join(other, OCT_COUNT), std::runtime_error); // index 5
    EXPECT_THROW(box.join(other, -1), std::runtime_error);
}

// ---------------------------------------------------------------------------
// intersection
// ---------------------------------------------------------------------------

TEST(OctBBIntersection, OverlapIsPerAxisIntersection)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4); // XY[0,8]  YX[-4,4]
    const oct_bb_t b = makeAxisAlignedOctBox(2, 8, 1, 5, 1, 5); // XY[3,13] YX[-7,3]
    const oct_bb_t r = oct_bb_t::intersection(a, b);

    EXPECT_FALSE(r.isEmpty());
    EXPECT_FLOAT_EQ(r._mins[OCT_X], 2.0f);  EXPECT_FLOAT_EQ(r._maxs[OCT_X], 4.0f);
    EXPECT_FLOAT_EQ(r._mins[OCT_Y], 1.0f);  EXPECT_FLOAT_EQ(r._maxs[OCT_Y], 4.0f);
    EXPECT_FLOAT_EQ(r._mins[OCT_Z], 1.0f);  EXPECT_FLOAT_EQ(r._maxs[OCT_Z], 4.0f);
    EXPECT_FLOAT_EQ(r._mins[OCT_XY], 3.0f); EXPECT_FLOAT_EQ(r._maxs[OCT_XY], 8.0f);
    EXPECT_FLOAT_EQ(r._mins[OCT_YX], -4.0f); EXPECT_FLOAT_EQ(r._maxs[OCT_YX], 3.0f);
}

TEST(OctBBIntersection, DisjointIsEmptyAndBothEmptyReturnsDefault)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0, 2, 0, 2, 0, 2);
    const oct_bb_t b = makeAxisAlignedOctBox(5, 7, 0, 2, 0, 2); // X gap -> minsX 5 > maxsX 2
    EXPECT_TRUE(oct_bb_t::intersection(a, b).isEmpty());
    EXPECT_TRUE(oct_bb_t::intersection(oct_bb_t(), oct_bb_t()).isEmpty());
}

// ---------------------------------------------------------------------------
// interpolate
// ---------------------------------------------------------------------------

TEST(OctBBInterpolate, MidpointAndExactEndpoints)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0, 2, 0, 2, 0, 2); // XY[0,4]  YX[-2,2]
    const oct_bb_t b = makeAxisAlignedOctBox(4, 8, 4, 8, 4, 8); // XY[8,16] YX[-4,4]

    const oct_bb_t mid = oct_bb_t::interpolate(a, b, 0.5f);
    EXPECT_FALSE(mid.isEmpty());
    EXPECT_FLOAT_EQ(mid._mins[OCT_X], 2.0f);   EXPECT_FLOAT_EQ(mid._maxs[OCT_X], 5.0f);
    EXPECT_FLOAT_EQ(mid._mins[OCT_XY], 4.0f);  EXPECT_FLOAT_EQ(mid._maxs[OCT_XY], 10.0f);
    EXPECT_FLOAT_EQ(mid._mins[OCT_YX], -3.0f); EXPECT_FLOAT_EQ(mid._maxs[OCT_YX], 3.0f);

    // flip==0.0 returns src1 exactly; flip==1.0 returns src2 exactly.
    const oct_bb_t at0 = oct_bb_t::interpolate(a, b, 0.0f);
    EXPECT_FLOAT_EQ(at0._mins[OCT_X], 0.0f); EXPECT_FLOAT_EQ(at0._maxs[OCT_X], 2.0f);
    const oct_bb_t at1 = oct_bb_t::interpolate(a, b, 1.0f);
    EXPECT_FLOAT_EQ(at1._mins[OCT_X], 4.0f); EXPECT_FLOAT_EQ(at1._maxs[OCT_X], 8.0f);
}

TEST(OctBBInterpolate, EmptyOperandsYieldEmpty)
{
    const oct_bb_t a = makeAxisAlignedOctBox(0, 2, 0, 2, 0, 2);
    const oct_bb_t empty;
    EXPECT_TRUE(oct_bb_t::interpolate(empty, empty, 0.5f).isEmpty());
    EXPECT_TRUE(oct_bb_t::interpolate(empty, a, 0.5f).isEmpty());
    EXPECT_TRUE(oct_bb_t::interpolate(a, empty, 0.5f).isEmpty());
}

// ---------------------------------------------------------------------------
// self_grow
// ---------------------------------------------------------------------------

TEST(OctBBSelfGrow, GrowsSymmetricallyByAbsoluteValue)
{
    oct_bb_t box = makeAxisAlignedOctBox(0, 4, 0, 4, 0, 4); // XY[0,8] YX[-4,4]
    oct_bb_t::self_grow(box, oct_vec_v2_t(1, -2, 3, -1, 2)); // abs() is applied per axis

    EXPECT_FALSE(box.isEmpty());
    EXPECT_FLOAT_EQ(box._mins[OCT_X], -1.0f);  EXPECT_FLOAT_EQ(box._maxs[OCT_X], 5.0f);
    EXPECT_FLOAT_EQ(box._mins[OCT_Y], -2.0f);  EXPECT_FLOAT_EQ(box._maxs[OCT_Y], 6.0f);
    EXPECT_FLOAT_EQ(box._mins[OCT_Z], -3.0f);  EXPECT_FLOAT_EQ(box._maxs[OCT_Z], 7.0f);
    EXPECT_FLOAT_EQ(box._mins[OCT_XY], -1.0f); EXPECT_FLOAT_EQ(box._maxs[OCT_XY], 9.0f);
    EXPECT_FLOAT_EQ(box._mins[OCT_YX], -6.0f); EXPECT_FLOAT_EQ(box._maxs[OCT_YX], 6.0f);
}

// ---------------------------------------------------------------------------
// to_points / points_to_oct_bb
// ---------------------------------------------------------------------------

TEST(OctBBToPoints, GuardReturnsZeroOnNullOrZeroCount)
{
    const oct_bb_t box = makeAxisAlignedOctBox(0, 2, 0, 2, 0, 2);
    Ego::Vector4f pts[32];
    EXPECT_EQ(oct_bb_t::to_points(box, nullptr, 32), 0);
    EXPECT_EQ(oct_bb_t::to_points(box, pts, 0), 0);
}

TEST(OctBBToPoints, AxisAlignedBoxEmits16PointsAndRoundTrips)
{
    // For an axis-aligned (square) box the diamond is strictly larger than the
    // square, so all four edges take the 4-point else-branch -> 16 points, which
    // are the four square corners (each at z in {minZ,maxZ}).
    const oct_bb_t box = makeAxisAlignedOctBox(0, 2, 0, 2, 0, 4);
    Ego::Vector4f pts[32];
    const int n = oct_bb_t::to_points(box, pts, 32);
    EXPECT_EQ(n, 16);

    oct_bb_t rebuilt;
    oct_bb_t::points_to_oct_bb(rebuilt, pts, static_cast<size_t>(n));
    EXPECT_FALSE(rebuilt.isEmpty());
    // The corner cloud reconstructs the original box exactly on all 5 axes.
    EXPECT_FLOAT_EQ(rebuilt._mins[OCT_X], 0.0f);  EXPECT_FLOAT_EQ(rebuilt._maxs[OCT_X], 2.0f);
    EXPECT_FLOAT_EQ(rebuilt._mins[OCT_Y], 0.0f);  EXPECT_FLOAT_EQ(rebuilt._maxs[OCT_Y], 2.0f);
    EXPECT_FLOAT_EQ(rebuilt._mins[OCT_Z], 0.0f);  EXPECT_FLOAT_EQ(rebuilt._maxs[OCT_Z], 4.0f);
    EXPECT_FLOAT_EQ(rebuilt._mins[OCT_XY], 0.0f); EXPECT_FLOAT_EQ(rebuilt._maxs[OCT_XY], 4.0f);
    EXPECT_FLOAT_EQ(rebuilt._mins[OCT_YX], -2.0f); EXPECT_FLOAT_EQ(rebuilt._maxs[OCT_YX], 2.0f);
}

TEST(OctBBPointsToOctBB, ThrowsOnNullOrZeroCount)
{
    oct_bb_t box;
    Ego::Vector4f pts[4];
    EXPECT_ANY_THROW(oct_bb_t::points_to_oct_bb(box, nullptr, 4));
    EXPECT_THROW(oct_bb_t::points_to_oct_bb(box, pts, 0), std::invalid_argument);
}

TEST(OctBBPointsToOctBB, SinglePointIsNotEmptyAndDerivesDiamondAxes)
{
    Ego::Vector4f pts[1];
    pts[0][kX] = 3.0f; pts[0][kY] = 5.0f; pts[0][kZ] = 7.0f; pts[0][kW] = 99.0f; // W ignored

    oct_bb_t box;
    oct_bb_t::points_to_oct_bb(box, pts, 1);

    EXPECT_FALSE(box.isEmpty()); // mins==maxs is not empty (empty_raw uses strict >)
    EXPECT_FLOAT_EQ(box._mins[OCT_X], 3.0f);  EXPECT_FLOAT_EQ(box._maxs[OCT_X], 3.0f);
    EXPECT_FLOAT_EQ(box._mins[OCT_XY], 8.0f); // 3+5
    EXPECT_FLOAT_EQ(box._mins[OCT_YX], 2.0f); // 5-3
}

} // namespace
