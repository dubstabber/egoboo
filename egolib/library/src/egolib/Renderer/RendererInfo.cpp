#include "egolib/Renderer/RendererInfo.hpp"

#include "egolib/egoboo_setup.h"

namespace Ego {

RendererInfo::RendererInfo()
{
    auto& configuration = Ego::activeConfig();

    try
    {
        m_connections.push_back(configuration.graphic_anisotropy_enable.ValueChanged.subscribe([this]()
            { 
                auto& configuration = Ego::activeConfig();
                m_isAnisotropyDesired = configuration.graphic_anisotropy_enable.getValue();
                AnisotropyDesiredChanged(); 
            }));
        m_connections.push_back(configuration.graphic_anisotropy_levels.ValueChanged.subscribe([this]()
            {
                auto& configuration = Ego::activeConfig();
                m_desiredAnisotropy = configuration.graphic_anisotropy_levels.getValue();
                DesiredAnisotropyChanged();
            }));
        m_connections.push_back(configuration.graphic_textureFilter_minFilter.ValueChanged.subscribe([this]()
        { 
            auto& configuration = Ego::activeConfig();
            m_desiredMinimizationFilter = configuration.graphic_textureFilter_minFilter.getValue();
            DesiredMinimizationFilterChanged(); 
        }));
        m_connections.push_back(configuration.graphic_textureFilter_magFilter.ValueChanged.subscribe([this]()
        { 
            auto& configuration = Ego::activeConfig();
            m_desiredMaximizationFilter = configuration.graphic_textureFilter_magFilter.getValue();
            DesiredMaximizationFilterChanged(); 
        }));
        m_connections.push_back(configuration.graphic_textureFilter_mipMapFilter.ValueChanged.subscribe([this]()
        { 
            auto& configuration = Ego::activeConfig();
            m_desiredMipMapFilter = configuration.graphic_textureFilter_mipMapFilter.getValue();
            DesiredMipMapFilterChanged(); 
        }));
    }
    catch (...)
    {
        for (auto& connection : m_connections)
        {
            connection.disconnect();
        }
        throw;
    }
    try
    {
        m_isAnisotropyDesired = configuration.graphic_anisotropy_enable.getValue();
        m_desiredAnisotropy = configuration.graphic_anisotropy_levels.getValue();
        m_desiredMinimizationFilter = configuration.graphic_textureFilter_minFilter.getValue();
        m_desiredMaximizationFilter = configuration.graphic_textureFilter_magFilter.getValue();
        m_desiredMipMapFilter = configuration.graphic_textureFilter_mipMapFilter.getValue();
    }
    catch (...)
    {
        for (auto& connection : m_connections)
        {
            connection.disconnect();
        }
        throw;
    }
}

RendererInfo::~RendererInfo()
{
    for (auto& connection : m_connections)
    {
        connection.disconnect();
    }
}

bool RendererInfo::isAnisotropyDesired() const noexcept
{
    return m_isAnisotropyDesired;
}

float RendererInfo::getDesiredAnisotropy() const noexcept
{
    return m_desiredAnisotropy;
}

idlib::texture_filter_method RendererInfo::getDesiredMinimizationFilter() const noexcept
{
    return m_desiredMinimizationFilter;
}

idlib::texture_filter_method RendererInfo::getDesiredMaximizationFilter() const noexcept
{
    return m_desiredMaximizationFilter;
}

idlib::texture_filter_method RendererInfo::getDesiredMipMapFilter() const noexcept
{
    return m_desiredMipMapFilter;
}

} // namespace Ego
