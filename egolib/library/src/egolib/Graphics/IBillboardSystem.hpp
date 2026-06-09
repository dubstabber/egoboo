#pragma once

/// @file egolib/Graphics/IBillboardSystem.hpp
/// @brief Lower-layer billboard-system service interface + active-instance accessor seam.
///
///        The interface lives at the egolib/Graphics layer (it names only lower-layer types:
///        Colour4f, ObjectRef, BIT_FIELD, Billboard) so callers below the game/ layer
///        (Entities, Physics) can publish billboards without depending on game/Core/EngineContext.
///        The concrete BillboardSystem implementation stays in game/Graphics/. Ownership of the
///        installed active instance lives in IBillboardSystem.cpp; EngineContext's billboard
///        methods are thin delegators over these accessors (preserving the swappable-install
///        indirection the billboard tests assert on).

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

/// @brief Install @a billboardSystem as the active billboard system.
/// @throw std::logic_error if a billboard system is already installed
///        (mirrors the previous EngineContext::installBillboardSystem semantics).
void installActiveBillboardSystem(IBillboardSystem& billboardSystem);

/// @brief Clear the active billboard system.
void clearActiveBillboardSystem();

/// @brief The installed billboard system, or @a nullptr if none is installed.
IBillboardSystem* tryActiveBillboardSystem();

/// @brief The installed billboard system.
/// @throw std::logic_error if no billboard system is installed.
IBillboardSystem& activeBillboardSystem();

} // namespace Graphics
} // namespace Ego
