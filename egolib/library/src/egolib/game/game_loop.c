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

/// @file egolib/game/game_loop.c
/// @brief Main game loop, player input, stat display, particles, and messaging

#include "egolib/Audio/AudioSystem.hpp"
#include "egolib/game/game_internal.h"

int chr_stoppedby_tests = 0;
int chr_pressure_tests = 0;

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

IAudioSystem& audioSystem()
{
    return EngineContext::get().audioSystem();
}

Ego::Input::IInputSystem& inputSystem()
{
    return EngineContext::get().inputSystem();
}

IScriptable& scriptable(Object& object)
{
    return object;
}

Object* tryCheatPlayerObject(GameModule& module, PLA_REF playerIndex)
{
    if (playerIndex == INVALID_PLA_REF || playerIndex >= module.getPlayerList().size())
    {
        return nullptr;
    }

    const std::shared_ptr<Ego::Player>& player = module.getPlayer(playerIndex);
    if (!player)
    {
        return nullptr;
    }

    Object* object = player->tryObject();
    return object != nullptr && !object->isTerminated() ? object : nullptr;
}
}

//--------------------------------------------------------------------------------------------
void MainLoop::move_all_objects()
{
    GameModule& module = activeModule();
	g_meshStats.mpdfxTests = 0;
    chr_stoppedby_tests = 0;

    // move every particle
    for(const std::shared_ptr<Ego::Particle> &particle : EngineContext::get().particleHandler().iterator())
    {
        if(particle->isTerminated()) {
            continue;
        }
        particle->getParticlePhysics().updatePhysics();
    }

    // Move every character
    for(const std::shared_ptr<Object> &object : module.getObjectHandler().iterator())
    {
        if(object->isTerminated()) {
            continue;
        }
        object->updatePhysics();
        //chr_update_matrix( object.get(), true );
    }
}

void MainLoop::updateLocalStats()
{
    GameModule& module = activeModule();
    GameSessionContext& session = GameSessionContext::get();
    const LocalPlayerStatus localPlayerStatus = collectLocalPlayerStatus(module.getPlayerList());
    const LocalPlayerPerceptionState localPlayerPerception = collectLocalPlayerPerception(module.getPlayerList());
    audioSystem().setMaxHearingDistance(AudioSystem::DEFAULT_MAX_DISTANCE);

    for(const std::shared_ptr<Ego::Player> &player : module.getPlayerList())
    {
        Object* pchr = player != nullptr ? player->tryObject() : nullptr;
        if(!pchr || pchr->isTerminated()) {
            continue;
        }

        if ( !pchr->isAlive() )
        {
            continue;
        }

        //Do they have the listening perk? (+100% hearing distance)
        if (pchr->hasPerk(Ego::Perks::PERCEPTIVE)) {
            audioSystem().setMaxHearingDistance(AudioSystem::DEFAULT_MAX_DISTANCE*2);
        }
    }

    session.publishLocalPlayerStatus(localPlayerStatus);
    session.publishLocalPlayerPerception(localPlayerPerception);

    // Timers
    characterStatClock()++;

    // Reset the respawn timer
    session.tickRespawnCooldown();
}

//--------------------------------------------------------------------------------------------
void MainLoop::readPlayerInput()
{
    GameModule& module = activeModule();
    GameSessionContext& session = GameSessionContext::get();
    Ego::Input::IInputSystem& input = inputSystem();
    for(const std::shared_ptr<Ego::Player>& player : module.getPlayerList()) {

        //Only valid players
        Object* pchr = player != nullptr ? player->tryObject() : nullptr;
        if(!pchr || pchr->isTerminated()) {
            continue;
        }

        //Read input from the device controlling the player into object latches
        player->updateLatches();

        //Press space to respawn!
        bool respawnRequested = false;
        if (input.isKeyDown(SDLK_SPACE)
            && (session.allLocalPlayersDead() || module.canRespawnAnyTime())
            && module.isRespawnValid()
            && config().game_difficulty.getValue() < Ego::GameDifficulty::Hard)
        {
            respawnRequested = true;
        }

        // Let players respawn
        if (config().game_difficulty.getValue() < Ego::GameDifficulty::Hard && respawnRequested && module.isRespawnValid())
        {
            if (!pchr->isAlive() && 0 == session.respawnCooldown())
            {
                IScriptable& scriptableCharacter = scriptable(*pchr);
                pchr->respawn();
                pchr->becomeTeamLeader();
                scriptableCharacter.addAIAlertBits(ALERTIF_CLEANEDUP);

                // cost some experience for doing this...  never lose a level
                pchr->setExperience(pchr->getExperience() * EXPKEEP);

                //Also lose some gold in non-easy modes
                if (config().game_difficulty.getValue() > Ego::GameDifficulty::Easy) {
                    pchr->giveMoney(-pchr->getMoney() * EXPKEEP);
                }
            }
        }
   }
}

//--------------------------------------------------------------------------------------------
void MainLoop::check_stats()
{
    /// @author ZZ
    /// @details This function lets the players check character stats
    GameModule& module = activeModule();
    Ego::Input::IInputSystem& input = inputSystem();

    static int stat_check_timer = 0;
    static int stat_check_delay = 0;

    int ticks = Time::now<Time::Unit::Ticks>();
    if ( ticks > stat_check_timer + 20 )
    {
        stat_check_timer = ticks;
    }

    stat_check_delay -= 20;
    if ( stat_check_delay > 0 )
        return;

    // Show map cheat
    if (config().debug_developerMode_enable.getValue() && input.isKeyDown(SDLK_m) && input.isKeyDown(SDLK_LSHIFT))
    {
        std::shared_ptr<PlayingState> playingState = activePlayingState();
        playingState->getMiniMap()->setVisible(true);
        playingState->getMiniMap()->setShowPlayerPosition(true);
        stat_check_delay = 150;
    }

    // XP CHEAT
    if (config().debug_developerMode_enable.getValue() &&
        input.isKeyDown(SDLK_x))
    {
        PLA_REF docheat = INVALID_PLA_REF;
        if (input.isKeyDown( SDLK_1 ) )  docheat = 0;
        else if (input.isKeyDown( SDLK_2 ) )  docheat = 1;
        else if (input.isKeyDown( SDLK_3 ) )  docheat = 2;
        else if (input.isKeyDown( SDLK_4 ) )  docheat = 3;

        //Apply the cheat if valid
        if ( docheat != INVALID_PLA_REF && docheat < module.getPlayerList().size() )
        {
            Object* object = tryCheatPlayerObject(module, docheat);
            if(object)
            {
                //Give 10% of XP needed for next level
                const uint8_t currentLevelIndex = object->getExperienceLevelIndex();
                uint32_t xpgain = 0.1f * ( object->getProfile()->getXPNeededForLevel( std::min(currentLevelIndex + 1, MAXLEVEL) ) - object->getProfile()->getXPNeededForLevel(currentLevelIndex));
                object->giveExperience(xpgain, XP_DIRECT, true);
                stat_check_delay = 1;
            }
        }
    }

    // LIFE CHEAT
    if (config().debug_developerMode_enable.getValue() && input.isKeyDown(SDLK_z))
    {
        PLA_REF docheat = INVALID_PLA_REF;

        if (input.isKeyDown( SDLK_1 ) )  docheat = 0;
        else if (input.isKeyDown( SDLK_2 ) )  docheat = 1;
        else if (input.isKeyDown( SDLK_3 ) )  docheat = 2;
        else if (input.isKeyDown( SDLK_4 ) )  docheat = 3;

        //Apply the cheat if valid
        if(docheat != INVALID_PLA_REF && docheat < module.getPlayerList().size()) {
            Object* object = tryCheatPlayerObject(module, docheat);
            if (object)
            {
                const std::shared_ptr<Object> objectHandle = module.getObjectHandler()[object->getObjRef()];
                if (objectHandle)
                {
                    //Heal 1 life
                    object->heal(objectHandle, 256, true);
                    stat_check_delay = 1;
                }
            }

        }

    }

    // Display armor stats?
    if (input.isKeyDown( SDLK_LSHIFT ) )
    {
        if (input.isKeyDown( SDLK_1 ) )  { show_armor( 0 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_2 ) )  { show_armor( 1 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_3 ) )  { show_armor( 2 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_4 ) )  { show_armor( 3 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_5 ) )  { show_armor( 4 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_6 ) )  { show_armor( 5 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_7 ) )  { show_armor( 6 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_8 ) )  { show_armor( 7 ); stat_check_delay = 1000; }
    }

    // Display enchantment stats?
    else if (input.isKeyDown( SDLK_LCTRL ) )
    {
        if (input.isKeyDown( SDLK_1 ) )  { show_full_status( 0 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_2 ) )  { show_full_status( 1 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_3 ) )  { show_full_status( 2 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_4 ) )  { show_full_status( 3 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_5 ) )  { show_full_status( 4 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_6 ) )  { show_full_status( 5 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_7 ) )  { show_full_status( 6 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_8 ) )  { show_full_status( 7 ); stat_check_delay = 1000; }
    }

    // Display character special powers?
    else if (input.isKeyDown( SDLK_LALT ) )
    {
        if (input.isKeyDown( SDLK_1 ) )  { show_magic_status( 0 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_2 ) )  { show_magic_status( 1 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_3 ) )  { show_magic_status( 2 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_4 ) )  { show_magic_status( 3 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_5 ) )  { show_magic_status( 4 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_6 ) )  { show_magic_status( 5 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_7 ) )  { show_magic_status( 6 ); stat_check_delay = 1000; }
        if (input.isKeyDown( SDLK_8 ) )  { show_magic_status( 7 ); stat_check_delay = 1000; }
    }
}

//--------------------------------------------------------------------------------------------
void show_armor( int statindex )
{
    /// @author ZF
    /// @details This function shows detailed armor information for the character

    Object *pchr = GameSessionContext::get().tryObject(activePlayingState()->getStatusCharacterRef(statindex));
    if(!pchr || pchr->isTerminated()) {
        return;
    }

    SKIN_T skinlevel = pchr->getSkin();

    const std::shared_ptr<ObjectProfile> &profile = pchr->getProfile();
    const SkinInfo &skinInfo = profile->getSkinInfo(skinlevel);

    // Armor Name
    DisplayMsg_printf("=%s=", skinInfo.name.c_str());

    // Armor Stats
    DisplayMsg_printf("~DEF: %d  SLASH:%3.0f%%~CRUSH:%3.0f%% POKE:%3.0f%%", skinInfo.defence,
                              skinInfo.damageResistance[DAMAGE_SLASH]*100.0f,
                              skinInfo.damageResistance[DAMAGE_CRUSH]*100.0f,
                              skinInfo.damageResistance[DAMAGE_POKE ]*100.0f);

    DisplayMsg_printf("~HOLY:%3.0f%%~EVIL:%3.0f%%~FIRE:%3.0f%%~ICE:%3.0f%%~ZAP:%3.0f%%",
                              skinInfo.damageResistance[DAMAGE_HOLY]*100.0f,
                              skinInfo.damageResistance[DAMAGE_EVIL]*100.0f,
                              skinInfo.damageResistance[DAMAGE_FIRE]*100.0f,
                              skinInfo.damageResistance[DAMAGE_ICE ]*100.0f,
                              skinInfo.damageResistance[DAMAGE_ZAP ]*100.0f );

    DisplayMsg_printf("~Type: %s", skinInfo.dressy ? "Light Armor" : "Heavy Armor");

    // jumps
    std::stringstream stringStream;
    switch ( static_cast<int>(pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS)) )
    {
        case 0:  stringStream << "None    (" << (int)pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) << ")"; break;
        case 1:  stringStream << "Novice  (" << (int)pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) << ")"; break;
        case 2:  stringStream << "Skilled (" << (int)pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) << ")"; break;
        case 3:  stringStream << "Adept   (" << (int)pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) << ")"; break;
        default: stringStream << "Master  (" << (int)pchr->getAttribute(Ego::Attribute::NUMBER_OF_JUMPS) << ")"; break;
    };

    DisplayMsg_printf( "~Speed:~%3.0f~Jump Skill:~%s", skinInfo.maxAccel*80, stringStream.str().c_str() );
}

//--------------------------------------------------------------------------------------------
void show_full_status( int statindex )
{
    /// @author ZF
    /// @details This function shows detailed armor information for the character including magic

    Object *pchr = GameSessionContext::get().tryObject(activePlayingState()->getStatusCharacterRef(statindex));
    if(!pchr || pchr->isTerminated()) {
        return;
    }

    SKIN_T skinlevel = pchr->getSkin();

    // Enchanted?
    DisplayMsg_printf("=%s is %s=", pchr->getName().c_str(), pchr->hasActiveEnchants() ? "enchanted" : "unenchanted" );

    // Armor Stats
    DisplayMsg_printf("~DEF: %d  SLASH:%3.0f%%~CRUSH:%3.0f%% POKE:%3.0f%%", pchr->getProfile()->getSkinInfo(skinlevel).defence,
                              pchr->getDamageReduction(DAMAGE_SLASH)*100.0f,
                              pchr->getDamageReduction(DAMAGE_CRUSH)*100.0f,
                              pchr->getDamageReduction(DAMAGE_POKE) *100.0f);

    DisplayMsg_printf("~HOLY:%3.0f%%~EVIL:%3.0f%%~FIRE:%3.0f%%~ICE:%3.0f%%~ZAP:%3.0f%%",
                              pchr->getDamageReduction(DAMAGE_HOLY)*100.0f,
                              pchr->getDamageReduction(DAMAGE_EVIL)*100.0f,
                              pchr->getDamageReduction(DAMAGE_FIRE)*100.0f,
                              pchr->getDamageReduction(DAMAGE_ICE) *100.0f,
                              pchr->getDamageReduction(DAMAGE_ZAP) *100.0f);

    DisplayMsg_printf("Mana Regen:~%4.2f Life Regen:~%4.2f", pchr->getAttribute(Ego::Attribute::MANA_REGEN), pchr->getAttribute(Ego::Attribute::LIFE_REGEN));
}

//--------------------------------------------------------------------------------------------
void show_magic_status( int statindex )
{
    /// @author ZF
    /// @details Displays special enchantment effects for the character

    Object *pchr = GameSessionContext::get().tryObject(activePlayingState()->getStatusCharacterRef(statindex));
    if(!pchr || pchr->isTerminated()) {
        return;
    }

    // Enchanted?
    DisplayMsg_printf("=%s is %s=", pchr->getName().c_str(), pchr->hasActiveEnchants() ? "enchanted" : "unenchanted");

    // Enchantment status
    DisplayMsg_printf("~See Invisible: %s~~See Kurses: %s",
                              pchr->getAttribute(Ego::Attribute::SEE_INVISIBLE) > 0 ? "Yes" : "No",
                              pchr->getAttribute(Ego::Attribute::SENSE_KURSES) > 0 ? "Yes" : "No");

    DisplayMsg_printf("~Channel Life: %s~~Waterwalking: %s",
                              pchr->getAttribute(Ego::Attribute::CHANNEL_LIFE) > 0 ? "Yes" : "No",
                              pchr->getAttribute(Ego::Attribute::WALK_ON_WATER) > 0 ? "Yes" : "No");

    DisplayMsg_printf("~Flying: %s", pchr->isFlying() ? "Yes" : "No");
}

//--------------------------------------------------------------------------------------------
void disaffirm_attached_particles(ObjectRef objectRef) {
    GameModule& module = activeModule();
    for(const std::shared_ptr<Ego::Particle> &particle : EngineContext::get().particleHandler().iterator()) {
        if (!particle->isTerminated() && particle->getAttachedObjectID() == objectRef) {
            particle->requestTerminate();
        }
    }
    if (module.getObjectHandler().exists(objectRef)) {
        // Set the alert for disaffirmation (wet torch).
        IScriptable& scriptableObject = scriptable(*module.getObjectHandler().get(objectRef));
        scriptableObject.addAIAlertBits(ALERTIF_DISAFFIRMED);
    }
}

int number_of_attached_particles(ObjectRef objectRef) {
    int cnt = 0;
    for(const std::shared_ptr<Ego::Particle> &particle : EngineContext::get().particleHandler().iterator()) {
		if (particle->isAttached() && !particle->isTerminated() && particle->getAttachedObject()->getObjRef() == objectRef) {
            cnt++;
        }
    }
    return cnt;
}

int reaffirm_attached_particles(ObjectRef objectRef) {
    GameModule& module = activeModule();
    const std::shared_ptr<Object>& object = module.getObjectHandler()[objectRef];
    if(!object) {
        return 0;
    }

    int amount = object->getProfile()->getAttachedParticleAmount();
    if (0 == amount) return 0;

    int number_attached = number_of_attached_particles(objectRef);
    if (number_attached >= amount) return 0;

    int number_added = 0;
    for (int attempts = 0; attempts < amount && number_attached < amount; ++attempts) {
        std::shared_ptr<Ego::Particle> particle = EngineContext::get().particleHandler().spawnParticle(
			object->getPosition(), idlib::canonicalize(object->getFacingZ()), object->getProfile()->getSlotNumber(),
			object->getProfile()->getAttachedParticleProfile(), objectRef, GRIP_LAST + number_attached,
			object->getTeam().toRef(), objectRef, ParticleRef::Invalid, number_attached);

        if (particle) {
            particle->placeAtVertex(object, particle->attachedto_vrt_off);
            number_added++;
            number_attached++;
        }
    }

    // Set the alert for reaffirmation ( for exploding barrels with fire )
    IScriptable& scriptableObject = scriptable(*object);
    scriptableObject.addAIAlertBits(ALERTIF_REAFFIRMED);

    return number_added;
}

//--------------------------------------------------------------------------------------------
void MainLoop::let_all_characters_think()
{
    /// @author ZZ
    /// @details This function funst the ai scripts for all eligible objects
    GameModule& module = activeModule();
    for(const std::shared_ptr<Object> &object : module.getObjectHandler().iterator())
    {
        if(object->isTerminated()) {
            continue;
        }

        //Only inventory items marked as equipment has active AI scripts
        if(object->isInsideInventory() && !object->getProfile()->isEquipment()) {
            continue;
        }

        IScriptable& scriptableObject = scriptable(*object);

        // check for actions that must always be handled
        bool is_cleanedup = scriptableObject.hasAnyAIAlertBits(ALERTIF_CLEANEDUP);
        bool is_crushed   = scriptableObject.hasAnyAIAlertBits(ALERTIF_CRUSHED);

        // only let dead/destroyed things think if they have beem crushed/cleanedup
        if (object->isAlive() || is_crushed || is_cleanedup )
        {
            // Figure out alerts that weren't already set
            set_alerts(object->getObjRef());

            // Cleaned up characters shouldn't be alert to anything else
            if (is_cleanedup) {
                scriptableObject.setAIAlertBits(ALERTIF_CLEANEDUP);
                /*object->ai.timer = worldUpdateCount() + 1;*/
            }

            // Crushed characters shouldn't be alert to anything else
            if (is_crushed)  {
                scriptableObject.setAIAlertBits(ALERTIF_CRUSHED);
                scriptableObject.setAITimer(worldUpdateCount() + 1);  //Prevents IfTimeOut from triggering
            }

            scr_run_chr_script(object.get());
        }
    }
}

//--------------------------------------------------------------------------------------------
int DisplayMsg_printf( const char *format, ... )
{
    STRING szTmp;

    va_list args;
    va_start( args, format );
    int retval = vsnprintf(szTmp, SDL_arraysize(szTmp), format, args);
    DisplayMsg_print(szTmp);
    va_end( args );

    return retval;
}

void DisplayMsg_print(const std::string &text)
{
    auto state = tryActivePlayingState();
    if (state) state->getMessageLog()->addMessage(text);
}
