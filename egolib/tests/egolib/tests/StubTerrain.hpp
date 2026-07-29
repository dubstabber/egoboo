#pragma once

#include "gtest/gtest.h"

#include "egolib/FileFormats/map_fx.hpp"
#include "egolib/Mesh/ITerrainQuery.hpp"

#include <cstdint>
#include <vector>

namespace Ego::Test
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

} // namespace Ego::Test
