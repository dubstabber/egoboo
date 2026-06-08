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

#include "egolib/game/Physics/Collidable.hpp"

#include "egolib/game/Core/GameSessionContext.hpp"
// setPosition/setSpawnPosition need the full GameModule definition
// (isInside/getMeshPointer). GameSessionContext.hpp only forward-declares it,
// and Collidable.hpp no longer drags Module.hpp in transitively.
#include "egolib/game/Module/Module.hpp"

namespace
{
GameModule& activeModule()
{
    return GameSessionContext::get().activeModule();
}
}

namespace Ego
{
namespace Physics
{

bool Collidable::setPosition(const Vector3f& pos)
{
    EGO_DEBUG_VALIDATE(pos);

    // Never allow positions outside the map.
    if (!activeModule().isInside(pos.x(), pos.y()))
    {
        return false;
    }

    if (pos == _position)
    {
        return false;
    }

    _oldPosition = _position;
    _position = pos;

    _tile = activeModule().getMeshPointer()->getTileIndex(Vector2f(getPosX(), getPosY()));

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
    assert(activeModule().isInside(pos.x(), pos.y()));
    _spawnPosition = pos;
}

} // namespace Physics
} // namespace Ego
