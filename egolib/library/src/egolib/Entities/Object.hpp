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

/// @file egolib/game/Entities/Object.hpp
/// @details An object representing instances of in-game egoboo objects (Object)
/// @author Johan Jansen

#pragma once
#if !defined(GAME_ENTITIES_PRIVATE) || GAME_ENTITIES_PRIVATE != 1
#error(do not include directly, include `game/Entities/_Include.hpp` instead)
#endif

#include "egolib/Script/script.h"
#include "egolib/Logic/Team.hpp"
#include "egolib/InputControl/InputDevice.hpp"

#include "egolib/Entities/IDamageable.hpp"
#include "egolib/Entities/IInventoryHolder.hpp"
#include "egolib/Entities/IPhysical.hpp"
#include "egolib/Entities/IRenderable.hpp"
#include "egolib/Entities/IScriptable.hpp"
#include "egolib/Entities/ITargetInfo.hpp"
#include "egolib/game/egoboo.h"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/physics.h"
#include "egolib/game/graphic_mad.h"
#include "egolib/Entities/Common.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/game/Physics/Collidable.hpp"
#include "egolib/game/Physics/ObjectPhysics.hpp"
#include "egolib/game/Graphics/ObjectGraphics.hpp"

//Forward declarations
namespace Ego { class Enchantment; }

/// The possible methods for characters to determine what direction they are facing
enum turn_mode_t : uint8_t
{
    TURNMODE_VELOCITY = 0,                       ///< Character gets rotation from velocity (normal)
    TURNMODE_WATCH,                              ///< For watch towers, look towards waypoint
    TURNMODE_SPIN,                               ///< For spinning objects
    TURNMODE_WATCHTARGET,                        ///< For combat intensive AI
    TURNMODE_COUNT
};

//--------------------------------------------------------------------------------------------

/// The offsets of Bits identifying in-game actions in a Bit set.
enum LatchButton
{
    LATCHBUTTON_LEFT      = 0,                      ///< Character button presses
    LATCHBUTTON_RIGHT     = 1,
    LATCHBUTTON_JUMP      = 2,
    LATCHBUTTON_ALTLEFT   = 3,                      ///< ( Alts are for grab/drop )
    LATCHBUTTON_ALTRIGHT  = 4,
    LATCHBUTTON_PACKLEFT  = 5,                      ///< ( Used by AI script for inventory cycle )
    LATCHBUTTON_PACKRIGHT = 6,                      ///< ( Used by AI script for inventory cycle )
    LATCHBUTTON_RESPAWN   = 7,

    LATCHBUTTON_COUNT                               //Always last
};


/// The definition of the character object.
class Object : public PhysicsData, private idlib::non_copyable, public Ego::Physics::Collidable,
               public IDamageable,
               public IInventoryHolder,
               public IPhysical,
               public IRenderable,
               public IScriptable,
               public ITargetInfo,
               public std::enable_shared_from_this<Object>
{
public:
    static const std::shared_ptr<Object> INVALID_OBJECT;    //< Invalid object reference
    static constexpr int SIZETIME = 100;                    //< Time it takes to resize a character
    static constexpr uint16_t MAXMONEY = 9999;              ///< Maximum money a character can carry
    static constexpr float DROPZVEL = 7;                    //< Vertical velocity of dropped items
    static constexpr uint8_t JUMPINFINITE = 255;            ///< Flying character TODO> deprecated?
    static constexpr uint8_t JUMPDELAY = 20;                ///< Time between jumps (game updates)
    static constexpr uint32_t PHYS_DISMOUNT_TIME = 50;      ///< time delay for full object-object interaction (approximately 1 second)
    static constexpr float DISMOUNTZVEL = 12;               //< Vertical velocity when jumping off mounts

public:
    /**
     * @brief
     *  Constructor
     * @param proRef
     *  the profile reference of the profile this object should be spawned with
     * @param objRef
     *  the unique object reference of this object
     */
    Object(ObjectProfileRef proRef, ObjectRef objRef);

    /**
     * @brief
     *  Destructor.
     */
    virtual ~Object();

    /**
    * @brief Gets a shared_ptr to the current ObjectProfile associated with this character.
    *        The ObjectProfile can change for polymorphing objects.
    **/
    const std::shared_ptr<ObjectProfile>& getProfile() const;

    /**
    * @return
    *   true if this Entity can collide physically with other Entities
    **/
    bool canCollide() const override;

    std::shared_ptr<const Ego::Texture> getSkinTexture() const override;

    bool isPhongMapped() const { return getProfile()->isPhongMapped(); }

    bool hasReflection() const { return getProfile()->hasReflection(); }

    bool isDontCullBackfaces() const { return getProfile()->isDontCullBackfaces(); }

    /**
    * @return
    *   The elevation of the floor
    **/
    float getFloorElevation() const { return _objectPhysics.getGroundElevation(); }

    void updatePhysics();

    bool attachToPlatform(const std::shared_ptr<Object>& platform);

    void detachFromPlatform();

    const Ego::Vector2f& getDesiredVelocity() const;

    void setDesiredVelocity(const Ego::Vector2f& velocity);

    float getMass() const;

    bool grabStuff(grip_offset_t gripOffset, bool grabPeople);

    bool attachToObject(const std::shared_ptr<Object>& holder, grip_offset_t gripOffset);

    void updateCollisionSize(bool updateMatrix);

    bool floorIsSlippy() const;

    bool isTouchingGround() const;

    /**
	 * @brief Get the unique object reference of this object.
     * @return the unique object reference of this object
     */
    ObjectRef getObjRef() const override { return _objRef; }

    /**
    * @return the current team this object is on. This can change in-game (mounts or pets for example)
    **/
    const Team& getTeam() const;

    void becomeTeamLeader();

    void callTeamForHelp();

    void giveTeamExperience(int amount, XPType type) const;

    TEAM_REF getTeamRef() const { return team; }

    void setTeamRef(TEAM_REF teamRef) { team = teamRef; }

    TEAM_REF getBaseTeamRef() const { return team_base; }

    void setBaseTeamRef(TEAM_REF teamRef) { team_base = teamRef; }

    /**
    * @brief
    *   True if this Object is a item that can be grabbed
    **/
    bool isItem() const {return isitem;}

    /**
    * @return
    *   true if this Object is currently levitating above the ground
    **/
    bool isFlying() const;

    /**
    * @brief
    *   Respawns a Object, bringing it back to life and moving it to its initial position and state.
    *   Does nothing if character is already alive.
    **/
    void respawn();

    /**
    * @brief
    *   This function updates stats and such for this Object (called once per update loop)
    **/
    void update();

    /**
    * @brief
    *   This function returns true if the character is on a water tile
    * @return 
    *   true if it is on a water tile
    **/
    bool isOnWaterTile() const;

    /**
    * @brief
    *   This function returns true if this Object is being held by another Object
    * @return
    *   true if held by another existing Object that is not marked for removal
    **/
    bool isBeingHeld() const;

    /**
    * @brief
    *   Get the Object that is holding this Object
    *   or nullptr if this Object is not being held.
    *   This can also be the mount if the Object is actually 
    *   riding the Holder.
    **/
    const std::shared_ptr<Object>& getHolder() const;

    /**
    * @brief
    *   Get the platform this object is attached to
    *   or nullptr if not attached
    **/
    const std::shared_ptr<Object>& getAttachedPlatform() const;

    /**
    * @brief
    *   This function returns true if this Object is inside another Objects inventory
    * @return
    *   true if inside another existing Object's inventory
    **/
    bool isInsideInventory() const;

    ObjectRef getHolderRef() const { return attachedto; }

    void setHolderRef(ObjectRef holderRef) { attachedto = holderRef; }

    slot_t getAttachmentSlot() const { return inwhich_slot; }

    void setAttachmentSlot(slot_t slot) { inwhich_slot = slot; }

    ObjectRef getInventoryHolderRef() const { return inwhich_inventory; }

    void setInventoryHolderRef(ObjectRef holderRef) { inwhich_inventory = holderRef; }

    bool isPlatform() const { return platform; }

    void setPlatform(bool platformState) { platform = platformState; }

    bool canUsePlatforms() const { return canuseplatforms; }

    void setCanUsePlatforms(bool enabled) { canuseplatforms = enabled; }

    int getHoldingWeight() const { return holdingweight; }

    void setHoldingWeight(int weight) { holdingweight = weight; }

    void adjustHoldingWeight(int delta) { holdingweight += delta; }

    /**
    * @return
    *   true if this Object has been terminated and will be removed from the game.
    *   If this value is true, then this Object is effectively no longer a part of
    *   the game and should not be interacted with.
    **/
    inline bool isTerminated() const {return _terminateRequested;}

	/**
	* @brief
	*    Remove this object from the game.
	*/
	static void removeFromGame(Object * pchr);

    /**
    * @brief
    *   This function returns true if this Object is submerged in liquid
    * @return 
    *   true if it is submerged (either fully or partially)
    **/
    bool isSubmerged() const;

    /**
    * @brief Translate the current X, Y, Z position of this object by the specified values
    **/
    void movePosition(const float x, const float y, const float z);

    /**
    * @brief Sets the transparency for this Object
    * @param alpha Transparency level between 0 (fully transparent) and 255 (fully opaque)
    **/
    void setAlpha(const int alpha);

    /**
    * @brief Sets the shininess of this Object
    * @param alpha Transparency level between 0 (no shine effect) and 255 (completely shiny)
    **/
    void setSheen(const int sheen);

    /**
    * @brief Sets the transparency for this Object
    * @param light Transparency level between 0 (fully transparent) and 255 (fully opaque)
    */
    void setLight(const int light);

    uint8_t getAlpha() const { return inst.getAlpha(); }

    uint8_t getLight() const { return inst.getLight(); }

    uint8_t getSheen() const { return inst.getSheen(); }

    const colorshift_t& getColorShift() const { return inst.getColorShift(); }

    void setColorShift(const colorshift_t& colorShift) { inst.setColorShift(colorShift); }

    SFP8_T getUOffset() const { return inst.getUOffset(); }

    void setUOffset(SFP8_T value) { inst.setUOffset(value); }

    SFP8_T getVOffset() const { return inst.getVOffset(); }

    void setVOffset(SFP8_T value) { inst.setVOffset(value); }

    bool hasModelDescriptor() const override { return inst.getModelDescriptor() != nullptr; }

    const std::shared_ptr<Ego::ModelDescriptor>& getModelDescriptor() const { return inst.getModelDescriptor(); }

    uint8_t getReflectionAlpha() const override { return inst.getReflectionAlpha(); }

    void getTint(GLXvector4f tint, bool reflection, int type) const override { inst.getTint(tint, reflection, type); }

    bool playAction(ModelAction action, bool actionReady) { return inst.playAction(action, actionReady); }

    bool startAnimation(ModelAction action, bool actionReady, bool overrideAction)
    {
        return inst.startAnimation(action, actionReady, overrideAction);
    }

    bool setAction(ModelAction action, bool actionReady, bool overrideAction)
    {
        return inst.setAction(action, actionReady, overrideAction);
    }

    bool setFrameFull(int frameAlong, int ilip) { return inst.setFrameFull(frameAlong, ilip); }

    bool canBeInterrupted() const { return inst.canBeInterrupted(); }

    void setActionKeep(bool val) { inst.setActionKeep(val); }

    void setActionLooped(bool val) { inst.setActionLooped(val); }

    ModelAction getCurrentAnimation() const { return inst.getCurrentAnimation(); }

    void setAnimationSpeed(float rate) { inst.setAnimationSpeed(rate); }

    float getAnimationSpeed() const { return inst.getAnimationSpeed(); }

    void removeInterpolation() { inst.removeInterpolation(); }

    void updateAnimation() { inst.updateAnimation(); }

    const Ego::Matrix4f4f& getMatrix() const override { return inst.getMatrix(); }

    const Ego::Matrix4f4f& getReflectionMatrix() const override { return inst.getReflectionMatrix(); }

    void setMatrix(const Ego::Matrix4f4f& matrix) { inst.setMatrix(matrix); }

    const GLvertex& getVertex(size_t index) const override { return inst.getVertex(index); }

    size_t getVertexCount() const override { return inst.getVertexCount(); }

    gfx_rv updateVertices(int vmin, int vmax, bool force) { return inst.updateVertices(vmin, vmax, force); }

    bool updateGripVertices(const uint16_t vrt_lst[], size_t vrt_count) { return inst.updateGripVertices(vrt_lst, vrt_count); }

    BIT_FIELD getFrameFX() const { return inst.getFrameFX(); }

    void updateLighting() { inst.updateLighting(); }

    void flash(uint8_t value) { inst.flash(value); }

    void flashVariableHeight(uint8_t valueLow, int16_t low, uint8_t valueHigh, int16_t high)
    {
        inst.flashVariableHeight(valueLow, low, valueHigh, high);
    }

    int getMaxLight() const { return inst.getMaxLight(); }

    int getAmbientColour() const override { return inst.getAmbientColour(); }

    oct_bb_t getBoundingBox() const { return inst.getBoundingBox(); }

    matrix_cache_t getMatrixCache() const { return inst.getMatrixCache(); }

    void setMatrixCache(const matrix_cache_t& cache) { inst.setMatrixCache(cache); }

    bool hasValidMatrixCache() const { return inst.hasValidMatrixCache(); }

    void setMatrixCacheValid(bool valid) { inst.setMatrixCacheValid(valid); }

    bool hasValidMatrixValue() const { return inst.hasValidMatrixValue(); }

    void setMatrixValueValid(bool valid) { inst.setMatrixValueValid(valid); }

    void invalidateMatrixCache() { inst.invalidateMatrixCache(); }

    /**
     * @brief Checks if this Object is able to mount (ride) another Object
     * @param mount Which Object we are trying to mount
     * @return true if we are able to mount the specified Object
     */
    bool canMount(const std::shared_ptr<Object> mount) const;

    /**
     * @return true if this Object is mountable by other Objects
     */
    bool isMount() const {return getProfile()->isMount();}

    /**
    * @brief
    *   Mark this object as terminated, it will be removed from the game by the update.
    **/
    void requestTerminate();

    /**
    * @brief 
    *   This function calculates and applies damage to a character.  It also
    *   sets alerts and begins actions.  Blocking and frame invincibility are done here too.  
    *
    * @param direction
    *   Direction is ATK_FRONT if the attack is coming head on, ATK_RIGHT if from the right, 
    *   ATK_BEHIND if from the back, ATK_LEFT if from the left.
    *
    * @param damage 
    *   is a random range of damage to deal
    *
    * @param damageType 
    *   indicates what kind of damage this is (ZAP, CRUSH, FIRE, etc.) which is again 
    *   affected by resistances immunities, etc.
    *
    * @param team 
    *   which team is dealing the damage
    *
    * @param attacker 
    *   The Object which is dealing the damage to this Object
    *
    * @param effects 
    *   is a BIT_FIELD of various flags which affect how we determine damage.
    *
    * @param ignore_invictus 
    *   if this is true, then we allow damaging this object even though it is normally immune to damage.
    **/
    int damage(Facing direction, const IPair  damage, const DamageType damagetype, const TEAM_REF attackerTeam,
               const std::shared_ptr<Object> &attacker, const bool ignoreArmour, const bool setDamageTime, const bool ignoreInvictus) override;

    /**
     * @brief
     *  This function gives some purelife points to the target, ignoring any resistances and so forth.
     * @param healer
     *  the healer
     * @param amount
     *  the amount to heal the character
     */
    bool heal(const std::shared_ptr<Object> &healer, const UFP8_T amount, const bool ignoreInvincibility) override;

    /**
    * @return true if this Object is currently doing an attack animation
    **/
    bool isAttacking() const;

    /**
    * @return true if this Object is controlled by a player
    **/
    bool isPlayer() const {return islocalplayer;}

    PLA_REF getPlayerNumber() const { return is_which_player; }

    void setPlayerNumber(PLA_REF playerNumber) { is_which_player = playerNumber; }

    bool isLocalPlayer() const { return islocalplayer; }

    void setLocalPlayer(bool localPlayer) { islocalplayer = localPlayer; }

    /**
    * @brief
    *   Returns true if this Object has not been killed by anything
    **/
    bool isAlive() const override {return _isAlive;}

    /**
    * @return
    *   true if the Object is currently in hidden state. In hidden state the object cannot be
    *   interacted with, is not rendered and is effectively not part of the game until it is
    *   unhidden again. Hidden state is determined by the Objects AI state.
    **/
    bool isHidden() const;

    bool isNameKnown() const {return nameknown;}

    void setNameKnown(bool known) { nameknown = known; }

    bool isAmmoKnown() const { return ammoknown; }

    void setAmmoKnown(bool known) { ammoknown = known; }

    bool isInvincible() const override {return invictus;}

    void setInvincible(bool invincible) override { invictus = invincible; }

    bool isKursed() const { return iskursed; }

    void setKursed(bool kursed) { iskursed = kursed; }

    bool isHitReady() const { return hitready; }

    void setHitReady(bool ready) { hitready = ready; }

    bool isEquipped() const { return isequipped; }

    void setEquipped(bool equipped) { isequipped = equipped; }

    void setItem(bool item) { isitem = item; }

    bool isShopItem() const { return isshopitem; }

    void setShopItem(bool shopItem) { isshopitem = shopItem; }

    bool canBeCrushed() const { return canbecrushed; }

    void setCanBeCrushed(bool crushable) { canbecrushed = crushable; }

    uint8_t getSparkle() const { return sparkle; }

    void setSparkle(uint8_t sparkleValue) { sparkle = sparkleValue; }

    /**
    * @brief
    *   Tries to teleport this Object to the specified location if it is valid
    * @result
    *   Success returns true, failure returns false;
    **/
    bool teleport(const Ego::Vector3f& position, Facing facing_z);

    /**
    * @brief
    *   Get the name of this character if it is known by the players (e.g Fluffy) or it's class name otherwise (e.g Sheep)
    * @param prefixArticle
    *   if the appropriate article "a" or "an" should be prefixed (only valid for class name)
    * @param prefixDefinite
    *   prefix defeinite article, i.e "the" (only valid for class name)
    * @param captialLetter
    *   Capitalize the first letter in the name or class name (e.g "fluffy" -> "Fluffy")
    **/
    std::string getName(bool prefixArticle = true, bool prefixDefinite = true, bool capitalLetter = true) const;

    /**
    * @brief
    *   Checks if this Object is facing (looking) towards the specified location
    * @return
    *   true if the specified location is within a 60 degree cone of vision for this Object
    **/
    bool isFacingLocation(const float x, const float y) const;

    /**
    * @brief
    *   This makes this Object detatch from any holder (or dismount of riding a mount)
    * @return
    *   true if the detach was successful (could fail because of a kurse for example)
    **/
    bool detatchFromHolder(const bool ignoreKurse, const bool doShop);

    /**
    * @return
    *   Any Object held in the LEFT grip of this Object (or nullptr if no item is held)
    **/
    const std::shared_ptr<Object>& getLeftHandItem() const;

    /**
    * @return
    *   Any Object held in the RIGHT grip of this Object (or nullptr if no item is held)
    **/
    const std::shared_ptr<Object>& getRightHandItem() const;

    ObjectRef getHeldObject(slot_t slot) const { return holdingwhich[slot]; }

    void setHeldObject(slot_t slot, ObjectRef objectRef) { holdingwhich[slot] = objectRef; }

    ObjectRef getEquipment(inventory_t slot) const { return equipment[slot]; }

    void setEquipment(inventory_t slot, ObjectRef objectRef) { equipment[slot] = objectRef; }

    /**
    * @return
    *   true if this Object has line of sight and can see the specified Object
    **/
    bool canSeeObject(const std::shared_ptr<Object> &target) const;

    /**
    * @brief Set the fat value of a character.
    * @param chr the character
    * @param fat the new fat value
    * @remark The fat value influences the character size.
    **/
    void setFat(const float fat);

    float getBaseFat() const { return fat_stt; }

    void setBaseFat(float fat) { fat_stt = fat; }

    float getFat() const { return fat; }

    void setFatRaw(float currentFat) { fat = currentFat; }

    float getTargetFat() const { return fat_goto; }

    void setTargetFat(float fat) { fat_goto = fat; }

    int16_t getResizeTimeRemaining() const { return fat_goto_time; }

    void setResizeTimeRemaining(int16_t remaining) { fat_goto_time = remaining; }

    const bumper_t& getInitialBump() const override { return bump_stt; }

    void setInitialBump(const bumper_t& baseBump) { bump_stt = baseBump; }

    const bumper_t& getCurrentBump() const override { return bump; }

    void setCurrentBump(const bumper_t& currentBump) { bump = currentBump; }

    const bumper_t& getSavedBump() const override { return bump_save; }

    void setSavedBump(const bumper_t& savedBump) { bump_save = savedBump; }

    void initializeBaseBump(const bumper_t& baseBump)
    {
        setInitialBump(baseBump);
        setSavedBump(baseBump);
    }

    const bumper_t& getLooseBump() const override { return bump_1; }

    void setLooseBump(const bumper_t& looseBump) { bump_1 = looseBump; }

    const oct_bb_t& getMinCollisionVolume() const override { return chr_min_cv; }

    void setMinCollisionVolume(const oct_bb_t& minCollisionVolume) { chr_min_cv = minCollisionVolume; }

    const oct_bb_t& getMaxCollisionVolume() const override { return chr_max_cv; }

    void setMaxCollisionVolume(const oct_bb_t& maxCollisionVolume) { chr_max_cv = maxCollisionVolume; }

    const oct_bb_t& getSlotCollisionVolume(slot_t slot) const override { return slot_cv[slot]; }

    void setSlotCollisionVolume(slot_t slot, const oct_bb_t& slotCollisionVolume) { slot_cv[slot] = slotCollisionVolume; }

    void setCollisionVolumes(const oct_bb_t& minCollisionVolume,
                             const oct_bb_t& maxCollisionVolume,
                             const std::array<oct_bb_t, SLOT_COUNT>& slotCollisionVolumes)
    {
        chr_min_cv = minCollisionVolume;
        chr_max_cv = maxCollisionVolume;
        slot_cv = slotCollisionVolumes;
    }

    /**
    * @brief Set the (base) height of a character.
    * @param chr the character
    * @param height the new height
    * @remark The (base) height influences the character size.
    **/
    void setBumpHeight(const float height);

    /**
    * @brief Set the (base) width of a character.
    * @param chr the character
    * @param width the new width
    * @remark Also modifies the shadow size.
    **/
    void setBumpWidth(const float width);

    //TODO: should be private
    /// @author BB
    /// @details Convert the base size values to the size values that are used in the game
    void recalculateCollisionSize();

    /**
    * @author BB
    * @details Handle a character death. Set various states, disconnect it from the world, etc.
    **/
    void kill(const std::shared_ptr<Object> &originalKiller, bool ignoreInvincibility) override;

    /// @author ZZ
    /// @details This function fixes an item's transparency
    void resetAlpha();

    /**
    * @brief
    *   Awards some experience points to this object, potentionally allowing it to reach another
    *   character level. This function handles additional experience gain modifiers such as
    *   XP bonus, roleplay or game difficulity.
    * @param xptype
    *   What kind of experience to give. Different classes gain experience differently depending
    *   on the kind of xp.
    * @param overrideInvincibility
    *   Invincible objects usually gain no experience (scenery objects such as a rock for example).
    *   Set this parameter to true to override this and give the experience anyways.
    **/
    void giveExperience(const int amount, const XPType xptype, const bool overrideInvincibility);


    /// @author BB
    /// @details determine the correct price for an item
    int getPrice() const;

	/** @override */
	BIT_FIELD hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float *pressure) override;
	/** @override */
	BIT_FIELD hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float *pressure, mesh_wall_data_t& data) override;

	/** @override */
	BIT_FIELD test_wall(const Ego::Vector3f& pos) override;

    inline const Ego::AxisAlignedBox2f& getAxisAlignedBox2D() const { return _objectPhysics.getAxisAlignedBox2D(); }

    /**
    * @brief
    *   This function takes mana from a character ( or gives mana ), and returns true if the character had enough to pay, or false
    *   otherwise. This can kill a character in hard mode.
    * @param amount
    *   How much mana to take (positive value) of give (negative value)
    * @param killer
    *   If characters have channeling they can use life instead of mana. This can actually kill them (ghosts that drain mana for example)
    * @return
    *   true if all the requested mana was successfully consumed by the Object
    **/
    bool costMana(int amount, const ObjectRef killer);

    /**
    * @return
    *   Get current mana
    **/
    float getMana() const;

    /**
    * @brief
    *   Get max allowed mana for this Object
    **/
    inline float getMaxMana() const { return getAttribute(Ego::Attribute::MAX_MANA); }

    /**
    * @return
    *   current life remaining in float format
    **/
    float getLife() const;

    /**
    * @brief
    *   Set the current life of this Object to the specified value.
    *   The value will automatically be clipped to a valid value between
    *   0.01f and the maximum life of this Object. This cannot kill the Object.
    **/
    void setLife(const float value);

    /**
    * @brief
    *   Set the current mana of this Object to the specified value.
    *   The value will automatically be clipped to a valid value between
    *   0.00f and the maximum mana of this Object
    **/
    void setMana(const float value);

    /**
    * @brief
    *   True if this object is added to a statusbar monitor
    **/
    bool getShowStatus() const { return _showStatus; }
    void setShowStatus(const bool val) { _showStatus = val; }

    /**
    * @return
    *   Get the experience level of this Object (1 being the first level)
    **/
    uint8_t getExperienceLevel() const { return experiencelevel + 1; }

    uint8_t getExperienceLevelIndex() const { return experiencelevel; }

    void setExperienceLevelIndex(uint8_t levelIndex) { experiencelevel = levelIndex; }

    /**
    * @return
    *   The gender of this Object (if applicable)
    **/
    Gender getGender() const { return gender; }

    void setGender(Gender objectGender) { gender = objectGender; }

    uint32_t getExperience() const { return experience; }

    void setExperience(uint32_t value) { experience = value; }

    /**
    * @brief
    *   Gets how resistant this Object is to a specific type of damage (ZAP, FIRE, POKE, etc.)
    *   For positive Defence, damage reduction =((defence)*0.06)/(1+0.06*(defence))
    *   For negative Defence, it is damage increase = 1-0.94^(defence).
    * @param type
    *   What kind of damage resistance to retrieve
    * @param includeArmor
    *   true if Defence should be included in damage reduction calculation
    * @return
    *   A floating point value representing the damage reduction (0.0f = no reduction, 1.0f = no damage, -1.0f = double damage)
    *   I.e a return value of 0.05f would mean damage reduction of 5%.
    **/
    float getDamageReduction(const DamageType type, const bool includeArmor = true) const override;

    /**
    * @brief
    *   Get character damage resistance to a specific damage. This value is non-linear.
    *   To get the actual damage scaling value, use getDamageReduction() instead.
    **/
    float getRawDamageResistance(const DamageType type, const bool includeArmor = true) const;

    /**
    * @brief
    *   Get total value for the specified attribute. Includes bonuses from Enchants, Perks
    *   and other active boni or penalties.
    **/
    float getAttribute(const Ego::Attribute::AttributeType type) const;

    /**
    * @brief
    *   Get base value for the specified attribute (without applying effects from Enchants and Perks)
    **/
    float getBaseAttribute(const Ego::Attribute::AttributeType type) const;

    /**
    * @brief
    *   Permanently increases or decreases an attribute of this Object
    **/
    void increaseBaseAttribute(const Ego::Attribute::AttributeType type, float value);

    /**
    * @brief
    *   Permanently changes the base attribute of this character to something else
    **/
    void setBaseAttribute(const Ego::Attribute::AttributeType type, float value);

    size_t getInventoryMaxItems() const;

    size_t getFirstFreeInventorySlot() const;

    std::shared_ptr<Object> getInventoryItem(size_t slotNumber) const;

    std::vector<std::shared_ptr<Object>> getInventoryItems() const;

    void setInventoryItem(size_t slotNumber, const std::shared_ptr<Object>& item);

    bool removeInventoryItem(const std::shared_ptr<Object>& item, bool ignoreKurse);

    uint16_t getAmmoMax() const { return ammomax; }

    void setAmmoMax(uint16_t maxAmmo) { ammomax = maxAmmo; }

    uint16_t getAmmo() const { return ammo; }

    void setAmmo(uint16_t ammoCount) { ammo = ammoCount; }

    /**
    * @return
    *   true if this Object has mastered the specified perk. Returns always true for NR_OF_PERKS
    **/
    bool hasPerk(Ego::Perks::PerkID perk) const;

    /**
    * @brief
    *   Generates a list of all Perks that the character can currently learn
    **/
    std::vector<Ego::Perks::PerkID> getValidPerks() const;

    /**
    * @brief
    *   permanently adds a new Perk to this character object
    **/
    void addPerk(Ego::Perks::PerkID perk);

    /**
    * @return
    *   true if this Object can detect and see invisible objects
    **/
    bool canSeeInvisible() const { return getAttribute(Ego::Attribute::SEE_INVISIBLE) > 0.0f; }

    bool canSeeKurses() const;

    bool canOpenStuff() const;

    bool isWeapon() const;

    /**
    * @return
    *   The logic update frame when the rally bonus ends
    **/
    uint32_t getRallyDuration() const { return _reallyDuration; }


    /**
    * @brief
    *   Get the random seed used for determining which perks will be available when leveling and
    *   how much attributes get improved
    **/
    uint32_t getLevelUpSeed() const { return _levelUpSeed; }

    /**
    * @brief
    *   Generates a new random level up seed. Should be called every time a level up is complete
    *   or first time generating a character from scratch (not a save game)
    **/
    void randomizeLevelUpSeed() { _levelUpSeed = Random::next(Random::next<uint32_t>(std::numeric_limits<uint32_t>::max())); }

    /**
    * @brief
    *   Applies an enchantment to this object
    * @param enchantProfile
    *   The unique profile ID for the Enchantment (ENC_REF)
    * @param spawnerProfile
    *   The unique ObjectProfile ID for the object that creates this enchant
    * @brief
    *   pointer to the enchant that was added (or nullptr if it failed)
    **/
    std::shared_ptr<Ego::Enchantment> addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile, const std::shared_ptr<Object>& owner, const std::shared_ptr<Object> &spawner);

    void removeEnchantsWithIDSZ(const IDSZ2& idsz);

    const std::forward_list<std::shared_ptr<Ego::Enchantment>>& getActiveEnchants() const;

    bool hasActiveEnchants() const;

    std::shared_ptr<Ego::Enchantment> getFirstActiveEnchant() const;

    void addActiveEnchant(const std::shared_ptr<Ego::Enchantment>& enchant);

    /**
    * @brief
    *   Removes all enchantments from character
    **/
    bool disenchant();

    /**
    * @brief
    *   Changes the skin of this Object to the specified skin number.
    *   This changes this Objects damage resistances and movement speed accordingly to the new
    *   armor of the skin.
    * @return
    *   true if the skin could be changed into the specified number or false if it fails
    **/
    SKIN_T getSkin() const { return skin; }

    bool setSkin(const size_t skinNumber);

    SKIN_T getBaseSkin() const { return skin_stt; }

    void setBaseSkin(SKIN_T skinNumber) { skin_stt = skinNumber; }

    ObjectProfileRef getBaseModelRef() const { return basemodel_ref; }

    void setBaseModelRef(ObjectProfileRef profileRef) { basemodel_ref = profileRef; }

    bool isOverlay() const { return is_overlay; }

    void setOverlay(bool overlayState) { is_overlay = overlayState; }

    float getBaseShadowSize() const { return shadow_size_stt; }

    void setBaseShadowSize(float shadowSize) { shadow_size_stt = shadowSize; }

    uint32_t getShadowSize() const { return shadow_size; }

    void setShadowSize(uint32_t shadowSize) { shadow_size = shadowSize; }

    uint32_t getSavedShadowSize() const { return shadow_size_save; }

    void setSavedShadowSize(uint32_t shadowSize) { shadow_size_save = shadowSize; }

    bool hasTempAttribute(Ego::Attribute::AttributeType type) const;

    float getTempAttributeValue(Ego::Attribute::AttributeType type) const;

    void setTempAttribute(Ego::Attribute::AttributeType type, float value);

    void adjustTempAttribute(Ego::Attribute::AttributeType type, float delta);

    void clearTempAttribute(Ego::Attribute::AttributeType type);

    std::shared_ptr<Ego::Enchantment> getLastEnchantmentSpawned() const;

    const std::shared_ptr<Object>& toSharedPointer() const;

    /**
    * @brief
    *   changes the name of this Object
    **/
    void setName(const std::string &name);

    /**
    * @brief
    *   Changes this Object into a different type. This effect is reversible (base profile is not changed)
    **/
    void polymorphObject(ObjectProfileRef profileID, const SKIN_T skin);

    ObjectProfileRef getProfileID() const {return _profileID;}

    /**
    * @return
    *   true if this Object is immune to damage from the specified direction
    **/
    bool isInvictusDirection(Facing direction) const;

    DamageType getDamageTargetType() const override { return damagetarget_damagetype; }

    void setDamageTargetType(DamageType damageType) { damagetarget_damagetype = damageType; }

    DamageType getReaffirmDamageType() const override { return reaffirm_damagetype; }

    void setReaffirmDamageType(DamageType damageType) { reaffirm_damagetype = damageType; }

    SFP8_T getDamageThreshold() const { return damage_threshold; }

    void setDamageThreshold(SFP8_T threshold) { damage_threshold = threshold; }

    /**
    * @return 
    *   true if this Object is actively trying to hide from others
    **/
    bool isStealthed() const;

    /**
    * @brief
    *  makes this creature enter Stealth mode. It will try to stay hidden from other Objects.
    *  It will only work if there are no enemies nearby. Depending on the skill level of the
    *  Object, it movement may or may not be restricted. Enemies try to detect stealthed objects
    *  once every second.
    * @return
    *   true if this object is now stealthed from other Objects
    **/
    bool activateStealth();

    /**
    * @brief
    *   This ends the stealth effect on this Object and reveals it to everyone else
    **/
    void deactivateStealth();

    /**
    * @return
    *   true if this Object is a scenery object like furniture, trees, plants, carpets, pillars or a well.
    *   A scenery object is defined by the following attributes:
    *    * cannot move by itself
    *    * is not an item
    *    * objects on team NULL
    **/
    bool isScenery() const;

    /**
    * @brief
    *   Checks if the character is wielding an item with the specified IDSZ in his or her hands
    * @return
    *   The Object that has the matching IDSZ
    **/
    const std::shared_ptr<Object>& isWieldingItemIDSZ(const IDSZ2& idsz) const;

    /**
    * @brief
    *   Changes the team of this Object to another team
    **/
    void setTeam(TEAM_REF team, bool permanent = true);

    /**
    * @brief
    *   checks if the object has a matching skill IDSZ. This function also maps between the old skill IDSZ
    *   system and the new Perk system.
    * @param whichskill
    *   The IDSZ of the skill to check. An IDSZ of [NONE] always matches true.
    * @return
    *   true if the Object has the matching skill IDSZ of a perk that matches the skill IDSZ
    **/
    bool hasSkillIDSZ(const IDSZ2& whichskill) const;

    bool hasTypeIDSZ(const IDSZ2& idsz) const;

    bool hasAnyIDSZ(const IDSZ2& idsz) const;

    bool matchesSpecialIDSZ(const IDSZ2& idsz) const;

    bool matchesVulnerabilityIDSZ(const IDSZ2& idsz) const;

    bool wieldsItemIDSZ(const IDSZ2& idsz) const;

    bool isOnSameTeam(TEAM_REF teamRef) const;

    bool isHatedByTeam(TEAM_REF teamRef) const;

    /**
    * @brief 
    *   This function drops all keys ( [KEYA] to [KEYZ] ) that are in a character's
    *   inventory (Not hands).
    **/
    void dropKeys();

    void dropAllItems();

    std::shared_ptr<const Ego::Texture> getIcon() const;

    /**
    * @brief
    *   Modifies the amount of money this character has. This method
    *   ensures the resulting amount is not negative and not above the
    *   maximum amount.
    * @param amount
    *   The amount to add or subtract (if negative)
    **/
    void giveMoney(int amount);

    /**
    * @return
    *   The amount of money (zennies) this Object currently has
    **/
    uint16_t getMoney() const;

    /**
    * @brief
    *   This function will make the Object drop the specified amount of money.
    *   Dropping money will spawn money particles around the Object
    * @param amount
    *   The amount of money to be dropped. If this is more than the max money,
    *   then all available money will be dropped
    **/
    void dropMoney(int amount);

    int16_t getGrogTimer() const { return grog_timer; }

    void setGrogTimer(int16_t timer) { grog_timer = timer; }

    int16_t getDazeTimer() const { return daze_timer; }

    bool isHurt() const;

    bool hasNotFullMana() const;

    void setDazeTimer(int16_t timer) { daze_timer = timer; }

    int16_t getBoredTimer() const { return bore_timer; }

    void setBoredTimer(int16_t timer) { bore_timer = timer; }

    uint8_t getCarefulTimer() const { return careful_timer; }

    void setCarefulTimer(uint8_t timer) { careful_timer = timer; }

    uint16_t getReloadTimer() const { return reload_timer; }

    void setReloadTimer(uint16_t timer) { reload_timer = timer; }

    uint8_t getDamageTimer() const override { return damage_timer; }

    void setDamageTimer(uint8_t timer) override { damage_timer = timer; }

    bool shouldDrawIcon() const { return draw_icon; }

    void setDrawIcon(bool drawIcon) { draw_icon = drawIcon; }

    bool isInWater() const { return inwater; }

    void setInWater(bool inWater) { inwater = inWater; }

    int getDismountTimer() const { return dismount_timer; }

    void setDismountTimer(int timer) { dismount_timer = timer; }

    ObjectRef getDismountObject() const { return dismount_object; }

    void setDismountObject(ObjectRef objectRef) { dismount_object = objectRef; }

    BIT_FIELD getAIAlertBits() const override;

    void setAIAlertBits(BIT_FIELD bits) override;

    void addAIAlertBits(BIT_FIELD bits) override;

    void clearAIAlertBits(BIT_FIELD bits) override;

    bool hasAnyAIAlertBits(BIT_FIELD bits) const override;

    int getAIStateValue() const override;

    void setAIStateValue(int value) override;

    int getAIContent() const override;

    void setAIContent(int value) override;

    int getAIPassage() const override;

    void setAIPassage(int value) override;

    uint32_t getAITimer() const override;

    void setAITimer(uint32_t timer) override;

    int32_t getAIPoofTime() const override;

    void setAIPoofTime(int32_t time) override;

    ObjectRef getAIOwner() const override;

    void setAIOwner(ObjectRef objectRef) override;

    ObjectRef getAIChild() const override;

    void setAIChild(ObjectRef objectRef) override;

    ObjectRef getAITarget() const override;

    void setAITarget(ObjectRef objectRef) override;

    ObjectRef getAILastAttacker() const override;

    void setAILastAttacker(ObjectRef objectRef) override;

    ObjectRef getAIBumped() const override;

    ObjectRef getAILastItemUsed() const override;

    void setAILastItemUsed(ObjectRef objectRef) override;

    ObjectRef getAILastHit() const override;

    void setAILastHit(ObjectRef objectRef) override;

    DamageType getAILastDamageType() const override;

    void setAILastDamageType(DamageType damageType) override;

    Facing getAILastDirection() const override;

    void setAILastDirection(Facing direction) override;

    float getAIMaxSpeed() const override;

    void setAIMaxSpeed(float speed) override;

    bool addAIOrder(uint32_t value, uint16_t counter) override;

    bool markAIChanged() override;

    bool recordAIBump(ObjectRef objectRef) override;

    void resetAIState() override;

    void spawnAIState(uint16_t rank) override;

    /**
    * @brief
    *   Re-initialized the bored timer to a random value
    **/
    void resetBoredTimer();

    void resetInputCommands();

    /**
    * @brief
    *   Set or unset a latch button. This triggers in game character commands such as attacking, grabbing items or jumping
    * @param latchButton
    *   Which button to set
    * @param pressed
    *   true if this button should be active or false if not
    * @see enum LatchButton
    **/
    void setLatchButton(const LatchButton latchButton, const bool pressed);

    inline bool isAnyLatchButtonPressed() { return _inputLatchesPressed.any(); }

    uint8_t getJumpTimer() const { return jump_timer; }

    void setJumpTimer(uint8_t timer) { jump_timer = timer; }

    uint8_t getJumpNumber() const { return jumpnumber; }

    void setJumpNumber(uint8_t count) { jumpnumber = count; }

    bool isJumpReady() const { return jumpready; }

    void setJumpReady(bool ready) { jumpready = ready; }

    uint8_t getStoppedByMask() const { return stoppedby; }

    void setStoppedByMask(uint8_t mask) { stoppedby = mask; }

    ObjectRef getBumpListNext() const { return bumplist_next; }

    void setBumpListNext(ObjectRef objectRef) { bumplist_next = objectRef; }

    turn_mode_t getTurnMode() const { return turnmode; }

    void setTurnMode(turn_mode_t mode) { turnmode = mode; }

    Facing getFacingZ() const override { return ori.facing_z; }

    void setFacingZ(Facing facing) { ori.facing_z = facing; }

    Facing getMapTwistFacingX() const override { return ori.map_twist_facing_x; }

    void setMapTwistFacingX(Facing facing) { ori.map_twist_facing_x = facing; }

    Facing getMapTwistFacingY() const override { return ori.map_twist_facing_y; }

    void setMapTwistFacingY(Facing facing) { ori.map_twist_facing_y = facing; }

    Facing getPreviousFacingZ() const override { return ori_old.facing_z; }

    void setPreviousFacingZ(Facing facing) { ori_old.facing_z = facing; }

private:

    Team& getMutableTeam() const;

    Team& getMutableTeam(TEAM_REF teamRef) const;

    void clearTeamLeadershipIfSelf(TEAM_REF teamRef);

    void claimTeamLeadershipIfUnset(TEAM_REF teamRef);

    /**
    * @brief This function should be used whenever a character gets attacked or healed. The function
    *        handles if the attacker is a held item (so that the holder becomes the attacker). The function also
    *        updates alerts, timers, etc. This function can trigger character cries like "That tickles!" or "Be careful!"
    **/
    void updateLastAttacker(const std::shared_ptr<Object> &attacker, bool healing);

    /**
    * @brief 
    *   This function makes the characters get bigger or smaller, depending
    *   on their fat_goto and fat_goto_time. Spellbooks do not resize
    */
    void updateResize();
    
    /**
    * @brief
    *   Checks if this Object has attained enough experience to increase its Experience Level
    **/
    void checkLevelUp();

    void updateLatchButtons();

private:
    friend ai_state_t& Ego::Script::runtimeState(Object& object);
    friend const ai_state_t& Ego::Script::runtimeState(const Object& object);

    // character state
    ai_state_t     ai;              ///< ai data

    // character stats
    Gender  gender;          ///< Gender

    uint32_t       experience;      ///< Experience
    uint8_t        experiencelevel; ///< Experience Level

    uint16_t       ammomax;          ///< Ammo stuff
    uint16_t       ammo;

private:
    // equipment and inventory
    std::array<ObjectRef, SLOT_COUNT> holdingwhich; ///< != ObjectRef::Invalid if character is holding something
    std::array<ObjectRef, INVEN_COUNT> equipment;   ///< != ObjectRef::Invalid if character has equipped something

    // team stuff
    TEAM_REF       team;            ///< Character's team
    TEAM_REF       team_base;        ///< Character's starting team

    float          fat_stt;                       ///< Character's initial size
    float          fat;                           ///< Character's size
    float          fat_goto;                      ///< Character's size goto
    int16_t        fat_goto_time;                 ///< Time left in size change

    // jump stuff
    uint8_t          jump_timer;                    ///< Delay until next jump
    uint8_t          jumpnumber;                    ///< Number of jumps remaining
    bool             jumpready;                     ///< For standing on a platform character

private:
    // attachments
    ObjectRef      attachedto;                    ///< != ObjectRef::Invalid if character is a held weapon
    slot_t         inwhich_slot;                  ///< SLOT_LEFT or SLOT_RIGHT
    ObjectRef      inwhich_inventory;             ///< != ObjectRef::Invalid if character is inside an inventory

    // platform stuff
    bool         platform;                      ///< Can it be stood on
    bool         canuseplatforms;               ///< Can use platforms?
    int            holdingweight;                 ///< For weighted buttons

private:
    // combat stuff
    DamageType          damagetarget_damagetype;       ///< Type of damage for AI DamageTarget
    DamageType          reaffirm_damagetype;           ///< For relighting torches
    SFP8_T         damage_threshold;              ///< Damage below this number is ignored (8.8 fixed point)

private:
    // "variable" properties
    PLA_REF      is_which_player;               ///< true = player
    bool         islocalplayer;                 ///< true = local player
    bool         invictus;                      ///< Totally invincible?
    bool         iskursed;                      ///< Can't be dropped?
    bool         nameknown;                     ///< Is the name known?
    bool         ammoknown;                     ///< Is the ammo known?
    bool         hitready;                      ///< Was it just dropped?
    bool         isequipped;                    ///< For boots and rings and stuff

    // "constant" properties
    bool         isitem;                        ///< Is it grabbable?
    bool         isshopitem;                    ///< Spawned in a shop?
    bool         canbecrushed;                  ///< Crush in a door?

private:
    uint8_t          sparkle;         ///< Sparkle color or 0 for off

    // misc timers
    int16_t         grog_timer;                    ///< Grog timer
    int16_t         daze_timer;                    ///< Daze timer
    int16_t         bore_timer;                    ///< Boredom timer
    uint8_t         careful_timer;                 ///< "You hurt me!" timer
    uint16_t        reload_timer;                  ///< Time before another shot
    uint8_t         damage_timer;                  ///< Invincibility timer

    // graphical info
    bool         draw_icon;       ///< Show the icon?

private:
    float          shadow_size_stt;  ///< Initial shadow size
    uint32_t         shadow_size;      ///< Size of shadow
    uint32_t         shadow_size_save; ///< Without size modifiers

    // model info
    bool         is_overlay;                    ///< Is this an overlay? Track aitarget...
    SKIN_T         skin;                          ///< Character's skin
	SKIN_T         skin_stt;                      ///< Character's initial skin
    ObjectProfileRef        basemodel_ref;                     ///< The true form

public:
    // collision info

    /// @note - to make it easier for things to "hit" one another (like a damage particle from
    ///        a torch hitting a grub bug), Aaron sometimes made the bumper size much different
    ///        than the shape of the actual object.
    ///        The old bumper data that is read from the data.txt file will be kept in
    ///        the struct "bump". A new bumper that actually matches the size of the object will
    ///        be kept in the struct "collision"
private:
    bumper_t bump_stt;
    bumper_t bump;
    bumper_t bump_save;

    bumper_t bump_1;       ///< the loosest collision volume that mimics the current bump
    oct_bb_t chr_max_cv;   ///< a looser collision volume for chr-prt interactions
    oct_bb_t chr_min_cv;   ///< the tightest collision volume for chr-chr interactions

    std::array<oct_bb_t, SLOT_COUNT> slot_cv;     ///< the cv's for the object's slots

private:
    orientation_t  ori;                           ///< Character's orientation
    orientation_t  ori_old;                       ///< Character's last orientation

    // data for doing the physics in bump_all_objects()|

    uint8_t stoppedby;                            ///< Collision mask

    ObjectRef bumplist_next;                      ///< Next character on fanblock

    // movement properties
    turn_mode_t turnmode;                         ///< Turning mode

    bool inwater;

    int               dismount_timer;                ///< a timer BB added in to make mounts and dismounts not so unpredictable
    ObjectRef         dismount_object;               ///< the object that you were dismounting from

private:
    static constexpr int RIPPLETOLERANCE = 60;
    static constexpr int RIPPLEAND = 15;             ///< How often ripples spawn
    static constexpr int HURTDAMAGE = 256;           //< Minimum damage for hurt animation
    static constexpr uint8_t CAREFULTIME = 50;       ///< Friendly fire timer (number of game updates)
    static constexpr uint8_t DAMAGETIME = 32;        ///< Invincibility time (number of game updates)
    static constexpr float DROPXYVEL = 12;           //< Horizontal velocity of dropped items
    static constexpr int GRABDELAY = 25;             ///< Time before grab again

    bool _terminateRequested;                        ///< True if this character no longer exists in the game and should be destructed
    ObjectRef _objRef;                               ///< The unique object reference of this object
    ObjectProfileRef _profileID;                     ///< The ID of our profile
    std::shared_ptr<ObjectProfile> _profile;         ///< Our Profile
    bool _showStatus;                                ///< Display stats?
    bool _isAlive;                                   ///< Is this Object alive or dead?
    std::string _name;                               ///< Name of the Object

    //Attributes
    float _currentLife;
    float _currentMana;
    std::array<float, Ego::Attribute::NR_OF_ATTRIBUTES> _baseAttribute; ///< Character attributes
    std::unordered_map<Ego::Attribute::AttributeType, float, std::hash<uint8_t>> _tempAttribute; ///< Character attributes with enchants

    Inventory _inventory;
    uint16_t  _money;                                    ///< Money
    std::bitset<Ego::Perks::NR_OF_PERKS> _perks;         ///< Perks known (super-efficient bool array)
    uint32_t _levelUpSeed;

    //Input commands
    std::bitset<LATCHBUTTON_COUNT> _inputLatchesPressed;

private:
    // Graphics
    Ego::Graphics::ObjectGraphics inst;                   ///< the render data

    //Physics
    Ego::Physics::ObjectPhysics _objectPhysics;

    //Non persistent variables. Once game ends these are not saved
    bool _hasBeenKilled;                              ///< If this Object has been killed at least once this module (many can respawn)
    uint32_t _reallyDuration;                         ///< Game Logic Update frame duration for rally bonus gained from the Perk
    bool _stealth;                                    ///< Is this Object actively trying to hide from others?
    uint16_t _stealthTimer;                           ///< Time before we can enter stealth again
    uint32_t _observationTimer;                       ///< Next update frame we are going to scan for hidden objects

    //Enchantment stuff
    std::forward_list<std::shared_ptr<Ego::Enchantment>> _activeEnchants;    ///< List of all active enchants on this Object
    std::weak_ptr<Ego::Enchantment> _lastEnchantSpawned;    //< Last enchantment that his Object has spawned

    friend class ObjectHandler;
};
