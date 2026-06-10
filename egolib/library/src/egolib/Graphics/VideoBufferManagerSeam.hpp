#pragma once

/// @file egolib/Graphics/VideoBufferManagerSeam.hpp
/// @brief Lower-layer active-instance accessor seam for idlib::video_buffer_manager.
///
///        Ownership of the installed active instance lives in VideoBufferManagerSeam.cpp;
///        EngineContext's videoBufferManager methods are thin delegators over these
///        accessors.  The seam lives at the egolib/Graphics layer so foundation-base
///        callers (Font.cpp) can reach the active buffer manager without depending on
///        game/Core/EngineContext.

namespace idlib { class video_buffer_manager; }

namespace Ego {

void installActiveVideoBufferManager(idlib::video_buffer_manager& mgr);

void clearActiveVideoBufferManager();

idlib::video_buffer_manager* tryActiveVideoBufferManager();

idlib::video_buffer_manager& activeVideoBufferManager();

} // namespace Ego
