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
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/game/Graphics/BillboardSystem.hpp"
#include "egolib/game/Inventory.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/mesh.h"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Module/Passage.hpp"
#include "egolib/game/Graphics/CameraSystem.hpp"
#include "egolib/game/Graphics/Billboard.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/Physics/PhysicalConstants.hpp"
#include "egolib/Script/Interpreter/SafeCast.hpp"
#include "egolib/game/GUI/MiniMap.hpp"

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

inline GameModule& activeModule()
{
    return GameSessionContext::get().activeModule();
}

inline auto& objectHandler()
{
    return activeModule().getObjectHandler();
}

inline std::shared_ptr<Passage> tryPassage(int passageId)
{
    return activeModule().getPassageByID(passageId);
}

inline ObjectRef teamLeaderRef(TEAM_REF teamRef)
{
    return activeModule().getTeamLeaderRef(teamRef);
}

inline ObjectRef teamLeaderRef(const ITargetInfo& targetInfo)
{
    return teamLeaderRef(targetInfo.getTeamRef());
}

inline ObjectRef teamCallerForHelpRef(TEAM_REF teamRef)
{
    return activeModule().getTeamCallerForHelpRef(teamRef);
}

inline ObjectRef teamCallerForHelpRef(const ITargetInfo& targetInfo)
{
    return teamCallerForHelpRef(targetInfo.getTeamRef());
}

inline Object* tryObject(ObjectRef objectRef)
{
    return objectHandler().exists(objectRef) ? objectHandler().get(objectRef) : nullptr;
}

inline bool hasLiveObjectRef(ObjectRef objectRef)
{
    return tryObject(objectRef) != nullptr;
}

inline bool hasLiveSelf(const ai_state_t& self)
{
    return hasLiveObjectRef(self.getSelf());
}

struct ResolvedSelfContext
{
    ObjectRef ref = ObjectRef::Invalid;
    Object* object = nullptr;
    ObjectProfile* profile = nullptr;

    bool isResolved() const
    {
        return ref != ObjectRef::Invalid && object != nullptr && profile != nullptr;
    }
};

inline ResolvedSelfContext resolveSelfContext(ObjectRef objectRef)
{
    ResolvedSelfContext context;
    context.ref = objectRef;
    context.object = tryObject(objectRef);
    if (context.object == nullptr)
    {
        return context;
    }

    const std::shared_ptr<ObjectProfile>& profile = context.object->getProfile();
    context.profile = profile.get();
    return context;
}

inline ResolvedSelfContext resolveSelfContext(const ai_state_t& self)
{
    return resolveSelfContext(self.getSelf());
}

inline std::shared_ptr<Object> tryObjectShared(ObjectRef objectRef)
{
    return objectHandler().exists(objectRef) ? objectHandler()[objectRef] : nullptr;
}

inline ObjectAttribution objectAttributionFromHandle(const std::shared_ptr<Object>& object)
{
    return object ? object->attribution() : ObjectAttribution();
}

inline ObjectAttribution objectAttributionFromHandle(const std::shared_ptr<Object>& object, TEAM_REF sourceTeam)
{
    return object ? object->attribution(sourceTeam) : ObjectAttribution(sourceTeam);
}

inline ObjectAttribution objectAttributionFromRef(ObjectRef objectRef)
{
    return objectAttributionFromHandle(tryObjectShared(objectRef));
}

inline ObjectAttribution objectAttributionFromRef(ObjectRef objectRef, TEAM_REF sourceTeam)
{
    return objectAttributionFromHandle(tryObjectShared(objectRef), sourceTeam);
}

inline IScriptable* tryScriptable(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IScriptable*>(object) : nullptr;
}

inline IAnimationControl* tryAnimationControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IAnimationControl*>(object) : nullptr;
}

inline IDamageable* tryDamageable(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IDamageable*>(object) : nullptr;
}

inline IEquipmentControl* tryEquipmentControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IEquipmentControl*>(object) : nullptr;
}

inline ICharacterState* tryCharacterState(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<ICharacterState*>(object) : nullptr;
}

inline IEnchantable* tryEnchantable(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IEnchantable*>(object) : nullptr;
}

inline IAppearanceProfile* tryAppearanceProfile(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IAppearanceProfile*>(object) : nullptr;
}

inline const IProfiled* tryProfiled(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<const IProfiled*>(object) : nullptr;
}

inline IInventoryHolder* tryInventoryHolder(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IInventoryHolder*>(object) : nullptr;
}

inline ILifecycleControl* tryLifecycleControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<ILifecycleControl*>(object) : nullptr;
}

inline IMorphControl* tryMorphControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IMorphControl*>(object) : nullptr;
}

inline const IItemInfo* tryItemInfo(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<const IItemInfo*>(object) : nullptr;
}

inline IMovementControl* tryMovementControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IMovementControl*>(object) : nullptr;
}

inline IRenderable* tryRenderable(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IRenderable*>(object) : nullptr;
}

inline ITeamMember* tryTeamMember(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<ITeamMember*>(object) : nullptr;
}

inline IVisualControl* tryVisualControl(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<IVisualControl*>(object) : nullptr;
}

inline const IPhysical* tryPhysical(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<const IPhysical*>(object) : nullptr;
}

inline const ITargetInfo* tryTargetInfo(ObjectRef objectRef)
{
    Object* object = tryObject(objectRef);
    return object ? static_cast<const ITargetInfo*>(object) : nullptr;
}

inline std::shared_ptr<Ego::Player> tryPlayer(size_t playerIndex)
{
    return activeModule().tryGetPlayer(playerIndex);
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
    Object* object = tryObject(objectRef);
    return object ? static_cast<IWallet*>(object) : nullptr;
}

inline uint32_t worldUpdateCount()
{
    return GameSessionContext::get().worldUpdateCount();
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
    if (const Object* selfObject = tryObject(self.getSelf()))
    {
        snapshot.selfName = selfObject->getName();
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

#define SET_TARGET_1(ITARGET,PTARGET) if( NULL != PTARGET ) { PTARGET = objectHandler().get(ITARGET); }
#define SET_TARGET(ITARGET,PTARGET)   self.setTarget(ITARGET); SET_TARGET_1(ITARGET,PTARGET)
