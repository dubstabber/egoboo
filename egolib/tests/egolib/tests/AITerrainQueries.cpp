#include "gtest/gtest.h"

#include "egolib/AI/AStar.hpp"
#include "egolib/AI/LineOfSight.hpp"
#include "egolib/AI/WaypointList.h"
#include "egolib/FileFormats/map_fx.hpp"
#include "egolib/FileFormats/map_file.h"
#include "egolib/Mesh/ITerrainQuery.hpp"
#include "egolib/Mesh/Info.hpp"

#include <cstdint>
#include <vector>

namespace
{

class StubTerrain : public Ego::Mesh::ITerrainQuery
{
public:
    StubTerrain(int width, int height) :
        _width(width),
        _height(height),
        _fx(static_cast<size_t>(width * height), 0),
        _fanOff(static_cast<size_t>(width * height), false)
    {}

    Index1D getTileIndex(const Index2D& tile) const override
    {
        if (tile.x() < 0 || tile.y() < 0 || tile.x() >= _width || tile.y() >= _height)
        {
            return Index1D::Invalid;
        }

        return Index1D(tile.x() + tile.y() * _width);
    }

    uint32_t testFX(const Index1D& tile, uint32_t flags) const override
    {
        if (!isValid(tile))
        {
            return flags & (MAPFX_WALL | MAPFX_IMPASS);
        }
        if (_fanOff[static_cast<size_t>(tile.i())])
        {
            return 0;
        }
        return _fx[static_cast<size_t>(tile.i())] & flags;
    }

    bool tileHasBits(const Index2D& tile, uint32_t flags) const override
    {
        const Index1D index = getTileIndex(tile);
        if (!isValid(index))
        {
            return 0 != (flags & (MAPFX_WALL | MAPFX_IMPASS));
        }

        return 0 != (_fx[static_cast<size_t>(index.i())] & flags);
    }

    bool isFanOff(const Index1D& tile) const override
    {
        return isValid(tile) && _fanOff[static_cast<size_t>(tile.i())];
    }

    void setFX(int x, int y, uint32_t flags)
    {
        const Index1D index = getTileIndex(Index2D(x, y));
        ASSERT_TRUE(isValid(index));
        _fx[static_cast<size_t>(index.i())] = flags;
    }

    void setFanOff(int x, int y)
    {
        const Index1D index = getTileIndex(Index2D(x, y));
        ASSERT_TRUE(isValid(index));
        _fanOff[static_cast<size_t>(index.i())] = true;
    }

private:
    bool isValid(const Index1D& tile) const
    {
        return !(Index1D::Invalid == tile) &&
               tile.i() >= 0 &&
               tile.i() < static_cast<int>(_fx.size());
    }

    int _width;
    int _height;
    std::vector<uint32_t> _fx;
    std::vector<bool> _fanOff;
};

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
