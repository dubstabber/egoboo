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

/// @file egolib/game/script_functions_internal.h
/// @brief Shared infrastructure for the split egoscript function implementation files.
/// @details This header contains includes, macros, templates, and accessor functions
/// that are used by all script_functions_*.c files.

#pragma once

#include "egolib/Graphics/ModelDescriptor.hpp"
#include "egolib/game/script_functions.h"
#include "egolib/game/script_implementation.h"
#include "egolib/game/IPlayingStateController.hpp"
#include "egolib/game/link.h"
#include "egolib/game/game.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Entities/IAnimationControl.hpp"
#include "egolib/Entities/IAppearanceProfile.hpp"
#include "egolib/Entities/IAttachmentControl.hpp"
#include "egolib/Entities/ICharacterState.hpp"
#include "egolib/Entities/IDamageable.hpp"
#include "egolib/Entities/IEnchantable.hpp"
#include "egolib/Entities/IEquipmentControl.hpp"
#include "egolib/Entities/IInventoryHolder.hpp"
#include "egolib/Entities/IItemInfo.hpp"
#include "egolib/Entities/ILifecycleControl.hpp"
#include "egolib/Entities/IMorphControl.hpp"
#include "egolib/Entities/IMovementControl.hpp"
#include "egolib/Entities/IPhysical.hpp"
#include "egolib/Entities/IProfiled.hpp"
#include "egolib/Entities/IRenderable.hpp"
#include "egolib/Entities/IScriptable.hpp"
#include "egolib/Entities/IScriptRuntimeState.hpp"
#include "egolib/Entities/ITeamMember.hpp"
#include "egolib/Entities/ITargetInfo.hpp"
#include "egolib/Entities/IVisibilityObserver.hpp"
#include "egolib/Entities/IVisualControl.hpp"
#include "egolib/Entities/IWallet.hpp"
#include "egolib/Entities/ObjectConstants.hpp"
#include "egolib/Entities/ObjectRoleAccess.hpp"
#include "egolib/Profiles/_Include.hpp"
#include "egolib/game/mesh.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Module/IModuleCommands.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/Physics/PhysicalConstants.hpp"
#include "egolib/Script/Interpreter/SafeCast.hpp"

/**
 * @brief Convert a value of type \f$Value\f$ value into a bit index.
 * @param taggedValue the \f$Value\f$
 * @throw Ego:Script::InvalidCastException
 * if the value of type \f$Value\f$ value can not be cast into a value of type \f$Integer\f$
 * @throw idlib::out_of_bounds_error
 * if the value of type \f$Integer\f$ is not within the specified bounds of <tt>[min, max]</tt>.
 */
template <int min, int max>
std::enable_if_t<min <= max, int> getBitIndex(const Ego::Script::Interpreter::TaggedValue& taggedValue) {
    int bitIndex = (int)taggedValue;
    if (bitIndex < min || bitIndex > max) {
        std::ostringstream os;
        os << "bit index must be within the bounds of " << min << " and " << max;
        throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, os.str());
    }
    return bitIndex;
}

// TODO: Remove this.
template <int min, int max>
std::enable_if_t<min <= max, int> getBitIndex(int value) {
    int bitIndex = (int)value;
    if (bitIndex < min || bitIndex > max) {
        std::ostringstream os;
        os << "bit index must be within the bounds of " << min << " and " << max;
        throw idlib::argument_out_of_bounds_error(__FILE__, __LINE__, os.str());
    }
    return bitIndex;
}

namespace Ego {
namespace Script {
namespace Interpreter {
template <>
inline IDSZ2 safeCast<IDSZ2, int>(const int& v) {
    return IDSZ2(safeCast<uint32_t>(v));
}
} // namespace Interpreter
} // namespace Script
} // namespace Ego

namespace script_detail
{
inline GameEngine& engine()
{
    return EngineContext::get().engine();
}

inline std::shared_ptr<IPlayingStateController> activePlayingState()
{
    return EngineContext::get().activePlayingState();
}

inline std::shared_ptr<IPlayingStateController> tryActivePlayingState()
{
    return EngineContext::get().tryActivePlayingState();
}

inline IModuleCommands& moduleCommands()
{
    return activeModuleCommands();
}

inline std::shared_ptr<Passage> tryPassage(int passageId)
{
    return moduleCommands().getPassageByID(passageId);
}

inline ObjectRef teamLeaderRef(TEAM_REF teamRef)
{
    return moduleCommands().getTeamLeaderRef(teamRef);
}

inline ObjectRef teamLeaderRef(const ITargetInfo& targetInfo)
{
    return teamLeaderRef(targetInfo.getTeamRef());
}

inline ObjectRef teamCallerForHelpRef(TEAM_REF teamRef)
{
    return moduleCommands().getTeamCallerForHelpRef(teamRef);
}

inline ObjectRef teamCallerForHelpRef(const ITargetInfo& targetInfo)
{
    return teamCallerForHelpRef(targetInfo.getTeamRef());
}

inline bool hasLiveObjectRef(ObjectRef objectRef)
{
    return Ego::Entities::activeObjectExists(objectRef);
}

inline bool hasLiveSelf(const ai_state_t& self)
{
    return hasLiveObjectRef(self.getSelf());
}

inline const IProfiled* tryProfiled(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveProfiled(objectRef);
}

struct ResolvedSelfContext
{
    ObjectRef ref = ObjectRef::Invalid;
    const ObjectProfile* profile = nullptr;

    bool isResolved() const
    {
        return ref != ObjectRef::Invalid && profile != nullptr;
    }
};

inline ResolvedSelfContext resolveSelfContext(ObjectRef objectRef)
{
    ResolvedSelfContext context;
    context.ref = objectRef;
    const IProfiled* profiled = tryProfiled(objectRef);
    if (profiled == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile>& profile = profiled->getProfile();
    context.profile = profile.get();
    return context;
}

inline ResolvedSelfContext resolveSelfContext(const ai_state_t& self)
{
    return resolveSelfContext(self.getSelf());
}

inline ObjectAttribution objectAttributionFromRef(ObjectRef objectRef)
{
    const ITargetInfo* targetInfo = Ego::Entities::tryActiveTargetInfo(objectRef);
    const IInventoryHolder* inventory = Ego::Entities::tryActiveInventoryHolder(objectRef);
    if (targetInfo == nullptr || inventory == nullptr)
    {
        return ObjectAttribution();
    }
    return ObjectAttribution(objectRef, targetInfo->getTeamRef(), targetInfo->getTeamRef(),
                             targetInfo->isPlayer(), inventory->isTerminated());
}

inline ObjectAttribution objectAttributionFromRef(ObjectRef objectRef, TEAM_REF sourceTeam)
{
    const ITargetInfo* targetInfo = Ego::Entities::tryActiveTargetInfo(objectRef);
    const IInventoryHolder* inventory = Ego::Entities::tryActiveInventoryHolder(objectRef);
    if (targetInfo == nullptr || inventory == nullptr)
    {
        return ObjectAttribution(sourceTeam);
    }
    return ObjectAttribution(objectRef, targetInfo->getTeamRef(), sourceTeam,
                             targetInfo->isPlayer(), inventory->isTerminated());
}

inline IScriptable* tryScriptable(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveScriptable(objectRef);
}

inline IScriptRuntimeState* tryScriptRuntimeState(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveScriptRuntimeState(objectRef);
}

inline IAnimationControl* tryAnimationControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveAnimationControl(objectRef);
}

inline IDamageable* tryDamageable(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveDamageable(objectRef);
}

inline IDamageable* tryLivingDamageable(ObjectRef objectRef)
{
    IDamageable* damageable = tryDamageable(objectRef);
    return damageable != nullptr && damageable->isAlive() ? damageable : nullptr;
}

inline IEquipmentControl* tryEquipmentControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveEquipmentControl(objectRef);
}

inline ICharacterState* tryCharacterState(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveCharacterState(objectRef);
}

inline ICharacterState* tryLivingCharacterState(ObjectRef objectRef)
{
    return tryLivingDamageable(objectRef) != nullptr ? tryCharacterState(objectRef) : nullptr;
}

inline IEnchantable* tryEnchantable(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveEnchantable(objectRef);
}

inline IAppearanceProfile* tryAppearanceProfile(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveAppearanceProfile(objectRef);
}

inline IInventoryHolder* tryInventoryHolder(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveInventoryHolder(objectRef);
}

inline IAttachmentControl* tryAttachmentControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveAttachmentControl(objectRef);
}

inline ILifecycleControl* tryLifecycleControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveLifecycleControl(objectRef);
}

inline IMorphControl* tryMorphControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveMorphControl(objectRef);
}

inline const IItemInfo* tryItemInfo(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveItemInfo(objectRef);
}

inline IMovementControl* tryMovementControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveMovementControl(objectRef);
}

inline IRenderable* tryRenderable(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveRenderable(objectRef);
}

inline ITeamMember* tryTeamMember(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveTeamMember(objectRef);
}

inline IVisualControl* tryVisualControl(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveVisualControl(objectRef);
}

inline const IPhysical* tryPhysical(ObjectRef objectRef)
{
    return Ego::Entities::tryActivePhysical(objectRef);
}

inline const ITargetInfo* tryTargetInfo(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveTargetInfo(objectRef);
}

inline const IVisibilityObserver* tryVisibilityObserver(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveVisibilityObserver(objectRef);
}

inline std::shared_ptr<Ego::Player> tryPlayer(size_t playerIndex)
{
    return moduleCommands().tryGetPlayer(playerIndex);
}

inline std::shared_ptr<Ego::Player> tryPlayer(const ITargetInfo& targetInfo)
{
    return targetInfo.isPlayer() ? tryPlayer(targetInfo.getPlayerNumber()) : nullptr;
}

inline Ego::QuestLog* tryQuestLog(const ITargetInfo& targetInfo)
{
    const std::shared_ptr<Ego::Player> player = tryPlayer(targetInfo);
    return player ? &player->getQuestLog() : nullptr;
}

inline IWallet* tryWallet(ObjectRef objectRef)
{
    return Ego::Entities::tryActiveWallet(objectRef);
}

inline uint32_t worldUpdateCount()
{
    return activeSessionState().worldUpdateCount();
}

struct SelfProfileSnapshot
{
    const ObjectProfile* profile = nullptr;
    std::string selfName;
    std::string className;
    ObjectProfileRef profileRef = ObjectProfileRef::Invalid;
    EVE_REF enchantRef = INVALID_EVE_REF;
    SKIN_T spellEffectSkin = ObjectProfile::NO_SKIN_OVERRIDE;
    ObjectProfileRef baseModelRef = ObjectProfileRef::Invalid;
    bool baseModelIsSpellbook = false;
    bool currentProfileMatchesBaseModel = false;

    bool isResolved() const
    {
        return profile != nullptr;
    }
};

inline SelfProfileSnapshot makeSelfProfileSnapshot(const ai_state_t& self)
{
    SelfProfileSnapshot snapshot;
    const IProfiled* selfProfiled = tryProfiled(self.getSelf());
    if (selfProfiled == nullptr)
    {
        return snapshot;
    }

    const std::shared_ptr<ObjectProfile>& selfProfile = selfProfiled->getProfile();
    if (selfProfile == nullptr)
    {
        return snapshot;
    }

    snapshot.profile = selfProfile.get();
    if (const ITargetInfo* selfInfo = tryTargetInfo(self.getSelf()))
    {
        snapshot.selfName = selfInfo->getDisplayName();
    }
    snapshot.className = selfProfile->getClassName();
    snapshot.profileRef = selfProfile->getSlotNumber();
    snapshot.enchantRef = selfProfile->getEnchantRef();
    snapshot.spellEffectSkin = selfProfile->getSpellEffectType();
    if (const IMorphControl* selfMorph = tryMorphControl(self.getSelf()))
    {
        snapshot.baseModelRef = selfMorph->getBaseModelRef();
    }
    snapshot.baseModelIsSpellbook = snapshot.baseModelRef == ObjectProfileRef(SPELLBOOK);
    snapshot.currentProfileMatchesBaseModel = snapshot.baseModelRef == snapshot.profileRef;
    return snapshot;
}
}
using namespace script_detail;

//--------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------
// turn off this annoying warning
#if defined _MSC_VER
#    pragma warning(disable : 4189) // local variable is initialized but not referenced
#endif
