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
#error "do not include directly, include game/Entities/_Include.hpp instead"
#endif

#include "egolib/Script/script.h"
#include "egolib/Logic/Team.hpp"
#include "egolib/InputControl/InputDevice.hpp"

#include "egolib/Entities/IAnimationControl.hpp"
#include "egolib/Entities/IAppearanceProfile.hpp"
#include "egolib/Entities/IDamageable.hpp"
#include "egolib/Entities/ICharacterState.hpp"
#include "egolib/Entities/IEnchantable.hpp"
#include "egolib/Entities/IEquipmentControl.hpp"
#include "egolib/Entities/IInventoryHolder.hpp"
#include "egolib/Entities/IItemInfo.hpp"
#include "egolib/Entities/ILifecycleControl.hpp"
#include "egolib/Entities/IMorphControl.hpp"
#include "egolib/Entities/IMovementControl.hpp"
#include "egolib/Entities/IPhysical.hpp"
#include "egolib/Entities/IProfiled.hpp"
#include "egolib/Entities/IRenderable.hpp"
#include "egolib/Entities/IScriptable.hpp"
#include "egolib/Entities/ITeamMember.hpp"
#include "egolib/Entities/ITargetInfo.hpp"
#include "egolib/Entities/IVisualControl.hpp"
#include "egolib/Entities/IWallet.hpp"
#include "egolib/PhysicsData.h"  // orientation_t (lower-layer primitive; game/physics.h not needed here)
#include "egolib/Entities/Common.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/Physics/Collidable.hpp"
#include "egolib/Entities/ObjectState.hpp"
#include "egolib/game/Physics/ObjectPhysics.hpp"
#include "egolib/game/Graphics/ObjectGraphics.hpp"

#include <forward_list>

//Forward declarations
namespace Ego { class Enchantment; }

/// The definition of the character object.
class Object : public PhysicsData, private idlib::non_copyable, public Ego::Physics::Collidable,
               public IAnimationControl,
               public IAppearanceProfile,
               public IDamageable,
               public ICharacterState,
               public IEnchantable,
               public IEquipmentControl,
               public IInventoryHolder,
               public IItemInfo,
               public ILifecycleControl,
               public IMorphControl,
               public IMovementControl,
               public IPhysical,
               public IProfiled,
               public IRenderable,
               public IScriptable,
               public ITeamMember,
               public ITargetInfo,
               public IVisualControl,
               public IWallet,
               public std::enable_shared_from_this<Object>,
               private ObjectState
{
public:
    static constexpr int SIZETIME = 100;                    //< Time it takes to resize a character
    static constexpr uint16_t MAXMONEY = 9999;              ///< Maximum money a character can carry
    static constexpr float DROPZVEL = 7;                    //< Vertical velocity of dropped items
    static constexpr uint8_t JUMPINFINITE = 255;            ///< Flying character TODO> deprecated?
    static constexpr uint8_t JUMPDELAY = 20;                ///< Time between jumps (game updates)
    static constexpr uint32_t PHYS_DISMOUNT_TIME = 50;      ///< time delay for full object-object interaction (approximately 1 second)
    static constexpr float DISMOUNTZVEL = 12;               //< Vertical velocity when jumping off mounts

public:
    Object(ObjectProfileRef proRef, ObjectRef objRef);

    virtual ~Object();

    const std::shared_ptr<ObjectProfile>& getProfile() const override;

    bool canCollide() const override;

    std::shared_ptr<const Ego::Texture> getSkinTexture() const override;

    ObjectAttribution attribution() const;

    ObjectAttribution attribution(TEAM_REF sourceTeam) const;

    bool isPhongMapped() const override;

    bool hasReflection() const override;

    bool isDontCullBackfaces() const override;

    float getPosZ() const override;

    const Index1D& getTile() const override;

    const Ego::Vector3f& getPosition() const override;

    float getFloorElevation() const override;

    const Ego::Vector3f& getVelocity() const override;

    void setVelocity(const Ego::Vector3f& velocity) override;

    const Ego::Vector3f& getSpawnPosition() const override;

    const Ego::Vector3f& getOldPosition() const override;

    void updatePhysics();

    bool attachToPlatform(ObjectRef platformRef);

    void detachFromPlatform();

    float getMass() const;

    bool grabStuff(grip_offset_t gripOffset, bool grabPeople);

    bool attachToObject(ObjectRef holderRef, grip_offset_t gripOffset);

    void updateCollisionSize(bool updateMatrix);

    bool floorIsSlippy() const;

    bool isTouchingGround() const;

    ObjectRef getObjRef() const override;

    /**
    * @return the current team this object is on. This can change in-game (mounts or pets for example)
    **/
    const Team& getTeam() const;

    void becomeTeamLeader() override;

    void callTeamForHelp() override;

    void giveTeamExperience(int amount, XPType type) const override;

    TEAM_REF getTeamRef() const override;

    void setTeamRef(TEAM_REF teamRef);

    TEAM_REF getBaseTeamRef() const override;

    void setBaseTeamRef(TEAM_REF teamRef);

    IDSZ2 getTypeIDSZ() const override;

    IDSZ2 getHateIDSZ() const override;

    bool isItem() const override;

    bool isFlying() const override;

    void respawn() override;

    void respawnInPlace() override;

    void update();

    bool isOnWaterTile() const override;

    bool isBeingHeld() const override;

    bool isInsideInventory() const override;

    ObjectRef getHolderRef() const override;

    void setHolderRef(ObjectRef holderRef);

    ObjectRef getAttachedPlatformRef() const;

    slot_t getAttachmentSlot() const override;

    void setAttachmentSlot(slot_t slot);

    ObjectRef getInventoryHolderRef() const;

    void setInventoryHolderRef(ObjectRef holderRef);

    bool isPlatform() const override;

    void setPlatform(bool platformState);

    bool canUsePlatforms() const;

    void setCanUsePlatforms(bool enabled);

    int getHoldingWeight() const override;

    void setHoldingWeight(int weight);

    void adjustHoldingWeight(int delta);

    bool isTerminated() const override;

	static void removeFromGame(Object * pchr);

    bool isSubmerged() const;

    void movePosition(const float x, const float y, const float z) override;

    void setAlpha(const int alpha) override;

    /**
    * @brief Sets the shininess of this Object
    * @param alpha Transparency level between 0 (no shine effect) and 255 (completely shiny)
    **/
    void setSheen(const int sheen);

    void setLight(const int light) override;

    /// Access this object's render data (animation, lighting, matrix cache).
    /// Used in place of the legacy inst.* forwarding wrappers.
    Ego::Graphics::ObjectGraphics& getGraphics() noexcept { return inst; }
    const Ego::Graphics::ObjectGraphics& getGraphics() const noexcept { return inst; }

    uint8_t getAlpha() const override;

    uint8_t getLight() const override;

    uint8_t getSheen() const override;

    SFP8_T getUOffset() const override;

    SFP8_T getVOffset() const override;

    bool hasModelDescriptor() const override;

    const std::shared_ptr<Ego::ModelDescriptor>& getModelDescriptor() const override;

    uint8_t getReflectionAlpha() const override;

    void getTint(GLXvector4f tint, bool reflection, int type) const override;

    ModelAction resolveModelAction(int actionIndex) const override;

    bool startAnimation(ModelAction action, bool actionReady, bool overrideAction) override;

    bool setEncodedActionFrame(int actionIndex, int encodedFrame) override;

    void setActionKeep(bool val) override;

    ModelAction getCurrentAnimation() const override;

    void removeInterpolation() override;

    const Ego::Matrix4f4f& getMatrix() const override;

    const Ego::Matrix4f4f& getReflectionMatrix() const override;

    const GLvertex& getVertex(size_t index) const override;

    size_t getVertexCount() const override;

    void flash(uint8_t value) override;

    void flashVariableHeight(uint8_t valueLow, int16_t low, uint8_t valueHigh, int16_t high) override;

    int getAmbientColour() const override;

    /**
     * @brief Checks if this Object is able to mount (ride) another Object
     * @param mount Which Object we are trying to mount
     * @return true if we are able to mount the specified Object
     */
    bool canMount(ObjectRef mountRef) const;

    bool isMount() const override;

    void requestTerminate() override;

    /**
    * @brief
    *   Set the terminated flag directly, without routing through the ObjectHandler removal
    *   path (which is what calls this, mid-removal). Use requestTerminate() for the normal
    *   "please remove me from the game" request; this low-level setter exists so ObjectHandler
    *   can flag the object during remove() without needing private (friend) access.
    **/
    void markTerminateRequested();

    int damage(Facing direction, const IPair  damage, const DamageType damagetype, ObjectAttribution attacker,
               const bool ignoreArmour, const bool setDamageTime, const bool ignoreInvictus) override;

    bool heal(ObjectAttribution healer, const UFP8_T amount, const bool ignoreInvincibility) override;

    bool isAttacking() const override;

    bool isPlayer() const override;

    PLA_REF getPlayerNumber() const override;

    void setPlayerNumber(PLA_REF playerNumber);

    void setLocalPlayer(bool localPlayer);

    bool isAlive() const override;

    bool isHidden() const override;

    bool isNameKnown() const override;

    void setNameKnown(bool known) override;

    bool isAmmoKnown() const;

    void setAmmoKnown(bool known) override;

    bool isInvincible() const override;

    void setInvincible(bool invincible) override;

    bool isKursed() const override;

    void setKursed(bool kursed) override;

    bool isHitReady() const;

    void setHitReady(bool ready);

    bool isEquipped() const override;

    void setEquipped(bool equipped) override;

    void setItem(bool item) override;

    bool isShopItem() const;

    void setShopItem(bool shopItem);

    bool canBeCrushed() const;

    void setCanBeCrushed(bool crushable) override;

    uint8_t getSparkle() const;

    void setSparkle(uint8_t sparkleValue) override;

    bool teleport(const Ego::Vector3f& position, Facing facing_z) override;

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
    std::string getDisplayName() const override;

    /**
    * @brief
    *   Checks if this Object is facing (looking) towards the specified location
    * @return
    *   true if the specified location is within a 60 degree cone of vision for this Object
    **/
    bool isFacingLocation(const float x, const float y) const;

    bool detachFromHolder(const bool ignoreKurse, const bool doShop) override;

    ObjectRef getHeldObject(slot_t slot) const override;

    void setHeldObject(slot_t slot, ObjectRef objectRef) override;

    ObjectRef getEquipment(inventory_t slot) const;

    void setEquipment(inventory_t slot, ObjectRef objectRef);

    /**
    * @return
    *   true if this Object has line of sight and can see the specified Object
    **/
    bool canSeeObject(ObjectRef targetRef) const;

    /**
    * @brief Set the fat value of a character.
    * @param chr the character
    * @param fat the new fat value
    * @remark The fat value influences the character size.
    **/
    void setFat(const float fat);

    float getBaseFat() const;

    void setBaseFat(float fat);

    float getFat() const override;

    void setFatRaw(float currentFat);

    float getTargetFat() const override;

    void setTargetFat(float fat) override;

    int16_t getResizeTimeRemaining() const override;

    void setResizeTimeRemaining(int16_t remaining) override;

    float getPosX() const override;

    float getPosY() const override;

    const bumper_t& getInitialBump() const override;

    void setInitialBump(const bumper_t& baseBump);

    const bumper_t& getCurrentBump() const override;

    void setCurrentBump(const bumper_t& currentBump);

    const bumper_t& getSavedBump() const override;

    void setSavedBump(const bumper_t& savedBump);

    void initializeBaseBump(const bumper_t& baseBump);

    const bumper_t& getLooseBump() const override;

    void setLooseBump(const bumper_t& looseBump);

    const oct_bb_t& getMinCollisionVolume() const override;

    void setMinCollisionVolume(const oct_bb_t& minCollisionVolume);

    const oct_bb_t& getMaxCollisionVolume() const override;

    void setMaxCollisionVolume(const oct_bb_t& maxCollisionVolume);

    const oct_bb_t& getSlotCollisionVolume(slot_t slot) const override;

    void setSlotCollisionVolume(slot_t slot, const oct_bb_t& slotCollisionVolume);

    void setCollisionVolumes(const oct_bb_t& minCollisionVolume,
                             const oct_bb_t& maxCollisionVolume,
                             const std::array<oct_bb_t, SLOT_COUNT>& slotCollisionVolumes);

    void setBumpHeight(const float height) override;

    void setBumpWidth(const float width) override;

    //TODO: should be private
    /// @author BB
    /// @details Convert the base size values to the size values that are used in the game
    void recalculateCollisionSize();

    void kill(ObjectAttribution originalKiller, bool ignoreInvincibility) override;

    /// @author ZZ
    /// @details This function fixes an item's transparency
    void resetAlpha();

    void giveExperience(const int amount, const XPType xptype, const bool overrideInvincibility) override;


    /// @author BB
    /// @details determine the correct price for an item
    int getPrice() const;

	BIT_FIELD hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float *pressure) override;
	BIT_FIELD hit_wall(const Ego::Vector3f& pos, Ego::Vector2f& nrm, float *pressure, mesh_wall_data_t& data) override;

	BIT_FIELD test_wall(const Ego::Vector3f& pos) override;

    const Ego::AxisAlignedBox2f& getAxisAlignedBox2D() const override;

    uint32_t getPhysicsWeight() const override;

    bool costMana(int amount, const ObjectRef killer) override;

    float getMana() const override;

    /**
    * @brief
    *   Get max allowed mana for this Object
    **/
    float getMaxMana() const;

    float getLife() const override;

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
    bool getShowStatus() const;
    void setShowStatus(const bool val);

    /**
    * @return
    *   Get the experience level of this Object (1 being the first level)
    **/
    uint8_t getExperienceLevel() const;

    uint8_t getExperienceLevelIndex() const override;

    void setExperienceLevelIndex(uint8_t levelIndex);

    /**
    * @return
    *   The gender of this Object (if applicable)
    **/
    Gender getGender() const override;

    void setGender(Gender objectGender);

    uint32_t getExperience() const override;

    void setExperience(uint32_t value);

    float getDamageReduction(const DamageType type, const bool includeArmor = true) const override;

    /**
    * @brief
    *   Get character damage resistance to a specific damage. This value is non-linear.
    *   To get the actual damage scaling value, use getDamageReduction() instead.
    **/
    float getRawDamageResistance(const DamageType type, const bool includeArmor = true) const;

    float getAttribute(const Ego::Attribute::AttributeType type) const override;

    /**
    * @brief
    *   Get base value for the specified attribute (without applying effects from Enchants and Perks)
    **/
    float getBaseAttribute(const Ego::Attribute::AttributeType type) const;

    void setRedShift(int value) override;

    void setGreenShift(int value) override;

    void setBlueShift(int value) override;

    void increaseBaseAttribute(const Ego::Attribute::AttributeType type, float value) override;

    /**
    * @brief
    *   Permanently changes the base attribute of this character to something else
    **/
    void setBaseAttribute(const Ego::Attribute::AttributeType type, float value);

    void setFlyHeight(float height) override;

    size_t getInventoryMaxItems() const override;

    size_t getFirstFreeInventorySlot() const override;

    ObjectRef getInventoryItemRef(size_t slotNumber) const override;

    std::vector<ObjectRef> getInventoryItemRefs() const override;

    void setInventoryItemRef(size_t slotNumber, ObjectRef itemRef) override;

    bool removeInventoryItemRef(ObjectRef itemRef, bool ignoreKurse) override;

    uint16_t getAmmoMax() const override;

    void setAmmoMax(uint16_t maxAmmo);

    uint16_t getAmmo() const override;

    void setAmmo(uint16_t ammoCount) override;

    bool hasPerk(Ego::Perks::PerkID perk) const override;

    /**
    * @brief
    *   Generates a list of all Perks that the character can currently learn
    **/
    std::vector<Ego::Perks::PerkID> getValidPerks() const;

    void addPerk(Ego::Perks::PerkID perk) override;

    bool canSeeInvisible() const override;

    bool canSeeKurses() const override;

    bool canOpenStuff() const override;

    bool isWeapon() const override;

    uint32_t getRallyDuration() const;

    uint32_t getLevelUpSeed() const;

    void randomizeLevelUpSeed();

    std::shared_ptr<Ego::Enchantment> addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile,
                                                 ObjectRef ownerRef,
                                                 ObjectRef spawnerRef) override;

    void removeEnchantsWithIDSZ(const IDSZ2& idsz) override;

    const std::forward_list<std::shared_ptr<Ego::Enchantment>>& getActiveEnchants() const;

    bool hasActiveEnchants() const override;

    std::shared_ptr<Ego::Enchantment> getFirstActiveEnchant() const override;

    void addActiveEnchant(const std::shared_ptr<Ego::Enchantment>& enchant);

    bool disenchant() override;

    SKIN_T getSkin() const override;

    bool setSkin(const size_t skinNumber) override;

    uint16_t getSkinCost(size_t skinNumber) const override;

    bool isCurrentSkinDressy() const override;

    bool hasIntellectDamageParticle() const override;

    SKIN_T getBaseSkin() const;

    void setBaseSkin(SKIN_T skinNumber);

    ObjectProfileRef getBaseModelRef() const override;

    void setBaseModelRef(ObjectProfileRef profileRef) override;

    bool isOverlay() const;

    void setOverlay(bool overlayState);

    float getBaseShadowSize() const;

    void setBaseShadowSize(float shadowSize);

    uint32_t getShadowSize() const;

    void setShadowSize(uint32_t shadowSize) override;

    uint32_t getSavedShadowSize() const;

    void setSavedShadowSize(uint32_t shadowSize) override;

    bool hasTempAttribute(Ego::Attribute::AttributeType type) const;

    float getTempAttributeValue(Ego::Attribute::AttributeType type) const;

    void setTempAttribute(Ego::Attribute::AttributeType type, float value);

    void adjustTempAttribute(Ego::Attribute::AttributeType type, float delta);

    void clearTempAttribute(Ego::Attribute::AttributeType type);

    std::shared_ptr<Ego::Enchantment> getLastEnchantmentSpawned() const override;

    void setName(const std::string &name);

    void polymorphObject(ObjectProfileRef profileID, const SKIN_T skin) override;

    ObjectProfileRef getProfileID() const;

    PRO_REF getProfileRef() const override;

    bool isInvictusDirection(Facing direction) const;

    DamageType getDamageTargetType() const override;

    void setDamageTargetType(DamageType damageType) override;

    DamageType getReaffirmDamageType() const override;

    void setReaffirmDamageType(DamageType damageType);

    SFP8_T getDamageThreshold() const;

    void setDamageThreshold(SFP8_T threshold) override;

    bool isStealthed() const override;

    bool activateStealth() override;

    void deactivateStealth() override;

    /**
    * @return
    *   true if this Object is a scenery object like furniture, trees, plants, carpets, pillars or a well.
    *   A scenery object is defined by the following attributes:
    *    * cannot move by itself
    *    * is not an item
    *    * objects on team NULL
    **/
    bool isScenery() const;

    void setTeam(TEAM_REF team, bool permanent = true) override;

    bool hasSkillIDSZ(const IDSZ2& whichskill) const override;

    bool hasTypeIDSZ(const IDSZ2& idsz) const override;

    bool isRangedWeapon() const override;

    bool isMeleeWeapon() const override;

    bool isShield() const override;

    bool hasAnyIDSZ(const IDSZ2& idsz) const override;

    bool matchesSpecialIDSZ(const IDSZ2& idsz) const override;

    bool matchesVulnerabilityIDSZ(const IDSZ2& idsz) const override;

    bool wieldsItemIDSZ(const IDSZ2& idsz) const override;

    bool isOnSameTeam(TEAM_REF teamRef) const override;

    bool isHatedByTeam(TEAM_REF teamRef) const override;

    void dropKeys() override;

    void dropAllItems() override;

    std::shared_ptr<const Ego::Texture> getIcon() const;

    void giveMoney(int amount) override;

    uint16_t getMoney() const override;

    void dropMoney(int amount) override;

    bool canBeGrogged() const override;

    bool canBeDazed() const override;

    int16_t getGrogTimer() const override;

    void setGrogTimer(int16_t timer) override;

    int16_t getDazeTimer() const override;

    bool isHurt() const override;

    bool hasNotFullMana() const override;

    void setDazeTimer(int16_t timer) override;

    int16_t getBoredTimer() const;

    void setBoredTimer(int16_t timer);

    uint8_t getCarefulTimer() const;

    void setCarefulTimer(uint8_t timer);

    uint16_t getReloadTimer() const override;

    void setReloadTimer(uint16_t timer) override;

    uint8_t getDamageTimer() const override;

    void setDamageTimer(uint8_t timer) override;

    bool shouldDrawIcon() const;

    void setDrawIcon(bool drawIcon);

    bool isInWater() const;

    void setInWater(bool inWater);

    int getDismountTimer() const;

    void setDismountTimer(int timer) override;

    ObjectRef getDismountObject() const;

    void setDismountObject(ObjectRef objectRef) override;

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

    void resetBoredTimer();

    void resetInputCommands();

    void setLatchButton(const LatchButton latchButton, const bool pressed) override;

    bool isAnyLatchButtonPressed();

    uint8_t getJumpTimer() const;

    void setJumpTimer(uint8_t timer) override;

    uint8_t getJumpNumber() const;

    void setJumpNumber(uint8_t count);

    bool isJumpReady() const;

    void setJumpReady(bool ready);

    uint8_t getStoppedByMask() const override;

    void setStoppedByMask(uint8_t mask);

    ObjectRef getBumpListNext() const;

    void setBumpListNext(ObjectRef objectRef);

    turn_mode_t getTurnMode() const;

    void setTurnMode(turn_mode_t mode) override;

    Facing getFacingZ() const override;

    void setFacingZ(Facing facing);

    Facing getMapTwistFacingX() const override;

    void setMapTwistFacingX(Facing facing);

    Facing getMapTwistFacingY() const override;

    void setMapTwistFacingY(Facing facing);

    Facing getPreviousFacingZ() const override;

    void setPreviousFacingZ(Facing facing);

private:

    const Ego::Vector2f& getDesiredVelocity() const override;

    void setDesiredVelocity(const Ego::Vector2f& velocity) override;

    Team& getMutableTeam() const;

    Team& getMutableTeam(TEAM_REF teamRef) const;

    void clearTeamLeadershipIfSelf(TEAM_REF teamRef);

    void claimTeamLeadershipIfUnset(TEAM_REF teamRef);

    /**
    * @brief This function should be used whenever a character gets attacked or healed. The function
    *        handles if the attacker is a held item (so that the holder becomes the attacker). The function also
    *        updates alerts, timers, etc. This function can trigger character cries like "That tickles!" or "Be careful!"
    **/
    void updateLastAttacker(ObjectAttribution attacker, bool healing);

    void updateResize();
    
    void checkLevelUp();

    void updateLatchButtons();

    ai_state_t& scriptRuntimeState() noexcept;

    const ai_state_t& scriptRuntimeState() const noexcept;

private:
    friend ai_state_t& Ego::Script::runtimeState(Object& object);
    friend const ai_state_t& Ego::Script::runtimeState(const Object& object);

    Ego::Graphics::ObjectGraphics inst;
    Ego::Physics::ObjectPhysics _objectPhysics;

    bool _hasBeenKilled;
    uint32_t _reallyDuration;
    bool _stealth;
    uint16_t _stealthTimer;
    uint32_t _observationTimer;
    std::forward_list<std::shared_ptr<Ego::Enchantment>> _activeEnchants;
    std::weak_ptr<Ego::Enchantment> _lastEnchantSpawned;
};
