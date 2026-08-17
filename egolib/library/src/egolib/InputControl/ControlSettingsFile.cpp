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

/// @brief Routines for reading and writing <tt>"controls.txt"</tt>.

#include "egolib/InputControl/ControlSettingsFile.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/Log/_Include.hpp"

#include "egolib/fileutil.h"

#include "idlib/exception.hpp"  // idlib::runtime_error

static std::string controlInputToString(const Ego::Input::InputDevice::InputButton &button);

namespace {

/// @brief Log one failed controls.txt load (unreadable file) for input_settings_load_vfs.
/// @remark Goes through Log::tryActiveTarget() rather than Log::activeTarget(): the latter
///         falls through to Log::get(), which throws std::logic_error when the logging system
///         is not initialized (Log/_Include.cpp). input_settings_load_vfs promises never to
///         throw for a missing/unreadable file, so its own diagnostic must not be able to
///         throw either.
void logControlsLoadFailure(const std::string& filename, std::string reason)
{
    Log::Target* logTarget = Log::tryActiveTarget();
    if (!logTarget) return;

    // Flatten the message to one line so a single failed load yields a single log record.
    // idlib::runtime_error::to_string() is deliberately multi-line (runtime_error.hpp emits
    // "runtime error:", the raise site, and the message on separate lines).
    for (char& c : reason)
    {
        if (c == '\n' || c == '\r') c = ' ';
    }

    *logTarget << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__,
                                     "unable to load input settings from ", "`", filename, "`",
                                     ": ", reason, Log::EndOfEntry);
}

} // namespace

//--------------------------------------------------------------------------------------------

bool input_settings_load_vfs(const std::string& filename)
{
    /// @author ZZ
    /// @details This function reads the controls.txt file, version 3

    // ReadContext's constructor (Scanner(const std::string&), Script/Scanner.hpp) reads the
    // whole file up front and raises idlib::runtime_error if it cannot be opened or read
    // (vfs_readEntireFile, vfs_bulk.c:56) - the common case being a missing/uninstalled
    // controls.txt. Guarded below so that case reports false through the same contract as a
    // truncated file (see below), rather than escaping uncaught into GameEngine::initialize()
    // (GameEngine_lifecycle.cpp), which before this fix also discarded this function's bool -
    // it now checks the bool and logs a boot-level Warning instead. No mappings are applied yet
    // at this point, so a construction failure leaves every device's bindings exactly as the
    // caller had them.
    //
    // Only idlib::runtime_error is caught here, matching the promise in
    // ControlSettingsFile.hpp. vfs_openRead's BAIL_IF_NOT_INIT guard (vfs.c:200,
    // vfs_internal.h) throws a plain std::runtime_error - unrelated to idlib::runtime_error,
    // since idlib::exception has no base class at all (idlib/exception/exception.hpp) - if the
    // VFS is not initialized when this function runs. That path is unreachable through the
    // documented boot flow (GameEngine_lifecycle.cpp initializes the VFS well before calling
    // this function) and is deliberately left uncaught here rather than widened into a
    // catch (...), which would also swallow std::bad_alloc.
    std::unique_ptr<ReadContext> ctxt = nullptr;
    try
    {
        ctxt = std::make_unique<ReadContext>(filename);
    }
    catch (const idlib::runtime_error& ex)
    {
        logControlsLoadFailure(filename, ex.to_string());
        return false;
    }

    // The parse loop below is deliberately NOT inside a try: it is provably throw-proof for any
    // content it can be given, by the same reasoning already documented at LoadingState.cpp's
    // loadGameTips (egolib/game/GameStates/LoadingState.cpp). skipToColon(true) is the optional
    // form - it returns false at end-of-input (the ordinary truncated-file case, handled below
    // without an exception) instead of throwing; its remaining throw path, ReadContext.cpp
    // skipToDelimiter's `ise(ERROR())` branch, and readToEndOfLine's own skipWhiteSpaces()
    // `ise(ERROR())` branches, all compare the current symbol against Traits<char>::error()
    // (Script/Traits.hpp), a Unicode private-use-area code point (0xee8083) outside the range any
    // byte read through this Traits<char> scanner can produce - the transform from `char` to that
    // extended type is a plain widening conversion, never a real UTF-8 decode. readToEndOfLine has
    // no throw of its own once past skipWhiteSpaces(). So idlib::hll::compilation_error cannot
    // reach this function at all today, and is deliberately not caught here.

    // Read input for each player
    for (size_t i = 0; i < Ego::Input::InputDevice::DeviceList.size(); i++)
    {
        std::string currenttag;
        Ego::Input::InputDevice &device = Ego::Input::InputDevice::DeviceList[i];

        //Read each input control button
        for (size_t icontrol = 0; icontrol < static_cast<size_t>(Ego::Input::InputDevice::InputButton::COUNT); ++icontrol) {
            if(!ctxt->skipToColon(true)) {
                return false;
            }

            currenttag = ctxt->readToEndOfLine();
            if (!currenttag.empty())
            {
                const SDL_Keycode keyCode = SDL_GetKeyFromName(currenttag.c_str());
                device.setInputMapping(static_cast<Ego::Input::InputDevice::InputButton>(icontrol), keyCode);
            }
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------------
bool input_settings_save_vfs( const std::string& filename )
{
    /// @author ZF
    /// @details This function saves all current game settings to "controls.txt"
    vfs_FILE* filewrite = vfs_openWrite( filename );
    if ( NULL == filewrite )
    {
        Log::activeTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "unable to save input settings to file ", "`", filename, "`", Log::EndOfEntry);
        return false;
    }

    //Add version number
    vfs_put_version(filewrite, 4);

    // Just some information
    vfs_puts( "Controls\n", filewrite );
    vfs_puts( "========\n", filewrite );
    vfs_puts( "This file lets users modify the handling of input devices.\n", filewrite );
    vfs_puts( "See the game manual for a list of settings and more info.\n", filewrite );
    vfs_puts( "Note that you can mix KEY_ type settings with other \n", filewrite );
    vfs_puts( "devices... Write the input after the colons!\n\n", filewrite );

    vfs_puts( "General Controls\n", filewrite );
    vfs_puts( "========\n", filewrite );
    vfs_puts( "These are general control codes and cannot be changed\n", filewrite );
    vfs_puts( "ESC                       - Open ingame menu\n", filewrite );
    vfs_puts( "SPACE                     - Respawn character (if dead and possible)\n", filewrite );
    vfs_puts( "1 to 7                    - Show character detailed stats\n", filewrite );
    vfs_puts( "F11                       - Take screenshot\n", filewrite );
    vfs_puts( "\n", filewrite );

    // The actual settings
    for (size_t i = 0; i < Ego::Input::InputDevice::DeviceList.size(); i++)
    {
        const Ego::Input::InputDevice &device = Ego::Input::InputDevice::DeviceList[i];
        const std::string player = "\nPLAYER " + std::to_string(i + 1) + "\n";

        //which player
        vfs_puts(player.c_str(), filewrite);
        vfs_puts("========\n", filewrite);

        //controller type
        //const std::string controller = "CONTROLLER:         " + translate_input_type_to_string(pdevice.device_type);
        //vfs_puts(controller.c_str(), filewrite);

        for (size_t icontrol = 0; icontrol < static_cast<size_t>(Ego::Input::InputDevice::InputButton::COUNT); ++icontrol) {
            Ego::Input::InputDevice::InputButton button = static_cast<Ego::Input::InputDevice::InputButton>(icontrol);

            std::stringstream output;
            output << controlInputToString(button) << "\t\t" << ": " << device.getMappedInputName(button) << '\n';
            vfs_puts(output.str().c_str(), filewrite);
        }

    }

    // All done
    vfs_close(filewrite);

    return true;
}

std::string controlInputToString(const Ego::Input::InputDevice::InputButton &button)
{
    switch(button)
    {
        case Ego::Input::InputDevice::InputButton::MOVE_UP:           return "Move Up";
        case Ego::Input::InputDevice::InputButton::MOVE_RIGHT:        return "Move Right";
        case Ego::Input::InputDevice::InputButton::MOVE_DOWN:         return "Move Down";
        case Ego::Input::InputDevice::InputButton::MOVE_LEFT:         return "Move Left";
        case Ego::Input::InputDevice::InputButton::JUMP:              return "Jump";
        case Ego::Input::InputDevice::InputButton::USE_LEFT:          return "Use Left";
        case Ego::Input::InputDevice::InputButton::GRAB_LEFT:         return "Grab Left";
        case Ego::Input::InputDevice::InputButton::USE_RIGHT:         return "Use Right";
        case Ego::Input::InputDevice::InputButton::GRAB_RIGHT:        return "Grab Right";
        case Ego::Input::InputDevice::InputButton::INVENTORY:         return "Inventory";
        case Ego::Input::InputDevice::InputButton::STEALTH:           return "Stealth";
        case Ego::Input::InputDevice::InputButton::CAMERA_LEFT:       return "CAMERA_LEFT";
        case Ego::Input::InputDevice::InputButton::CAMERA_RIGHT:      return "CAMERA_RIGHT";
        case Ego::Input::InputDevice::InputButton::CAMERA_ZOOM_IN:    return "CAMERA_ZOOM_IN";
        case Ego::Input::InputDevice::InputButton::CAMERA_ZOOM_OUT:   return "CAMERA_ZOOM_OUT";
        case Ego::Input::InputDevice::InputButton::CAMERA_CONTROL:    return "Camera Control";
        case Ego::Input::InputDevice::InputButton::COUNT:             return "UNKNOWN";
    }

    throw idlib::unhandled_switch_case_error(__FILE__, __LINE__, "unreachable code reached");
}
