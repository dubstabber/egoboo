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

/// @file egolib/game/Logic/LevelUp.cpp
/// @brief GUI-free character level-up computation, extracted verbatim (statement order
///        and RNG-consumption order preserved) from Ego::GUI::LevelUpWindow::doLevelUp.

#include "egolib/game/Logic/LevelUp.hpp"

#include "egolib/Entities/_Include.hpp"    // Object (complete type)
#include "egolib/Profiles/_Include.hpp"    // ObjectProfile (complete type; getAttributeGain/getSizeGainPerMight)
#include "egolib/Logic/Perk.hpp"           // Ego::Perks::Perk
#include "egolib/Math/Random.hpp"          // Random
#include "egolib/Script/script.h"          // ALERTIF_LEVELUP
#include "egolib/game/Logic/Player.hpp"    // Ego::Player::setLevelUpIndicator

namespace Ego
{

LevelUpReport applyCharacterLevelUp(Object& character,
                                     const Perks::Perk& selectedPerk,
                                     const std::vector<std::shared_ptr<Player>>& playerList)
{
    LevelUpReport report;

    //Set random seed for deterministic level ups (no aborting or re-loading game for better results)
    Random::setSeed(character.getLevelUpSeed());

    //Calculate attribute improvements
    for (uint8_t i = 0; i < Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i) {
        const Attribute::AttributeType type = static_cast<Attribute::AttributeType>(i);
        report.increase[i] = Random::next(character.getProfile()->getAttributeGain(type));
    }

    //Gain new Perk
    character.addPerk(selectedPerk.getID());

    //Gain attribute bonus from perks
    report.increase[selectedPerk.getType()] += 1.0f;

    //Some perks give flat attribute bonuses
    switch (selectedPerk.getID()) {
        case Perks::TOUGHNESS:
            report.increase[Attribute::MAX_LIFE] += 2.0f;
            break;

        case Perks::SOLDIERS_FORTITUDE:
            report.increase[Attribute::LIFE_REGEN] += 0.15f;
            break;

        case Perks::TROLL_BLOOD:
            report.increase[Attribute::LIFE_REGEN] += 0.25f;
            break;

        case Perks::GIGANTISM:
            report.increase[Attribute::MIGHT] += 2.00f;
            report.increase[Attribute::AGILITY] -= 2.00f;
            break;

        case Perks::BRUTE:
            report.increase[Attribute::MIGHT] += 1.00f;
            report.increase[Attribute::INTELLECT] -= 2.00f;
            break;

        case Perks::DRAGON_BLOOD:
            report.increase[Attribute::MANA_REGEN] += 0.25f;
            break;

        case Perks::ACROBATIC:
            character.increaseBaseAttribute(Attribute::NUMBER_OF_JUMPS, 1.0f);
            break;

        case Perks::MASTER_ACROBAT:
            character.increaseBaseAttribute(Attribute::NUMBER_OF_JUMPS, 1.0f);
            break;

        case Perks::POWER:
            report.increase[Attribute::MAX_MANA] += 2.00f;
            break;

        case Perks::PERFECTION:
            report.increase[Attribute::INTELLECT] += 1.00f;
            report.increase[Attribute::AGILITY] += 1.00f;
            break;

        case Perks::ANCIENT_BLUD:
            report.increase[Attribute::LIFE_REGEN] += 0.25f;
            break;

        case Perks::SPELL_MASTERY:
            report.increase[Attribute::SPELL_POWER] += 1.0f;
            break;

        case Perks::MYSTIC_INTELLECT:
            report.increase[Attribute::MAX_MANA] += 1.0f;
            report.increase[Attribute::MANA_REGEN] += 0.1f;
            break;

        case Perks::MEDITATION:
            report.increase[Attribute::MANA_REGEN] += 0.15f;
            break;

        case Perks::BOOKWORM:
            report.increase[Attribute::INTELLECT] += 2.00f;
            report.increase[Attribute::MIGHT] -= 2.00f;
            break;

        case Perks::NIGHT_VISION:
            character.increaseBaseAttribute(Attribute::DARKVISION, 1.0f);
            break;

        case Perks::SENSE_KURSES:
            character.increaseBaseAttribute(Attribute::SENSE_KURSES, 1.0f);
            break;

        case Perks::SENSE_INVISIBLE:
            character.increaseBaseAttribute(Attribute::SEE_INVISIBLE, 1.0f);
            break;

        default:
            //nothing
            break;
    }

    //Increase character level by 1
    character.setExperienceLevelIndex(character.getExperienceLevelIndex() + 1);
    character.addAIAlertBits(ALERTIF_LEVELUP);

    //NOTE: this indexes playerList without an isPlayer()/bounds guard, exactly like the
    //original GUI code did (LevelUpWindow.cpp historically) -- calling this for a non-player
    //Object is a pre-existing hazard (invalid PLA_REF / out-of-bounds access), preserved here
    //rather than fixed as part of this extraction.
    playerList[character.getPlayerNumber()]->setLevelUpIndicator(false);

    //Generate random seed for next level increase
    character.randomizeLevelUpSeed();

    //Might slightly increases character size
    if (report.increase[Attribute::MIGHT] != 0) {
        character.setTargetFat(character.getTargetFat() + character.getProfile()->getSizeGainPerMight() * 0.1f * report.increase[Attribute::MIGHT]);
        character.setResizeTimeRemaining(character.getResizeTimeRemaining() + Object::SIZETIME);
    }

    //Actually give attributes to character, recording the value displayed just before each is applied
    for (uint8_t i = 0; i < Attribute::NR_OF_PRIMARY_ATTRIBUTES; ++i) {
        const Attribute::AttributeType type = static_cast<Attribute::AttributeType>(i);
        report.displayedValue[i] = character.getAttribute(type);
        character.increaseBaseAttribute(type, report.increase[i]);
    }

    return report;
}

} // namespace Ego
