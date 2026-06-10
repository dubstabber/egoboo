#include "egolib/game/GUI/Component.hpp"
#include "egolib/game/GUI/Container.hpp"

namespace Ego {
namespace GUI {

void Component::bringToFront() {
    if (!_parent) return;
    _parent->bringComponentToFront(shared_from_this());
}

} // namespace GUI
} // namespace Ego
