/// @file egolib/game/script_functions_teams.c
/// @brief Team-management script functions

#include "egolib/game/script_functions_internal.h"

namespace
{
struct SelfRoleContext
{
    const ITargetInfo* targetInfo = nullptr;
    ITeamMember* teamMember = nullptr;
};

struct TargetTeamContext
{
    const ITargetInfo* info = nullptr;
    ITeamMember* teamMember = nullptr;
};

struct ModuleEffectsContext
{
    IModuleCommands* module = nullptr;
};

SelfRoleContext makeSelfRoleContext(const ai_state_t& self)
{
    SelfRoleContext context;
    const ObjectRef selfRef = self.getSelf();
    context.targetInfo = tryTargetInfo(selfRef);
    context.teamMember = tryTeamMember(selfRef);
    return context;
}

TargetTeamContext makeTargetTeamContext(const ai_state_t& self)
{
    TargetTeamContext context;
    context.info = tryTargetInfo(self.getTarget());
    context.teamMember = tryTeamMember(self.getTarget());
    return context;
}

ModuleEffectsContext makeModuleEffectsContext(const ai_state_t& self)
{
    ModuleEffectsContext context;
    context.module = tryActiveModuleCommands();
    return context;
}

IModuleCommands& compatibleModule(const ModuleEffectsContext& context)
{
    if (context.module != nullptr)
    {
        return *context.module;
    }

    return moduleCommands();
}

void giveGoodTeamExperience(const ModuleEffectsContext& context, int amount, XPType type)
{
    compatibleModule(context).giveTeamExperience(static_cast<TEAM_REF>(Team::TEAM_GOOD), amount, type);
}

bool setSelfTeam(SelfRoleContext& selfContext, TEAM_REF teamRef)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(teamRef);
    return true;
}

bool joinSelfTeamToResolvedTarget(const TargetTeamContext& targetContext,
                                  SelfRoleContext& selfContext)
{
    if (targetContext.info == nullptr || selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->setTeam(targetContext.info->getTeamRef());
    return true;
}

bool becomeSelfLeader(SelfRoleContext& selfContext)
{
    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->becomeTeamLeader();
    return true;
}

bool isSelfLeaderAlive(const SelfRoleContext& selfContext)
{
    return selfContext.targetInfo != nullptr &&
           teamLeaderRef(*selfContext.targetInfo) != ObjectRef::Invalid;
}

bool giveSelfTeamExperience(const script_state_t& state, SelfRoleContext& selfContext)
{
    if (state.distance < 0 || state.distance >= XP_COUNT)
    {
        return true;
    }

    if (selfContext.teamMember == nullptr)
    {
        return false;
    }

    selfContext.teamMember->giveTeamExperience(state.argument, static_cast<XPType>(state.distance));
    return true;
}

bool setResolvedTargetTeam(const TargetTeamContext& targetContext, TEAM_REF teamRef)
{
    if (targetContext.teamMember == nullptr)
    {
        return false;
    }

    targetContext.teamMember->setTeam(teamRef);
    return true;
}
} // namespace


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTargetTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTargetTeam()
    /// @author ZZ
    /// @details This function lets a character join a different team.  Used
    /// mostly for pets

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetTeamContext targetContext = makeTargetTeamContext(self);
    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return joinSelfTeamToResolvedTarget(targetContext, selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderKilled( script_state_t& state, ai_state_t& self )
{
    // IfLeaderKilled()
    /// @author ZZ
    /// @details This function proceeds if the team's leader died this update

    if (!resolveSelfContext(self).isResolved()) return false;

    return HAS_SOME_BITS( self.alert, ALERTIF_LEADERKILLED );
}


//--------------------------------------------------------------------------------------------
uint8_t scr_BecomeLeader( script_state_t& state, ai_state_t& self )
{
    // BecomeLeader()
    /// @author ZZ
    /// @details This function makes the character the leader of the team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return becomeSelfLeader(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_IfLeaderIsAlive( script_state_t& state, ai_state_t& self )
{
    // IfLeaderIsAlive()
    /// @author ZZ
    /// @details This function proceeds if the team has a leader

    if (!resolveSelfContext(self).isResolved()) return false;

    const SelfRoleContext selfContext = makeSelfRoleContext(self);
    return isSelfLeaderAlive(selfContext);
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinTeam( script_state_t& state, ai_state_t& self )
{
    // JoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the character itself join a specified team (A = 0, B = 1, 23 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfTeam(selfContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_TargetJoinTeam( script_state_t& state, ai_state_t& self )
{
    // TargetJoinTeam( tmpargument = "team" )
    /// @author ZZ
    /// @details This makes the Target join a Team specified in tmpargument (A = 0, 25 = Z, etc.)

    if (!resolveSelfContext(self).isResolved()) return false;

    const TargetTeamContext targetContext = makeTargetTeamContext(self);
    return setResolvedTargetTeam(targetContext, static_cast<TEAM_REF>(state.argument));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinEvilTeam( script_state_t& state, ai_state_t& self )
{
    // JoinEvilTeam()
    /// @author ZZ
    /// @details This function adds the character to the evil Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_EVIL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinNullTeam( script_state_t& state, ai_state_t& self )
{
    // JoinNullTeam()
    /// @author ZZ
    /// @details This function adds the character to the null Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_NULL));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_JoinGoodTeam( script_state_t& state, ai_state_t& self )
{
    // JoinGoodTeam()
    /// @author ZZ
    /// @details This function adds the character to the good Team.

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return setSelfTeam(selfContext, static_cast<TEAM_REF>(Team::TEAM_GOOD));
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToGoodTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToGoodTeam(  tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the G Team

    if (!resolveSelfContext(self).isResolved()) return false;

    if(state.distance < XP_COUNT)
    {
        const ModuleEffectsContext moduleContext = makeModuleEffectsContext(self);
        giveGoodTeamExperience(moduleContext, state.argument, static_cast<XPType>(state.distance));
    }


    return true;
}


//--------------------------------------------------------------------------------------------
uint8_t scr_GiveExperienceToTargetTeam( script_state_t& state, ai_state_t& self )
{
    // GiveExperienceToTargetTeam( tmpargument = "amount", tmpdistance = "type" )
    /// @author ZZ
    /// @details This function gives experience to everyone on the target's team

    if (!resolveSelfContext(self).isResolved()) return false;

    SelfRoleContext selfContext = makeSelfRoleContext(self);
    return giveSelfTeamExperience(state, selfContext);
}
