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

/// @file egolib/game/game_export.c
/// @brief Character export/import and save management

#include "egolib/game/game_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace
{
enum class ExportCharacterResult
{
    Exported,
    Skipped,
    Error
};

const Object* tryPlayerExportObject(const std::shared_ptr<Ego::Player>& player)
{
    if (!player)
    {
        return nullptr;
    }

    const Object* object = player->tryObject();
    return object != nullptr && !object->isTerminated() ? object : nullptr;
}

//--------------------------------------------------------------------------------------------
ExportCharacterResult export_one_character( ObjectRef character, ObjectRef owner, int chr_obj_index, bool is_local )
{
    /// @author ZZ
    /// @details This function exports a character
    std::string fromdir;
    std::string todir;
    std::string fromfile;
    std::string tofile;
    std::string todirname;
    std::string todirfullname;

    GameModule& module = activeModule();
    ObjectHandler& objectHandler = module.getObjectHandler();
    Object* object = objectHandler.get(character);
    if(!object) {
        return ExportCharacterResult::Error;
    }

    if ( !module.isExportValid() || ( object->getProfile()->isItem() && !object->getProfile()->canCarryToNextModule() ) )
    {
        return ExportCharacterResult::Skipped;
    }

    const Object* ownerObject = objectHandler.get(owner);
    if (!ownerObject)
    {
        return ExportCharacterResult::Error;
    }

    // TWINK_BO.OBJ
    todirname = str_encode_path(ownerObject->getName());

    // Is it a character or an item?
    if ( chr_obj_index < 0 )
    {
        // Character directory
        todirfullname = todirname;
    }
    else
    {
        // Item is a subdirectory of the owner directory...
        std::stringstream stringStream;
        stringStream << todirname << "/" << chr_obj_index << ".obj";
        todirfullname = stringStream.str();
    }

    // players/twink.obj or players/twink.obj/0.obj
    if ( is_local )
    {
        todir = "/players/" + todirfullname;
    }
    else
    {
        todir = "/remote/" + todirfullname;
    }

    // Remove all the original info
    if ( chr_obj_index < 0 )
    {
        vfs_removeDirectoryAndContents( todir.c_str() );
        if ( !vfs_mkdir( todir ) )
        {
			EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to create object directory ", "`", todir, "`", Log::EndOfEntry);
            return ExportCharacterResult::Error;
        }
    }

    // modules/advent.mod/objects/advent.obj
    fromdir = object->getProfile()->getPathname();

    // Build the DATA.TXT file
    if(!ObjectProfile::exportCharacterToFile(todir + "/data.txt", object)) {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to save ", "`", todir, "/data.txt`", Log::EndOfEntry);
        return ExportCharacterResult::Error;
    }

    // Build the NAMING.TXT file
    tofile = todir + "/naming.txt"; /*NAMING.TXT*/
    export_one_character_name_vfs( tofile.c_str(), character );

    // Build the QUEST.TXT file
    export_one_character_quest_vfs( todir.c_str(), character );

    // copy every file that does not already exist in the todir
    {
        SearchContext *ctxt = new SearchContext(Ego::VfsPath(fromdir), VFS_SEARCH_FILE | VFS_SEARCH_BARE );
        if (!ctxt) return ExportCharacterResult::Exported;
        while (ctxt->hasData()) {
            auto searchResult = ctxt->getData();
            fromfile = fromdir + "/" + searchResult.string();
            tofile = todir + "/" + searchResult.string();
            if (!vfs_exists(tofile)) {
                vfs_copyFile(fromfile, tofile);
            }
            ctxt->nextData();
        }

        delete ctxt;
        ctxt = nullptr;
    }

    return ExportCharacterResult::Exported;
}
} // namespace

//--------------------------------------------------------------------------------------------
bool export_all_players( bool require_local )
{
    /// @author ZZ
    /// @details This function saves all the local players in the
    ///    PLAYERS directory

    (void)require_local;

    ExportCharacterResult exportResult;
    bool exportedAllPlayers = true;
    int number;

    GameModule& module = activeModule();
    ObjectHandler& objectHandler = module.getObjectHandler();

    // Stop if export isnt valid
    if ( !module.isExportValid() ) return false;

    // Check each player
    for(const std::shared_ptr<Ego::Player> &player : module.getPlayerList()) {
        ObjectRef item;
        if (!player) {
            continue;
        }

        // Is it alive?
        const ObjectRef character = player->getObjectRef();
        Object* pchr = objectHandler.get(character);
        if(!pchr || pchr->isTerminated()) continue;

        // don't export dead characters
        if ( !pchr->isAlive() ) continue;

        // Export the character
        exportResult = export_one_character( character, character, -1, true );
        if ( ExportCharacterResult::Error == exportResult )
        {
            exportedAllPlayers = false;
        }

        // Export the left hand item
        item = pchr->getHeldObject(SLOT_LEFT);
        if ( objectHandler.exists( item ) )
        {
            exportResult = export_one_character( item, character, SLOT_LEFT, true );
            if ( ExportCharacterResult::Error == exportResult )
            {
                exportedAllPlayers = false;
            }
        }

        // Export the right hand item
        item = pchr->getHeldObject(SLOT_RIGHT);
        if ( objectHandler.exists( item ) )
        {
            exportResult = export_one_character( item, character, SLOT_RIGHT, true );
            if ( ExportCharacterResult::Error == exportResult )
            {
                exportedAllPlayers = false;
            }
        }

        // Export the inventory
        number = 0;
        for (size_t slot = 0; slot < pchr->getInventoryMaxItems(); ++slot)
        {
            const ObjectRef itemRef = pchr->getInventoryItemRef(slot);
            if (ObjectRef::Invalid == itemRef) continue;

            exportResult = export_one_character( itemRef, character, number + SLOT_COUNT, true);
            if ( ExportCharacterResult::Error == exportResult )
            {
                exportedAllPlayers = false;
            }
            else if ( ExportCharacterResult::Exported == exportResult )
            {
                number++;
            }
        }
    }

    return exportedAllPlayers;
}

//--------------------------------------------------------------------------------------------
bool export_one_character_quest_vfs( const char *szSaveName, ObjectRef character )
{
    /// @author ZZ
    /// @details This function makes the naming.txt file for the character

    GameModule& module = activeModule();
    Object* object = module.getObjectHandler().get(character);
    if(!object) {
        return false;
    }

    if(!object->isPlayer()) {
        return false;
    }

    std::shared_ptr<Ego::Player>& player = module.getPlayer(object->getPlayerNumber());

    return player->getQuestLog().exportToFile(szSaveName);
}

//--------------------------------------------------------------------------------------------
bool export_one_character_name_vfs( const char *szSaveName, ObjectRef character )
{
    /// @author ZZ
    /// @details This function makes the naming.txt file for the character

    GameModule& module = activeModule();
    const Object* object = module.getObjectHandler().get(character);
    if ( !object ) return false;

    return RandomName::exportName(object->getName(), szSaveName);
}

//--------------------------------------------------------------------------------------------
bool game_copy_imports(import_list_t& imports)
{
    if (0 == imports.count)
    {
        return true;
    }

    // assume the best
    bool copiedAllImports = true;

    // delete the data in the directory
    vfs_removeDirectoryAndContents( "import" );
    vfs_remove_mount_point(Ego::VfsPath("import"));

    // make sure the directory exists
    if ( !vfs_mkdir( "/import" ) )
    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to create import folder: ", vfs_getError(), Log::EndOfEntry);
        return false;
    }
    vfs_add_mount_point( fs_getUserDirectory(), Ego::FsPath("import"), Ego::VfsPath("mp_import"), 1 );

    // copy all of the imports over
    for (auto import_idx = 0; import_idx < imports.count; import_idx++ )
    {
        // grab the loadplayer info
        import_element_t* import_ptr = imports.lst + import_idx;

        std::stringstream stringStream;
        stringStream << "/import/temp" << std::setfill('0') << std::setw(4) << import_ptr->slot << ".obj";
        import_ptr->dstDir = stringStream.str();

        if ( !vfs_copyDirectory( import_ptr->srcDir.c_str(), import_ptr->dstDir.c_str() ) )
        {
            copiedAllImports = false;
			EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to copy an import character ",
                                             "from ", "`", import_ptr->srcDir, "`", " to ", "`", import_ptr->dstDir, "`", "(",
                                             vfs_getError(), ")");
        }

        // Copy all of the character's items to the import directory
        for (auto tnc = 0; tnc < MAX_IMPORT_OBJECTS; tnc++ )
        {
            stringStream.str(std::string());
            stringStream.clear();
            stringStream << import_ptr->srcDir << "/" << tnc << ".obj";
            auto tmp_src_dir = stringStream.str();

            // make sure the source directory exists
            if ( vfs_isDirectory( tmp_src_dir ) )
            {
                stringStream.str(std::string());
                stringStream.clear();
                stringStream << "/import/temp" << std::setfill('0') << std::setw(4) << import_ptr->slot + tnc + 1 << ".obj";
                auto tmp_dst_dir = stringStream.str();
                if ( !vfs_copyDirectory( tmp_src_dir.c_str(), tmp_dst_dir.c_str() ) )
                {
                    copiedAllImports = false;
					EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to copy an import inventory item from ",
                                                     "`", tmp_src_dir, "` to `", tmp_dst_dir, "`: ", vfs_getError());
                }
            }
        }
    }

    return copiedAllImports;
}

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
void import_list_t::init(import_list_t& self)
{
    for (size_t i = 0; i < (size_t)MAX_IMPORTS; ++i)
    {
        self.lst[i] = import_element_t();
    }
    self.count = 0;
}

//--------------------------------------------------------------------------------------------
size_t import_list_t::from_players(import_list_t& self)
{
    GameModule& module = activeModule();
    // blank out the ImportList list
    import_list_t::init(self);

    // generate the ImportList list from the player info
    for(size_t player_idx = 0; player_idx < module.getPlayerList().size(); ++player_idx) {
        const std::shared_ptr<Ego::Player>& player = module.getPlayerList()[player_idx];
        const Object* pchr = tryPlayerExportObject(player);
        if(!pchr) {
            continue;
        }

		// grab a pointer
		import_element_t *import_ptr = self.lst + self.count;
		self.count++;

		import_ptr->player = player_idx;
		import_ptr->slot = player_idx * MAX_IMPORT_PER_PLAYER;
		import_ptr->srcDir[0] = CSTR_END;
		import_ptr->dstDir[0] = CSTR_END;
        import_ptr->name = pchr->getName();
        import_ptr->srcDir = "mp_players/" + str_encode_path(pchr->getName());
	}

	return self.count;
}
