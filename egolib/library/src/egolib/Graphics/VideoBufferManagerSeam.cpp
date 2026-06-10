#include "egolib/Graphics/VideoBufferManagerSeam.hpp"

#include <stdexcept>

namespace Ego {
namespace {
idlib::video_buffer_manager* g_activeVideoBufferManager = nullptr;
}

void installActiveVideoBufferManager(idlib::video_buffer_manager& mgr)
{
    if (g_activeVideoBufferManager)
    {
        throw std::logic_error("video buffer manager already installed");
    }
    g_activeVideoBufferManager = &mgr;
}

void clearActiveVideoBufferManager()
{
    g_activeVideoBufferManager = nullptr;
}

idlib::video_buffer_manager* tryActiveVideoBufferManager()
{
    return g_activeVideoBufferManager;
}

idlib::video_buffer_manager& activeVideoBufferManager()
{
    if (!g_activeVideoBufferManager)
    {
        throw std::logic_error("no active video buffer manager");
    }
    return *g_activeVideoBufferManager;
}

} // namespace Ego
