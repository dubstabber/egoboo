//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

/// @file egolib/game/Logic/Player.cpp
/// @author Zefz aka Johan Jansen

#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/GameStates/PlayingState.hpp"
#include "egolib/game/Module/Module.hpp"

namespace
{
IMovementControl& movementControl(Object& object)
{
    return object;
}

GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

uint32_t worldUpdateCount()
{
    return gameSession().worldUpdateCount();
}

Object* trySessionObject(ObjectRef objectRef)
{
    if (objectRef == ObjectRef::Invalid || !gameSession().hasActiveModule())
    {
        return nullptr;
    }

    return gameSession().tryObject(objectRef);
}

const Object* tryConstSessionObject(ObjectRef objectRef)
{
    if (objectRef == ObjectRef::Invalid || !gameSession().hasActiveModule())
    {
        return nullptr;
    }

    return static_cast<const GameSessionContext&>(gameSession()).tryObject(objectRef);
}
}

namespace Ego
{

Player::Player(const std::shared_ptr<Object>& object, const Ego::Input::InputDevice &device) :
    _objectRef(object ? object->getObjRef() : ObjectRef::Invalid),
    _bootstrapObject(object),
    _unspentLevelUp(false),

    _currentCharge(0),
    _maxCharge(0),
    _chargeBarFrame(0),
    _chargeTick(0),

    _inventoryMode(false),
    _inventorySlot(0),
    _inventoryCooldown(0),

    _questLog(),

    _inputDevice(device)
{
}

ObjectRef Player::getObjectRef() const
{
    return _objectRef;
}

Object* Player::tryObject()
{
    if (gameSession().hasActiveModule())
    {
        return trySessionObject(_objectRef);
    }

    std::shared_ptr<Object> object = _bootstrapObject.lock();
    return object ? object.get() : nullptr;
}

const Object* Player::tryObject() const
{
    if (gameSession().hasActiveModule())
    {
        return tryConstSessionObject(_objectRef);
    }

    std::shared_ptr<Object> object = _bootstrapObject.lock();
    return object ? object.get() : nullptr;
}

const Ego::Input::InputDevice& Player::getInputDevice() const
{
    return _inputDevice;
}

Ego::QuestLog& Player::getQuestLog()
{
    return _questLog;
}

void Player::updateLatches()
{
    //Ensure this player is controlling a valid object
    Object* object = tryObject();
    if(!object || object->isTerminated()) {
        return;
    }
    object->resetInputCommands();

    // find the camera that is following this character
    const auto &pcam = EngineContext::get().cameraSystem().getCamera(object->getObjRef());
    if (!pcam) {
        return;
    }

    // fast camera turn if it is enabled and there is only 1 local player
    bool fast_camera_turn = (1 == gameSession().localPlayerCount()) && (CameraTurnMode::Good == pcam->getTurnMode());

    // Clear the player's latch buffers
    Vector2f movementInput = idlib::zero<Vector2f>();
    Vector2f joy_pos = idlib::zero<Vector2f>();

    // generate the transforms relative to the camera
    // this needs to be changed for multicamera
    float fsin = std::sin(pcam->getOrientation().facing_z);
    float fcos = std::cos(pcam->getOrientation().facing_z);

    if(fast_camera_turn || !getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_CONTROL))
    {
        joy_pos = getInputDevice().getInputMovement();

        //Rotate movement input from body frame to earth frame
        movementInput.x() = ( joy_pos[XX] * fcos + joy_pos[YY] * fsin );
        movementInput.y() = ( -joy_pos[XX] * fsin + joy_pos[YY] * fcos );
    }

    // Read control buttons
    if (!_inventoryMode)
    {
        // Now update movement and input
        object->setLatchButton(LATCHBUTTON_JUMP, getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::JUMP));
        object->setLatchButton(LATCHBUTTON_LEFT, getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::USE_LEFT));
        object->setLatchButton(LATCHBUTTON_RIGHT, getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::USE_RIGHT));
        object->setLatchButton(LATCHBUTTON_ALTLEFT, getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::GRAB_LEFT));
        object->setLatchButton(LATCHBUTTON_ALTRIGHT, getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::GRAB_RIGHT));
        movementControl(*object).setDesiredVelocity(movementInput);
    }

    //inventory mode
    else if (_inventoryCooldown < worldUpdateCount())
    {
        int new_selected = _inventorySlot;

        //ZF> dirty hack here... mouse seems to be inverted in inventory mode?
        if (getInputDevice().getDeviceType() == Ego::Input::InputDevice::InputDeviceType::MOUSE)
        {
            joy_pos[XX] = -joy_pos[XX];
            joy_pos[YY] = -joy_pos[YY];
        }

        //handle inventory movement
        if ( joy_pos[XX] < 0 )       new_selected--;
        else if ( joy_pos[XX] > 0 )  new_selected++;

        //clip to a valid value
        if ( _inventorySlot != new_selected )
        {
            _inventoryCooldown = worldUpdateCount() + 5;

            //Make inventory movement wrap around
            if(new_selected < 0) {
                _inventorySlot = object->getInventoryMaxItems() - 1;
            }
            else if(new_selected >= object->getInventoryMaxItems()) {
                _inventorySlot = 0;
            }
            else {
                _inventorySlot = new_selected;
            }
        }

        //handle item control
        if ( object->canBeInterrupted() && 0 == object->getReloadTimer() )
        {
            //handle LEFT hand control
            if (getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::USE_LEFT) || getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::GRAB_LEFT))
            {
                //put it away and swap with any existing item
                Inventory::swap_item(object->getObjRef(), _inventorySlot, SLOT_LEFT, false);

                // Make it take a little time
                object->playAction(ACTION_MG, false);
                object->setReloadTimer(Inventory::PACKDELAY);
            }

            //handle RIGHT hand control
            if (getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::USE_RIGHT) || getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::GRAB_RIGHT))
            {
                // put it away and swap with any existing item
                Inventory::swap_item(object->getObjRef(), _inventorySlot, SLOT_RIGHT, false);

                // Make it take a little time
                object->playAction(ACTION_MG, false);
                object->setReloadTimer(Inventory::PACKDELAY);
            }
        }
    }

    //enable inventory mode?
    if ( worldUpdateCount() > _inventoryCooldown && getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::INVENTORY) )
    {
        GameModule& module = gameSession().activeModule();
        for(uint8_t ipla = 0; ipla < module.getPlayerList().size(); ++ipla) {
            if(module.getPlayer(ipla).get() == this) {
                if (std::shared_ptr<PlayingState> playingState = EngineContext::get().tryActivePlayingState())
                {
                    playingState->displayCharacterWindow(ipla);
                }
                _inventoryCooldown = worldUpdateCount() + ( ONESECOND / 4 );
                break;
            }
        }
    }

    //Enter or exit stealth mode?
    if(getInputDevice().isButtonPressed(Ego::Input::InputDevice::InputButton::STEALTH) && worldUpdateCount() > _inventoryCooldown) {
        if(!object->isStealthed()) {
            object->activateStealth();
        }
        else {
            object->deactivateStealth();
        }
        _inventoryCooldown = worldUpdateCount() + ONESECOND;
    }
}

void Player::setChargeBar(const uint32_t currentCharge, const uint32_t maxCharge, const uint32_t chargeTick)
{
    _maxCharge = maxCharge;
    _currentCharge = Ego::Math::constrain<uint32_t>(currentCharge, 0, maxCharge);
    _chargeTick = chargeTick;
    _chargeBarFrame = EngineContext::get().engine().getCurrentUpdateFrame() + 10;    
}

bool Player::hasUnspentLevel() const
{
    return _unspentLevelUp;
}

void Player::setLevelUpIndicator(const bool hasLevelUp)
{
    _unspentLevelUp = hasLevelUp;
}

void Player::setInventoryMode(const bool inventoryMode)
{
    _inventoryMode = inventoryMode;
}

uint8_t Player::getSelectedInventorySlot() const
{
    return _inventorySlot;
}

void Player::setSelectedInventorySlot(const uint8_t slot)
{
    _inventorySlot = slot;
}

uint32_t Player::getBarPipWidth() const
{
    return _chargeTick;
}

uint32_t Player::getBarCurrentCharge() const
{
    return _currentCharge;
}

uint32_t Player::getBarMaxCharge() const
{
    return _maxCharge;
}

uint32_t Player::getChargeBarFrame() const
{
    return _chargeBarFrame;
}

} //namespace Ego
