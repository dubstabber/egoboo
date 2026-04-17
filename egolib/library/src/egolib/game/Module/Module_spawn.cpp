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

/// @file egolib/game/Module/Module_spawn.cpp
/// @brief GameModule spawn, player-join, and spawn-file realization helpers.

#include "egolib/game/Module/Module_internal.h"
#include "egolib/game/Module/Module_player_startup.hpp"
#include "egolib/game/Module/Module_spawn_plan.hpp"
#include "egolib/game/Module/Module_spawn_realization.hpp"

ObjectRef GameModule::getShopOwner(const float x, const float y)
{
    // Loop through every passage.
    for (const std::shared_ptr<Passage>& passage : _passages) {
        // Only check actual shops.
        if (!passage->isShop()) {
            continue;
        }

        // Is item inside this shop?
        if (passage->isPointInside(x, y))
        {
            return passage->getShopOwner();
        }
    }

    return Passage::SHOP_NOOWNER;
}

void GameModule::removeShopOwner(ObjectRef owner)
{
    // Loop through every passage:
    for (const std::shared_ptr<Passage> &passage : _passages)
    {
        // Only check actual shops:
        if (!passage->isShop())
        {
            continue;
        }

        if (passage->getShopOwner() == owner)
        {
            passage->removeShop();
        }

        // TODO: Mark all items in shop as normal items again.
    }
}

std::shared_ptr<Object> GameModule::spawnObject(const Ego::Vector3f& pos, ObjectProfileRef profile, const TEAM_REF team, const int skin,
                                                const Facing& facing, const std::string &name, const ObjectRef override)
{
    const std::shared_ptr<ObjectProfile> &ppro = ProfileSystem::get().getProfile(profile);
    if (!ppro)
    {
        if (profile.get() > getImportAmount() * MAX_IMPORT_PER_PLAYER)
        {
            Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "attempt to spawn object from an invalid object profile ", "`", profile, "`", Log::EndOfEntry);
        }
        return Object::INVALID_OBJECT;
    }

    // count all the requests for this character type
    ppro->_spawnRequestCount++;

    // allocate a new character
    std::shared_ptr<Object> pchr = getObjectHandler().insert(profile, override);
    if (!pchr) {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to spawn character", Log::EndOfEntry);
        return Object::INVALID_OBJECT;
    }

    // just set the spawn info
    pchr->setBaseSkin(skin);

    // download all the values from the character spawn_ptr->profile
    // Set up model stuff
    pchr->setStoppedByMask(ppro->getStoppedByMask());
    pchr->setNameKnown(ppro->isNameKnown());
    pchr->setAmmoKnown(ppro->isNameKnown());
    pchr->setDrawIcon(ppro->isDrawIcon());

    // Starting Perks
    for (size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i) {
        Ego::Perks::PerkID id = static_cast<Ego::Perks::PerkID>(i);
        if (ppro->beginsWithPerk(id)) {
            pchr->addPerk(id);
        }
    }

    // Ammo
    pchr->setAmmoMax(ppro->getMaxAmmo());
    pchr->setAmmo(ppro->getAmmo());

    // Gender
    switch (ppro->getGender()) {
        case GenderProfile::Male:
            pchr->setGender(Gender::Male);
            break;
        case GenderProfile::Female:
            pchr->setGender(Gender::Female);
            break;
        case GenderProfile::Neuter:
            pchr->setGender(Gender::Neuter);
            break;
        case GenderProfile::Random:
            /// 50% male or female.
            /// @todo And what about Neuter?
            if (Random::nextBool()) {
                pchr->setGender(Gender::Female);
            } else {
                pchr->setGender(Gender::Male);
            }
            break;
    }

    // Life and Mana bars
    pchr->setBaseAttribute(Ego::Attribute::LIFE_BARCOLOR, ppro->getLifeColor());
    pchr->setBaseAttribute(Ego::Attribute::MANA_BARCOLOR, ppro->getManaColor());

    // Flags
    pchr->setDamageTargetType(ppro->getDamageTargetType());
    pchr->setBaseAttribute(Ego::Attribute::WALK_ON_WATER, ppro->canWalkOnWater() ? 1.0f : 0.0f);
    pchr->setPlatform(ppro->isPlatform());
    pchr->setCanUsePlatforms(ppro->canUsePlatforms());
    pchr->setItem(ppro->isItem());
    pchr->setInvincible(ppro->isInvincible());

    // Jumping
    pchr->setBaseAttribute(Ego::Attribute::JUMP_POWER, ppro->getJumpPower());
    pchr->setBaseAttribute(Ego::Attribute::NUMBER_OF_JUMPS, ppro->getJumpNumber());

    // Other junk
    pchr->setBaseAttribute(Ego::Attribute::FLY_TO_HEIGHT, ppro->getFlyHeight());
    pchr->phys.dampen = ppro->getBounciness();

    pchr->phys.bumpdampen = ppro->getBumpDampen();
    if (CAP_INFINITE_WEIGHT == ppro->getWeight())
    {
        pchr->phys.weight = Ego::Physics::CHR_INFINITE_WEIGHT;
    }
    else
    {
        uint32_t itmp = ppro->getWeight() * ppro->getSize() * ppro->getSize() * ppro->getSize();
        pchr->phys.weight = std::min(itmp, Ego::Physics::CHR_MAX_WEIGHT);
    }

    // Extra spawn money is added later
    pchr->giveMoney(ppro->getStartingMoney());

    // Experience
    pchr->setExperience(Random::next(ppro->getStartingExperience()));
    pchr->setExperienceLevelIndex(ppro->getStartingLevel());

    // Particle attachments
    pchr->setReaffirmDamageType(ppro->getReaffirmDamageType());

    // Character size and bumping
    pchr->setBaseFat(ppro->getSize());
    pchr->setBaseShadowSize(ppro->getShadowSize());
    bumper_t baseBump;
    baseBump.size = ppro->getBumpSize();
    baseBump.size_big = ppro->getBumpSizeBig();
    baseBump.height = ppro->getBumpHeight();
    pchr->initializeBaseBump(baseBump);

    //Initialize size and collision box
    pchr->setFatRaw(pchr->getBaseFat());
    pchr->setSavedShadowSize(pchr->getBaseShadowSize());
    pchr->recalculateCollisionSize();

    // Character size and bumping
    pchr->setTargetFat(pchr->getFat());
    pchr->setResizeTimeRemaining(0);

    // Kurse state
    if (ppro->isItem())
    {
        uint16_t kursechance = ppro->getKurseChance();
        if (egoboo_config_t::get().game_difficulty.getValue() >= Ego::GameDifficulty::Hard)
        {
            kursechance *= 2.0f;  // Hard mode doubles chance for Kurses
        }
        if (egoboo_config_t::get().game_difficulty.getValue() < Ego::GameDifficulty::Normal && kursechance != 100)
        {
            kursechance *= 0.5f;  // Easy mode halves chance for Kurses
        }
        pchr->setKursed(Random::getPercent() <= kursechance);
    }

    //Set our position
    pchr->setPosition(pos);
    pchr->setSpawnPosition(pos);

    // AI stuff
    ai_state_t::spawn(pchr->ai, pchr->getObjRef(), pchr->getProfileID().get(), getTeamList()[team].getMorale());

    // Team stuff
    pchr->setTeamRef(team);
    pchr->setBaseTeamRef(team);
    if (!pchr->isInvincible()) {
        getTeamList()[team].increaseMorale();
    }

    // Firstborn becomes the leader
    if (!getTeamList()[team].getLeader())
    {
        getTeamList()[team].setLeader(pchr);
    }

    // getSkinOverride() can return NO_SKIN_OVERRIDE, so we need to check
    // for the "random skin marker" even if that function is called
    if (ppro->getSkinOverride() != ObjectProfile::NO_SKIN_OVERRIDE)
    {
        pchr->setBaseSkin(ppro->getSkinOverride());
    }

    //Negative skin number means random skin
    if (pchr->getBaseSkin() < 0 || !ppro->isValidSkin(pchr->getBaseSkin()))
    {
        // This is a "random" skin.
        // Force it to some specific value so it will go back to the same skin every respawn
        // We are now ensuring that there are skin graphics for all skins, so there
        // is no need to count the skin graphics loaded into the profile.
        // Limiting the available skins to ones that had unique graphics may have been a mistake since
        // the skin-dependent properties in data.txt may exist even if there are no unique graphics.
        pchr->setBaseSkin(ppro->getRandomSkinID());
    }

    // actually set the character skin
    pchr->setSkin(pchr->getBaseSkin());

    // override the default behavior for an "easy" game
    if (egoboo_config_t::get().game_difficulty.getValue() < Ego::GameDifficulty::Normal)
    {
        pchr->setLife(pchr->getAttribute(Ego::Attribute::MAX_LIFE));
        pchr->setMana(pchr->getAttribute(Ego::Attribute::MAX_MANA));
    }
    else {
        pchr->setLife(ppro->getSpawnLife());
        pchr->setMana(ppro->getSpawnMana());
    }

    //Facing
    pchr->setFacingZ(Facing(FACING_T(facing)));
    pchr->setPreviousFacingZ(pchr->getFacingZ());

    // Name the character
    if (name.empty())
    {
        // Generate a random name
        pchr->setName(ppro->generateRandomName());
    }
    else
    {
        // A name has been given
        pchr->setName(name);
    }

    // Particle attachments
    for (uint8_t tnc = 0; tnc < ppro->getAttachedParticleAmount(); tnc++)
    {
        ParticleHandler::get().spawnParticle(pchr->getPosition(), pchr->getFacingZ(), ppro->getSlotNumber(), ppro->getAttachedParticleProfile(),
                                             pchr->getObjRef(), GRIP_LAST + tnc, pchr->getTeamRef(), pchr->getObjRef(), ParticleRef::Invalid, tnc);
    }

    // is the object part of a shop's inventory?
    if (pchr->isItem())
    {
        // Items that are spawned inside shop passages are more expensive than normal

        ObjectRef shopOwner = getShopOwner(pchr->getPosX(), pchr->getPosY());
        if (shopOwner != Passage::SHOP_NOOWNER) {
            pchr->setShopItem(true);               // Full value
            pchr->setKursed(false);                // Shop items are never kursed
            pchr->setNameKnown(true);              // identified
            pchr->setAmmoKnown(true);
        }
        else {
            pchr->setShopItem(false);
        }
    }

    chr_update_matrix(pchr.get(), true);

    // start the character out in the "dance" animation
    pchr->inst.startAnimation(ACTION_DA, true, true);

    // count all the successful spawns of this character
    ppro->_spawnCount++;

#if defined(DEBUG_OBJECT_SPAWN) && defined(_DEBUG)
    log_debug("spawnObject() - slot: %i, index: %i, name: %s, class: %s\n", REF_TO_INT(profile), REF_TO_INT(pchr->getCharacterID()), name.c_str(), ppro->getClassName().c_str());
#endif

    return pchr;
}

bool GameModule::addPlayer(const std::shared_ptr<Object>& object, const Ego::Input::InputDevice &device)
{
    return addPlayer(object, device, false);
}

bool GameModule::addPlayer(const std::shared_ptr<Object>& object,
                           const Ego::Input::InputDevice& device,
                           const bool identifySpawnOnSuccess)
{
    return module_player_startup::addPlayer(_playerList, object, device, identifySpawnOnSuccess);
}

void GameModule::spawnAllObjects()
{
    //First load treasure tables
    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");
    auto spawnPlan = module_spawn_plan::buildSpawnPlan(
        SpawnFileReader().read("mp_data/spawn.txt"),
        treasureTables,
        [](ObjectProfileRef profileSlot)
        {
            return ProfileSystem::get().isLoaded(profileSlot);
        });

    std::shared_ptr<Object> parent = nullptr;
    for (auto& spawnInfo : spawnPlan.entries)
    {
        // If nothing is already in that slot, try to load it.
        if (!ProfileSystem::get().isLoaded(spawnInfo.slot))
        {
            bool import_object = spawnInfo.slot > (getImportAmount() * MAX_IMPORT_PER_PLAYER);

            if (!activate_spawn_file_load_object(spawnInfo))
            {
                // no, give a warning if it is useful
                if (import_object)
                {
                    Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                                     "the object ", "`", spawnInfo.spawn_comment, "`", " in slot ",
                                                     spawnInfo.slot, " in file ", "`", "mp_data/spawn,txt", "`",
                                                     "does not exist on this machine", Log::EndOfEntry);
                }
                continue;
            }
        }

        // we only reach this if everything was loaded properly
        std::shared_ptr<Object> spawnedObject = spawnObjectFromFileEntry(spawnInfo, parent);

        //We might become the new parent
        if (spawnedObject != nullptr && spawnInfo.attach == ATTACH_NONE) {
            parent = spawnedObject;
        }
    }

    // Fix tilting trees problem
    tiltCharactersToTerrain();

    //now load the profile AI, do last so that all reserved slot numbers are initialized
    game_load_profile_ai();
}

std::shared_ptr<Object> GameModule::spawnObjectFromFileEntry(const spawn_file_info_t& psp_info, const std::shared_ptr<Object> &parent)
{
    module_spawn_realization::SpawnRealizationState state;
    state.importValid = isImportValid();
    state.importAmount = getImportAmount();
    state.playerAmount = getPlayerAmount();
    state.importList = &importList();
    state.importData = &import_data;
    state.isProfileLoaded = [](ObjectProfileRef profile)
    {
        return ProfileSystem::get().isLoaded(profile);
    };

    module_spawn_realization::SpawnRealizationOps ops;
    ops.spawnObject = [this](const spawn_file_info_t& spawnInfo)
    {
        const auto profile = ObjectProfileRef(static_cast<PRO_REF>(spawnInfo.slot));
        return spawnObject(spawnInfo.pos, profile, spawnInfo.team, spawnInfo.skin, spawnInfo.facing,
                           spawnInfo.spawn_name == "NONE" ? "" : spawnInfo.spawn_name, ObjectRef::Invalid);
    };
    ops.makeCharacterMatrix = [](const std::shared_ptr<Object>& object)
    {
        make_one_character_matrix(object->getObjRef());
    };
    ops.attachInventoryItem = [](const std::shared_ptr<Object>& parentObject, const std::shared_ptr<Object>& object)
    {
        Inventory::add_item(parentObject->getObjRef(), object->getObjRef(), parentObject->getInventory().getFirstFreeSlotNumber(), true);
    };
    ops.attachToGrip = [](const std::shared_ptr<Object>& parentObject, const std::shared_ptr<Object>& object, grip_offset_t grip)
    {
        return object->getObjectPhysics().attachToObject(parentObject, grip);
    };
    ops.currentPlayerCount = [this]()
    {
        return getPlayerList().size();
    };
    ops.currentLocalPlayerCount = []()
    {
        return GameSessionContext::get().localPlayerCount();
    };
    ops.addPlayer = [this](const std::shared_ptr<Object>& object, const module_spawn_realization::PlayerBindingRequest& request)
    {
        return addPlayer(object, Ego::Input::InputDevice::DeviceList[request.deviceIndex],
                         request.identifySpawnOnSuccess);
    };

    return module_spawn_realization::realizeSpawnEntry(psp_info, parent, state, ops);
}

void GameModule::tiltCharactersToTerrain()
{
    for (const std::shared_ptr<Object> &object : getObjectHandler().iterator())
    {
        if (object->isTerminated()) {
            continue;
        }

        if (object->getProfile()->hasStickyButt())
        {
            uint8_t twist = getMeshPointer()->get_twist(object->getTile());
            object->setMapTwistFacingY(Facing(g_meshLookupTables.twist_facing_y[twist]));
            object->setMapTwistFacingX(Facing(g_meshLookupTables.twist_facing_x[twist]));
        }
        else
        {
            object->setMapTwistFacingY(orientation_t::MAP_TURN_OFFSET);
            object->setMapTwistFacingX(orientation_t::MAP_TURN_OFFSET);
        }
    }
}
