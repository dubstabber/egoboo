//********************************************************************************************
//*
//*    This file is part of Egoboo.
//*
//*    Egoboo is free software: you can redistribute it and/or modify it
//*    under the terms of the GNU General Public License as published by
//*    the Free Software Foundation, either version 3 of the License, or
//*    (at your option) any later version.
//*
//*    Egoboo is distributed in the hope that it will be useful, but
//*    WITHOUT ANY WARRANTY; without even the implied warranty of
//*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//*    General Public License for more details.
//*
//*    You should have received a copy of the GNU General Public License
//*    along with Egoboo.  If not, see <http://www.gnu.org/licenses/>.
//*
//********************************************************************************************

#include "egolib/Core/System.hpp"
#include "egolib/egoboo_setup.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace Ego {
namespace Core {

System *SystemCreateFunctor::operator()(const std::string& x) const
{
	return new System(x);
}

System *SystemCreateFunctor::operator()(const std::string& x, const std::string& y) const
{
	return new System(x, y);
}

const std::string SystemService::VERSION = "0.1.9";

SystemService::SystemService(const std::string& binaryPath) {
    // Initialize virtual file system.
    vfs_init(binaryPath.c_str(), nullptr);
    // Add search paths.
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */
    setup_init_base_vfs_paths();
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */

    // Initialize logging.
    Log::initialize("/debug/log.txt", Log::Level::Debug);
    EngineContext::get().installLogTarget(Log::get());
    EngineContext::get().installConfig(egoboo_config_t::get());

    // Say hello.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "starting Egoboo Engine ", VERSION, Log::EndOfEntry);

    // Load "setup.txt" and download "setup.txt" into the Egoboo configuration.
    Setup::begin();
    Setup::download(EngineContext::get().config());

    // Initialize SDL timer.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "initialize SDL timer ",
                                              SDL_MAJOR_VERSION, ".", SDL_MINOR_VERSION, ".", SDL_PATCHLEVEL, Log::EndOfEntry);
    SDL_Init(SDL_INIT_TIMER);
    // Initialize SDL events.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "initialize SDL events ",
                                              SDL_MAJOR_VERSION, ".", SDL_MINOR_VERSION, ".", SDL_PATCHLEVEL, Log::EndOfEntry);
    SDL_Init(SDL_INIT_EVENTS);
}

SystemService::SystemService(const std::string& binaryPath, const std::string& egobooPath) {
    // Initialize virtual file system.
    vfs_init(binaryPath.c_str(), egobooPath.c_str());
    // Add search paths.
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */
    setup_init_base_vfs_paths();
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */
    
    // Initialize logging.
    Log::initialize("/debug/log.txt", Log::Level::Debug);
    EngineContext::get().installLogTarget(Log::get());
    EngineContext::get().installConfig(egoboo_config_t::get());
    
    // Say hello.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "starting Egoboo engine ", VERSION, Log::EndOfEntry);

    // Load "setup.txt".
    Setup::begin();

    // Load "setup.txt" and download "setup.txt" into the Egoboo configuration.
    Setup::download(EngineContext::get().config());

    // Initialize SDL timer.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "initialize SDL timer ",
                                              SDL_MAJOR_VERSION, ".", SDL_MINOR_VERSION, ".", SDL_PATCHLEVEL, Log::EndOfEntry);
    SDL_Init(SDL_INIT_TIMER);
    // Initialize SDL events.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "initialize SDL events ",
                                              SDL_MAJOR_VERSION, ".", SDL_MINOR_VERSION, ".", SDL_PATCHLEVEL, Log::EndOfEntry);
    SDL_Init(SDL_INIT_EVENTS);
}

SystemService::~SystemService() {
    // Uninitialize SDL.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "uninitializing SDL ",
                                              SDL_MAJOR_VERSION, ".", SDL_MINOR_VERSION, ".", SDL_PATCHLEVEL, Log::EndOfEntry);
    SDL_Quit();
    // Save "setup.txt".
    Setup::end();
    EngineContext::get().clearConfig();
    // Say bye.
    Log::activeTarget() << Log::Entry::create(Log::Level::Message, __FILE__, __LINE__, "exiting Egoboo engine ", VERSION, Log::EndOfEntry);
    // Uninitialize logging.
    EngineContext::get().clearLogTarget();
    Log::uninitialize();
    // Remove search paths.
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */
    setup_clear_base_vfs_paths();
    /*
    // Uncomment to display the search paths.
    vfs_listSearchPaths();
    */
    // Uninitialize virtual file system.
#if 0
    vfs_uninit();
#endif
}

uint32_t SystemService::getTicks() {
    return SDL_GetTicks();
}

VideoService::VideoService()
{
    Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "intializing SDL video", Log::EndOfEntry);
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to initialize SDL video: ", SDL_GetError(), Log::EndOfEntry);
        Log::activeTarget() << e;
        throw idlib::environment_error(__FILE__, __LINE__, "[SDL]", e.getText());
    }
}

VideoService::~VideoService()
{
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

AudioService::AudioService()
{
    Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "intializing SDL audio", Log::EndOfEntry);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
    {
        auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to initialize SDL audio: ", SDL_GetError(), Log::EndOfEntry);
        Log::activeTarget() << e;
        throw idlib::environment_error(__FILE__, __LINE__, "[SDL]", e.getText());
    }
}

AudioService::~AudioService() {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

InputService::InputService()
{
    Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "intializing SDL joystick/game controller/haptic support", Log::EndOfEntry);

    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
    if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to initialize joystick support", Log::EndOfEntry);
    }
    else
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "joytick support initialized", Log::EndOfEntry);
    }
    //
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to initialize game controller support", Log::EndOfEntry);
    }
    else
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "game controller support initialized", Log::EndOfEntry);
    }
    //
    if (SDL_WasInit(SDL_INIT_HAPTIC) == 0)
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "unable to initialize haptic support", Log::EndOfEntry);
    }
    else
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Info, __FILE__, __LINE__, "haptic support initialized", Log::EndOfEntry);
    }
}

InputService::~InputService()
{
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
}

System::System(const std::string& binaryPath) {
    try {
        systemService = new SystemService(binaryPath);
    } catch (...) {
        std::rethrow_exception(std::current_exception());
    }
    try {
        videoService = new VideoService();
    } catch (...) {
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
    try {
        audioService = new AudioService();
    } catch (...) {
        if (videoService) {
            delete videoService;
            videoService = nullptr;
        }
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
    try {
        inputService = new InputService();
    } catch (...) {
        if (audioService) {
            delete audioService;
            audioService = nullptr;
        }
        if (videoService) {
            delete videoService;
            videoService = nullptr;
        }
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
}

System::System(const std::string& binaryPath, const std::string& egobooPath) {
    try {
        systemService = new SystemService(binaryPath);
    } catch (...) {
        std::rethrow_exception(std::current_exception());
    }
    try {
        videoService = new VideoService();
    } catch (...) {
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
    try {
        audioService = new AudioService();
    } catch (...) {
        if (videoService) {
            delete videoService;
            videoService = nullptr;
        }
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
    try {
        inputService = new InputService();
    } catch (...) {
        if (audioService) {
            delete audioService;
            audioService = nullptr;
        }
        if (videoService) {
            delete videoService;
            videoService = nullptr;
        }
        if (systemService) {
            delete systemService;
            systemService = nullptr;
        }
        std::rethrow_exception(std::current_exception());
    }
}

System::~System() {
    if (inputService) {
        delete inputService;
        inputService = nullptr;
    }
    if (audioService) {
        delete audioService;
        audioService = nullptr;
    }
    if (videoService) {
        delete videoService;
        videoService = nullptr;
    }
    if (systemService) {
        delete systemService;
        systemService = nullptr;
    }
}

} // namespace Core
} // namespace Ego
