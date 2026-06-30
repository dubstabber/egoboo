#include "InventorySlot.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Graphics/ModelDescriptor.hpp"  //for model action enum
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/Graphics/Font.hpp"  // Ego::Font (complete type)

namespace Ego {
namespace GUI {

namespace
{
Object* tryObservedCharacter(ObjectRef objectRef)
{
    Object* object = Ego::Entities::tryActiveObject(objectRef);
    return object != nullptr && !object->isTerminated() ? object : nullptr;
}

const Object* tryObservedInventoryItem(const IInventoryHolder& holder, size_t slotNumber)
{
    const ObjectRef itemRef = holder.getInventoryItemRef(slotNumber);
    const Object* item = Ego::Entities::tryActiveConstObject(itemRef);
    return item != nullptr && !item->isTerminated() ? item : nullptr;
}
}

InventorySlot::InventorySlot(ObjectRef characterRef, const size_t slotNumber, const std::shared_ptr<Player>& player) :
    _characterRef(characterRef),
    _slotNumber(slotNumber),
    _player(player) {
    //ctor
}

const Object* InventorySlot::tryObservedItem() const
{
    const Object* character = tryObservedCharacter(_characterRef);
    return character ? tryObservedInventoryItem(*character, _slotNumber) : nullptr;
}

void InventorySlot::draw(DrawingContext& drawingContext) {
    const Object* item = tryObservedItem();

    // grab the icon reference
    std::shared_ptr<const Texture> icon_ref;

    if (item) {
        icon_ref = item->getIcon();
    } else {
        icon_ref = EngineContext::get().textureManager().getTexture("mp_data/nullicon");
    }

    bool selected = false;
    if (_player) {
        selected = _player->getSelectedInventorySlot() == _slotNumber;
    }

    //Draw the icon
    draw_game_icon(icon_ref, getDerivedPosition().x(), getDerivedPosition().y(), selected ? COLOR_WHITE : NOSPARKLE, GameSessionContext::get().worldUpdateCount(), getWidth());

    //Draw ammo
    if (item) {
        if (0 != item->getAmmoMax() && item->isAmmoKnown()) {
            if (!item->getProfile()->isStackable() || item->getAmmo() > 1) {
                // Show amount of ammo left
                uiManager().getFont(UIManager::FONT_GAME)->drawTextBox(std::to_string(item->getAmmo()), getDerivedPosition().x(), getDerivedPosition().y(), getWidth(), getHeight(), 0);
            }
        }
    }
}

bool InventorySlot::notifyMousePointerMoved(const Events::MousePointerMovedEvent& e) {
    bool mouseOver = contains(e.get_position());

    if (mouseOver) {
        if (_player) {
            _player->setSelectedInventorySlot(_slotNumber);
        }
        return true;
    }

    return false;
}


bool InventorySlot::notifyMouseButtonPressed(const Events::MouseButtonPressedEvent& e) {
    if (!_player || !contains(e.get_position())) {
        return false;
    }

    if (e.get_button() != SDL_BUTTON_LEFT && e.get_button() != SDL_BUTTON_RIGHT) {
        return false;
    }

    Object* pchr = _player->tryObject();
    if (pchr && pchr->isAlive() && pchr->getGraphics().canBeInterrupted() && 0 == pchr->getReloadTimer()) {
        //put it away and swap with any existing item
        Inventory::swap_item(pchr->getObjRef(), _slotNumber, e.get_button() == SDL_BUTTON_LEFT ? SLOT_LEFT : SLOT_RIGHT, false);

        // Make it take a little time
        pchr->getGraphics().playAction(ACTION_MG, false);
        pchr->setReloadTimer(Inventory::PACKDELAY);
        return true;
    }

    return false;
}

} //GUI
} //Ego
