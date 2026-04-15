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

//--------------------------------------------------------------------------------------------
egolib_rv export_one_character( ObjectRef character, ObjectRef owner, int chr_obj_index, bool is_local )
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
    const std::shared_ptr<Object> &object = module.getObjectHandler()[character];
    if(!object) {
        return rv_error;
    }

    if ( !module.isExportValid() || ( object->getProfile()->isItem() && !object->getProfile()->canCarryToNextModule() ) )
    {
        return rv_fail;
    }

    // TWINK_BO.OBJ
    todirname = str_encode_path(module.getObjectHandler()[owner]->getName());

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
			Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to create object directory ", "`", todir, "`", Log::EndOfEntry);
            return rv_error;
        }
    }

    // modules/advent.mod/objects/advent.obj
    fromdir = object->getProfile()->getPathname();

    // Build the DATA.TXT file
    if(!ObjectProfile::exportCharacterToFile(todir + "/data.txt", object.get())) {
		Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to save ", "`", todir, "/data.txt`", Log::EndOfEntry);
        return rv_error;
    }

    // Build the NAMING.TXT file
    tofile = todir + "/naming.txt"; /*NAMING.TXT*/
    export_one_character_name_vfs( tofile.c_str(), character );

    // Build the QUEST.TXT file
    export_one_character_quest_vfs( todir.c_str(), character );

    // copy every file that does not already exist in the todir
    {
        SearchContext *ctxt = new SearchContext(Ego::VfsPath(fromdir), VFS_SEARCH_FILE | VFS_SEARCH_BARE );
        if (!ctxt) return rv_success;
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

    return rv_success;
}

//--------------------------------------------------------------------------------------------
egolib_rv export_all_players( bool require_local )
{
    /// @author ZZ
    /// @details This function saves all the local players in the
    ///    PLAYERS directory

    egolib_rv export_chr_rv;
    egolib_rv retval;
    int number;

    GameModule& module = activeModule();

    // Stop if export isnt valid
    if ( !module.isExportValid() ) return rv_fail;

    // assume the best
    retval = rv_success;

    // Check each player
    for(const std::shared_ptr<Ego::Player> &player : module.getPlayerList()) {
        ObjectRef item;

        // Is it alive?
        std::shared_ptr<Object> pchr = player->getObject();
        if(!pchr || pchr->isTerminated()) continue;

        ObjectRef character = pchr->getObjRef();

        // don't export dead characters
        if ( !pchr->isAlive() ) continue;

        // Export the character
        export_chr_rv = export_one_character( character, character, -1, true );
        if ( rv_error == export_chr_rv )
        {
            retval = rv_error;
        }

        // Export the left hand item
        item = pchr->holdingwhich[SLOT_LEFT];
        if ( module.getObjectHandler().exists( item ) )
        {
            export_chr_rv = export_one_character( item, character, SLOT_LEFT, true );
            if ( rv_error == export_chr_rv )
            {
                retval = rv_error;
            }
        }

        // Export the right hand item
        item = pchr->holdingwhich[SLOT_RIGHT];
        if ( module.getObjectHandler().exists( item ) )
        {
            export_chr_rv = export_one_character( item, character, SLOT_RIGHT, true );
            if ( rv_error == export_chr_rv )
            {
                retval = rv_error;
            }
        }

        // Export the inventory
        number = 0;
        for(const std::shared_ptr<Object> pitem : pchr->getInventory().iterate())
        {
            if ( number >= pchr->getInventory().getMaxItems() ) break;

            export_chr_rv = export_one_character( pitem->getObjRef(), character, number + SLOT_COUNT, true);
            if ( rv_error == export_chr_rv )
            {
                retval = rv_error;
            }
            else if ( rv_success == export_chr_rv )
            {
                number++;
            }
        }
    }

    return retval;
}

//--------------------------------------------------------------------------------------------
bool export_one_character_quest_vfs( const char *szSaveName, ObjectRef character )
{
    /// @author ZZ
    /// @details This function makes the naming.txt file for the character

    GameModule& module = activeModule();
    const std::shared_ptr<Object> &object = module.getObjectHandler()[character];
    if(!object) {
        return false;
    }

    if(!object->isPlayer()) {
        return false;
    }

    std::shared_ptr<Ego::Player>& player = module.getPlayer(object->is_which_player);

    return player->getQuestLog().exportToFile(szSaveName);
}

//--------------------------------------------------------------------------------------------
bool export_one_character_name_vfs( const char *szSaveName, ObjectRef character )
{
    /// @author ZZ
    /// @details This function makes the naming.txt file for the character

    GameModule& module = activeModule();
    if ( !module.getObjectHandler().exists( character ) ) return false;

    return RandomName::exportName(module.getObjectHandler()[character]->getName(), szSaveName);
}

//--------------------------------------------------------------------------------------------
egolib_rv game_copy_imports( import_list_t * imp_lst )
{
    egolib_rv retval;

    if ( NULL == imp_lst ) return rv_error;

    if ( 0 == imp_lst->count ) return rv_success;

    // assume the best
    retval = rv_success;

    // delete the data in the directory
    vfs_removeDirectoryAndContents( "import" );
    vfs_remove_mount_point(Ego::VfsPath("import"));

    // make sure the directory exists
    if ( !vfs_mkdir( "/import" ) )
    {
		Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to create import folder: ", vfs_getError(), Log::EndOfEntry);
        return rv_error;
    }
    vfs_add_mount_point( fs_getUserDirectory(), Ego::FsPath("import"), Ego::VfsPath("mp_import"), 1 );

    // copy all of the imports over
    for (auto import_idx = 0; import_idx < imp_lst->count; import_idx++ )
    {
        // grab the loadplayer info
        import_element_t * import_ptr = imp_lst->lst + import_idx;

        std::stringstream stringStream;
        stringStream << "/import/temp" << std::setfill('0') << std::setw(2) << import_ptr->slot << ".obj";
        import_ptr->dstDir = stringStream.str();

        if ( !vfs_copyDirectory( import_ptr->srcDir.c_str(), import_ptr->dstDir.c_str() ) )
        {
            retval = rv_error;
			Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to copy an import character ",
                                             "from ", "`", import_ptr->srcDir, "`", " to ", "`", import_ptr->dstDir, "`", "(",
                                             vfs_getError(), ")");
        }

        // Copy all of the character's items to the import directory
        for (auto tnc = 0; tnc < MAX_IMPORT_OBJECTS; tnc++ )
        {
            stringStream.clear();
            stringStream << import_ptr->srcDir << tnc;
            auto tmp_src_dir = stringStream.str();

            // make sure the source directory exists
            if ( vfs_isDirectory( tmp_src_dir ) )
            {
                stringStream.clear();
                stringStream << "/import/temp" << std::setfill('0') << std::setw(4) << import_ptr->slot + tnc + 1 << ".obj";
                auto tmp_dst_dir = stringStream.str();
                if ( !vfs_copyDirectory( tmp_src_dir.c_str(), tmp_dst_dir.c_str() ) )
                {
                    retval = rv_error;
					Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "failed to copy an import inventory item from ",
                                                     "`", tmp_src_dir, "` to `", tmp_dst_dir, "`: ", vfs_getError());
                }
            }
        }
    }

    return retval;
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
egolib_rv import_list_t::from_players(import_list_t& self)
{
    GameModule& module = activeModule();
    // blank out the ImportList list
    import_list_t::init(self);

    // generate the ImportList list from the player info
    for(size_t player_idx = 0; player_idx < module.getPlayerList().size(); ++player_idx) {
        const std::shared_ptr<Ego::Player>& player = module.getPlayerList()[player_idx];

		std::shared_ptr<Object> pchr = player->getObject();
        if(!pchr || pchr->isTerminated()) {
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

	return (self.count > 0) ? rv_success : rv_fail;
}
