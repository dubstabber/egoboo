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

/// @file egolib/game/Shop.cpp
/// @brief Shop interaction
#include "egolib/game/Shop.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/game.h"

#include <memory>

namespace
{
GameModule& activeModule()
{
    return GameSessionContext::get().activeModule();
}

auto& objectHandler()
{
    return activeModule().getObjectHandler();
}

std::shared_ptr<Object> objectHandle(ObjectRef objectRef)
{
    return objectHandler()[objectRef];
}

IScriptable& scriptable(Object& object)
{
    return object;
}

void publishShopOrder(IScriptable& shopKeeper, uint32_t value, uint16_t orderCounter)
{
    shopKeeper.addAIOrder(value, orderCounter);
}

void publishTheftTarget(IScriptable& shopKeeper, ObjectRef thiefRef)
{
    shopKeeper.setAITarget(thiefRef);
}
}

bool Shop::drop(ObjectRef dropperRef, ObjectRef itemRef)
{
    std::shared_ptr<Object> dropper = objectHandle(dropperRef);
    std::shared_ptr<Object> item = objectHandle(itemRef);
    if (!dropper || !item) return false;
    if (dropper->isTerminated() || item->isTerminated()) return false;

    bool inShop = false;
    if (item->isItem())
    {
        ObjectRef ownerRef = activeModule().getShopOwner(item->getPosX(), item->getPosY());
        if (objectHandler().exists(ownerRef))
        {
            Object *owner = objectHandler().get(ownerRef);

            inShop = true;

            int price = item->getPrice();

            // Are they are trying to sell junk or quest items?
            if (0 == price)
            {
                publishShopOrder(scriptable(*owner), static_cast<uint32_t>(price), Passage::SHOP_BUY);
            }
            else
            {
                dropper->giveMoney(price);
                owner->giveMoney(-price);

                publishShopOrder(scriptable(*owner), static_cast<uint32_t>(price), Passage::SHOP_BUY);
            }
        }
    }

    return inShop;
}

bool Shop::buy(ObjectRef buyerRef, ObjectRef itemRef)
{
    std::shared_ptr<Object> buyer = objectHandle(buyerRef);
    std::shared_ptr<Object> item = objectHandle(itemRef);
    if (!buyer || !item) return false;
    if (buyer->isTerminated() || item->isTerminated()) return false;

    bool canGrab = true;
    if (item->isItem())
    {
        ObjectRef ownerRef = activeModule().getShopOwner(item->getPosX(), item->getPosY());
        if (objectHandler().exists(ownerRef))
        {
            Object *owner = objectHandler().get(ownerRef);

            //in_shop = true;
            int price = item->getPrice();

            if (buyer->getMoney() >= price)
            {
                // Okay to sell
                publishShopOrder(scriptable(*owner), static_cast<uint32_t>(price), Passage::SHOP_SELL);

                buyer->giveMoney(-price);
                owner->giveMoney(price);

                canGrab = true;
            }
            else
            {
                // Don't allow purchase
                publishShopOrder(scriptable(*owner), static_cast<uint32_t>(price), Passage::SHOP_NOAFFORD);
                canGrab = false;
            }
        }
    }

    /// @note some of these are handled in scripts, so they could be disabled
    // print some feedback messages
    /*
    if (can_grab)
    {
        if (in_shop)
        {
            if (can_pay)
            {
                DisplayMsg_printf("%s bought %s", chr_get_name(ipicker, CHRNAME_ARTICLE | CHRNAME_DEFINITE | CHRNAME_CAPITAL), chr_get_name(iitem, CHRNAME_ARTICLE));
            }
            else
            {
                DisplayMsg_printf("%s can't afford %s", chr_get_name(ipicker, CHRNAME_ARTICLE | CHRNAME_DEFINITE | CHRNAME_CAPITAL), chr_get_name(iitem, CHRNAME_ARTICLE));
            }
        }
        else
        {
            DisplayMsg_printf("%s picked up %s", chr_get_name(ipicker, CHRNAME_ARTICLE | CHRNAME_DEFINITE | CHRNAME_CAPITAL), chr_get_name( iitem, CHRNAME_ARTICLE));
        }
    }
    */

    return canGrab;
}

bool Shop::steal(ObjectRef thiefRef, ObjectRef itemRef)
{
    std::shared_ptr<Object> thief = objectHandle(thiefRef);
    std::shared_ptr<Object> item = objectHandle(itemRef);
    if (!thief || !item) return false;
    if (thief->isTerminated() || item->isTerminated()) return false;

    bool canSteal = true;
    if (item->isItem())
    {
        ObjectRef ownerRef = activeModule().getShopOwner(item->getPosX(), item->getPosY());
        if (objectHandler().exists(ownerRef))
        {
            int detection = Random::getPercent();
            Object *owner = objectHandler().get(ownerRef);

            canSteal = true;
            if (owner->canSeeObject(thief->getObjRef()) || detection <= 5 || (detection - thief->getAttribute(Ego::Attribute::AGILITY) + owner->getAttribute(Ego::Attribute::INTELLECT)) > 50)
            {
                publishShopOrder(scriptable(*owner), Passage::SHOP_STOLEN, Passage::SHOP_THEFT);
                publishTheftTarget(scriptable(*owner), thief->getObjRef());
                canSteal = false;
            }
        }
    }

    return canSteal;
}

bool Shop::canGrabItem(ObjectRef grabberRef, ObjectRef itemRef)
{
    std::shared_ptr<Object> grabber = objectHandle(grabberRef);
    std::shared_ptr<Object> item = objectHandle(itemRef);
    if (!grabber || !item) return false;
    if (grabber->isTerminated() || item->isTerminated()) return false;
    // Assume there is no shop so that the character can grab anything.
    bool canGrab = true;

    // check if we are doing this inside a shop
    ObjectRef iShopKeeper = activeModule().getShopOwner(item->getPosX(), item->getPosY());
    Object *shopKeeper = objectHandler().get(iShopKeeper);
    if (INGAME_PCHR(shopKeeper))
    {
        // check for a stealthy pickup
        bool isInvisible = !shopKeeper->canSeeObject(grabber->getObjRef());

        // pets are automatically stealthy
        bool canSteal = isInvisible || grabber->isItem();

        if (canSteal)
        {
            canGrab = Shop::steal(grabberRef, itemRef);

            if (!canGrab)
            {
                DisplayMsg_printf("%s was detected!!", grabber->getName().c_str());
            }
            else
            {
                DisplayMsg_printf("%s stole %s", grabber->getName().c_str(), item->getName(true, false, false).c_str());
            }
        }
        else
        {
            canGrab = Shop::buy(grabberRef, itemRef);
        }
    }

    return canGrab;
}
