//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

#include "egolib/Physics/Collidable.hpp"

// Position validation (bounds check + tile lookup) goes through the lower-layer collision-world
// seam rather than reaching up into the game-layer GameModule. The game session installs the
// active GameModule as the ICollisionWorld for the module's lifetime. This keeps Collidable.cpp
// free of any game/ dependency, completing the lower-layer extraction of the Collidable base.
#include "egolib/Physics/ICollisionWorld.hpp"
#include "egolib/Debug.hpp"  // EGO_DEBUG_VALIDATE

#include <cassert>

namespace Ego
{
namespace Physics
{

bool Collidable::setPosition(const Vector3f& pos)
{
    EGO_DEBUG_VALIDATE(pos);

    // Never allow positions outside the map.
    if (!activeCollisionWorld().isInside(pos.x(), pos.y()))
    {
        return false;
    }

    if (pos == _position)
    {
        return false;
    }

    _oldPosition = _position;
    _position = pos;

    _tile = activeCollisionWorld().getTileIndex(Vector2f(getPosX(), getPosY()));

    Vector2f nrm;
    float pressure = 0.0f;
    BIT_FIELD hit_a_wall = hit_wall(nrm, &pressure);
    if (EMPTY_BIT_FIELD == hit_a_wall && 0.0f <= pressure)
    {
        setSafePosition(getPosition());
    }

    return true;
}

void Collidable::setSpawnPosition(const Vector3f& pos)
{
    assert(activeCollisionWorld().isInside(pos.x(), pos.y()));
    _spawnPosition = pos;
}

} // namespace Physics
} // namespace Ego
