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

} // namespace Ego
