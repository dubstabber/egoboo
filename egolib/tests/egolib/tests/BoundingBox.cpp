#include "gtest/gtest.h"

#include "egolib/bbox.h"

namespace
{

oct_bb_t makeBoundingBox(const oct_vec_v2_t& mins, const oct_vec_v2_t& maxs)
{
    oct_bb_t box;
    box._empty = false;
    box._mins = mins;
    box._maxs = maxs;
    return box;
}

TEST(BoundingBox, ValidateRecomputesEmptyStateFromBounds)
{
    oct_bb_t inverted = makeBoundingBox(oct_vec_v2_t(5.0f, 1.0f, 1.0f, 1.0f, 1.0f),
                                        oct_vec_v2_t(4.0f, 2.0f, 2.0f, 2.0f, 2.0f));
    inverted._empty = false;

    oct_bb_t::validate(inverted);
    EXPECT_TRUE(inverted.isEmpty());

    oct_bb_t valid = makeBoundingBox(oct_vec_v2_t(1.0f, 2.0f, 3.0f, 4.0f, 5.0f),
                                     oct_vec_v2_t(6.0f, 7.0f, 8.0f, 9.0f, 10.0f));
    valid._empty = true;

    oct_bb_t::validate(valid);
    EXPECT_FALSE(valid.isEmpty());
}

TEST(BoundingBox, CutReturnsFalseForEmptyOtherAndLeavesBoxUnchanged)
{
    oct_bb_t box = makeBoundingBox(oct_vec_v2_t(0.0f, 1.0f, 2.0f, 3.0f, 4.0f),
                                   oct_vec_v2_t(10.0f, 11.0f, 12.0f, 13.0f, 14.0f));
    const oct_bb_t before = box;

    EXPECT_FALSE(box.cut(oct_bb_t()));

    for (size_t index = 0; index < OCT_COUNT; ++index)
    {
        EXPECT_FLOAT_EQ(box._mins[index], before._mins[index]);
        EXPECT_FLOAT_EQ(box._maxs[index], before._maxs[index]);
    }
    EXPECT_EQ(box.isEmpty(), before.isEmpty());
}

TEST(BoundingBox, RestrictedCutReturnsTrueAndOnlyMutatesRequestedAxis)
{
    oct_bb_t box = makeBoundingBox(oct_vec_v2_t(0.0f, 1.0f, 2.0f, 3.0f, 4.0f),
                                   oct_vec_v2_t(10.0f, 11.0f, 12.0f, 13.0f, 14.0f));
    const oct_bb_t before = box;
    const oct_bb_t other = makeBoundingBox(oct_vec_v2_t(2.0f, 20.0f, 30.0f, 40.0f, 50.0f),
                                           oct_vec_v2_t(8.0f, 21.0f, 31.0f, 41.0f, 51.0f));

    EXPECT_TRUE(box.cut(other, OCT_X));
    EXPECT_FLOAT_EQ(box._mins[OCT_X], 2.0f);
    EXPECT_FLOAT_EQ(box._maxs[OCT_X], 8.0f);

    for (size_t index = 0; index < OCT_COUNT; ++index)
    {
        if (index == OCT_X)
        {
            continue;
        }
        EXPECT_FLOAT_EQ(box._mins[index], before._mins[index]);
        EXPECT_FLOAT_EQ(box._maxs[index], before._maxs[index]);
    }
    EXPECT_FALSE(box.isEmpty());
}

TEST(BoundingBox, DowngradeToBoundingBoxPreservesExistingZeroSizeAndHeightSemantics)
{
    const oct_bb_t source = makeBoundingBox(oct_vec_v2_t(-4.0f, -5.0f, -6.0f, -7.0f, -8.0f),
                                            oct_vec_v2_t(9.0f, 10.0f, 11.0f, 12.0f, 13.0f));
    bumper_t state;
    state.size = 0.0f;
    state.height = 0.0f;

    bumper_t base;
    base.height = 3.0f;

    oct_bb_t downgraded = makeBoundingBox(oct_vec_v2_t(-1.0f, -1.0f, -1.0f, -1.0f, -1.0f),
                                          oct_vec_v2_t(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));

    oct_bb_t::downgrade(source, state, base, downgraded);

    EXPECT_FLOAT_EQ(downgraded._mins[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._maxs[OCT_X], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._mins[OCT_Y], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._maxs[OCT_Y], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._mins[OCT_XY], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._maxs[OCT_XY], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._mins[OCT_YX], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._maxs[OCT_YX], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._mins[OCT_Z], 0.0f);
    EXPECT_FLOAT_EQ(downgraded._maxs[OCT_Z], 0.0f);
    EXPECT_FALSE(downgraded.isEmpty());
}

TEST(BoundingBox, DowngradeToBumperUsesCurrentExtentsAndBaseHeight)
{
    const oct_bb_t source = makeBoundingBox(oct_vec_v2_t(-4.0f, -5.0f, -2.0f, -10.0f, -8.0f),
                                            oct_vec_v2_t(3.0f, 6.0f, 7.0f, 9.0f, 11.0f));
    bumper_t state;
    state.size = 1.0f;
    state.size_big = 1.0f;
    state.height = 1.0f;

    bumper_t base;
    base.height = 12.0f;

    bumper_t downgraded;
    oct_bb_t::downgrade(source, state, base, downgraded);

    EXPECT_FLOAT_EQ(downgraded.size, 6.0f);
    EXPECT_FLOAT_EQ(downgraded.size_big, 11.0f);
    EXPECT_FLOAT_EQ(downgraded.height, 12.0f);
}

} // namespace
