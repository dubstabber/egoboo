#pragma once

#include "egolib/game/egoboo.h"

#include <memory>

class Object;

namespace Ego
{
class Enchantment;
}

class IEnchantable
{
public:
    virtual ~IEnchantable() = default;

    virtual std::shared_ptr<Ego::Enchantment> addEnchant(ENC_REF enchantProfile, PRO_REF spawnerProfile,
                                                         const std::shared_ptr<Object>& owner,
                                                         const std::shared_ptr<Object>& spawner) = 0;
    virtual bool hasActiveEnchants() const = 0;
    virtual std::shared_ptr<Ego::Enchantment> getFirstActiveEnchant() const = 0;
    virtual bool disenchant() = 0;
    virtual std::shared_ptr<Ego::Enchantment> getLastEnchantmentSpawned() const = 0;
};
