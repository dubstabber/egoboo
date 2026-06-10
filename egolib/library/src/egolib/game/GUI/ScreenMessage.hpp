#pragma once

#include <functional>
#include <string>

namespace Ego {
namespace GUI {

/// @brief Post a transient on-screen status message through the installed sink.
/// @param message the message text
/// @remark No-op if no sink is installed. This is the GUI-layer seam that lets lower-layer GUI
///         code (e.g. UIManager screenshot status) reach the game's on-screen message log without
///         a direct upward dependency on the game layer's DisplayMsg system. The game installs the
///         sink (wiring it to DisplayMsg_print) at engine startup.
void postScreenMessage(const std::string& message);

/// @brief Install the sink that renders on-screen status messages.
/// @param sink the callable invoked by postScreenMessage()
void installScreenMessageSink(std::function<void(const std::string&)> sink);

/// @brief Clear the installed on-screen status-message sink.
void clearScreenMessageSink();

} // namespace GUI
} // namespace Ego
