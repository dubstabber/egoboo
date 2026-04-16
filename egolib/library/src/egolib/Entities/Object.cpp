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

/// @file egolib/game/Entities/Object.hpp
/// @details An object representing instances of in-game egoboo objects (Object)
/// @author Johan Jansen

#include "egolib/Entities/Object_internal.h"

//Declare class static constants
const std::shared_ptr<Object> Object::INVALID_OBJECT = nullptr;

/// @brief Ouf-of-class definition for GCC & Clang.
/// @todo Remove this if GCC & Clang are fixed.
constexpr float Object::DROPZVEL;

/// @brief Out-of-class definition for GCC/Clang.
/// @todo Remove this if GCC & Clang are fixed.
constexpr float Object::DISMOUNTZVEL;

Team& Object::getTeam() const
{
    return activeModule().getTeamList()[team];
}

bool Object::canMount(const std::shared_ptr<Object> mount) const
{
    //Cannot mount ourselves!
    if(this == mount.get())
    {
        return false;
    }

    //Make sure they are a mount and alive
    if(!mount->isMount() || !mount->isAlive())
    {
        return false;
    }

    //We must be alive and not an item to become a rider
    if(!isAlive() || isitem || isBeingHeld())
    {
        return false;
    }

    //Cannot mount while flying
    if(isFlying())
    {
        return false;
    }

    //Make sure they aren't mounted already
    if(!mount->getProfile()->isSlotValid(SLOT_LEFT) || activeModule().getObjectHandler().exists(mount->holdingwhich[SLOT_LEFT]))
    {
        return false;
    }

    //We need a riding animation to be able to mount stuff
    int action_mi = getProfile()->getModel()->getAction(ACTION_MI);
    bool has_ride_anim = ( ACTION_COUNT != action_mi && !ACTION_IS_TYPE( action_mi, D ) );

    return has_ride_anim;
}
