#pragma once

#include "egolib/typedef.h"  // ENC_REF, ObjectRef, PRO_REF

#include <memory>

namespace Ego
{
class Enchantment;
}

class IEnchantable
{
public:
    virtual ~IEnchantable() = default;

    /**
    * @brief
    *   Applies an enchantment to this object
    * @param enchantProfile
    *   The unique profile ID for the Enchantment (ENC_REF)
    * @param spawnerProfile
    *   The unique ObjectProfile ID for the object that creates this enchant
    * @brief
    *   pointer to the enchant that was added (or nullptr if it failed)
    **/
    virtual std::shared_ptr<Ego::Enchantment> addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile,
                                                         ObjectRef ownerRef,
                                                         ObjectRef spawnerRef) = 0;
    virtual bool hasActiveEnchants() const = 0;
    virtual std::shared_ptr<Ego::Enchantment> getFirstActiveEnchant() const = 0;
    /**
    * @brief
    *   Removes all enchantments from character
    **/
    virtual bool disenchant() = 0;
    virtual std::shared_ptr<Ego::Enchantment> getLastEnchantmentSpawned() const = 0;
};
