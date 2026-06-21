#pragma once

#include "egolib/typedef.h"  // ObjectRef, SFP8_T

class ILifecycleControl
{
public:
    virtual ~ILifecycleControl() = default;

    /**
    * @brief
    *   Mark this object as terminated, it will be removed from the game by the update.
    **/
    virtual void requestTerminate() = 0;
    /**
    * @brief
    *   This makes this Object detach from any holder (or dismount of riding a mount)
    * @return
    *   true if the detach was successful (could fail because of a kurse for example)
    **/
    virtual bool detachFromHolder(bool ignoreKurse, bool doShop) = 0;
    /**
    * @brief
    *   This function drops all keys ( [KEYA] to [KEYZ] ) that are in a character's
    *   inventory (Not hands).
    **/
    virtual void dropKeys() = 0;
    virtual void dropAllItems() = 0;
    /**
    * @brief
    *   Respawns a Object, bringing it back to life and moving it to its initial position and state.
    *   Does nothing if character is already alive.
    **/
    virtual void respawn() = 0;
    virtual void respawnInPlace() = 0;
    virtual void setDismountTimer(int timer) = 0;
    virtual void setDismountObject(ObjectRef objectRef) = 0;
    virtual void setItem(bool item) = 0;
    virtual void setCanBeCrushed(bool crushable) = 0;
    virtual void setDamageThreshold(SFP8_T threshold) = 0;
    /**
    * @brief
    *  makes this creature enter Stealth mode. It will try to stay hidden from other Objects.
    *  It will only work if there are no enemies nearby. Depending on the skill level of the
    *  Object, it movement may or may not be restricted. Enemies try to detect stealthed objects
    *  once every second.
    * @return
    *   true if this object is now stealthed from other Objects
    **/
    virtual bool activateStealth() = 0;
    /**
    * @brief
    *   This ends the stealth effect on this Object and reveals it to everyone else
    **/
    virtual void deactivateStealth() = 0;
};
