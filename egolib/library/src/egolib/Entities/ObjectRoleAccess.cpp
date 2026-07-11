#include "egolib/Entities/ObjectRoleAccess.hpp"

#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/Entities/_Include.hpp"

#include <iterator>

namespace Ego
{
namespace Entities
{

namespace
{
template <typename Role>
Role* tryActiveRole(ObjectRef objectRef)
{
    Object* object = tryActiveObject(objectRef);
    return object ? static_cast<Role*>(object) : nullptr;
}

template <typename Role>
const Role* tryActiveConstRole(ObjectRef objectRef)
{
    const Object* object = tryActiveConstObject(objectRef);
    return object ? static_cast<const Role*>(object) : nullptr;
}
} // namespace

std::vector<ObjectRef> activeObjectRefs()
{
    ObjectHandler* handler = tryActiveObjectHandler();
    if (handler == nullptr)
    {
        return {};
    }

    std::vector<ObjectRef> liveRefs;
    const auto refs = handler->objectRefIterator();
    liveRefs.reserve(std::distance(refs.begin(), refs.end()));
    for (const ObjectRef ref : refs)
    {
        if (activeObjectExists(ref))
        {
            liveRefs.push_back(ref);
        }
    }
    return liveRefs;
}

const IProfiled* tryActiveProfiled(ObjectRef objectRef) { return tryActiveConstRole<IProfiled>(objectRef); }
IScriptable* tryActiveScriptable(ObjectRef objectRef) { return tryActiveRole<IScriptable>(objectRef); }
IScriptRuntimeState* tryActiveScriptRuntimeState(ObjectRef objectRef) { return tryActiveRole<IScriptRuntimeState>(objectRef); }
IAnimationControl* tryActiveAnimationControl(ObjectRef objectRef) { return tryActiveRole<IAnimationControl>(objectRef); }
IDamageable* tryActiveDamageable(ObjectRef objectRef) { return tryActiveRole<IDamageable>(objectRef); }
IEquipmentControl* tryActiveEquipmentControl(ObjectRef objectRef) { return tryActiveRole<IEquipmentControl>(objectRef); }
ICharacterState* tryActiveCharacterState(ObjectRef objectRef) { return tryActiveRole<ICharacterState>(objectRef); }
IEnchantable* tryActiveEnchantable(ObjectRef objectRef) { return tryActiveRole<IEnchantable>(objectRef); }
IAppearanceProfile* tryActiveAppearanceProfile(ObjectRef objectRef) { return tryActiveRole<IAppearanceProfile>(objectRef); }
IInventoryHolder* tryActiveInventoryHolder(ObjectRef objectRef) { return tryActiveRole<IInventoryHolder>(objectRef); }
IAttachmentControl* tryActiveAttachmentControl(ObjectRef objectRef) { return tryActiveRole<IAttachmentControl>(objectRef); }
ILifecycleControl* tryActiveLifecycleControl(ObjectRef objectRef) { return tryActiveRole<ILifecycleControl>(objectRef); }
IMorphControl* tryActiveMorphControl(ObjectRef objectRef) { return tryActiveRole<IMorphControl>(objectRef); }
const IItemInfo* tryActiveItemInfo(ObjectRef objectRef) { return tryActiveConstRole<IItemInfo>(objectRef); }
IMovementControl* tryActiveMovementControl(ObjectRef objectRef) { return tryActiveRole<IMovementControl>(objectRef); }
IRenderable* tryActiveRenderable(ObjectRef objectRef) { return tryActiveRole<IRenderable>(objectRef); }
ITeamMember* tryActiveTeamMember(ObjectRef objectRef) { return tryActiveRole<ITeamMember>(objectRef); }
IVisualControl* tryActiveVisualControl(ObjectRef objectRef) { return tryActiveRole<IVisualControl>(objectRef); }
IWallet* tryActiveWallet(ObjectRef objectRef) { return tryActiveRole<IWallet>(objectRef); }
const IPhysical* tryActivePhysical(ObjectRef objectRef) { return tryActiveConstRole<IPhysical>(objectRef); }
const ITargetInfo* tryActiveTargetInfo(ObjectRef objectRef) { return tryActiveConstRole<ITargetInfo>(objectRef); }
const IVisibilityObserver* tryActiveVisibilityObserver(ObjectRef objectRef) { return tryActiveConstRole<IVisibilityObserver>(objectRef); }

} // namespace Entities
} // namespace Ego
