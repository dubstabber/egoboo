#pragma once

#include "egolib/Clock.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/Module/IModuleCommands.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/IModuleStatus.hpp"
#include "egolib/Renderer/Renderer.hpp"

namespace gfx_internal
{
inline GameEngine& engine()
{
    return EngineContext::get().engine();
}

inline Ego::GUI::IUIManager& uiManager()
{
    return *engine().getUIManager();
}

inline IModuleCommands& moduleCommands()
{
    return activeModuleCommands();
}

inline IModuleEnvironment& moduleEnvironment()
{
    return activeModuleEnvironment();
}

inline IModuleStatus& moduleStatus()
{
    return activeModuleStatus();
}

inline ISessionState& sessionState()
{
    return activeSessionState();
}

inline uint32_t renderedFrameCount()
{
    return engine().getNumberOfFramesRendered();
}

inline uint32_t worldUpdateCount()
{
    return sessionState().worldUpdateCount();
}
} // namespace gfx_internal

extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> sortDoListUnreflected_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> sortDoListReflected_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> render_scene_init_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> render_scene_mesh_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> gfx_make_tileList_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> gfx_make_entityList_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> do_grid_lighting_timer;
extern Ego::Time::Clock<Ego::Time::ClockPolicy::NonRecursive> light_fans_timer;

gfx_rv gfx_make_dynalist(dynalist_t& dyl, Camera& cam);
