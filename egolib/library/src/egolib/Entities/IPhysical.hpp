#pragma once

#include "egolib/game/egoboo.h"

class IPhysical
{
public:
    virtual ~IPhysical() = default;

    virtual const bumper_t& getInitialBump() const = 0;
    virtual const bumper_t& getCurrentBump() const = 0;
    virtual const bumper_t& getSavedBump() const = 0;
    virtual const bumper_t& getLooseBump() const = 0;

    virtual const oct_bb_t& getMinCollisionVolume() const = 0;
    virtual const oct_bb_t& getMaxCollisionVolume() const = 0;
    virtual const oct_bb_t& getSlotCollisionVolume(slot_t slot) const = 0;

    virtual Facing getFacingZ() const = 0;
    virtual Facing getPreviousFacingZ() const = 0;
    virtual Facing getMapTwistFacingX() const = 0;
    virtual Facing getMapTwistFacingY() const = 0;
};
