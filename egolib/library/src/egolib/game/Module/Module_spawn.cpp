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
    pchr->skin_stt  = skin;

    // download all the values from the character spawn_ptr->profile
    // Set up model stuff
    pchr->stoppedby = ppro->getStoppedByMask();
    pchr->nameknown = ppro->isNameKnown();
    pchr->ammoknown = ppro->isNameKnown();
    pchr->draw_icon = ppro->isDrawIcon();

    // Starting Perks
    for (size_t i = 0; i < Ego::Perks::NR_OF_PERKS; ++i) {
        Ego::Perks::PerkID id = static_cast<Ego::Perks::PerkID>(i);
        if (ppro->beginsWithPerk(id)) {
            pchr->addPerk(id);
        }
    }

    // Ammo
    pchr->ammomax = ppro->getMaxAmmo();
    pchr->ammo = ppro->getAmmo();

    // Gender
    switch (ppro->getGender()) {
        case GenderProfile::Male:
            pchr->gender = Gender::Male;
            break;
        case GenderProfile::Female:
            pchr->gender = Gender::Female;
            break;
        case GenderProfile::Neuter:
            pchr->gender = Gender::Neuter;
            break;
        case GenderProfile::Random:
            /// 50% male or female.
            /// @todo And what about Neuter?
            if (Random::nextBool()) {
                pchr->gender = Gender::Female;
            } else {
                pchr->gender = Gender::Male;
            }
            break;
    }

    // Life and Mana bars
    pchr->setBaseAttribute(Ego::Attribute::LIFE_BARCOLOR, ppro->getLifeColor());
    pchr->setBaseAttribute(Ego::Attribute::MANA_BARCOLOR, ppro->getManaColor());

    // Flags
    pchr->damagetarget_damagetype = ppro->getDamageTargetType();
    pchr->setBaseAttribute(Ego::Attribute::WALK_ON_WATER, ppro->canWalkOnWater() ? 1.0f : 0.0f);
    pchr->platform        = ppro->isPlatform();
    pchr->canuseplatforms = ppro->canUsePlatforms();
    pchr->isitem          = ppro->isItem();
    pchr->invictus        = ppro->isInvincible();

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
    pchr->experience = Random::next(ppro->getStartingExperience());
    pchr->experiencelevel = ppro->getStartingLevel();

    // Particle attachments
    pchr->reaffirm_damagetype = ppro->getReaffirmDamageType();

    // Character size and bumping
    pchr->fat_stt           = ppro->getSize();
    pchr->shadow_size_stt   = ppro->getShadowSize();
    pchr->bump_stt.size     = ppro->getBumpSize();
    pchr->bump_stt.size_big = ppro->getBumpSizeBig();
    pchr->bump_stt.height   = ppro->getBumpHeight();

    //Initialize size and collision box
    pchr->fat                = pchr->fat_stt;
    pchr->shadow_size_save   = pchr->shadow_size_stt;
    pchr->bump_save.size     = pchr->bump_stt.size;
    pchr->bump_save.size_big = pchr->bump_stt.size_big;
    pchr->bump_save.height   = pchr->bump_stt.height;
    pchr->recalculateCollisionSize();

    // Character size and bumping
    pchr->fat_goto      = pchr->fat;
    pchr->fat_goto_time = 0;

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
        pchr->iskursed = Random::getPercent() <= kursechance;
    }

    //Set our position
    pchr->setPosition(pos);
    pchr->setSpawnPosition(pos);

    // AI stuff
    ai_state_t::spawn(pchr->ai, pchr->getObjRef(), pchr->getProfileID().get(), getTeamList()[team].getMorale());

    // Team stuff
    pchr->team     = team;
    pchr->team_base = team;
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
        pchr->skin_stt = ppro->getSkinOverride();
    }

    //Negative skin number means random skin
    if (pchr->skin_stt < 0 || !ppro->isValidSkin(pchr->skin_stt))
    {
        // This is a "random" skin.
        // Force it to some specific value so it will go back to the same skin every respawn
        // We are now ensuring that there are skin graphics for all skins, so there
        // is no need to count the skin graphics loaded into the profile.
        // Limiting the available skins to ones that had unique graphics may have been a mistake since
        // the skin-dependent properties in data.txt may exist even if there are no unique graphics.
        pchr->skin_stt = ppro->getRandomSkinID();
    }

    // actually set the character skin
    pchr->setSkin(pchr->skin_stt);

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
    pchr->ori.facing_z     = Facing(FACING_T(facing));
    pchr->ori_old.facing_z = pchr->ori.facing_z;

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
        ParticleHandler::get().spawnParticle(pchr->getPosition(), pchr->ori.facing_z, ppro->getSlotNumber(), ppro->getAttachedParticleProfile(),
                                             pchr->getObjRef(), GRIP_LAST + tnc, pchr->team, pchr->getObjRef(), ParticleRef::Invalid, tnc);
    }

    // is the object part of a shop's inventory?
    if (pchr->isItem())
    {
        // Items that are spawned inside shop passages are more expensive than normal

        ObjectRef shopOwner = getShopOwner(pchr->getPosX(), pchr->getPosY());
        if (shopOwner != Passage::SHOP_NOOWNER) {
            pchr->isshopitem = true;               // Full value
            pchr->iskursed   = false;              // Shop items are never kursed
            pchr->nameknown  = true;               // identified
            pchr->ammoknown  = true;
        }
        else {
            pchr->isshopitem = false;
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
    if (!object || object->isTerminated()) {
        return false;
    }

    std::shared_ptr<Ego::Player> player = std::make_shared<Ego::Player>(object, device);
    _playerList.push_back(player);

    // set the reference to the player
    object->is_which_player = _playerList.size() - 1;

    // download the quest info
    player->getQuestLog().loadFromFile(object->getProfile()->getPathname());

    //Local player added
    local_stats.noplayers = false;
    object->islocalplayer = true;
    local_stats.player_count++;

    return true;
}

void GameModule::spawnAllObjects()
{
    std::unordered_map<int, std::string> reservedSlots;
    std::unordered_set<std::string> dynamicObjectList;
    std::vector<spawn_file_info_t> objectsToSpawn;

    //First load treasure tables
    Ego::TreasureTables treasureTables("mp_data/randomtreasure.txt");

    // Turn some back on
    auto entries = (SpawnFileReader()).read("mp_data/spawn.txt");
    {
        std::shared_ptr<Object> parent = nullptr;

        for (auto& entry : entries)
        {
            //Spit out a warning if they break the limit
            if (objectsToSpawn.size() >= OBJECTS_MAX)
            {
                Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "too many objects in file ", "`",
                                                 "mp_data/spawn,txt", "`", ". Maximum number of objects is ", OBJECTS_MAX,
                                                 Log::EndOfEntry);
                break;
            }

            // check to see if the slot is valid
            if (entry.slot >= INVALID_PRO_REF)
            {
                Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "invalid slot ", entry.slot,
                                                 " for ", "`", entry.spawn_comment, "`", " in file ", "`", "mp_data/spawn,txt", "`",
                                                 Log::EndOfEntry);
                continue;
            }

            //convert the spawn name into a format we like
            convert_spawn_file_load_name(entry, treasureTables);

            // If it is a dynamic slot, remember to dynamically allocate it for later
            if (entry.slot <= -1)
            {
                dynamicObjectList.insert(entry.spawn_comment);
            }

            //its a static slot number, mark it as reserved if it isnt already
            else if (reservedSlots[entry.slot].empty())
            {
                reservedSlots[entry.slot] = entry.spawn_comment;
            }

            //Finished with this object for now
            objectsToSpawn.push_back(entry);
        }

        //Next we dynamically find slot numbers for each of the objects in the dynamic list
        for (const std::string &spawnName : dynamicObjectList)
        {
            ObjectProfileRef profileSlot;

            //Find first free slot that is not the spellbook slot
            for (profileSlot = ObjectProfileRef(1 + MAX_IMPORT_PER_PLAYER * MAX_PLAYER); profileSlot < ObjectProfileRef::Invalid; ++profileSlot)
            {
                //don't try to grab loaded profiles
                if (ProfileSystem::get().isLoaded(profileSlot)) continue;

                //the slot already dynamically loaded by a different spawn object of the same type that we are, no need to reload in a new slot
                if (reservedSlots[profileSlot.get()] == spawnName) {
                    break;
                }

                //found a completely free slot
                if (reservedSlots[profileSlot.get()].empty())
                {
                    //Reserve this one for us
                    reservedSlots[profileSlot.get()] = spawnName;
                    break;
                }
            }

            //If all slots are reserved, spit out a warning (very unlikely unless there is a bug somewhere)
            if (profileSlot == ObjectProfileRef::Invalid) {
                Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to acquire free dynamic slot for object ", spawnName, ". All slots in use?", Log::EndOfEntry);
            }
        }

        //Now spawn each object in order
        for (spawn_file_info_t &spawnInfo : objectsToSpawn)
        {
            //Dynamic slot number? Then figure out what slot number is assigned to us
            if (spawnInfo.slot <= -1) {
                for (const auto &element : reservedSlots)
                {
                    if (element.second == spawnInfo.spawn_comment)
                    {
                        spawnInfo.slot = element.first;
                        break;
                    }
                }
            }

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
    }

    // Fix tilting trees problem
    tiltCharactersToTerrain();

    //now load the profile AI, do last so that all reserved slot numbers are initialized
    game_load_profile_ai();
}

std::shared_ptr<Object> GameModule::spawnObjectFromFileEntry(const spawn_file_info_t& psp_info, const std::shared_ptr<Object> &parent)
{
    if (!psp_info.do_spawn || psp_info.slot < 0) {
        return nullptr;
    }

    auto iprofile = ObjectProfileRef(static_cast<PRO_REF>(psp_info.slot));

    //Require a valid parent?
    if (psp_info.attach != ATTACH_NONE && !parent) {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to spawn ", "`", psp_info.spawn_name, "`", " due to missing parent", Log::EndOfEntry);
        return nullptr;
    }

    // Spawn the character
    std::shared_ptr<Object> pobject = spawnObject(psp_info.pos, iprofile, psp_info.team, psp_info.skin, psp_info.facing, psp_info.spawn_name == "NONE" ? "" : psp_info.spawn_name, ObjectRef::Invalid);

    //Failed to spawn?
    if (!pobject) {
        Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to spawn ", "`", psp_info.spawn_name, "`", Log::EndOfEntry);
        return nullptr;
    }

    //Add money
    pobject->giveMoney(psp_info.money);

    //Set AI stuff
    pobject->ai.content = psp_info.content;
    pobject->ai.passage = psp_info.passage;

    // determine the attachment
    switch (psp_info.attach)
    {
        case ATTACH_NONE:
            make_one_character_matrix(pobject->getObjRef());
        break;

        case ATTACH_INVENTORY:
            // Inventory character
            Inventory::add_item(parent->getObjRef(), pobject->getObjRef(), parent->getInventory().getFirstFreeSlotNumber(), true);

            //If the character got merged into a stack, then it will be marked as terminated
            if (pobject->isTerminated()) {
                return nullptr;
            }

            // Make spellbooks change
            SET_BIT(pobject->ai.alert, ALERTIF_GRABBED);
        break;

        case ATTACH_LEFT:
        case ATTACH_RIGHT:
            // Wielded character
            grip_offset_t grip_off = (ATTACH_LEFT == psp_info.attach) ? GRIP_LEFT : GRIP_RIGHT;

            if (pobject->getObjectPhysics().attachToObject(parent, grip_off)) {
                // Preserve the initial grabbed alert so startup equipment can
                // consume IfGrabbed on its first script update.
            }
        break;
    }

    // Set the starting pinfo->level
    if (psp_info.level > 0) {
        if (pobject->experiencelevel < psp_info.level) {
            pobject->experience = pobject->getProfile()->getXPNeededForLevel(psp_info.level);
        }
    }

    // automatically identify and unkurse all player starting equipment? I think yes.
    if (!isImportValid() && nullptr != parent && parent->isPlayer()) {
        pobject->nameknown = true;
        pobject->iskursed = false;
    }

    // Turn on input devices
    if (psp_info.stat)
    {
        // what we do depends on what kind of module we're loading
        if (0 == getImportAmount() && getPlayerList().size() < getPlayerAmount())
        {
            // a single player module
            bool player_added = addPlayer(pobject, Ego::Input::InputDevice::DeviceList[local_stats.player_count]);

            if (getImportAmount() == 0 && player_added)
            {
                // !!!! make sure the player is identified !!!!
                pobject->nameknown = true;
            }
        }
        else if (getPlayerList().size() < getImportAmount() && getPlayerList().size() < getPlayerAmount() && getPlayerList().size() < importList().count)
        {
            // A multiplayer module
            int local_index = -1;
            for (size_t tnc = 0; tnc < importList().count; tnc++)
            {
                if (pobject->getProfileID().get() <= import_data.max_slot && ProfileSystem::get().isLoaded(pobject->getProfileID()))
                {
                    int islot = pobject->getProfileID().get();

                    if (import_data.slot_lst[islot] == importList().lst[tnc].slot)
                    {
                        local_index = tnc;
                        break;
                    }
                }
            }

            if (-1 != local_index)
            {
                // It's a local input
                addPlayer(pobject, Ego::Input::InputDevice::DeviceList[importList().lst[local_index].local_player_num]);
            }
            else
            {
                // It's a remote input
                std::logic_error("Remote input control no longer supported");
            }
        }
    }

    return pobject;
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
            object->ori.map_twist_facing_y = Facing(g_meshLookupTables.twist_facing_y[twist]);
            object->ori.map_twist_facing_x = Facing(g_meshLookupTables.twist_facing_x[twist]);
        }
        else
        {
            object->ori.map_twist_facing_y = orientation_t::MAP_TURN_OFFSET;
            object->ori.map_twist_facing_x = orientation_t::MAP_TURN_OFFSET;
        }
    }
}
