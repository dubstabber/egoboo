#include "gtest/gtest.h"

#include "egolib/game/physics.h"

namespace
{

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

TEST(PhysicsIntersection, NoRelativeMotionOverlapReturnsWholeFrameInterval)
{
    const oct_bb_t src1 = makeAxisAlignedOctBox(0.0f, 2.0f, 0.0f, 2.0f, 0.0f, 2.0f);
    const oct_bb_t src2 = makeAxisAlignedOctBox(1.0f, 3.0f, 1.0f, 3.0f, 1.0f, 3.0f);

    oct_bb_t dst;
    float tmin = -1.0f;
    float tmax = -1.0f;

    EXPECT_TRUE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(),
                                      src2, Ego::Vector3f(), Ego::Vector3f(),
                                      PHYS_PLATFORM_NONE, dst, &tmin, &tmax));

    EXPECT_FLOAT_EQ(tmin, 0.0f);
    EXPECT_FLOAT_EQ(tmax, 1.0f);
    EXPECT_FALSE(dst.isEmpty());
    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 1.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 2.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], 1.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 2.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 1.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 2.0f);
}

TEST(PhysicsIntersection, MotionWithoutOverlapWithinFrameReturnsFalse)
{
    const oct_bb_t src1 = makeAxisAlignedOctBox(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    const oct_bb_t src2 = makeAxisAlignedOctBox(3.0f, 4.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    oct_bb_t dst;
    float tmin = -1.0f;
    float tmax = -1.0f;

    EXPECT_FALSE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(1.0f, 0.0f, 0.0f),
                                       src2, Ego::Vector3f(), Ego::Vector3f(),
                                       PHYS_PLATFORM_NONE, dst, &tmin, &tmax));

    EXPECT_FLOAT_EQ(tmin, 2.0f);
    EXPECT_FLOAT_EQ(tmax, 4.0f);
}

TEST(PhysicsIntersection, MovingIntersectionReturnsComputedIntervalAndExpandedBounds)
{
    const oct_bb_t src1 = makeAxisAlignedOctBox(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    const oct_bb_t src2 = makeAxisAlignedOctBox(3.0f, 4.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    oct_bb_t dst;
    float tmin = -1.0f;
    float tmax = -1.0f;

    EXPECT_TRUE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(3.0f, 0.0f, 0.0f),
                                      src2, Ego::Vector3f(), Ego::Vector3f(),
                                      PHYS_PLATFORM_NONE, dst, &tmin, &tmax));

    EXPECT_NEAR(tmin, 2.0f / 3.0f, 1.0e-6f);
    EXPECT_NEAR(tmax, 5.0f / (3.0f * idlib::sqrt_two<float>()), 1.0e-6f);
    EXPECT_FALSE(dst.isEmpty());
    EXPECT_FLOAT_EQ(dst._mins[OCT_X], 3.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_X], 4.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Y], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Y], 1.0f);
    EXPECT_FLOAT_EQ(dst._mins[OCT_Z], 0.0f);
    EXPECT_FLOAT_EQ(dst._maxs[OCT_Z], 1.0f);
}

TEST(PhysicsIntersection, PlatformToleranceAllowsNearVerticalGapDuringHorizontalSweep)
{
    const oct_bb_t src1 = makeAxisAlignedOctBox(0.0f, 1.0f, 0.0f, 1.0f, 21.0f, 31.0f);
    const oct_bb_t src2 = makeAxisAlignedOctBox(3.0f, 4.0f, 0.0f, 1.0f, 0.0f, 20.0f);

    oct_bb_t withoutPlatformDst;
    EXPECT_FALSE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(3.0f, 0.0f, 0.0f),
                                       src2, Ego::Vector3f(), Ego::Vector3f(),
                                       PHYS_PLATFORM_NONE, withoutPlatformDst, nullptr, nullptr));

    oct_bb_t withPlatformDst;
    EXPECT_TRUE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(3.0f, 0.0f, 0.0f),
                                      src2, Ego::Vector3f(), Ego::Vector3f(),
                                      PHYS_PLATFORM_OBJ2, withPlatformDst, nullptr, nullptr));

    EXPECT_FALSE(withPlatformDst.isEmpty());
    EXPECT_FLOAT_EQ(withPlatformDst._mins[OCT_Z], 21.0f);
    EXPECT_FLOAT_EQ(withPlatformDst._maxs[OCT_Z], 70.0f);
}

TEST(PhysicsIntersection, NullTimeOutputsUseLocalTemporaries)
{
    const oct_bb_t src1 = makeAxisAlignedOctBox(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    const oct_bb_t src2 = makeAxisAlignedOctBox(3.0f, 4.0f, 0.0f, 1.0f, 0.0f, 1.0f);

    oct_bb_t dst;

    EXPECT_TRUE(phys_intersect_oct_bb(src1, Ego::Vector3f(), Ego::Vector3f(3.0f, 0.0f, 0.0f),
                                      src2, Ego::Vector3f(), Ego::Vector3f(),
                                      PHYS_PLATFORM_NONE, dst, nullptr, nullptr));
    EXPECT_FALSE(dst.isEmpty());
}

} // namespace
