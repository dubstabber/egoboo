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

/// @file egolib/Entities/ObjectState.hpp
/// @brief Private storage base for Object.

#pragma once
#if !defined(GAME_ENTITIES_PRIVATE) || GAME_ENTITIES_PRIVATE != 1
#error "do not include directly, include egolib/Entities/Object.hpp instead"
#endif

#include "egolib/Script/script.h"
#include "egolib/Logic/Team.hpp"
#include "egolib/Entities/IMovementControl.hpp"
#include "egolib/PhysicsData.h"
#include "egolib/Entities/Common.hpp"
#include "egolib/game/Inventory.hpp"

#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <unordered_map>

class ObjectProfile;

struct ObjectState
{
    static constexpr int RIPPLETOLERANCE = 60;
    static constexpr int RIPPLEAND = 15;
    static constexpr int HURTDAMAGE = 256;
    static constexpr uint8_t CAREFULTIME = 50;
    static constexpr uint8_t DAMAGETIME = 32;
    static constexpr float DROPXYVEL = 12;
    static constexpr int GRABDELAY = 25;

    ObjectState(ObjectProfileRef proRef, ObjectRef objRef);

    // character state
    ai_state_t ai;

    // character stats
    Gender gender;
    uint32_t experience;
    uint8_t experiencelevel;
    uint16_t ammomax;
    uint16_t ammo;

    // equipment and inventory
    std::array<ObjectRef, SLOT_COUNT> holdingwhich;
    std::array<ObjectRef, INVEN_COUNT> equipment;

    // team stuff
    TEAM_REF team;
    TEAM_REF team_base;

    float fat_stt;
    float fat;
    float fat_goto;
    int16_t fat_goto_time;

    // jump stuff
    uint8_t jump_timer;
    uint8_t jumpnumber;
    bool jumpready;

    // attachments
    ObjectRef attachedto;
    slot_t inwhich_slot;
    ObjectRef inwhich_inventory;

    // platform stuff
    bool platform;
    bool canuseplatforms;
    int holdingweight;

    // combat stuff
    DamageType damagetarget_damagetype;
    DamageType reaffirm_damagetype;
    SFP8_T damage_threshold;

    // variable properties
    PLA_REF is_which_player;
    bool islocalplayer;
    bool invictus;
    bool iskursed;
    bool nameknown;
    bool ammoknown;
    bool hitready;
    bool isequipped;

    // constant properties
    bool isitem;
    bool isshopitem;
    bool canbecrushed;

    uint8_t sparkle;

    // misc timers
    int16_t grog_timer;
    int16_t daze_timer;
    int16_t bore_timer;
    uint8_t careful_timer;
    uint16_t reload_timer;
    uint8_t damage_timer;

    // graphical info
    bool draw_icon;

    float shadow_size_stt;
    uint32_t shadow_size;
    uint32_t shadow_size_save;

    // model info
    bool is_overlay;
    SKIN_T skin;
    SKIN_T skin_stt;
    ObjectProfileRef basemodel_ref;

    // collision info
    bumper_t bump_stt;
    bumper_t bump;
    bumper_t bump_save;
    bumper_t bump_1;
    oct_bb_t chr_max_cv;
    oct_bb_t chr_min_cv;
    std::array<oct_bb_t, SLOT_COUNT> slot_cv;

    orientation_t ori;
    orientation_t ori_old;

    // data for doing the physics in bump_all_objects()
    uint8_t stoppedby;
    ObjectRef bumplist_next;

    // movement properties
    turn_mode_t turnmode;

    bool inwater;
    int dismount_timer;
    ObjectRef dismount_object;

    bool _terminateRequested;
    ObjectRef _objRef;
    ObjectProfileRef _profileID;
    std::shared_ptr<ObjectProfile> _profile;
    bool _showStatus;
    bool _isAlive;
    std::string _name;

    // attributes
    float _currentLife;
    float _currentMana;
    std::array<float, Ego::Attribute::NR_OF_ATTRIBUTES> _baseAttribute;
    std::unordered_map<Ego::Attribute::AttributeType, float, std::hash<uint8_t>> _tempAttribute;

    Inventory _inventory;
    uint16_t _money;
    std::bitset<Ego::Perks::NR_OF_PERKS> _perks;
    uint32_t _levelUpSeed;

    // Input commands
    std::bitset<LATCHBUTTON_COUNT> _inputLatchesPressed;
};
