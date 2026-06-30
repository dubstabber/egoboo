#pragma once

#include "egolib/Clock.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameEngine.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/GUI/UIManager.hpp"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/Module/Module.hpp"

namespace gfx_internal
{
inline GameEngine& engine()
{
    return EngineContext::get().engine();
}

inline Ego::GUI::UIManager& uiManager()
{
    return *engine().getUIManager();
}

inline GameSessionContext& gameSession()
{
    return GameSessionContext::get();
}

inline GameModule& activeModule()
{
    return gameSession().activeModule();
}

inline IModuleEnvironment& moduleEnvironment()
{
    return activeModuleEnvironment();
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
