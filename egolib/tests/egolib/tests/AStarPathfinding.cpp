// Characterization tests for AStar (egolib/library/src/egolib/AI/AStar.cpp).
// These pin CURRENT observable behavior, including bugs — do not fix silently.
//
// Notable internals these tests are built around:
// (1) the priority_queue comparator at AStar.cpp:101-103 makes a MAX-heap, so the
//     "cheapest open node" comment at :130 is wrong — the search pops the
//     HIGHEST-weight node first;
// (2) neighbors are closed at GENERATION (AStar.cpp:144-148), never re-parented,
//     and the start tile is not closed at INITIALIZATION (AStar.cpp:114-117) — it
//     gets closed the first time it is re-generated as a neighbor, permitting
//     exactly one out-and-back echo, never repeated ones;
// (3) both quirks are unobservable in a 1-tile-wide forced corridor, which is why
//     every exact-sequence pin below uses one — open-field routes are
//     std::priority_queue tie-break-sensitive and must never be exactly pinned.

#include "gtest/gtest.h"

#include "egolib/AI/AStar.hpp"
#include "egolib/AI/WaypointList.h"
#include "egolib/FileFormats/map_fx.hpp"
#include "egolib/FileFormats/map_file.h"

#include "StubTerrain.hpp"

namespace
{

using Ego::Test::StubTerrain;

constexpr int GRID = Info<int>::Grid::Size(); // == 128
constexpr int HALF = GRID / 2;                // == 64

// Quirk pin — characterizes current behavior, do not fix silently: the final
// waypoint is the VERBATIM (pos_x, dst_y) arguments of get_path (AStar.cpp:258-263),
// not a value derived from the destination tile; a straight path emits no
// intermediate waypoints because change_direction (AStar.cpp:237) requires BOTH
// axes to differ.
TEST(AStarPathfinding, StraightCorridorEmitsSingleRawDestinationWaypoint)
{
    StubTerrain terrain(4, 1);
    AStar astar;
    waypoint_list_t wp = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 3, 0));
    EXPECT_TRUE(astar.get_path(999, 555, wp));

    EXPECT_EQ(wp._head, 1);
    EXPECT_FLOAT_EQ(wp._pos[0][0], 999.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 555.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
}

// change_direction fires at node (2,1) because BOTH its ix and iy differ from the
// last emitted waypoint node (0,0) (AStar.cpp:237); the emitted waypoint is the
// PREVIOUS safe node (2,0) translated to tile center ix*Grid::Size()+Size()/2
// (AStar.cpp:266-269, Grid Size 128 per map_file.h:44-54).
TEST(AStarPathfinding, ForcedLCorridorEmitsCornerTileCenterThenDestination)
{
    StubTerrain terrain(3, 3);
    terrain.setFX(0, 1, MAPFX_IMPASS);
    terrain.setFX(1, 1, MAPFX_IMPASS);
    terrain.setFX(0, 2, MAPFX_IMPASS);
    terrain.setFX(1, 2, MAPFX_IMPASS);
    // Forced path: (0,0)->(1,0)->(2,0)->(2,1)->(2,2).
    AStar astar;
    waypoint_list_t wp = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 2));
    EXPECT_TRUE(astar.get_path(2 * GRID + HALF, 2 * GRID + HALF, wp)); // (320, 320)

    EXPECT_EQ(wp._head, 2);
    // Corner tile (2,0) center = 2*128+64, 0*128+64.
    EXPECT_FLOAT_EQ(wp._pos[0][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
    // Raw get_path arguments.
    EXPECT_FLOAT_EQ(wp._pos[1][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][1], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][2], 0.0f);
}

// Quirk pin — characterizes current behavior, do not fix silently:
// (a) the emission loop guard waypoint_num < MAXWAY (AStar.cpp:226, MAXWAY 8 per
//     WaypointList.h:28) stops after 8 corner waypoints, so the promised final
//     destination waypoint (docstring AStar.cpp:196-199, raw push at :258-263) is
//     NEVER emitted — the raw args 9999/8888 appear nowhere in the list;
// (b) waypoint_list_t::push clamps _head to MAXWAY-1 == 7 after the 8th push
//     (WaypointList.c:71-76), so _head reads 7 even though _pos[7] holds the
//     8th waypoint.
// Determinism audited: the 3x9 one-tile-wide snake keeps exactly one open node at
// every step (after one benign start-tile echo with a distinct weight), so this
// exact sequence is invariant to priority_queue tie-breaking.
TEST(AStarPathfinding, SerpentineForcedCorridorStopsAtMaxwayAndDropsDestination)
{
    StubTerrain terrain(3, 9);
    terrain.setFX(0, 1, MAPFX_IMPASS);
    terrain.setFX(1, 1, MAPFX_IMPASS);
    terrain.setFX(1, 3, MAPFX_IMPASS);
    terrain.setFX(2, 3, MAPFX_IMPASS);
    terrain.setFX(0, 5, MAPFX_IMPASS);
    terrain.setFX(1, 5, MAPFX_IMPASS);
    terrain.setFX(1, 7, MAPFX_IMPASS);
    terrain.setFX(2, 7, MAPFX_IMPASS);
    // Forces the unique 1-wide snake:
    // (0,0)->(1,0)->(2,0)->(2,1)->(2,2)->(1,2)->(0,2)->(0,3)->(0,4)->(1,4)->
    // (2,4)->(2,5)->(2,6)->(1,6)->(0,6)->(0,7)->(0,8)->(1,8)->(2,8).
    AStar astar;
    waypoint_list_t wp = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 8));
    EXPECT_TRUE(astar.get_path(9999, 8888, wp));

    EXPECT_EQ(wp._head, 7);
    // Corner tiles (2,0),(2,2),(0,2),(0,4),(2,4),(2,6),(0,6),(0,8) at
    // center = tile*128+64. The exact pins also imply that no slot contains the
    // raw 9999/8888 destination arguments.
    EXPECT_FLOAT_EQ(wp._pos[0][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][1], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[2][0], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[2][1], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[2][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[3][0], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[3][1], 576.0f);
    EXPECT_FLOAT_EQ(wp._pos[3][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[4][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[4][1], 576.0f);
    EXPECT_FLOAT_EQ(wp._pos[4][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[5][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[5][1], 832.0f);
    EXPECT_FLOAT_EQ(wp._pos[5][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[6][0], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[6][1], 832.0f);
    EXPECT_FLOAT_EQ(wp._pos[6][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[7][0], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[7][1], 1088.0f);
    EXPECT_FLOAT_EQ(wp._pos[7][2], 0.0f);
}

// Quirk pin — characterizes current behavior, do not fix silently: find_path
// validates only that the SOURCE is on the mesh (AStar.cpp:68-75), never that it
// is passable, so pathing out of a wall succeeds; only the destination is checked
// for impassability (AStar.cpp:78).
TEST(AStarPathfinding, FindPathSucceedsFromImpassableSourceTile)
{
    StubTerrain terrain(3, 1);
    terrain.setFX(0, 0, MAPFX_IMPASS);
    AStar astar;
    waypoint_list_t wp = {};

    // ASSERT (not EXPECT): if a future change makes this fail AFTER both entry
    // checks pass, reset() has run and the get_path below would hit the nullptr
    // walk (AStar.cpp:218/221) instead of failing cleanly.
    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 0));
    EXPECT_TRUE(astar.get_path(2 * GRID + HALF, HALF, wp));

    EXPECT_EQ(wp._head, 1);
    EXPECT_FLOAT_EQ(wp._pos[0][0], 320.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 64.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
}

// Quirk pin — characterizes current behavior, do not fix silently: the
// destination entry check (AStar.cpp:78) tests tileHasBits and off-mesh but NOT
// isFanOff, and during the search the destination identity check
// (AStar.cpp:150-156) runs BEFORE the fan-off rejection (AStar.cpp:165-171), so a
// fan-off DESTINATION is reachable even though fan-off INTERMEDIATE tiles block
// the path (see AITerrainQueries.AStarTreatsFanOffTilesAsUnavailable).
TEST(AStarPathfinding, FanOffDestinationIsReachable)
{
    StubTerrain terrain(3, 1);
    terrain.setFanOff(2, 0);
    AStar astar;
    waypoint_list_t wp = {};

    // ASSERT (not EXPECT): the likely "fix" for this quirk — rejecting the fan-off
    // destination after the entry checks — would leave final_node null; EXPECT
    // would then fall through into get_path's nullptr walk (AStar.cpp:218/221).
    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 0));
    EXPECT_TRUE(astar.get_path(2 * GRID + HALF, HALF, wp));

    EXPECT_EQ(wp._head, 1);
}

// Quirk pin — characterizes current behavior, do not fix silently:
// (a) entry-check failures return before reset() (AStar.cpp:69-84 vs :111-112),
//     so a failed find_path leaves the previous path installed and get_path
//     happily replays it with fresh raw args;
// (b) get_path never clears the caller's list — it only pushes (AStar.cpp:283-286),
//     so the replayed waypoint APPENDS at _pos[1].
TEST(AStarPathfinding, FailedEntryCheckPreservesPreviousPath)
{
    StubTerrain terrain(4, 1);
    AStar astar;
    waypoint_list_t wp = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 3, 0));
    ASSERT_TRUE(astar.get_path(111, 222, wp));
    ASSERT_EQ(wp._head, 1);
    EXPECT_FLOAT_EQ(wp._pos[0][0], 111.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 222.0f);

    // dst (9,0) is OFF-MESH, so this fails the ENTRY check at AStar.cpp:78-84 and
    // returns BEFORE reset() at :111-112. This is the ONLY safe way to get a
    // failing find_path before get_path — a failure AFTER both entry checks pass
    // leaves final_node null and get_path would dereference it (see
    // UnreachableEnclosedDestinationReturnsFalse).
    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 9, 0));

    // SAME list: the stale path replays and appends.
    EXPECT_TRUE(astar.get_path(777, 666, wp));
    EXPECT_EQ(wp._head, 2);
    EXPECT_FLOAT_EQ(wp._pos[0][0], 111.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 222.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][0], 777.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][1], 666.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][2], 0.0f);
}

// Safe by construction: the ctor nulls both node pointers (AStar.cpp:37-40), so
// the chain walk at :215 is skipped (nullptr == start_node.get()), path_length
// stays 0, and "i = path_length - 1" wraps the size_t and converts to int -1 on
// this platform, skipping the emission loop (AStar.cpp:226) — returns false with
// the list untouched. Contrast with the FORBIDDEN case documented in
// UnreachableEnclosedDestinationReturnsFalse.
TEST(AStarPathfinding, GetPathWithoutAnyFindReturnsFalse)
{
    AStar astar;
    waypoint_list_t wp = {};

    EXPECT_FALSE(astar.get_path(100, 100, wp));
    EXPECT_EQ(wp._head, 0);
}

// CRASH HAZARD — do NOT add a get_path call here. Because both entry checks
// passed, reset() ran (AStar.cpp:112) and the exhausted search left
// final_node == nullptr with start_node set; get_path would store nullptr into
// node_path (AStar.cpp:218) and dereference current_node->parent (AStar.cpp:221)
// — segfault. This test pins the failure return only; the nullptr walk is pinned
// by omission. Characterizes current behavior — do not fix silently. (The search
// fails via open-queue exhaustion — roughly 45 closed keys, far under
// MAX_ASTAR_NODES 512 — so the boolean is deterministic regardless of pop order.)
TEST(AStarPathfinding, UnreachableEnclosedDestinationReturnsFalse)
{
    StubTerrain terrain(5, 5);
    terrain.setFX(2, 1, MAPFX_IMPASS);
    terrain.setFX(1, 2, MAPFX_IMPASS);
    terrain.setFX(3, 2, MAPFX_IMPASS);
    terrain.setFX(2, 3, MAPFX_IMPASS);
    // Destination (2,2) is fully enclosed but itself passable, so BOTH entry
    // checks pass.
    AStar astar;

    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 2));
    // ABSOLUTELY NO get_path CALL IN THIS TEST — see comment above.
}

// Quirk pin — characterizes current behavior, do not fix silently: the max-heap
// comparator (AStar.cpp:101-103) pops the FARTHEST-from-goal open node first. On a
// large open field with the goal 2 tiles away, the only weight-2 open nodes are the
// destination-adjacent ones, and they never pop while ANY farther node (weight > 2)
// remains open — the frontier floods outward until the MAX_ASTAR_NODES budget
// (AStar.cpp:123-127) kills the search. A min-heap comparator would trivially
// succeed, so this test flips loudly on a comparator "fix". Deterministic: the
// argument depends only on "some open node has weight > 2", never on tie order.
// NO get_path here — both entry checks passed and the search failed (see
// UnreachableEnclosedDestinationReturnsFalse).
TEST(AStarPathfinding, NearbyGoalOnLargeOpenFieldFailsViaWorstFirstPops)
{
    StubTerrain terrain(30, 30);
    AStar astar;

    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 15, 15, 17, 15));
}

// Quirk pin — characterizes current behavior, do not fix silently: the search
// budget MAX_ASTAR_NODES == 512 (AStar.hpp:70) counts closed-set KEYS, which are
// inserted at GENERATION (AStar.cpp:144-148) BEFORE the off-mesh/wall validity
// checks — in a 1-wide corridor that is roughly 3 keys per tile advanced (the
// forward neighbor plus both off-mesh sides), so the practical straight-line range
// is about 170 tiles, not 512. Deterministic: 1-wide forced chain; the single
// start-tile echo has a distinct weight. NO get_path after the failing call —
// both entry checks passed (see UnreachableEnclosedDestinationReturnsFalse).
TEST(AStarPathfinding, NodeBudgetLimitsStraightCorridorRangeToWellUnder512)
{
    StubTerrain terrain(180, 1);
    {
        AStar astar;
        EXPECT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 100, 0));
    }
    {
        AStar astar;
        EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 179, 0));
    }
}

// Quirk pin — characterizes current behavior, do not fix silently: the destination
// identity check (AStar.cpp:150-156) tests only GENERATED neighbors, never the
// popped tile itself, so src == dst:
// (a) FAILS on a 1x1 mesh — there are no on-mesh neighbors to generate;
// (b) SUCCEEDS on a 2x1 mesh via the out-and-back (0,0)->(1,0)->(0,0), possible
//     only because the start tile is not closed at initialization
//     (AStar.cpp:114-117) and so can be re-generated once as a neighbor. A change
//     that closes the start tile up front flips (b) loudly. get_path then emits the
//     single raw-args waypoint (the out-and-back nodes collapse: no both-axes
//     direction change ever fires on a 1-tall mesh).
TEST(AStarPathfinding, StartEqualsDestinationFailsOn1x1ButSucceedsViaOutAndBackOn2x1)
{
    {
        StubTerrain terrain(1, 1);
        AStar astar;
        EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 0, 0));
        // NO get_path — both entry checks passed and the search failed (see
        // UnreachableEnclosedDestinationReturnsFalse).
    }
    {
        StubTerrain terrain(2, 1);
        AStar astar;
        waypoint_list_t wp = {};

        ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 0, 0));
        EXPECT_TRUE(astar.get_path(HALF, HALF, wp));

        EXPECT_EQ(wp._head, 1);
        EXPECT_FLOAT_EQ(wp._pos[0][0], 64.0f);
        EXPECT_FLOAT_EQ(wp._pos[0][1], 64.0f);
        EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
    }
}

// Quirk pin — characterizes current behavior, do not fix silently: get_path's
// chain walk keeps only the LAST MAX_ASTAR_PATH == 128 nodes (AStar.cpp:215,
// AStar.hpp:71), so on this 145-node forced L-path the walk stops 128 nodes short
// of the start — at (5,13), far past the real corner (5,0). The first iterated
// node (5,13) differs from the start node on BOTH axes, so change_direction
// (AStar.cpp:237) fires immediately and emits a PHANTOM corner at the (5,13) tile
// center — a point where the real path runs dead straight — while the real
// corner's center (704, 64) never appears in the list.
TEST(AStarPathfinding, PathsBeyond128NodesEmitPhantomCornerFromTruncatedWalk)
{
    StubTerrain terrain(6, 141);
    // Wall everything except the L: row y=0 fully open (0,0)..(5,0), then the
    // 1-wide column x=5 open down to (5,140).
    for (int y = 1; y <= 140; ++y)
    {
        for (int x = 0; x <= 4; ++x)
        {
            terrain.setFX(x, y, MAPFX_IMPASS);
        }
    }
    AStar astar;
    waypoint_list_t wp = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 5, 140));
    EXPECT_TRUE(astar.get_path(5 * GRID + HALF, 140 * GRID + HALF, wp));

    EXPECT_EQ(wp._head, 2);
    // Phantom corner: tile center of (5,13) — 5*128+64, 13*128+64.
    EXPECT_FLOAT_EQ(wp._pos[0][0], 704.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][1], 1728.0f);
    EXPECT_FLOAT_EQ(wp._pos[0][2], 0.0f);
    // Raw destination args; the real corner center (704, 64) is nowhere.
    EXPECT_FLOAT_EQ(wp._pos[1][0], 704.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][1], 17984.0f);
    EXPECT_FLOAT_EQ(wp._pos[1][2], 0.0f);
}

} // namespace
