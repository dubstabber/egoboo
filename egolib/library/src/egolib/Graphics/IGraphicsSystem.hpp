#pragma once

namespace Ego {

class GraphicsWindow;

/// @brief Service interface exposing the active graphics window, decoupling
///        callers from the concrete GraphicsSystem singleton. Published
///        through EngineContext.
class IGraphicsSystem {
public:
    virtual ~IGraphicsSystem() = default;

    virtual GraphicsWindow* getWindow() const = 0;
};

} // namespace Ego
