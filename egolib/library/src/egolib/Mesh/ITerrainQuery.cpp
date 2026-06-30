/// @file egolib/Mesh/ITerrainQuery.cpp
/// @brief Ownership of the installed active terrain-query surface.

#include "egolib/Mesh/ITerrainQuery.hpp"

#include <stdexcept>

namespace Ego
{
namespace Mesh
{

namespace
{
ITerrainQuery* g_activeTerrainQuery = nullptr;
}

void installTerrainQuery(ITerrainQuery* terrain)
{
    g_activeTerrainQuery = terrain;
}

void clearTerrainQuery()
{
    g_activeTerrainQuery = nullptr;
}

ITerrainQuery* tryActiveTerrainQuery()
{
    return g_activeTerrainQuery;
}

ITerrainQuery& activeTerrainQuery()
{
    if (!g_activeTerrainQuery)
    {
        throw std::logic_error("no active terrain query");
    }
    return *g_activeTerrainQuery;
}

} // namespace Mesh
} // namespace Ego
