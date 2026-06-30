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

/// @file egolib/game/Entities/Object_combat_stats.cpp
/// @brief Attribute-derived combat math split out of the Object_combat.cpp event
///        pipeline: damage-resistance computation, directional invincibility, and
///        experience/level progression.

#include "egolib/Entities/Object_internal.h"
#include "egolib/Audio/IAudioSystem.hpp"
#include "egolib/game/CharacterParticleOps.h"                    // DisplayMsg_printf
#include "egolib/Script/IScriptSystem.hpp"                       // activeScriptSystem() driver seam
#include "egolib/Graphics/IBillboardSystem.hpp"  // Ego::Graphics::activeBillboardSystem
#include "egolib/egoboo_setup.h"                 // activeConfig
#include "egolib/game/Graphics/Billboard.hpp"  // Ego::Graphics::Billboard::Flags
#include "egolib/game/Logic/Player.hpp"        // Ego::Player (complete type)

namespace
{
egoboo_config_t& config()
{
    return Ego::activeConfig();
}

IAudioSystem& audioSystem()
{
    return activeAudioSystem();
}

Object* heldItem(const IInventoryHolder& object, slot_t slot)
{
    return worldObjectHandler().get(object.getHeldObject(slot));
}
}

void Object::checkLevelUp()
{
    // Do level ups and stat changes
    uint8_t curlevel = experiencelevel + 1;
    if ( curlevel < MAXLEVEL )
    {
        uint32_t xpcurrent = experience;
        uint32_t xpneeded  = getProfile()->getXPNeededForLevel(curlevel);

        if ( xpcurrent >= xpneeded )
        {
            // The character is ready to advance...
            if(isPlayer()) {
                const std::shared_ptr<Ego::Player> &player = sessionState().playerList()[is_which_player];
                if(!player->hasUnspentLevel()) {
                    player->setLevelUpIndicator(true);
                    DisplayMsg_printf("%s gained a level!!!", getName().c_str());
                    audioSystem().playSoundFull(audioSystem().getGlobalSound(GSND_LEVELUP));
                }
                return;
            }

            //Automatic level up for AI characters
            experiencelevel++;
            SET_BIT(ai.alert, ALERTIF_LEVELUP);

            // Size Increase
            fat_goto += getProfile()->getSizeGainPerMight() * 0.25f;  // Limit this?
            fat_goto_time += SIZETIME;

            //Primary Attribute increase
            for(size_t i = 0; i < Ego::Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i) {
                _baseAttribute[i] += Random::next(getProfile()->getAttributeGain(static_cast<Ego::Attribute::AttributeType>(i)));
            }

            //Grab random Perk? (ZF> just uncomment if we want to do this for AI characters as well)
            //std::vector<Ego::Perks::PerkID> perkPool = getValidPerks();
            //if(!perkPool.empty()) {
            //    addPerk(Random::getRandomElement(perkPool));
            //}
            //else {
            //    addPerk(Ego::Perks::TOUGHNESS); //Add TOUGHNESS as default perk if none other are available
            //}
        }
    }
}

void Object::giveExperience(const int amount, const XPType xptype, const bool overrideInvincibility)
{
    //No xp to give
    if (0 == amount) return;

    if (!isInvincible() || overrideInvincibility)
    {
        // Figure out how much experience to give
        float newamount = amount;
        if ( xptype < XP_COUNT )
        {
            newamount = amount * getProfile()->getExperienceRate(xptype);
        }

        // Intellect affects xp gained (1% per intellect above 10, -1% per intellect below 10)
        float intadd = (getAttribute(Ego::Attribute::INTELLECT) - 10.0f) / 100.0f;
        newamount *= 1.00f + intadd;

        // Apply XP bonus/penality depending on game difficulty
        if (config().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
        {
            newamount *= 1.20f; // 20% extra on hard
        }
        else if (config().game_difficulty.getValue() >= Ego::GameDifficulty::Normal)
        {
            newamount *= 1.10f; // 10% extra on normal
        }


        //Fast Learner Perk gives +20% XP gain
        if(hasPerk(Ego::Perks::FAST_LEARNER)) {
            newamount *= 1.20f;
        }

        //Bookworm Perk gives +10% XP gain
        if(hasPerk(Ego::Perks::BOOKWORM)) {
            newamount *= 1.10f;
        }

        experience += newamount;
    }
}

float Object::getRawDamageResistance(const DamageType type, const bool includeArmor) const
{
    if(type >= DAMAGE_COUNT) {
        return 0.0f;
    }

    float resistance = getAttribute(Ego::Attribute::resistFromDamageType(type));

    //Stalwart perk increases CRUSH, SLASH and POKE by 1
    if(type == DAMAGE_CRUSH || type == DAMAGE_POKE || type == DAMAGE_SLASH) {
        if(hasPerk(Ego::Perks::STALWART)) {
            resistance += 1.0f;
        }
    }

    //Elemental Resistance perk increases FIRE, ICE and ZAP by 1
    if(type == DAMAGE_FIRE || type == DAMAGE_ICE || type == DAMAGE_ZAP) {
        if(hasPerk(Ego::Perks::ELEMENTAL_RESISTANCE)) {
            resistance += 1.0f;
        }

        //Ward perks increases it by further 3
        if(type == DAMAGE_FIRE && hasPerk(Ego::Perks::FIRE_WARD)) {
            resistance += 3.0f;
        }
        else if(type == DAMAGE_ZAP && hasPerk(Ego::Perks::ZAP_WARD)) {
            resistance += 3.0f;
        }
        else if(type == DAMAGE_ICE && hasPerk(Ego::Perks::ICE_WARD)) {
            resistance += 3.0f;
        }
    }

    //Rosemary perk gives +4
    if(type == DAMAGE_EVIL && hasPerk(Ego::Perks::ROSEMARY)) {
        resistance += 4.0f;
    }

    //Pyromaniac and Troll Blood perks *reduces* FIRE resistance by 10 each
    if(type == DAMAGE_FIRE) {
        if(hasPerk(Ego::Perks::PYROMANIAC)) {
            resistance -= 10.0f;
        }
        if(hasPerk(Ego::Perks::TROLL_BLOOD)) {
            resistance -= 10.0f;
        }
    }

    //Negative armor means it's a weakness
    if(resistance < 0.0f) {

        //Defence reduces weakness, but cannot eliminate it completely (50% weakness reduction at 255 defence)
        if(includeArmor) {
            resistance *= 1.0f - (getAttribute(Ego::Attribute::DEFENCE) / 512.0f);
        }
        return resistance;
    }

    //Defence bonus increases all damage type resistances (every 14 points gives +1.0 resistance)
    //This means at 255 defence and 0% resistance results in 52% damage reduction
    if(includeArmor && HAS_NO_BITS( static_cast<int>(getAttribute(Ego::Attribute::modifierFromDamageType(type))), DAMAGEINVERT)) {
        resistance += getAttribute(Ego::Attribute::DEFENCE) / 14.0f;
    }

    return resistance;
}

float Object::getDamageReduction(const DamageType type, const bool includeArmor) const
{
    //DAMAGE_COUNT simply means not affected by damage resistances
    if(type >= DAMAGE_COUNT) {
        return 0.0f;
    }

    //Immunity to damage type?
    if( HAS_SOME_BITS(static_cast<int>(getAttribute(Ego::Attribute::modifierFromDamageType(type))), DAMAGEINVICTUS) ) {
        return 1.0f;
    }

    const float resistance = getRawDamageResistance(type, includeArmor);

    //Negative resistance *increases* damage a lot
    if(resistance < 0.0f) {
        return 1.0f - std::pow(0.94f, resistance);
    }

    //Positive resistance reduces damage, but never 100%
    return ((resistance*0.06f) / (1.0f + resistance*0.06f));
}

bool Object::isInvictusDirection(Facing direction) const
{
    Facing left, right;

    static const Facing MAX = Facing(std::numeric_limits<uint16_t>::max());

    // if the invictus flag is set, we are invictus
    if (isInvincible()) return true;

    // if the character's frame is invictus, then check the angles
    if (HAS_SOME_BITS(inst.getFrameFX(), MADFX_INVICTUS))
    {
        //I Frame
        direction -= Facing(getProfile()->getInvictusFrameFacing());
        left       = MAX - Facing(getProfile()->getInvictusFrameAngle());
        right      = Facing(getProfile()->getInvictusFrameAngle());

        // If using shield, use the shield invictus instead
        if (ACTION_IS_TYPE(inst.getCurrentAnimation(), P))
        {
            bool parry_left = ( inst.getCurrentAnimation() < ACTION_PC );
            const Object* leftHandItem = heldItem(*this, SLOT_LEFT);
            const Object* rightHandItem = heldItem(*this, SLOT_RIGHT);

            // Using a shield?
            if (parry_left && leftHandItem)
            {
                // Check left hand
                // 0x00010000L ~ 65536 ~ 2^16
                left = MAX - Facing(leftHandItem->getProfile()->getInvictusFrameAngle());
                right = Facing(leftHandItem->getProfile()->getInvictusFrameAngle());
            }
            else if (rightHandItem)
            {
                // Check right hand
                left = MAX - Facing(rightHandItem->getProfile()->getInvictusFrameAngle());
                right = Facing(rightHandItem->getProfile()->getInvictusFrameAngle());
            }
        }
    }
    else
    {
        // Non invictus Frame
        direction -= Facing(getProfile()->getNormalFrameFacing());
        left = MAX - Facing(getProfile()->getNormalFrameAngle());
        right = Facing(getProfile()->getNormalFrameAngle());
    }

    // Check that direction
    if (direction <= left && direction <= right) {
        return true;
    }

    return false;
}
