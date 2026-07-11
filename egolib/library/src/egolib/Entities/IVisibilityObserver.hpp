#pragma once

#include "egolib/typedef.h"  // ObjectRef

/// @brief Narrow role for querying whether an entity can currently see another entity.
class IVisibilityObserver
{
public:
    virtual ~IVisibilityObserver() = default;

    virtual bool canSeeObject(ObjectRef targetRef) const = 0;
};
