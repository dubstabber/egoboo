#pragma once

#include "egolib/game/GUI/Component.hpp"
#include "egolib/typedef.h"

class Object;

namespace Ego { class Player; }

namespace Ego {
namespace GUI {

class InventorySlot : public Component {
public:
    InventorySlot(ObjectRef characterRef, const size_t slotNumber, const std::shared_ptr<Player>& player);

    virtual void draw(Ego::GUI::DrawingContext& drawingContext) override;

    bool notifyMousePointerMoved(const Events::MousePointerMovedEvent& e) override;
    bool notifyMouseButtonPressed(const Events::MouseButtonPressedEvent& e) override;

private:
    const Object* tryObservedItem() const;

    ObjectRef _characterRef;
    size_t _slotNumber;
    std::shared_ptr<Player> _player;
};

} // namespace GUI
} // namespace Ego
