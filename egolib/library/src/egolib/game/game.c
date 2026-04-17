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

/// @file egolib/game/game.c
/// @brief Residual game helpers after splitting the original game implementation
/// @details

#include "egolib/game/game_internal.h"

EndText g_endText;

//--------------------------------------------------------------------------------------------
// Session lifecycle and message text helpers
//--------------------------------------------------------------------------------------------

void game_load_module_profiles( const std::string& modname )
{
    /// @author BB
    /// @details Search for .obj directories in the module directory and load them
    import_data.slot = -100;
    std::string folderPath = modname + "/objects";

    SearchContext* ctxt = new SearchContext(Ego::VfsPath(folderPath), Ego::Extension("obj"), VFS_SEARCH_DIR);
    if (!ctxt) return;

    while (ctxt->hasData()) {
        auto searchResult = ctxt->getData();
        ProfileSystem::get().loadOneProfile(searchResult.string());
        ctxt->nextData();
    }
    delete ctxt;
    ctxt = nullptr;
}

//--------------------------------------------------------------------------------------------
// Player/session reset and terrain queries
//--------------------------------------------------------------------------------------------
void game_quit_module()
{
    /// @author BB
    /// @details all of the de-initialization code after the module actually ends
    GameSessionContext::get().quitModule();
}

//--------------------------------------------------------------------------------------------
bool game_begin_module(const std::shared_ptr<ModuleProfile> &module)
{
    /// @author BB
    /// @details all of the initialization code before the module actually starts
    return GameSessionContext::get().beginModule(module);
}

//--------------------------------------------------------------------------------------------
bool game_finish_module()
{
    /// @author BB
    /// @details This function saves all the players to the players dir
    ///    and also copies them into the imports dir to prepare for the next module
    return GameSessionContext::get().finishModule();
}

//--------------------------------------------------------------------------------------------
void reset_end_text()
{
    /// @author ZZ
    /// @details This function resets the end-module text

    g_endText.setText("The game has ended ...");

    /*
    if ( PlaStack.count > 1 )
    {
        endtext_carat = snprintf( endtext, SDL_arraysize( endtext), "Sadly, they were never heard from again..." );
    }
    else
    {
        if ( 0 == PlaStack.count )
        {
            // No players???
            endtext_carat = snprintf( endtext, SDL_arraysize( endtext), "The game has ended..." );
        }
        else
        {
            // One player
            endtext_carat = snprintf( endtext, SDL_arraysize( endtext), "Sadly, no trace was ever found..." );
        }
    }
    */
}

std::string expandEscapeCodes(const std::shared_ptr<Object> &object, const script_state_t &scriptState, const std::string &text)
{
    GameModule& module = activeModule();
    std::stringstream result;
    bool escapeEncountered = false;

    for(const char &c : text)
    {
        if(escapeEncountered)
        {
            switch(c)
            {
                //Percentile symbol
                case '%':
                    result << '%';
                break;

                //Character name
                case 'n':
                    result << object->getName(true, false, false);
                break;

                //Class name
                case 'c':
                    result << object->getProfile()->getClassName();
                break;

                //AI target name
                case 't':
                {
                    const std::shared_ptr<Object> &target = module.getObjectHandler()[object->ai.getTarget()];
                    if(target) {
                        result << target->getName();
                    }
                }
                break;

                //Owner's name
                case 'o':
                {
                    const std::shared_ptr<Object> &owner = module.getObjectHandler()[object->ai.owner];
                    if(owner) {
                        result << owner->getName(true, false, false);
                    }
                }
                break;

                //Target class name
                case 's':
                {
                    const std::shared_ptr<Object> &target = module.getObjectHandler()[object->ai.getTarget()];
                    if(target) {
                        result << target->getProfile()->getClassName();
                    }
                }
                break;

                //Character's ammo
                case 'a':
                    if(object->ammoknown) {
                        result << object->getAmmo();
                    }
                    else {
                        result << '?';
                    }
                break;

                // Kurse state
                case 'k':
                    if (object->iskursed) {
                        result << "kursed";
                    }
                    else {
                        result << "unkursed";
                    }
                break;

                //Character's possessive
                case 'p':
                    if (object->gender == Gender::Female) {
                        result << "her";
                    }
                    else if (object->gender == Gender::Male) {
                        result << "his";
                    }
                    else {
                        result << "its";
                    }
                break;

                //Character's gender
                case 'm':
                    if (object->gender == Gender::Female) {
                        result << "female ";
                    }
                    else if (object->gender == Gender::Male) {
                        result << "male ";
                    }
                    else {
                        result << "other ";
                    }
                break;

                case 'g':  // Target's possessive
                {
                    const std::shared_ptr<Object> &target = module.getObjectHandler()[object->ai.getTarget()];
                    if(target) {
                        if (target->gender == Gender::Female) {
                            result << "her";
                        }
                        else if (target->gender == Gender::Male) {
                            result << "his";
                        }
                        else {
                            result << "its";
                        }
                    }
                }
                break;

                //Target's skin name
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    const std::shared_ptr<Object> &target = module.getObjectHandler()[object->ai.getTarget()];
                    if(target) {
                        result << target->getProfile()->getSkinInfo(c-'0').name;
                    }
                }
                break;

                // New line (enter)
                case '#':
                    result << '\n';
                break;

                // tmpdistance value
                case 'd':
                    result << scriptState.distance;
                break;

                // tmpx value
                case 'x':
                    result << scriptState.x;
                break;

                // tmpy value
                case 'y':
                    result << scriptState.y;
                break;

                // tmpdistance value with fixed width
                case 'D':
                    result << std::setw(2) << scriptState.distance;
                break;

                //tmpx value with fixed width
                case 'X':
                    result << std::setw(2) << scriptState.x;
                break;

                //tmpy value with fixed width
                case 'Y':
                    result << std::setw(2) << scriptState.y;
                break;

                //Unknown escape character
                default:
                    result << '%' << c;
                    Log::get() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unknown escape character ",
                                                     "`", c, "`", Log::EndOfEntry);
                break;
            }

            //Escape character is now handled
            escapeEncountered = false;
            continue;
        }

        //Is it an escape character?
        if(c == '%') {
            escapeEncountered = true;
        }
        else {
            //Normal character, append to string
            result << c;
        }
    }

    //Ensure that the frist character in the string is always capitalized
    std::string stringResult = result.str();
    if(!stringResult.empty()) {
        stringResult[0] = std::toupper(stringResult[0]);
    }
    return stringResult;
}

//--------------------------------------------------------------------------------------------
void game_reset_players()
{
    /// @author ZZ
    /// @details This function clears the player list data

    // Reset the local data stuff
    GameSessionContext& session = GameSessionContext::get();
    session.resetLocalPlayerState();
    session.resetLocalPlayerPerception();
    session.resetEnemySense();
    session.resetRespawnCooldown();
}

//--------------------------------------------------------------------------------------------
float get_mesh_max_vertex_1( ego_mesh_t *mesh, const Index2D& point, oct_bb_t& bump, bool waterwalk )
{
    GameModule& module = activeModule();
    float zdone = mesh->get_max_vertex_1( point, bump._mins[OCT_X], bump._mins[OCT_Y], bump._maxs[OCT_X], bump._maxs[OCT_Y] );

    if ( waterwalk && module.getWater()._surface_level > zdone && module.getWater()._is_water )
    {
        Index1D tile = mesh->getTileIndex( point );

        if ( 0 != mesh->test_fx( tile, MAPFX_WATER ) )
        {
            zdone = module.getWater()._surface_level;
        }
    }

    return zdone;
}

float get_mesh_max_vertex_2( ego_mesh_t *mesh, Object *object)
{
    /// @author BB
    /// @details the object does not overlap a single grid corner. Check the 4 corners of the collision volume

	if (nullptr == mesh) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "mesh");
	}
	if (nullptr == object) {
		throw idlib::argument_null_error(__FILE__, __LINE__, "object");
	}

    int corner;
    int ix_off[4] = {1, 1, 0, 0};
    int iy_off[4] = {0, 1, 1, 0};

    float pos_x[4];
    float pos_y[4];
    float zmax;

    for ( corner = 0; corner < 4; corner++ )
    {
        pos_x[corner] = object->getPosX() + (( 0 == ix_off[corner] ) ? object->chr_min_cv._mins[OCT_X] : object->chr_min_cv._maxs[OCT_X] );
        pos_y[corner] = object->getPosY() + (( 0 == iy_off[corner] ) ? object->chr_min_cv._mins[OCT_Y] : object->chr_min_cv._maxs[OCT_Y] );
    }

    zmax = mesh->getElevation(Ego::Vector2f(pos_x[0], pos_y[0]), object->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0 );
    for ( corner = 1; corner < 4; corner++ )
    {
        float fval = mesh->getElevation(Ego::Vector2f(pos_x[corner], pos_y[corner]), object->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0 );
        zmax = std::max( zmax, fval );
    }

    return zmax;
}
//--------------------------------------------------------------------------------------------
float get_chr_level( ego_mesh_t *mesh, Object *object )
{
    float zmax;
    int ix, ixmax, ixmin;
    int iy, iymax, iymin;

    int grid_vert_count = 0;
    int grid_vert_x[1024];
    int grid_vert_y[1024];

    oct_bb_t bump;

    if (!mesh || !object || object->isTerminated()) return 0;

    // certain scenery items like doors and such just need to be able to
    // collide with the mesh. They all have 0 == pchr->bump.size
    if ( 0.0f == object->bump_stt.size )
    {
        return mesh->getElevation(Ego::Vector2f(object->getPosX(), object->getPosY()),
			                      object->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0);
    }

    // otherwise, use the small collision volume to determine which tiles the object overlaps
    // move the collision volume so that it surrounds the object
    bump = idlib::translate(object->chr_min_cv, object->getPosition());

    // determine the size of this object in tiles
    ixmin = bump._mins[OCT_X] / Info<float>::Grid::Size(); ixmin = Ego::Math::constrain( ixmin, 0, int(mesh->_info.getTileCountX()) - 1 );
    ixmax = bump._maxs[OCT_X] / Info<float>::Grid::Size(); ixmax = Ego::Math::constrain( ixmax, 0, int(mesh->_info.getTileCountX()) - 1 );

    iymin = bump._mins[OCT_Y] / Info<float>::Grid::Size(); iymin = Ego::Math::constrain( iymin, 0, int(mesh->_info.getTileCountY()) - 1 );
    iymax = bump._maxs[OCT_Y] / Info<float>::Grid::Size(); iymax = Ego::Math::constrain( iymax, 0, int(mesh->_info.getTileCountY()) - 1 );

    // do the simplest thing if the object is just on one tile
    if ( ixmax == ixmin && iymax == iymin )
    {
        return get_mesh_max_vertex_2( mesh, object);
    }

    // otherwise, make up a list of tiles that the object might overlap
    for ( iy = iymin; iy <= iymax; iy++ )
    {
        float grid_y = iy * Info<int>::Grid::Size();

        for ( ix = ixmin; ix <= ixmax; ix++ )
        {
            float ftmp;
            float grid_x = ix * Info<int>::Grid::Size();

            ftmp = grid_x + grid_y;
            if ( ftmp < bump._mins[OCT_XY] || ftmp > bump._maxs[OCT_XY] ) continue;

            ftmp = -grid_x + grid_y;
            if ( ftmp < bump._mins[OCT_YX] || ftmp > bump._maxs[OCT_YX] ) continue;

            Index1D itile = mesh->getTileIndex(Index2D(ix, iy));
            if (Index1D::Invalid == itile ) continue;

            grid_vert_x[grid_vert_count] = ix;
            grid_vert_y[grid_vert_count] = iy;
            grid_vert_count++;
        }
    }

    // we did not intersect a single tile corner
    // this could happen for, say, a very long, but thin shape that fits between the tiles.
    // the current system would not work for that shape
    if ( 0 == grid_vert_count )
    {
        return get_mesh_max_vertex_2( mesh, object);
    }
    else
    {
        int cnt;
        float fval;

        // scan through the vertices that we know will interact with the object
        zmax = get_mesh_max_vertex_1( mesh, Index2D(grid_vert_x[0], grid_vert_y[0]), bump, object->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0 );
        for ( cnt = 1; cnt < grid_vert_count; cnt ++ )
        {
            fval = get_mesh_max_vertex_1( mesh, Index2D(grid_vert_x[cnt], grid_vert_y[cnt]), bump, object->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0 );
            zmax = std::max( zmax, fval );
        }
    }

    if ( zmax == -1e6 ) zmax = 0.0f;

    return zmax;
}

//--------------------------------------------------------------------------------------------
// Real-world time hooks and menu music
//--------------------------------------------------------------------------------------------
namespace Zeitgeist {
bool CheckTime(Time time) {
    Ego::Time::LocalTime localTime;
    switch (time)
    {
    // Halloween is from 31th october 31th (incl.) until the november 1st (incl.).
    case Time::Halloween:
        return ((10 == localTime.getMonth() + 1 && localTime.getDayOfMonth() >= 31) ||
                (11 == localTime.getMonth() + 1 && localTime.getDayOfMonth() <= 1));

    // Chrsitmas is from december 16th (incl.) until january 1st/newyear (excl.).
    case Time::Christmas:
        return (12 == localTime.getMonth() + 1 && localTime.getDayOfMonth() >= 16);

    // From 0:00 to 6:00 (spooky time!).
    case Time::Nighttime:
        return localTime.getHours() <= 6;

     // Its day whenever it's not night.
    case Time::Daytime:
        return localTime.getHours() > 6;

    // Unhandled check.
    default:
        throw idlib::unhandled_switch_case_error(__FILE__, __LINE__);
    }
}
}

//--------------------------------------------------------------------------------------------
void playMainMenuSong()
{
    //Special xmas theme
    if (Zeitgeist::CheckTime(Zeitgeist::Time::Christmas)) {
        AudioSystem::get().playMusic("xmas.ogg");
    }

    //Special Halloween theme
    else if (Zeitgeist::CheckTime(Zeitgeist::Time::Halloween)) {
        AudioSystem::get().playMusic("halloween.ogg");
    }

    //Default egoboo theme
    else {
        AudioSystem::get().playMusic("themesong.ogg");
    }
}
