#pragma once

#include "egolib/Graphics/IGraphicsSystem.hpp"

namespace Ego { class GraphicsWindow; class Display; }

namespace Ego::Test {

/// @brief A minimal, properly-constructed IGraphicsSystem that returns a
///        caller-provided (stub) window. Used by headless test fixtures to
///        install a graphics system into EngineContext so seamed code that
///        calls EngineContext::get().graphicsSystem() works without a real
///        SDL window. (The fixtures' raw-allocated fake GraphicsSystem has an
///        uninitialized vtable and cannot serve virtual calls.)
class MockGraphicsSystem : public Ego::IGraphicsSystem {
public:
    explicit MockGraphicsSystem(Ego::GraphicsWindow* window) : _window(window) {}
    Ego::GraphicsWindow* getWindow() const override { return _window; }
    void setCursorVisibility(bool) override {}
    void update() override {}
    const std::vector<std::shared_ptr<Ego::Display>>& getDisplays() const override { static const std::vector<std::shared_ptr<Ego::Display>> empty; return empty; }
private:
    Ego::GraphicsWindow* _window;
};

} // namespace Ego::Test
