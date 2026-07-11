#pragma once

#include "egolib/Logic/ObjectSlot.hpp"  // grip_offset_t
#include "egolib/typedef.h"             // ObjectRef

class IAttachmentControl
{
public:
    virtual ~IAttachmentControl() = default;

    virtual bool attachToObject(ObjectRef holderRef, grip_offset_t gripOffset) = 0;
    virtual void setHolderRef(ObjectRef holderRef) = 0;
};
