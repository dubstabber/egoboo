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

/// @file egolib/game/script_functions_action_internal.h
/// @brief Shared action-helper infrastructure for the three script_functions_action*.c TUs.
/// @details Private to script_functions_action.c / script_functions_action_audio.c /
/// script_functions_action_visual.c — do not include from other script_functions_*.c TUs.

#pragma once

#include "egolib/game/script_functions_internal.h"

namespace script_action_detail
{
struct SelfActionContext
{
    ObjectRef selfRef = ObjectRef::Invalid;
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;
    IAnimationControl* animation = nullptr;
    IVisualControl* visual = nullptr;
    ITeamMember* teamMember = nullptr;
    const ITargetInfo* targetInfo = nullptr;

    bool isResolved() const
    {
        return selfRef != ObjectRef::Invalid &&
               object != nullptr &&
               profile != nullptr &&
               animation != nullptr &&
               visual != nullptr &&
               teamMember != nullptr &&
               targetInfo != nullptr;
    }

    const Ego::Vector3f& oldPosition() const
    {
        return object->getOldPosition();
    }

    PRO_REF profileRef() const
    {
        return object->getProfileID().get();
    }

    SoundID soundID(int soundIndex) const
    {
        return profile->getSoundID(soundIndex);
    }

    bool hasMessageID(int messageId) const
    {
        return profile->isValidMessageID(messageId);
    }

    const std::string& messageText(int messageId) const
    {
        return profile->getMessage(messageId);
    }
};

inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline SelfActionContext makeSelfActionContext(const ai_state_t& self)
{
    SelfActionContext context;
    context.selfRef = self.getSelf();
    context.object = tryObject(context.selfRef);
    if (context.object == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile> profile = context.object->getProfile();
    if (profile == nullptr)
    {
        context.object = nullptr;
        return context;
    }

    context.profile = profile.get();
    context.animation = static_cast<IAnimationControl*>(context.object);
    context.visual = static_cast<IVisualControl*>(context.object);
    context.teamMember = static_cast<ITeamMember*>(context.object);
    context.targetInfo = static_cast<const ITargetInfo*>(context.object);
    return context;
}
}
using namespace script_action_detail;
