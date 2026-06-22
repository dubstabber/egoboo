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
#include "egolib/Entities/IObjectWorld.hpp"

namespace
{
IScriptable& scriptable(Object& object)
{
    return object;
}

ObjectHandler* tryObjectHandler()
{
    Ego::Entities::IObjectWorld* world = Ego::Entities::tryActiveObjectWorld();
    return world ? &world->getObjectHandler() : nullptr;
}

ObjectHandler& objectHandler()
{
    return Ego::Entities::activeObjectWorld().getObjectHandler();
}

std::vector<Team>& teamList()
{
    return Ego::Entities::activeObjectWorld().getTeamList();
}

Object* tryLiveObject(ObjectRef objectRef)
{
    ObjectHandler* objectHandler = tryObjectHandler();
    if (objectHandler == nullptr || !objectHandler->exists(objectRef))
    {
        return nullptr;
    }

    return objectHandler->get(objectRef);
}

ObjectRef resolvedObjectRef(ObjectRef objectRef)
{
    return tryLiveObject(objectRef) ? objectRef : ObjectRef::Invalid;
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
    for(const std::shared_ptr<Object> &chr : objectHandler().iterator())
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

ObjectRef Team::getLeaderRef() const
{
    return resolvedObjectRef(_leaderRef);
}

void Team::setLeaderRef(ObjectRef objectRef)
{
    _leaderRef = resolvedObjectRef(objectRef);
}

void Team::clearLeader()
{
    setLeaderRef(ObjectRef::Invalid);
}

void Team::callForHelp(ObjectRef callerRef)
{
    _callerForHelpRef = resolvedObjectRef(callerRef);
    const Object* caller = tryLiveObject(_callerForHelpRef);
    if (caller == nullptr)
    {
        return;
    }

    //Notify all other characters who are friendly that this character has called for help
    for(const std::shared_ptr<Object> &chr : objectHandler().iterator())
    {
        if ( chr.get() != caller && !chr->getTeam().hatesTeam(caller->getTeam()) )
        {
            scriptable(*chr).addAIAlertBits(ALERTIF_CALLEDFORHELP);
        }
    }
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
    return team_hates_team(teamList()[a], teamList()[b]);
}
