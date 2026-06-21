#pragma once

#include "egolib/Profiles/_Include.hpp"  // XPType

class ITeamMember
{
public:
    virtual ~ITeamMember() = default;

    /**
    * @brief
    *   Changes the team of this Object to another team
    **/
    virtual void setTeam(TEAM_REF team, bool permanent = true) = 0;
    virtual void becomeTeamLeader() = 0;
    virtual void callTeamForHelp() = 0;
    virtual void giveTeamExperience(int amount, XPType type) const = 0;
};
