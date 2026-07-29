#include "gtest/gtest.h"

#include "egolib/AI/AStar.hpp"
#include "egolib/AI/LineOfSight.hpp"
#include "egolib/AI/WaypointList.h"
#include "egolib/FileFormats/map_fx.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/Mesh/ITerrainQuery.hpp"
#include "egolib/Mesh/Info.hpp"

#include "StubTerrain.hpp"

#include <cstdint>
#include <vector>

namespace
{

using Ego::Test::StubTerrain;

line_of_sight_info_t horizontalLine(float startTileX, float endTileX)
{
    const float gridSize = Info<float>::Grid::Size();
    line_of_sight_info_t line = {};
    line.x0 = startTileX * gridSize + gridSize * 0.5f;
    line.y0 = gridSize * 0.5f;
    line.z0 = 0.0f;
    line.x1 = endTileX * gridSize + gridSize * 0.5f;
    line.y1 = gridSize * 0.5f;
    line.z1 = 0.0f;
    line.stopped_by = MAPFX_WALL;
    return line;
}

TEST(AITerrainQueries, LineOfSightReportsClearPath)
{
    StubTerrain terrain(4, 1);
    line_of_sight_info_t line = horizontalLine(0.0f, 3.0f);

    EXPECT_FALSE(line_of_sight_info_t::blocked(line, terrain));
}

TEST(AITerrainQueries, LineOfSightReportsBlockedTileAndCollisionData)
{
    StubTerrain terrain(4, 1);
    terrain.setFX(2, 0, MAPFX_WALL);
    line_of_sight_info_t line = horizontalLine(0.0f, 3.0f);

    EXPECT_TRUE(line_of_sight_info_t::blocked(line, terrain));
    EXPECT_EQ(line.collide_x, 2);
    EXPECT_EQ(line.collide_y, 0);
    EXPECT_EQ(line.collide_fx, MAPFX_WALL);
}

TEST(AITerrainQueries, AStarRejectsInvalidOrBlockedEndpoints)
{
    StubTerrain terrain(3, 3);
    terrain.setFX(2, 1, MAPFX_IMPASS);
    AStar astar;

    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, -1, 0, 1, 0));
    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 1));
    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 3, 0));
}

TEST(AITerrainQueries, AStarFindsPathAroundBlockedTile)
{
    StubTerrain terrain(3, 3);
    terrain.setFX(1, 1, MAPFX_IMPASS);
    AStar astar;
    waypoint_list_t waypoints = {};

    ASSERT_TRUE(astar.find_path(terrain, MAPFX_IMPASS, 0, 1, 2, 1));
    EXPECT_TRUE(astar.get_path(2 * Info<int>::Grid::Size(), Info<int>::Grid::Size(), waypoints));
    EXPECT_FALSE(waypoint_list_t::empty(waypoints));
}

TEST(AITerrainQueries, AStarTreatsFanOffTilesAsUnavailable)
{
    StubTerrain terrain(3, 1);
    terrain.setFanOff(1, 0);
    AStar astar;

    EXPECT_FALSE(astar.find_path(terrain, MAPFX_IMPASS, 0, 0, 2, 0));
}

} // namespace
