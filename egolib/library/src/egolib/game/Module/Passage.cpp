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

/// @file egolib/game/Module/Passage.cpp
/// @brief A scanner for the passage file for a given module ( /modules/*.mod/basicdat/passages.txt )
/// @author Johan Jansen

#include "egolib/game/Module/Passage.hpp"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/game/game.h"
#include "egolib/game/mesh.h"
#include "egolib/Entities/_Include.hpp"

namespace
{
void publishCrushedAlert(IScriptable& object)
{
    object.addAIAlertBits(ALERTIF_CRUSHED);
}
}

const ObjectRef Passage::SHOP_NOOWNER = ObjectRef::Invalid;

Passage::Passage(ego_mesh_t &mesh, ObjectHandler &objectHandler, const int x0, const int y0, const int x1, const int y1, const uint8_t mask) :
    _mesh(mesh),
    _objectHandler(objectHandler),
    _area(Ego::Point2f(x0 * Info<float>::Grid::Size(), y0 * Info<float>::Grid::Size()),
          Ego::Point2f((x1+1) * Info<float>::Grid::Size(), (y1+1) * Info<float>::Grid::Size())),
    _music(NO_MUSIC),
    _mask(mask),
    _open(true),
    _isShop(false),
    _shopOwner(SHOP_NOOWNER),
    _passageFans()
{
    //Build the list of all tiles contained within this passage
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            _passageFans.push_back(mesh.getTileIndex(Index2D(x, y)));
        }
    }
}

bool Passage::isOpen() const
{
    return _open;
}

void Passage::open()
{
    //no need to do this if it already is open
    if (isOpen()) {
       return; 
    } 
    _open = true;

    //clear impassable and wall bits
    for(const Index1D &fan : _passageFans) {
        _mesh.clear_fx(fan, MAPFX_WALL | MAPFX_IMPASS);
    }
}

bool Passage::close()
{
    //is it already closed?
    if(!isOpen()) {
        return true;
    }

    // don't compute all of this for nothing
    if (EMPTY_BIT_FIELD == _mask) {
        return true;
    }

    // check to see if a wall can close
    if (0 != HAS_SOME_BITS(_mask, MAPFX_IMPASS | MAPFX_WALL))
    {
        ObjectHandler& objectHandler = _objectHandler;
        std::vector<ObjectRef> crushedCharacters;

        // Make sure it isn't blocked
        for(const ObjectRef& objectRef : objectHandler.objectRefIterator())
        {
            Object* object = objectHandler.get(objectRef);
            if (object == nullptr)
            {
                continue;
            }

            //Scenery can neither be crushed nor prevents doors from closing
            if(object->isScenery()) {
                continue;
            }

            if (object->canCollide())
            {
                if (objectIsInPassage(*object))
                {
                    if (!object->canBeCrushed() || (object->isAlive() && object->getProfile()->canOpenStuff()))
                    {
                        // Someone is blocking who can open stuff, stop here
                        return false;
                    }
                    else
                    {
                        crushedCharacters.push_back(object->getObjRef());
                    }
                }
            }
        }

        // Crush any unfortunate characters
        for(const ObjectRef& characterRef : crushedCharacters) {
            Object* character = objectHandler.get(characterRef);
            if (character == nullptr)
            {
                continue;
            }
            publishCrushedAlert(*character);
        }
    }

    // Close it off
    _open = false;
    for(const Index1D &fan : _passageFans) {
        _mesh.add_fx(fan, _mask);
    }
 
    return true;    
}

bool Passage::objectIsInPassage(const IPhysical& object) const
{
    return idlib::is_intersecting(_area, object.getAxisAlignedBox2D());
}

ObjectRef Passage::whoIsBlockingPassage( ObjectRef objRef, const IDSZ2& idsz, const BIT_FIELD targeting_bits, const IDSZ2& require_item ) const
{
    // Skip if the one who is looking doesn't exist
    ObjectHandler& objectHandler = _objectHandler;
    if ( !objectHandler.exists(objRef) ) return ObjectRef::Invalid;
    Object *psrc = objectHandler.get(objRef);

    // Look at each character
    for(const ObjectRef& characterRef : objectHandler.objectRefIterator())
    {
        Object* pchr = objectHandler.get(characterRef);
        if (pchr == nullptr)
        {
            continue;
        }

        if(pchr->isTerminated()) {
            continue;
        }

        // dont do scenery objects unless we allow items
        if (!HAS_SOME_BITS(targeting_bits, TARGET_ITEMS) && pchr->isScenery()) continue;

        //Check if the object has the requirements
        if ( !chr_check_target( psrc, *pchr, idsz, targeting_bits ) ) continue;

        //Now check if it actually is inside the passage area
        if (objectIsInPassage(*pchr))
        {

            // Found a live one, do we need to check for required items as well?
            if ( IDSZ2::None == require_item )
            {
                return pchr->getObjRef();
            }

            // It needs to have a specific item as well
            else
            {
                // I: Check hands
                if(pchr->wieldsItemIDSZ(require_item)) {
                    return pchr->getObjRef();
                }

                // II: Check the pack
                for (size_t slot = 0; slot < pchr->getInventoryMaxItems(); ++slot)
                {
                    const ObjectRef itemRef = pchr->getInventoryItemRef(slot);
                    const Object* pitem = objectHandler.get(itemRef);
                    if (pitem == nullptr)
                    {
                        continue;
                    }

                    if ( pitem->getProfile()->hasTypeIDSZ(require_item) )
                    {
                        // It has the required item in inventory...
                        return pchr->getObjRef();
                    }
                }
            }
        }
    }

    // No characters found
    return ObjectRef::Invalid;
}

void Passage::flashColor(uint8_t color)
{
    for(const Index1D &fan : _passageFans)
    {
        ego_tile_info_t& ptile = _mesh.getTileInfo(fan);

        for (size_t cnt = 0; cnt < 4; ++cnt)
        {
            // set the color
            ptile._lightingCache._contents[cnt] = color;

            // force the lighting code to update
            ptile._vertexLightingCache.setNeedUpdate(true);
            ptile._vertexLightingCache._lastFrame = -1;
        }
    }
}

bool Passage::isPointInside(float xpos, float ypos) const
{
    return xpos >= _area.get_min().x() && 
           xpos <= _area.get_max().x() &&
           ypos >= _area.get_min().y() && 
           ypos <= _area.get_max().y();
}

bool Passage::checkPassageMusic(const IPhysical& object, IAudioSystem& audioSystem) const
{
    if (_music == INVALID_SOUND_ID) {
       return false; 
    } 

    if(!objectIsInPassage(object)) {
        return false;
    }

    // character is inside, start music track
    audioSystem.playMusic(_music);
    return true;
}

void Passage::setMusic(const int32_t musicID)
{
    _music = musicID;
}

bool Passage::isShop() const
{
    return ObjectRef::Invalid != _shopOwner;
}

ObjectRef Passage::getShopOwner() const {
    return _shopOwner;
}

void Passage::makeShop(ObjectRef owner)
{
    ObjectHandler& objectHandler = _objectHandler;

    //Make sure owner is valid
    const Object* powner = objectHandler.get(owner);
    if ( powner == nullptr || powner->isTerminated() || !powner->isAlive() ) return;

    //Mark as shop
    _isShop = true;
    _shopOwner = owner;

    // flag every item in the shop as a shop item
    for(const ObjectRef& objectRef : objectHandler.objectRefIterator())
    {
        Object* object = objectHandler.get(objectRef);
        if (object == nullptr)
        {
            continue;
        }

        if (object->isTerminated()) continue;

        if ( object->isItem() )
        {
            if (objectIsInPassage(*object))
            {
                object->setShopItem(true);               // Full value
                object->setKursed(false);                // Shop items are never kursed
                object->setNameKnown(true);              // Identify it!
            }
        }
    }    
}

void Passage::removeShop()
{
    if(!isShop()) {
        return;
    }

    _isShop = false;
    _shopOwner = SHOP_NOOWNER;
}

const Ego::AxisAlignedBox2f& Passage::getAxisAlignedBox2f() const
{
    return _area;
}
