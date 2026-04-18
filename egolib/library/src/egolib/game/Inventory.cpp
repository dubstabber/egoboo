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

ObjectRef hasStack(const std::shared_ptr<Object>& item, const IInventoryHolder& owner)
{
    if (!item || !item->getProfile()->isStackable())
    {
        return ObjectRef::Invalid;
    }

    for (const std::shared_ptr<Object>& stack : owner.getInventoryItems())
    {
        bool found = stack->getProfile()->isStackable();

        if (stack->getAmmo() >= stack->getAmmoMax())
        {
            found = false;
        }

        if (found && (stack->getProfile()->getSlotNumber() != item->getProfileID()))
        {
            for (uint16_t id = 0; id < IDSZ_COUNT && found; ++id)
            {
                if (stack->getProfile()->getIDSZ(id) != item->getProfile()->getIDSZ(id))
                {
                    found = false;
                }
            }
        }

        if (found)
        {
            return stack->getObjRef();
        }
    }

    return ObjectRef::Invalid;
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

    return findItem(static_cast<const IInventoryHolder&>(*pobj), idsz, equippedOnly);
}

ObjectRef Inventory::findItem(const IInventoryHolder& owner, const IDSZ2& idsz, bool equippedOnly)
{
    if (owner.isTerminated())
    {
        return ObjectRef::Invalid;
    }

    ObjectRef result = ObjectRef::Invalid;

    for (const std::shared_ptr<Object>& pitem : owner.getInventoryItems())
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
    return findItem(static_cast<const IInventoryHolder&>(*objectHandler().get(iowner)), idsz, equippedOnly);
}

//--------------------------------------------------------------------------------------------
bool Inventory::add_item( ObjectRef iowner, ObjectRef iitem, uint8_t inventorySlot, bool ignoreKurse )
{
    // Are owner and item valid?
	if (!objectHandler().exists(iowner) || !objectHandler().exists(iitem)) {
		return false;
	}
    return add_item(*objectHandler().get(iowner), objectHandler()[iitem], inventorySlot, ignoreKurse);
}

bool Inventory::add_item(IInventoryHolder& owner, const std::shared_ptr<Object>& item, uint8_t inventorySlot, bool ignoreKurse)
{
    // Does the owner have free slot in her inventory?
    if (inventorySlot >= owner.getInventoryMaxItems()) {
        return false;
    }

    // If there is an item in the slot, do nothing.
	if (owner.getInventoryItem(inventorySlot)) {
		return false;
	}

    // Don't allow sub-inventories.
	if (!item || item->isInsideInventory()) {
		return false;
	}

    // Kursed?
	if (item->isKursed() && !ignoreKurse)
	{
		// Flag the item as not put away.
		item->addAIAlertBits(ALERTIF_NOTPUTAWAY);
		if (owner.isPlayer()) DisplayMsg_printf("%s is sticky...", item->getName().c_str());
		return false;
	}

    // too big item?
	if (item->getProfile()->isBigItem())
	{
		item->addAIAlertBits(ALERTIF_NOTPUTAWAY);
		if (owner.isPlayer()) DisplayMsg_printf("%s is too big to be put away...", item->getName().c_str());
		return false;
	}

    // Check if item can be stacked on other items.
    ObjectRef stack = hasStack(item, owner);
    if ( objectHandler().exists( stack ) )
    {
        // We found a similar, stackable item in the inventory.
        Object *pstack = objectHandler().get( stack );

        // Reveal the name of the item or the stack.
		if (item->isNameKnown() || pstack->getProfile()->isNameKnown())
		{
			item->setNameKnown(true);
			pstack->setNameKnown(true);
		}

        // Reveal the usage of the item or the stack.
		if (item->getProfile()->isUsageKnown() || pstack->getProfile()->isUsageKnown())
		{
			item->getProfile()->makeUsageKnown();
			pstack->getProfile()->makeUsageKnown();
		}

        // Add the item ammo to the stack.
        int newammo = item->getAmmo() + pstack->getAmmo();
		if (newammo <= pstack->getAmmoMax())
		{
			// All transfered, so kill the in hand item
			pstack->setAmmo(newammo);

			item->requestTerminate();
			return true;
		}
        else
        {
            // Only some were transfered,
            item->setAmmo(item->getAmmo() + pstack->getAmmo() - pstack->getAmmoMax());
            pstack->setAmmo(pstack->getAmmoMax());
            objectHandler().get(owner.getObjRef())->addAIAlertBits(ALERTIF_TOOMUCHBAGGAGE);
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
        item->detatchFromHolder(true, false);

        // clear the dropped flag
        item->clearAIAlertBits(ALERTIF_DROPPED);

        //Do not trigger dismount logic on putting items into inventory
        item->setDismountObject(ObjectRef::Invalid);
        item->setDismountTimer(0);

        //now put the item into the inventory
        item->setHolderRef(ObjectRef::Invalid);
        item->setInventoryHolderRef(owner.getObjRef());
        owner.setInventoryItem(inventorySlot, item);

        // fix the flags
		if (item->getProfile()->isEquipment())
		{
			item->addAIAlertBits(ALERTIF_PUTAWAY);  // same as ALERTIF_ATLASTWAYPOINT;
		}

        //@todo: add in the equipment code here
    }

    return true;
}

bool Inventory::swap_item( ObjectRef iobj, uint8_t inventory_slot, const slot_t grip_off, const bool ignorekurse )
{
    //valid character?
    const std::shared_ptr<Object>& owner = objectHandler()[iobj];
    if(!owner) {
        return false;
    }

    return swap_item(*owner, inventory_slot, grip_off, ignorekurse);
}

bool Inventory::swap_item(IInventoryHolder& owner, uint8_t inventory_slot, slot_t grip_off, const bool ignorekurse)
{
    //Validate slot number
    if(inventory_slot >= owner.getInventoryMaxItems()) {
        return false;
    }

    // Make sure everything is hunkydori
    if (owner.isItem() || owner.isInsideInventory()) return false;

    const std::shared_ptr<Object> inventory_item = owner.getInventoryItem(inventory_slot);
    const std::shared_ptr<Object> item = objectHandler()[owner.getHeldObject(grip_off)];

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
            if ( owner.isPlayer() ) DisplayMsg_printf("%s is sticky...", item->getName().c_str());
            return false;

        }

        if(inventory_item && inventory_item->isKursed()) {
            // Flag the last found_item as not removed
            inventory_item->addAIAlertBits(ALERTIF_NOTTAKENOUT);  // Same as ALERTIF_NOTPUTAWAY
            if ( owner.isPlayer() ) DisplayMsg_printf( "%s won't go out!", inventory_item->getName().c_str() );
            return false;

        }
    }

    //remove existing item from inventory and into the character's hand
    if (inventory_item) {
        owner.removeInventoryItem(inventory_item, ignorekurse);

        inventory_item->attachToObject(objectHandler()[owner.getObjRef()], grip_off == SLOT_RIGHT ? GRIP_RIGHT : GRIP_LEFT);

        //fix flags
        inventory_item->clearAIAlertBits(ALERTIF_GRABBED);
        inventory_item->addAIAlertBits(ALERTIF_TAKENOUT);
    }

    //put the new item in the inventory
    if (item) {
        add_item(owner, item, inventory_slot, ignorekurse);
    }

    return true;
}

bool Inventory::remove_item( ObjectRef iholder, const size_t inventory_slot, const bool ignorekurse )
{
    const std::shared_ptr<Object>& holder = objectHandler()[iholder];
    if(!holder) {
        return false;
    }

    return remove_item(*holder, inventory_slot, ignorekurse);
}

bool Inventory::remove_item(IInventoryHolder& holder, const size_t inventory_slot, const bool ignorekurse)
{
    return holder.removeInventoryItem(holder.getInventoryItem(inventory_slot), ignorekurse);
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
