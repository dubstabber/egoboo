#include "egolib/Graphics/SDL/Utilities.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/egoboo_setup.h"

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}
}

Requirement::Requirement()
{}

Requirement::~Requirement()
{}

AliasingRequirement::AliasingRequirement()
{
    auto& configuration = config();
    m_oldValue = configuration.graphic_antialiasing.getValue();
}

void AliasingRequirement::reset()
{
    auto& configuration = config();
    configuration.graphic_antialiasing.setValue(m_oldValue);
}

bool AliasingRequirement::relax()
{
    auto& configuration = config();
    if (configuration.graphic_antialiasing.getValue() == 0) return false;
    configuration.graphic_antialiasing.setValue(configuration.graphic_antialiasing.getValue() - 1);
    return true;
}

FullscreenRequirement::FullscreenRequirement()
{
    auto& configuration = config();
    m_oldValue = configuration.graphic_fullscreen.getValue();
}

void FullscreenRequirement::reset()
{
    auto& configuration = config();
    configuration.graphic_fullscreen.setValue(m_oldValue);
}

bool FullscreenRequirement::relax()
{
    auto& configuration = config();
    if (!configuration.graphic_fullscreen.getValue()) return false;
    configuration.graphic_fullscreen.setValue(false);
    return true;
}

void Requirements::reset()
{
    for (auto& requirement : requirements)
    {
        requirement->reset();
    }
}

bool Requirements::relax()
{
    for (auto& requirement : requirements)
    {
        if (requirement->relax())
        {
            return true;
        }
    }
    return false;
}
