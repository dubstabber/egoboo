#include "egolib/game/GUI/ScreenMessage.hpp"

namespace Ego {
namespace GUI {

namespace {
std::function<void(const std::string&)> g_screenMessageSink;
}

void postScreenMessage(const std::string& message)
{
    if (g_screenMessageSink)
    {
        g_screenMessageSink(message);
    }
}

void installScreenMessageSink(std::function<void(const std::string&)> sink)
{
    g_screenMessageSink = std::move(sink);
}

void clearScreenMessageSink()
{
    g_screenMessageSink = nullptr;
}

} // namespace GUI
} // namespace Ego
