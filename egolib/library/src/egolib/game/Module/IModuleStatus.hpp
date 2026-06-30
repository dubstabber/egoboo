#pragma once

/// @file egolib/game/Module/IModuleStatus.hpp
/// @brief Active read-only module-status seam.

#include <list>
#include <memory>
#include <string>

class ModuleProfile;

/// @brief The active module-status surface for callers that need module
///        completion, respawn/export, passage-count, or menu metadata without
///        depending on GameModule.
class IModuleStatus
{
public:
    virtual ~IModuleStatus() = default;

    virtual bool isExportValid() const = 0;
    virtual bool isRespawnValid() const = 0;
    virtual bool canRespawnAnyTime() const = 0;
    virtual bool isBeaten() const = 0;
    virtual int passageCount() const = 0;
    virtual const std::shared_ptr<ModuleProfile>& moduleProfile() const = 0;
    virtual const std::list<std::string>& importPlayers() const = 0;
};

/// @brief Install @a status as the active module status. Passing nullptr is
///        equivalent to clearModuleStatus().
void installModuleStatus(IModuleStatus* status);

/// @brief Clear the installed active module status.
void clearModuleStatus();

/// @brief The installed module status, or nullptr if none is installed.
IModuleStatus* tryActiveModuleStatus();

/// @brief The installed active module status.
/// @throw std::logic_error if no module status is installed.
IModuleStatus& activeModuleStatus();
