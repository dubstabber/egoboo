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

/// @file egolib/game/Module/Module_loading.cpp
/// @brief GameModule content-loading helpers and debug load reporting.

#include "egolib/game/Module/Module_internal.h"
#include "egolib/Profiles/IProfileSystem.hpp"

namespace
{
void loadModuleProfiles(const std::string& modulePath, IProfileSystem& profileSystem)
{
    import_data.slot = -100;
    const std::string folderPath = modulePath + "/objects";

    SearchContext ctxt(Ego::VfsPath(folderPath), Ego::Extension("obj"), VFS_SEARCH_DIR);
    while (ctxt.hasData())
    {
        profileSystem.loadOneProfile(ctxt.getData().string());
        ctxt.nextData();
    }
}
}

void GameModule::loadProfiles()
{
    IProfileSystem& profileSystem = _runtime.profileSystem();
    OverrideSlotsScope moduleSlotOverride(_runtime.overrideSlots());

    //Load the spell book profile
    profileSystem.loadOneProfile("mp_data/globalobjects/book.obj", SPELLBOOK);

    // Clear the import slots...
    import_data.slot_lst.fill(INVALID_PRO_REF);
    import_data.max_slot = -1;

    // This overwrites existing loaded slots that are loaded globally
    import_data.player = -1;
    import_data.slot   = -100;

    //Load any saved player characters from disk (if needed)
    if (isImportValid()) {
        for (int cnt = 0; cnt < getImportAmount() * MAX_IMPORT_PER_PLAYER; cnt++)
        {
            std::ostringstream pathFormat;
            pathFormat << "mp_import/temp" << std::setw(4) << std::setfill('0') << cnt << ".obj";

            // Make sure the object exists...
            const std::string importPath = pathFormat.str();
            const std::string dataFilePath = importPath + "/data.txt";

            if (vfs_exists(dataFilePath.c_str()))
            {
                // new player found
                if (0 == (cnt % MAX_IMPORT_PER_PLAYER)) import_data.player++;

                // store the slot info
                import_data.slot = cnt;

                // load it
                import_data.slot_lst[cnt] = profileSystem.loadOneProfile(importPath).get();
                import_data.max_slot      = std::max(import_data.max_slot, cnt);
            }
        }
    }

    // load all module-specific object profiles
    loadModuleProfiles(_moduleProfile->getPath(), profileSystem);
}

void GameModule::loadAllPassages()
{
    // Reset all of the old passages
    _passages.clear();

    // Load the file
    std::unique_ptr<ReadContext> ctxt = nullptr;
    try {
        ctxt = std::make_unique<ReadContext>("mp_data/passage.txt");
    } catch (...) {
        return;
    }

    //Load all passages in file
    while (ctxt->skipToColon(true))
    {
        //read passage area and constrain passage area within the level
        int x0 = Ego::Math::constrain<int>(ctxt->readIntegerLiteral(), 0, _mesh->_info.getTileCountX() - 1);
        int y0 = Ego::Math::constrain<int>(ctxt->readIntegerLiteral(), 0, _mesh->_info.getTileCountY() - 1);
        int x1 = Ego::Math::constrain<int>(ctxt->readIntegerLiteral(), 0, _mesh->_info.getTileCountX() - 1);
        int y1 = Ego::Math::constrain<int>(ctxt->readIntegerLiteral(), 0, _mesh->_info.getTileCountY() - 1);

        //Read if open by default
        bool open = ctxt->readBool();

        //Read mask (optional)
        uint8_t mask = MAPFX_IMPASS | MAPFX_WALL;
        if (ctxt->readBool()) mask = MAPFX_IMPASS;
        if (ctxt->readBool()) mask = MAPFX_SLIPPY;

        std::shared_ptr<Passage> passage = std::make_shared<Passage>(*this, x0, y0, x1, y1, mask);

        //check if we need to close the passage
        if (!open) {
            passage->close();
        }

        //finished loading this one!
        _passages.push_back(passage);
    }
}

void GameModule::loadTeamAlliances()
{
    std::unique_ptr<ReadContext> ctxt = nullptr;
    // Load the file if it exists
    try {
        ctxt = std::make_unique<ReadContext>("mp_data/alliance.txt");
    } catch (...) {
        return;
    }

    //Found the file, parse the contents
    while (ctxt->skipToColon(true))
    {
        std::string buffer = vfs_read_string_lit(*ctxt);
        if (buffer.length() < 1) {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::syntactical, ctxt->get_location(),
                                                "empty string literal");
        }
        TEAM_REF teama = (buffer[0] - 'A') % Team::TEAM_MAX;

        buffer = vfs_read_string_lit(*ctxt);
        if (buffer.length() < 1) {
            throw idlib::hll::compilation_error(__FILE__, __LINE__, idlib::hll::compilation_error_kind::syntactical, ctxt->get_location(),
                                                "empty string literal");
        }
        TEAM_REF teamb = (buffer[0] - 'A') % Team::TEAM_MAX;
        _teamList[teama].makeAlliance(_teamList[teamb]);
    }
}

void GameModule::logSlotUsage(const std::string& savename)
{
    /// @author ZZ
    /// @details This is a debug function for checking model loads

    vfs_FILE* hFileWrite = vfs_openWrite(savename);
    if (hFileWrite)
    {
        vfs_printf(hFileWrite, "Slot usage for objects in last module loaded...\n");

        ObjectProfileRef lastSlotNumber(0);
        IProfileSystem& profileSystem = _runtime.profileSystem();
        for (const auto &element : profileSystem.getLoadedProfiles())
        {
            const std::shared_ptr<ObjectProfile> &profile = element.second;

            //ZF> ugh, import objects are currently handled in a weird special way.
            for (ObjectProfileRef i = lastSlotNumber; i < profile->getSlotNumber() && i <= ObjectProfileRef(36); ++i)
            {
                if (!profileSystem.isLoaded(i))
                {
                    vfs_printf(hFileWrite, "%3" PRIuZ " %32s.\n", i.get(), "Slot reserved for import players");
                }
            }
            lastSlotNumber = profile->getSlotNumber();

            vfs_printf(hFileWrite, "%3d %32s %s\n", profile->getSlotNumber().get(), profile->getClassName().c_str(), profile->getModel()->getName().c_str());
        }

        vfs_close(hFileWrite);
    }
}
