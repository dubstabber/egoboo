#pragma once

#include "egolib/typedef.h"  // ObjectRef

#include <vector>

class IAnimationControl;
class IAppearanceProfile;
class IAttachmentControl;
class ICharacterState;
class IDamageable;
class IEnchantable;
class IEquipmentControl;
class IInventoryHolder;
class IItemInfo;
class ILifecycleControl;
class IMorphControl;
class IMovementControl;
class IPhysical;
class IProfiled;
class IRenderable;
class IScriptable;
class IScriptRuntimeState;
class ITeamMember;
class ITargetInfo;
class IVisibilityObserver;
class IVisualControl;
class IWallet;

namespace Ego
{
namespace Entities
{

/// @brief Snapshot the live references in the active object world.
/// @return An empty vector when there is no active world.
std::vector<ObjectRef> activeObjectRefs();

/// @brief Resolve individual roles from the active object world without exposing Object.
/// @return The requested role, or nullptr when there is no active world or the ref is invalid/stale.
const IProfiled* tryActiveProfiled(ObjectRef objectRef);
IScriptable* tryActiveScriptable(ObjectRef objectRef);
IScriptRuntimeState* tryActiveScriptRuntimeState(ObjectRef objectRef);
IAnimationControl* tryActiveAnimationControl(ObjectRef objectRef);
IDamageable* tryActiveDamageable(ObjectRef objectRef);
IEquipmentControl* tryActiveEquipmentControl(ObjectRef objectRef);
ICharacterState* tryActiveCharacterState(ObjectRef objectRef);
IEnchantable* tryActiveEnchantable(ObjectRef objectRef);
IAppearanceProfile* tryActiveAppearanceProfile(ObjectRef objectRef);
IInventoryHolder* tryActiveInventoryHolder(ObjectRef objectRef);
IAttachmentControl* tryActiveAttachmentControl(ObjectRef objectRef);
ILifecycleControl* tryActiveLifecycleControl(ObjectRef objectRef);
IMorphControl* tryActiveMorphControl(ObjectRef objectRef);
const IItemInfo* tryActiveItemInfo(ObjectRef objectRef);
IMovementControl* tryActiveMovementControl(ObjectRef objectRef);
IRenderable* tryActiveRenderable(ObjectRef objectRef);
ITeamMember* tryActiveTeamMember(ObjectRef objectRef);
IVisualControl* tryActiveVisualControl(ObjectRef objectRef);
IWallet* tryActiveWallet(ObjectRef objectRef);
const IPhysical* tryActivePhysical(ObjectRef objectRef);
const ITargetInfo* tryActiveTargetInfo(ObjectRef objectRef);
const IVisibilityObserver* tryActiveVisibilityObserver(ObjectRef objectRef);

} // namespace Entities
} // namespace Ego
