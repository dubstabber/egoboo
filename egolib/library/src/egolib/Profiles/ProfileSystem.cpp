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

/// @file  egolib/Profiles/ProfileSystem.cpp
/// @brief Implementation of functions for controlling and accessing object profiles
/// @details
/// @author Johan Jansen

#define EGOLIB_PROFILES_PRIVATE 1
#include "egolib/Profiles/ProfileSystem.hpp"
#include "egolib/Profiles/ObjectProfile.hpp"
#include "egolib/Profiles/ModuleProfile.hpp"
#include "egolib/game/GameStates/LoadPlayerElement.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h"  // MAX_IMPORT_PER_PLAYER / MAX_IMPORT_OBJECTS macros
#include "egolib/game/script_compile.h"
#include "egolib/fileutil.h"
#include "idlib/exception.hpp"                 // idlib::exception (base of the parser exceptions)

AbstractProfileSystem<EnchantProfile, EnchantProfileRef> EnchantProfileSystem("enchant", "/debug/enchant_profile_usage.txt");
AbstractProfileSystem<ParticleProfile, ParticleProfileRef> ParticleProfileSystem("particle", "/debug/particle_profile_usage.txt");

//Globals
pro_import_t import_data;

static const std::shared_ptr<ObjectProfile> NULL_PROFILE = nullptr;

ProfileSystem *ProfileSystemCreateFunctor::operator()(Log::Target& logTarget) const
{ return new ProfileSystem(logTarget); }

void ProfileSystemDestroyFunctor::operator()(ProfileSystem *p) const
{ delete p; }

ProfileSystem::ProfileSystem(Log::Target& logTarget) :
    _profilesLoaded(),
    _profilesLoadedByName(),
    _moduleProfilesLoaded(),
    _loadPlayerList(),
    _logTarget(logTarget),
    EnchantProfileSystem("enchant", "/debug/enchant_profile_usage.txt"),
    ParticleProfileSystem("particle", "/debug/particle_profile_usage.txt")
{
    // Initialize the script compiler.
    parser_state_t::initialize();
}


ProfileSystem::~ProfileSystem()
{
    // Uninitialize the script compiler.
    parser_state_t::uninitialize();
}

void ProfileSystem::reset()
{
    /// @author ZZ
    /// @details This function clears out all of the model data

    // Release the allocated data in all profiles (sounds, textures, etc.).
    _profilesLoaded.clear();
    _profilesLoadedByName.clear();

    // Release list of loadable characters.
    _loadPlayerList.clear();

    // Reset particle, enchant and models.
    ParticleProfileSystem.unloadAll();
    EnchantProfileSystem.unloadAll();
}

const std::shared_ptr<ObjectProfile>& ProfileSystem::getProfile(const std::string& name) const
{
    const auto& foundElement = _profilesLoadedByName.find(name);
    if (foundElement == _profilesLoadedByName.end()) return NULL_PROFILE;
    return foundElement->second;    
}

bool ProfileSystem::isLoaded(PRO_REF ref) const
{
    return _profilesLoaded.find(ref) != _profilesLoaded.end();
}

bool ProfileSystem::isLoaded(ObjectProfileRef ref) const
{
    return _profilesLoaded.find(ref.get()) != _profilesLoaded.end();
}

const std::shared_ptr<ObjectProfile>& ProfileSystem::getProfile(PRO_REF ref) const
{
    auto foundElement = _profilesLoaded.find(ref);
    if (foundElement == _profilesLoaded.end()) return NULL_PROFILE;
    return foundElement->second;
}

const std::shared_ptr<ObjectProfile>& ProfileSystem::getProfile(ObjectProfileRef ref) const
{
    auto foundElement = _profilesLoaded.find(ref.get());
    if (foundElement == _profilesLoaded.end()) return NULL_PROFILE;
    return foundElement->second;
}

int ProfileSystem::getProfileSlotNumber(const std::string &folderPath, int slot_override)
{
    if (slot_override >= 0 && slot_override != INVALID_PRO_REF)
    {
        // just use the slot that was provided
        return slot_override;
    }

    // grab the slot from the file
    std::string dataFilePath = folderPath + "/data.txt";

    if (!vfs_exists(dataFilePath.c_str())) {

        return -1;
    }

    // Open the file
    ReadContext ctxt(dataFilePath);

    // load the slot's slot no matter what
    int slot = vfs_get_next_int(ctxt);

    // set the slot slot
    if (slot >= 0)
    {
        return slot;
    }
    else if (import_data.slot >= 0)
    {
        return import_data.slot;
    }

    return -1;
}

ObjectProfileRef ProfileSystem::loadOneProfile(const std::string &pathName, int slot_override)
{
    bool required = !(slot_override < 0 || slot_override >= INVALID_PRO_REF);

    // get a slot value
    int islot = getProfileSlotNumber(pathName, slot_override);

    // throw an error code if the slot is invalid of if the file doesn't exist
    if (islot < 0 || islot >= INVALID_PRO_REF)
    {
        // The data file wasn't found
        if (required)
        {
			_logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "`", pathName, "`", " was not found. Do you attempt to override a global object?", Log::EndOfEntry);
        }
        else if (required && slot_override > 4 * MAX_IMPORT_PER_PLAYER)
        {
			_logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to open file ", "`", pathName, "`", Log::EndOfEntry);
        }

        return ObjectProfileRef::Invalid;
    }

    // convert the slot to a profile reference
    ObjectProfileRef iobj = ObjectProfileRef(static_cast<PRO_REF>(islot));

    // throw an error code if we are trying to load over an existing profile
    // without permission
    if (_profilesLoaded.find(iobj.get()) != _profilesLoaded.end())
    {
        // Make sure global objects don't load over existing models
        if (required && ObjectProfileRef(SPELLBOOK) == iobj)
        {
            auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "object slot ", ObjectProfileRef(SPELLBOOK), " is a special "
                                        "reserved slot number and can not be used by ", "`", pathName, "`", Log::EndOfEntry);
            _logTarget << e;
			throw std::runtime_error(e.getText());
        }
        else if (required && GameSessionContext::get().overrideSlots())
        {
            Log::Entry e(Log::Level::Error, __FILE__, __LINE__);
            e << "object slot " << SPELLBOOK << " is already used by " << _profilesLoaded[iobj.get()]->getPathname() << " and cannot be used by " << pathName << Log::EndOfEntry;
            _logTarget << e;
			throw std::runtime_error(e.getText());
        }
        else
        {
            // Stop, we don't want to override it
            return ObjectProfileRef::Invalid;
        }
    }

    std::shared_ptr<ObjectProfile> profile = ObjectProfile::loadFromFile(pathName, iobj);
    if (!profile)
    {
        Log::Entry e(Log::Level::Warning, __FILE__, __LINE__);
        e << "failed to load " << pathName << " into slot number " << iobj << Log::EndOfEntry;
        _logTarget << e;
        return ObjectProfileRef::Invalid;
    }

    //Success! Store object into the loaded profile map
    _profilesLoaded[iobj.get()] = profile;
    _profilesLoadedByName[profile->getPathname().substr(profile->getPathname().find_last_of('/') + 1)] = profile;

    return iobj;
}

const Ego::DeferredTexture& ProfileSystem::getSpellBookIcon(size_t index) const
{
    return _profilesLoaded.find(SPELLBOOK)->second->getIcon(index);
}

void ProfileSystem::loadModuleProfiles()
{
    //Clear any previously loaded first
    _moduleProfilesLoaded.clear();

    // Search for all .mod directories and load the module info
    SearchContext ctxt(Ego::VfsPath("mp_modules"), Ego::Extension("mod"), VFS_SEARCH_DIR);

    while (ctxt.hasData())
    {
        auto vfs_ModPath = ctxt.getData();

        // A module that fails to load must be skipped, not allowed to abort the whole scan.
        // Users install modules by dropping folders into the user directory (which is mounted
        // into `mp_modules` by setup_init_base_vfs_paths, egoboo_setup.c), so one bad drop
        // would otherwise leave the caller with a truncated module list: the throw escapes
        // after _moduleProfilesLoaded.clear() above has already run.
        //
        // ModuleProfile::loadFromFile never returns nullptr - it reports failure by throwing.
        // Note that `catch (const std::exception&)` alone would catch NOTHING that the engine's
        // own parsers raise: idlib::exception (idlib/exception/exception.hpp:64) is declared
        // with no base class at all, and both idlib::runtime_error (runtime_error.hpp:41 -
        // thrown by vfs_readEntireFile at vfs_bulk.c:56 via the ReadContext/Scanner ctor when
        // gamedat/menu.txt is missing or unreadable) and idlib::hll::compilation_error
        // (compilation_error.hpp:60, plus its subclass Ego::Script::MissingDelimiterError at
        // Script/Errors.hpp:29, raised for a malformed or truncated menu.txt) derive from it.
        // The std::exception arm is therefore a second net for the remaining failures on this
        // path, chiefly std::bad_alloc. Catch by reference only - idlib::exception's copy
        // constructor and destructor are protected, so a by-value handler would not compile.
        //
        // The try deliberately covers only loadFromFile: a throw out of the SearchContext
        // constructor or out of nextData() means the VFS itself failed, which must keep
        // propagating rather than be misreported as a per-module problem. Keeping nextData()
        // outside the handler also guarantees the loop always advances.
        std::shared_ptr<ModuleProfile> module;
        std::string reason;
        try
        {
            //Try to load menu.txt
            module = ModuleProfile::loadFromFile(vfs_ModPath.string());
        }
        catch (const idlib::exception& ex)
        {
            reason = ex.to_string();
        }
        catch (const std::exception& ex)
        {
            reason = ex.what();
        }

        if (module)
        {
            _moduleProfilesLoaded.push_back(module);
        }
        else
        {
            // idlib::runtime_error::to_string() is deliberately multi-line (runtime_error.hpp
            // emits "runtime error:", the raise site, and the message on separate lines), and
            // that is the shape produced by the likeliest bad drop: a folder with no
            // gamedat/menu.txt. Flatten it so one skipped module stays one log record.
            for (char& c : reason)
            {
                if (c == '\n' || c == '\r') c = ' ';
            }
			_logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to load module ", "`", vfs_ModPath.string(), "`",
			                                 reason.empty() ? std::string() : (": " + reason), Log::EndOfEntry);
        }
        ctxt.nextData();
    }
}


void ProfileSystem::printDebugModuleList()
{
    // Log a directory list
    vfs_FILE* filesave = vfs_openWrite("/debug/modules.txt");

    if (filesave == nullptr) {
        return;
    }

    vfs_printf(filesave, "This file logs all of the modules found\n");
    vfs_printf(filesave, "** Denotes an invalid module\n");
    vfs_printf(filesave, "## Denotes an unlockable module\n\n");

    for (size_t imod = 0; imod < _moduleProfilesLoaded.size(); ++imod)
    {
        const std::shared_ptr<ModuleProfile> &module = _moduleProfilesLoaded[imod];

        if (!module->_loaded)
        {
            vfs_printf(filesave, "**.  %s\n", module->_vfsPath.c_str());
        }
        else if (module->isModuleUnlocked())
        {
            vfs_printf(filesave, "%02d.  %s\n", REF_TO_INT(imod), module->_vfsPath.c_str());
        }
        else
        {
            vfs_printf(filesave, "##.  %s\n", module->_vfsPath.c_str());
        }
    }


    vfs_close(filesave);
}

void ProfileSystem::loadAllSavedCharacters(const std::string &saveGameDirectory)
{
    /// @author ZZ
    /// @details This function figures out which players may be imported, and loads basic
    ///     data for each

    //Clear any old imports
    _loadPlayerList.clear();

    // Search for all objects
    SearchContext ctxt(Ego::VfsPath(saveGameDirectory), Ego::Extension("obj"), VFS_SEARCH_DIR);

    while (ctxt.hasData())
    {
        auto foundFile = ctxt.getData();
        auto folderPath = foundFile;

        // is it a valid filename?
        if (folderPath.empty()) {
            ctxt.nextData();
            continue;
        }

        // does the directory exist?
        if (!vfs_exists(folderPath.string())) {
            ctxt.nextData();
            continue;
        }

        // offset the slots so that ChoosePlayer will have space to load the inventory objects
        int slot = (MAX_IMPORT_OBJECTS + 2) * _loadPlayerList.size();

        // try to load the character profile (do a lightweight load, we don't need all data)
        std::shared_ptr<ObjectProfile> profile = ObjectProfile::loadFromFile(folderPath.string(), slot, true);
        if (!profile) {
            ctxt.nextData();
            continue;
        }

        //Loaded!
        _loadPlayerList.push_back(std::make_shared<LoadPlayerElement>(profile));

        //Get next player object
        ctxt.nextData();
    }
}

void ProfileSystem::loadGlobalParticleProfiles()
{
    static const std::vector<std::pair<std::string, PIP_REF>> profiles =
    {
        // Load in the standard global particles ( the coins for example )
        {"mp_data/1money.txt", PIP_COIN1},
        {"mp_data/5money.txt", PIP_COIN5},
        {"mp_data/25money.txt", PIP_COIN25},
        {"mp_data/100money.txt", PIP_COIN100},
        {"mp_data/200money.txt", PIP_GEM200},
        {"mp_data/500money.txt", PIP_GEM500},
        {"mp_data/1000money.txt", PIP_GEM1000},
        {"mp_data/2000money.txt", PIP_GEM2000},
        {"mp_data/disintegrate_start.txt", PIP_DISINTEGRATE_START},
        {"mp_data/disintegrate_particle.txt", PIP_DISINTEGRATE_PARTICLE},
    #if 0
        // Load module specific information
        {"mp_data/weather4.txt", PIP_WEATHER},
        {"mp_data/weather5.txt", PIP_WEATHER_FINISH},
    #endif
        {"mp_data/splash.txt", PIP_SPLASH},
        {"mp_data/ripple.txt", PIP_RIPPLE},
        // This is also global...
        {"mp_data/defend.txt", PIP_DEFEND},

    };

    for (const auto& profile : profiles)
    {
        if (INVALID_PIP_REF == ParticleProfileSystem.load(profile.first, profile.second))
        {
            auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "data file ", "`", profile.first, "`", " was not found", Log::EndOfEntry);
            _logTarget << e;
            throw std::runtime_error(e.getText());
        }
    }
}
