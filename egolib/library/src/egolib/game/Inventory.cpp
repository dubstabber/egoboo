#include "egolib/game/Inventory.hpp"
#include "egolib/Entities/_Include.hpp"

#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"

//Class constants
const size_t Inventory::MAXNUMINPACK;

namespace
{
auto& objectHandler()
{
    return GameSessionContext::get().activeModule().getObjectHandler();
}
}

Inventory::Inventory() :
    _items()
{
    //ctor
}

ObjectRef Inventory::findItem(Object *pobj, const IDSZ2& idsz, bool equippedOnly) {
	if (!pobj || pobj->isTerminated()) {
		return ObjectRef::Invalid;
	}

    ObjectRef result = ObjectRef::Invalid;

    for (const std::shared_ptr<Object>& pitem : pobj->getInventoryItems())
    {
        bool matches_equipped = (!equippedOnly || pitem->isEquipped());

        if (pitem->getProfile()->hasTypeIDSZ(idsz) && matches_equipped)
        {
            result = pitem->getObjRef();
            break;
        }
    }

    return result;
}

ObjectRef Inventory::findItem(ObjectRef iowner, const IDSZ2& idsz, bool equippedOnly)
{
    if (!objectHandler().exists(iowner))
    {
        return ObjectRef::Invalid;
    }
    return findItem(objectHandler().get(iowner), idsz, equippedOnly);
}

//--------------------------------------------------------------------------------------------
bool Inventory::add_item( ObjectRef iowner, ObjectRef iitem, uint8_t inventorySlot, bool ignoreKurse )
{
    // Are owner and item valid?
	if (!objectHandler().exists(iowner) || !objectHandler().exists(iitem)) {
		return false;
	}
	Object *powner = objectHandler().get(iowner);
    const std::shared_ptr<Object> &pitem = objectHandler()[iitem];

    // Does the owner have free slot in her inventory?
    if (inventorySlot >= powner->getInventoryMaxItems()) {
        return false;
    }

    // If there is an item in the slot, do nothing.
	if (powner->getInventoryItem(inventorySlot)) {
		return false;
	}

    // Don't allow sub-inventories.
	if (pitem->isInsideInventory()) {
		return false;
	}

    // Kursed?
	if (pitem->isKursed() && !ignoreKurse)
	{
		// Flag the item as not put away.
		pitem->addAIAlertBits(ALERTIF_NOTPUTAWAY);
		if (powner->isPlayer()) DisplayMsg_printf("%s is sticky...", pitem->getName().c_str());
		return false;
	}

    // too big item?
	if (pitem->getProfile()->isBigItem())
	{
		pitem->addAIAlertBits(ALERTIF_NOTPUTAWAY);
		if (powner->isPlayer()) DisplayMsg_printf("%s is too big to be put away...", pitem->getName().c_str());
		return false;
	}

    // Check if item can be stacked on other items.
    ObjectRef stack = Inventory::hasStack(iitem, iowner);
    if ( objectHandler().exists( stack ) )
    {
        // We found a similar, stackable item in the inventory.
        Object *pstack = objectHandler().get( stack );

        // Reveal the name of the item or the stack.
		if (pitem->isNameKnown() || pstack->getProfile()->isNameKnown())
		{
			pitem->setNameKnown(true);
			pstack->setNameKnown(true);
		}

        // Reveal the usage of the item or the stack.
		if (pitem->getProfile()->isUsageKnown() || pstack->getProfile()->isUsageKnown())
		{
			pitem->getProfile()->makeUsageKnown();
			pstack->getProfile()->makeUsageKnown();
		}

        // Add the item ammo to the stack.
        int newammo = pitem->getAmmo() + pstack->getAmmo();
		if (newammo <= pstack->getAmmoMax())
		{
			// All transfered, so kill the in hand item
			pstack->setAmmo(newammo);

			pitem->requestTerminate();
			return true;
		}
        else
        {
            // Only some were transfered,
            pitem->setAmmo(pitem->getAmmo() + pstack->getAmmo() - pstack->getAmmoMax());
            pstack->setAmmo(pstack->getAmmoMax());
            powner->addAIAlertBits(ALERTIF_TOOMUCHBAGGAGE);
        }
    }
    else
    {
        //@todo: implement weight check here
        // Make sure we have room for another item
        //if ( pchr_pack->count >= Inventory::MAXNUMINPACK )
        // {
        //    SET_BIT( powner->ai.alert, ALERTIF_TOOMUCHBAGGAGE );
        //    return false;
        //}

        // Take the item out of hand
        pitem->detatchFromHolder(true, false);

        // clear the dropped flag
        pitem->clearAIAlertBits(ALERTIF_DROPPED);

        //Do not trigger dismount logic on putting items into inventory
        pitem->setDismountObject(ObjectRef::Invalid);
        pitem->setDismountTimer(0);

        //now put the item into the inventory
        pitem->setHolderRef(ObjectRef::Invalid);
        pitem->setInventoryHolderRef(iowner);
        powner->setInventoryItem(inventorySlot, pitem);

        // fix the flags
		if (pitem->getProfile()->isEquipment())
		{
			pitem->addAIAlertBits(ALERTIF_PUTAWAY);  // same as ALERTIF_ATLASTWAYPOINT;
		}

        //@todo: add in the equipment code here
    }

    return true;
}

bool Inventory::swap_item( ObjectRef iobj, uint8_t inventory_slot, const slot_t grip_off, const bool ignorekurse )
{
    //valid character?
    const std::shared_ptr<Object> &pobj = objectHandler()[iobj];
    if(!pobj) {
        return false;
    }

    //Validate slot number
    if(inventory_slot >= pobj->getInventoryMaxItems()) {
        return false;
    }

    // Make sure everything is hunkydori
    if (pobj->isItem() || pobj->isInsideInventory()) return false;

    const std::shared_ptr<Object> &inventory_item = pobj->getInventoryItem(inventory_slot);
    const std::shared_ptr<Object> &item           = objectHandler()[pobj->getHeldObject(static_cast<slot_t>(grip_off))];

    //Nothing to do?
    if(!item && !inventory_item) {
        return true;
    }

    //Check if either item is kursed first
    if(!ignorekurse) 
    {
        if(item && item->isKursed()) {
            // Flag the last found_item as not put away
            item->addAIAlertBits(ALERTIF_NOTPUTAWAY);  // Same as ALERTIF_NOTTAKENOUT
            if ( pobj->isPlayer() ) DisplayMsg_printf("%s is sticky...", item->getName().c_str());
            return false;

        }

        if(inventory_item && inventory_item->isKursed()) {
            // Flag the last found_item as not removed
            inventory_item->addAIAlertBits(ALERTIF_NOTTAKENOUT);  // Same as ALERTIF_NOTPUTAWAY
            if ( pobj->isPlayer() ) DisplayMsg_printf( "%s won't go out!", inventory_item->getName().c_str() );
            return false;

        }
    }

    //remove existing item from inventory and into the character's hand
    if (inventory_item) {
        pobj->removeInventoryItem(inventory_item, ignorekurse);

        inventory_item->attachToObject(pobj, grip_off == SLOT_RIGHT ? GRIP_RIGHT : GRIP_LEFT);

        //fix flags
        inventory_item->clearAIAlertBits(ALERTIF_GRABBED);
        inventory_item->addAIAlertBits(ALERTIF_TAKENOUT);
    }

    //put the new item in the inventory
    if (item) {
        add_item(pobj->getObjRef(), item->getObjRef(), inventory_slot, ignorekurse);
    }

    return true;
}

bool Inventory::remove_item( ObjectRef iholder, const size_t inventory_slot, const bool ignorekurse )
{
    const std::shared_ptr<Object> &holder = objectHandler()[iholder];
    if(!holder) {
        return false;
    }

    return holder->removeInventoryItem(holder->getInventoryItem(inventory_slot), ignorekurse);
}

ObjectRef Inventory::hasStack( const ObjectRef item, const ObjectRef character )
{
    bool found  = false;
    ObjectRef istack = ObjectRef::Invalid;

    std::shared_ptr<Object> pitem = objectHandler()[item];
    if(!pitem) {
        return ObjectRef::Invalid;
    }

    //Only check items that are actually stackable
    if(!pitem->getProfile()->isStackable()) {
        return ObjectRef::Invalid;
    }

    for (const std::shared_ptr<Object>& pstack : objectHandler().get(character)->getInventoryItems())
    {

        found = pstack->getProfile()->isStackable();

        if ( pstack->getAmmo() >= pstack->getAmmoMax() )
        {
            found = false;
        }

        // you can still stack something even if the profiles don't match exactly,
        // but they have to have all the same IDSZ properties
        if ( found && ( pstack->getProfile()->getSlotNumber() != pitem->getProfileID() ) )
        {
            for ( uint16_t id = 0; id < IDSZ_COUNT && found; id++ )
            {
                if ( pstack->getProfile()->getIDSZ(id) != pitem->getProfile()->getIDSZ(id) )
                {
                    found = false;
                }
            }
        }

        if ( found )
        {
            istack = pstack->getObjRef();
            break;
        }
    }

    return istack;
}

ObjectRef Inventory::getItemID(const size_t slotNumber) const
{
    std::shared_ptr<Object> item = getItem(slotNumber);
    if(!item) {
        return ObjectRef::Invalid;
    }
    return item->getObjRef();
}

std::shared_ptr<Object> Inventory::getItem(const size_t slotNumber) const
{
    if(slotNumber >= _items.size()) {
        return Object::INVALID_OBJECT;
    }

    std::shared_ptr<Object> item = _items[slotNumber].lock();
    if(item && item->isTerminated()) {
        //_items[slotNumber].reset();
        return Object::INVALID_OBJECT;
    }

    return item;
}

void Inventory::setItem(const size_t slotNumber, const std::shared_ptr<Object> &item)
{
    _items[slotNumber] = item;
}

std::vector<std::shared_ptr<Object>> Inventory::iterate() const
{
    std::vector<std::shared_ptr<Object>> result;
    for(const std::weak_ptr<Object> &weak : _items)
    {
        std::shared_ptr<Object> item = weak.lock();
        if(item && !item->isTerminated()) {
            result.push_back(item);
        }
    }
    return result;
}

size_t Inventory::getFirstFreeSlotNumber() const
{
    for(size_t i = 0; i < _items.size(); ++i)
    {
        if(!_items[i].lock()) {
            return i;
        }
    }

    return _items.size();
}

bool Inventory::removeItem(const std::shared_ptr<Object> &item, const bool ignorekurse)
{
    //Empty or invalid items always returns false
    if(!item) {
        return false;
    }

    for(std::weak_ptr<Object> &inventoryItem : _items)
    {
        //Is this the item we are looking for?
        if(inventoryItem.lock() == item)
        {
            //is it kursed?
            if (item->isKursed() && !ignorekurse)
            {
                //Flag the item as not removed
                item->addAIAlertBits(ALERTIF_NOTTAKENOUT);  // Same as ALERTIF_NOTPUTAWAY
                DisplayMsg_printf( "%s won't go out!", item->getName().c_str());
                return false;
            }

            //Remove it from the inventory!
            item->setInventoryHolderRef(ObjectRef::Invalid);
            inventoryItem.reset();
            return true;
        }
    }
    return false;
}

size_t Inventory::getMaxItems() const
{
    return _items.size();
}
