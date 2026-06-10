#pragma once

#include "egolib/typedef.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class EnchantProfile;
class LoadPlayerElement;
class ModuleProfile;
class ObjectProfile;
class ParticleProfile;
namespace Ego
{
class DeferredTexture;
}

class IProfileSystem
{
public:
    virtual ~IProfileSystem() = default;

    virtual void reset() = 0;

    virtual bool isLoaded(PRO_REF ref) const = 0;
    virtual bool isLoaded(ObjectProfileRef ref) const = 0;

    virtual ObjectProfileRef loadOneProfile(const std::string& folderPath, int slot_override = -1) = 0;
    virtual const std::shared_ptr<ObjectProfile>& getProfile(PRO_REF ref) const = 0;
    virtual const std::shared_ptr<ObjectProfile>& getProfile(ObjectProfileRef ref) const = 0;
    virtual const std::unordered_map<PRO_REF, std::shared_ptr<ObjectProfile>>& getLoadedProfiles() const = 0;
    virtual const Ego::DeferredTexture& getSpellBookIcon(size_t index) const = 0;

    virtual void loadModuleProfiles() = 0;
    virtual const std::vector<std::shared_ptr<ModuleProfile>>& getModuleProfiles() const = 0;

    virtual void loadAllSavedCharacters(const std::string& saveGameDirectory) = 0;
    virtual const std::vector<std::shared_ptr<LoadPlayerElement>>& getSavedPlayers() const = 0;

    virtual void loadGlobalParticleProfiles() = 0;

    virtual bool isParticleProfileLoaded(PIP_REF ref) const = 0;
    virtual const std::shared_ptr<ParticleProfile>& getParticleProfile(PIP_REF ref) const = 0;
    virtual PIP_REF loadParticleProfile(const std::string& pathname, PIP_REF overrideRef) = 0;

    virtual bool isEnchantProfileLoaded(EVE_REF ref) const = 0;
    virtual const std::shared_ptr<EnchantProfile>& getEnchantProfile(EVE_REF ref) const = 0;
    virtual EVE_REF loadEnchantProfile(const std::string& pathname, EVE_REF overrideRef) = 0;
};

/// @brief Install the active profile system.
/// @throw std::logic_error if already installed
void installActiveProfileSystem(IProfileSystem& profileSystem);

/// @brief Clear the installed active profile system.
void clearActiveProfileSystem();

/// @brief The installed active profile system, or @a nullptr if none is installed.
IProfileSystem* tryActiveProfileSystem();

/// @brief The active profile system.
/// @throw std::logic_error if none is installed
IProfileSystem& activeProfileSystem();
