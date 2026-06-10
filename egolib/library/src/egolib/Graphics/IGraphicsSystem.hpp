#pragma once

#include <memory>
#include <vector>

namespace Ego {

class GraphicsWindow;
class Display;

/// @brief Service interface exposing the active graphics window, decoupling
///        callers from the concrete GraphicsSystem singleton. Published
///        through EngineContext.
class IGraphicsSystem {
public:
    virtual ~IGraphicsSystem() = default;

    virtual GraphicsWindow* getWindow() const = 0;

    virtual void setCursorVisibility(bool visibility) = 0;

    virtual void update() = 0;

    virtual const std::vector<std::shared_ptr<Display>>& getDisplays() const = 0;
};

/// @brief Install the active graphics system (the graphics system the engine context publishes).
/// @param graphicsSystem the graphics system to install
/// @throw std::logic_error if a graphics system is already installed
/// @remark Subsystem-owned ownership for the installed graphics-system pointer (mirrors the Log
///         active-target / audio-system ownership moves); EngineContext delegates its
///         graphics-system lifecycle here. Lets lower-layer callers (e.g. the GUI toolkit) reach
///         the installed graphics system without depending on the upper-layer EngineContext.
void installActiveGraphicsSystem(IGraphicsSystem& graphicsSystem);

/// @brief Clear the installed active graphics system.
void clearActiveGraphicsSystem();

/// @brief The installed active graphics system, or @a nullptr if none is installed.
IGraphicsSystem* tryActiveGraphicsSystem();

/// @brief The active graphics system.
/// @throw std::logic_error if none is installed
/// @remark Returns the INSTALLED graphics system (which may be a test stub, e.g. MockGraphicsSystem),
///         preserving the swappable-install indirection the graphics tests assert on.
IGraphicsSystem& activeGraphicsSystem();

} // namespace Ego
