#pragma once

#include "egolib/integrations/color.hpp"  // Colour4f
#include "egolib/typedef.h"                // ObjectRef, BIT_FIELD

#include <memory>
#include <string>

namespace Ego {
namespace Graphics {

struct Billboard;

class IBillboardSystem
{
public:
    virtual ~IBillboardSystem() = default;

    virtual void update() = 0;
    virtual void reset() = 0;
    virtual std::shared_ptr<Billboard> makeBillboard(ObjectRef obj_ref,
                                                     const std::string& text,
                                                     const Ego::Colour4f& textColor,
                                                     const Ego::Colour4f& tint,
                                                     int lifetime_secs,
                                                     const BIT_FIELD opt_bits,
                                                     float size = 0.75f) = 0;
};

} // namespace Graphics
} // namespace Ego
