#pragma once

#include "egolib/game/egoboo.h"

class ITeamMember
{
public:
    virtual ~ITeamMember() = default;

    virtual void setTeam(TEAM_REF team, bool permanent = true) = 0;
    virtual void becomeTeamLeader() = 0;
    virtual void callTeamForHelp() = 0;
    virtual void giveTeamExperience(int amount, XPType type) const = 0;
};
