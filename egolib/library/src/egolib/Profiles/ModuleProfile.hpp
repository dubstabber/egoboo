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

/// @file egolib/Profiles/ModuleProfile.hpp
/// @author Johan Jansen

#pragma once
#if !defined(EGOLIB_PROFILES_PRIVATE) || EGOLIB_PROFILES_PRIVATE != 1
#error(do not include directly, include `egolib/Profiles/_Include.hpp` instead)
#endif

#include "egolib/typedef.h"
#include "egolib/IDSZ.hpp"
#include "egolib/Renderer/DeferredTexture.hpp"

enum ModuleFilter : uint8_t
{
    FILTER_OFF,
    FILTER_MAIN,
    FILTER_SIDE_QUEST,
    FILTER_TOWN,
    FILTER_FUN,
    NR_OF_MODULE_FILTERS,

    FILTER_STARTER        //Starter modules are special, place after last
};

class ModuleProfile
{
public:
    static const uint8_t RESPAWN_ANYTIME = 0xFF;

public:
    ModuleProfile();

    ~ModuleProfile();

    bool isModuleUnlocked() const;

    ModuleFilter getModuleType() const;

    inline Ego::DeferredTexture& getIcon() {
        return _icon;
    }

    const std::string& getName() const {
        return _name;
    }

    bool isStarterModule() const {
        return _importAmount == 0;
    }

    uint8_t getRank() const {
        return _rank;
    }

    /**
     * @return
     *  the virtual pathname of the module
     */
    const std::string& getPath() const {
        return _vfsPath;
    }

    const std::string& getFolderName() const {
        return _folderName;
    }

    uint8_t getImportAmount() const {
        return _importAmount;
    }

    uint8_t getMinPlayers() const {
        return _minPlayers;
    }

    uint8_t getMaxPlayers() const {
        return _maxPlayers;
    }

    bool isExportAllowed() const {
        return _allowExport;
    }

    bool hasRespawnAnytime() const {
        return _respawnValid == RESPAWN_ANYTIME;
    }

    bool isRespawnValid() const {
        return 0 != _respawnValid;
    }

    const std::vector<std::string>& getSummary() const {
        return _summary;
    }

    static std::shared_ptr<ModuleProfile> loadFromFile(const std::string &filePath);

    /// @brief Test whether a module declares an IDSZ expansion in its gamedat/menu.txt.
    /// @param szModName the module folder name, e.g. "advent.mod"; typically content-supplied
    /// @param idsz the IDSZ to look for
    /// @return @a true if the module declares @a idsz, @a false otherwise
    /// @remark Two answers are given without reading anything: a requirement of IDSZ2::None is
    ///         always satisfied (@a true), and the sentinel name "NONE" never is (@a false).
    /// @remark <b>Miss contract.</b> A module that cannot be found, opened, or parsed simply
    ///         "does not have the IDSZ" and yields @a false. The name reaches this function from
    ///         an object's message table by way of scr_IfModuleHasIDSZ, so an absent or malformed
    ///         module is ordinary input, not a precondition violation. As a corollary of reading
    ///         the file linearly, a malformed expansion line also ends the scan, so an IDSZ
    ///         listed after it is not found.
    /// @remark The contract covers the two exception families the parsers raise for bad input:
    ///         idlib::runtime_error for a file that cannot be read, and
    ///         idlib::hll::compilation_error for one that does not parse. Anything outside those
    ///         - std::bad_alloc in particular - still propagates, so do not wrap calls to this
    ///         function in catch (...).
    /// @throw std::bad_alloc if a string allocation fails
    static bool moduleHasIDSZ(const std::string& szModName, const IDSZ2& idsz);

    /// @brief Append an IDSZ expansion to a module's gamedat/menu.txt in the user directory.
    /// @param szModName the module folder name
    /// @param idsz the IDSZ to append
    /// @return @a true if the expansion was written, @a false if it was already present or if the
    ///         module's menu.txt could not be copied into the user directory or opened
    static bool moduleAddIDSZ(const std::string& szModName, const IDSZ2& idsz);

private:
    bool _loaded;

    // data from menu.txt
    std::string _name;                      ///< Example: "Adventurer Starter"
    uint8_t _rank;                          ///< Number of stars

    std::string _reference;                 ///< the module reference string

    uint8_t   _importAmount;                ///< # of import characters
    bool    _allowExport;                   ///< Export characters?
    uint8_t   _minPlayers;                  ///< Number of players
    uint8_t   _maxPlayers;
    uint8_t   _respawnValid;                ///< Allow respawn
    std::vector<std::string> _summary;      ///< Quest description

    IDSZ2    _unlockQuest;                  ///< the quest required to unlock this module
    int      _unlockQuestLevel;
    ModuleFilter    _moduleType;            ///< Main quest, town, sidequest or whatever
    bool            _beaten;                ///< The module has been marked with the [BEAT] eapansion

    Ego::DeferredTexture _icon;             ///< the module's tile image
    std::string _vfsPath;                   ///< the virtual pathname of the module ("mp_module/advent.mod")
    std::string _folderName;                ///< Folder name of module ("advent.mod")

    friend class ProfileSystem;
};
