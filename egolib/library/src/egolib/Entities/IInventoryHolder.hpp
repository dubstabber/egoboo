#pragma once

#include "egolib/_math.h"               // float_t
#include "egolib/Logic/ObjectSlot.hpp"  // slot_t

#include <memory>
#include <vector>

class Object;

class IInventoryHolder
{
public:
    virtual ~IInventoryHolder() = default;

    virtual ObjectRef getObjRef() const = 0;
    virtual bool isTerminated() const = 0;
    virtual bool isItem() const = 0;
    virtual bool isInsideInventory() const = 0;
    virtual bool isPlayer() const = 0;
    virtual int getHoldingWeight() const = 0;

    virtual size_t getInventoryMaxItems() const = 0;
    virtual size_t getFirstFreeInventorySlot() const = 0;
    virtual ObjectRef getInventoryItemRef(size_t slotNumber) const = 0;
    virtual std::shared_ptr<Object> getInventoryItem(size_t slotNumber) const = 0;
    virtual std::vector<ObjectRef> getInventoryItemRefs() const = 0;
    virtual void setInventoryItem(size_t slotNumber, const std::shared_ptr<Object>& item) = 0;
    virtual bool removeInventoryItem(const std::shared_ptr<Object>& item, bool ignoreKurse) = 0;

    virtual ObjectRef getHeldObject(slot_t slot) const = 0;
    virtual void setHeldObject(slot_t slot, ObjectRef objectRef) = 0;
};
