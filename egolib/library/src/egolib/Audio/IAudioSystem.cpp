#include "egolib/Audio/IAudioSystem.hpp"

#include <stdexcept>

static IAudioSystem* g_activeAudioSystem = nullptr;

void installActiveAudioSystem(IAudioSystem& audioSystem)
{
    if (g_activeAudioSystem)
    {
        throw std::logic_error("audio system already installed");
    }
    g_activeAudioSystem = &audioSystem;
}

void clearActiveAudioSystem()
{
    g_activeAudioSystem = nullptr;
}

IAudioSystem* tryActiveAudioSystem()
{
    return g_activeAudioSystem;
}

IAudioSystem& activeAudioSystem()
{
    if (!g_activeAudioSystem)
    {
        throw std::logic_error("no active audio system");
    }
    return *g_activeAudioSystem;
}
