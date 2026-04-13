#pragma once

#include "idlib/non_copyable.hpp"

#include <cstdint>
#include <memory>

class GameModule;
class ModuleProfile;
struct import_list_t;

class GameSessionContext : private idlib::non_copyable
{
public:
    static GameSessionContext& get();

    bool hasActiveModule() const;

    GameModule* tryActiveModule();
    const GameModule* tryActiveModule() const;

    GameModule& activeModule();
    const GameModule& activeModule() const;

    bool beginModule(const std::shared_ptr<ModuleProfile>& module);
    bool beginModule(const std::shared_ptr<ModuleProfile>& module, uint32_t seed);
    void quitModule();
    bool finishModule();

    import_list_t& importList();
    const import_list_t& importList() const;

    bool& overrideSlots();
    uint32_t& worldUpdateCount();
    uint32_t& characterStatClock();
    uint32_t& enchantStatClock();

    void resetClocks();

private:
    GameSessionContext() = default;
};
