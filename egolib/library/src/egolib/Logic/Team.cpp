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
/**
 * @brief
 *  Game logic handling of character teams
 * @author
 *  Johan Jansen
 */

#include "Team.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Module/Module.hpp"

namespace
{
IScriptable& scriptable(Object& object)
{
    return object;
}

std::shared_ptr<Object> tryObjectShared(ObjectRef objectRef)
{
    ObjectHandler* objectHandler = GameSessionContext::get().tryObjectHandler();
    if (objectHandler == nullptr || !objectHandler->exists(objectRef))
    {
        return nullptr;
    }

    return (*objectHandler)[objectRef];
}

ObjectRef resolvedObjectRef(ObjectRef objectRef)
{
    return tryObjectShared(objectRef) ? objectRef : ObjectRef::Invalid;
}
}

Team::Team(const TEAM_REF teamID) :
    _teamID(teamID),
    _leaderRef(ObjectRef::Invalid),
    _callerForHelpRef(ObjectRef::Invalid),
    _hatesTeam{},
    _morale(0)
{

    // Make the team hate everyone else
    if(_teamID != TEAM_NULL) {
        _hatesTeam.fill(true);

        //keep the null team neutral
        _hatesTeam[TEAM_NULL] = false;

        //Make the team like itself
        _hatesTeam[_teamID] = false;
    }
    else {
        //TEAM_NULL likes everybody
        _hatesTeam.fill(false);
    }

}

void Team::giveTeamExperience(const int amount, const XPType xptype) const
{
    for(const std::shared_ptr<Object> &chr : GameSessionContext::get().objectHandler().iterator())
    {
        if ( chr->getTeam()._teamID == _teamID )
        {
            chr->giveExperience(amount, xptype, false);
        }
    }
}

bool Team::hatesTeam(const Team &other) const
{
    return _hatesTeam[other._teamID];
}

std::shared_ptr<Object> Team::getLeader() const
{
	return tryObjectShared(_leaderRef);
}

ObjectRef Team::getLeaderRef() const
{
    return resolvedObjectRef(_leaderRef);
}

void Team::setLeaderRef(ObjectRef objectRef)
{
    _leaderRef = resolvedObjectRef(objectRef);
}

void Team::setLeader(const std::shared_ptr<Object> &object)
{
	setLeaderRef(object ? object->getObjRef() : ObjectRef::Invalid);
}

void Team::clearLeader()
{
    setLeaderRef(ObjectRef::Invalid);
}

void Team::callForHelp(const std::shared_ptr<Object> &caller)
{
    callForHelp(caller ? caller->getObjRef() : ObjectRef::Invalid);
}

void Team::callForHelp(ObjectRef callerRef)
{
    _callerForHelpRef = resolvedObjectRef(callerRef);
    const std::shared_ptr<Object> caller = tryObjectShared(_callerForHelpRef);
    if (!caller)
    {
        return;
    }

    //Notify all other characters who are friendly that this character has called for help
    for(const std::shared_ptr<Object> &chr : GameSessionContext::get().objectHandler().iterator())
    {
        if ( chr != caller && !chr->getTeam().hatesTeam(caller->getTeam()) )
        {
            scriptable(*chr).addAIAlertBits(ALERTIF_CALLEDFORHELP);
        }
    }
}

std::shared_ptr<Object> Team::getSissy() const
{
    return tryObjectShared(_callerForHelpRef);
}

ObjectRef Team::getSissyRef() const
{
    return getCallerForHelpRef();
}

ObjectRef Team::getCallerForHelpRef() const
{
    return resolvedObjectRef(_callerForHelpRef);
}

void Team::setCallerForHelpRef(ObjectRef objectRef)
{
    _callerForHelpRef = resolvedObjectRef(objectRef);
}

void Team::makeAlliance(const Team &other)
{
    _hatesTeam[other._teamID] = false;
}

uint16_t Team::getMorale() const
{
    return _morale;
}

void Team::increaseMorale()
{
    _morale++;
}

void Team::decreaseMorale()
{
    if(_morale > 0)
    {
        _morale--;
    }
}

bool team_hates_team(const Team& a, const Team& b) {
    return a.hatesTeam(b);
}

bool team_hates_team(TEAM_REF a, TEAM_REF b) {
    GameModule& module = GameSessionContext::get().activeModule();
    return team_hates_team(module.getTeamList()[a], module.getTeamList()[b]);
}
