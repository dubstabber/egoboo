#pragma once

#include "egolib/_math.h"               // float_t
#include "egolib/Logic/ObjectSlot.hpp"  // slot_t

#include <vector>

class IInventoryHolder
{
public:
    virtual ~IInventoryHolder() = default;

    virtual ObjectRef getObjRef() const = 0;
    /**
    * @return
    *   true if this Object has been terminated and will be removed from the game.
    *   If this value is true, then this Object is effectively no longer a part of
    *   the game and should not be interacted with.
    **/
    virtual bool isTerminated() const = 0;
    /**
    * @brief
    *   True if this Object is a item that can be grabbed
    **/
    virtual bool isItem() const = 0;
    /**
    * @brief
    *   This function returns true if this Object is inside another Objects inventory
    * @return
    *   true if inside another existing Object's inventory
    **/
    virtual bool isInsideInventory() const = 0;
    /**
    * @return true if this Object is controlled by a player
    **/
    virtual bool isPlayer() const = 0;
    virtual int getHoldingWeight() const = 0;

    virtual size_t getInventoryMaxItems() const = 0;
    virtual size_t getFirstFreeInventorySlot() const = 0;
    virtual ObjectRef getInventoryItemRef(size_t slotNumber) const = 0;
    virtual std::vector<ObjectRef> getInventoryItemRefs() const = 0;
    virtual void setInventoryItemRef(size_t slotNumber, ObjectRef itemRef) = 0;
    virtual bool removeInventoryItemRef(ObjectRef itemRef, bool ignoreKurse) = 0;

    virtual ObjectRef getHeldObject(slot_t slot) const = 0;
    virtual void setHeldObject(slot_t slot, ObjectRef objectRef) = 0;
};
