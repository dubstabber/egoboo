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

#include "egolib/game/Graphics/Camera.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Logic/Player.hpp"
#include "egolib/InputControl/InputDevice.hpp"
#include "egolib/InputControl/IInputSystem.hpp"
#include "egolib/Graphics/GraphicsWindow.hpp" // Ego::GraphicsWindow
#include "egolib/game/Graphics/TileList.hpp"
#include "egolib/game/Graphics/EntityList.hpp"
#include "egolib/Graphics/Viewport.hpp"
#include "egolib/game/Core/ISessionState.hpp"

#include "egolib/game/game.h" // TODO: remove only needed for mesh

#include "egolib/game/mesh.h"
#include "egolib/Entities/_Include.hpp"

const Ego::Degrees Camera::DEFAULT_FOV = Ego::Degrees(60.0f);

const float Camera::DEFAULT_TURN_JOY = 64;

const float Camera::DEFAULT_TURN_KEY = DEFAULT_TURN_JOY;

const uint8_t Camera::DEFAULT_TURN_TIME = 16;

const float Camera::CAM_ZADD_AVG = (0.5f * (CAM_ZADD_MIN + CAM_ZADD_MAX));
const float Camera::CAM_ZOOM_AVG = (0.5f * (CAM_ZOOM_MIN + CAM_ZOOM_MAX));

Camera::Camera(const CameraOptions &options) :
    _options(options),

    _frustumInvalid(true),
    _frustum(),
    _moveMode(CameraMovementMode::Player),

    _turnMode(_options.turnMode),
    _turnTime(DEFAULT_TURN_TIME),

    _ori(),

    _trackPos(),

    _zoom(CAM_ZOOM_AVG),
    _center{0.0f, 0.0f, 0.0f},
    _zadd(CAM_ZADD_AVG),
    _zaddGoto(CAM_ZADD_AVG),
    _zGoto(CAM_ZADD_AVG),
    _pitch(idlib::pi_over<float, 2>()),

    _turnZ_radians(-idlib::pi_over<float,4>()),
    _turnZ_turns(idlib::semantic_cast<Ego::Turns>(_turnZ_radians)),
    _turnZAdd(0.0f),

    m_viewMatrix(),
    m_projectionMatrix(),
    m_position(),
    m_forward{0.0f, 0.0f, 0.0f},
    m_up{0.0f, 0.0f, 0.0f},
    m_right{0.0f, 0.0f, 0.0f},
    m_viewport(std::make_unique<Ego::Graphics::Viewport>()),

    // Special effects.
    _motionBlur(0.0f),
    _motionBlurOld(0.0f),
    _swing(_options.swing),
    _swingRate(_options.swingRate),
    _swingAmp(_options.swingAmp),
    _roll(0.0f),

    // Extended camera data.
    _trackList(),
    _lastFrame(-1),
    _tileList(std::make_shared<Ego::Graphics::TileList>()),
    _entityList(std::make_shared<Ego::Graphics::EntityList>())
{
    // Derived values.
    _trackPos = _center;
    m_position = _center + Ego::Vector3f(_zoom * std::sin(_turnZ_radians), _zoom * std::cos(_turnZ_radians), CAM_ZADD_MAX);

    _turnZ_turns = idlib::semantic_cast<Ego::Turns>(_turnZ_radians);
    _ori.facing_z = TurnToFacing(_turnZ_turns);
    resetView();

    // Assume that the camera is fullscreen.
    setScreen(0, 0, EngineContext::get().graphicsSystem().getWindow()->size().x(), EngineContext::get().graphicsSystem().getWindow()->size().y());
}

Camera::~Camera()
{
    // Free any locked tile list.
    _tileList = nullptr;

    // Free any locked entity list.
    _entityList = nullptr;
}

float Camera::multiplyFOV(const float old_fov_deg, const float factor)
{
    float old_fov_rad = Ego::Math::degToRad(old_fov_deg);

    float new_fov_rad = 2.0f * std::atan(factor * std::tan(old_fov_rad * 0.5f));
    float new_fov_deg = Ego::Math::radToDeg(new_fov_rad);

    return new_fov_deg;
}

void Camera::updateProjection(const Ego::Degrees& fov, const float aspect_ratio, const float frustum_near, const float frustum_far)
{
    m_projectionMatrix = idlib::perspective_projection_matrix(fov, aspect_ratio, frustum_near, frustum_far);

    // Invalidate the frustum.
    _frustumInvalid = true;
}

void Camera::resetView()
{
    // Check for stupidity.
    if (m_position != _center)
    {
        static const Ego::Vector3f Z = Ego::Vector3f(0.0f, 0.0f, 1.0f);
        Ego::Matrix4f4f tmp = idlib::scaling_matrix(Ego::Vector3f(-1.0f, 1.0f, 1.0f)) *  idlib::rotation_matrix(Z, Ego::Degrees(Ego::Math::radToDeg(_roll)));
        m_viewMatrix = tmp * idlib::look_at_matrix(m_position, _center, Z);
    }
    // Invalidate the frustum.
    _frustumInvalid = true;
}

void Camera::makeMatrix()
{
    resetView();

    // Pre-compute some camera vectors.
    m_forward = mat_getCamForward(m_viewMatrix);
    m_forward = Ego::normalize(m_forward).get_vector();

    m_up = mat_getCamUp(m_viewMatrix);
    m_up = Ego::normalize(m_up).get_vector();

    m_right = mat_getCamRight(m_viewMatrix);
    m_right = Ego::normalize(m_right).get_vector();
}

void Camera::readInput(const Ego::Input::InputDevice &device)
{

    auto& inputSystem = EngineContext::get().inputSystem();
    // Autoturn camera only works in single player and when it is enabled.
    bool autoturn_camera = (CameraTurnMode::Good == _turnMode) && (1 == activeSessionState().localPlayerCount());

    switch(device.getDeviceType())
    {
        // Mouse control
        case Ego::Input::InputDevice::InputDeviceType::MOUSE:
        {
            // Autoturn camera.
            if (autoturn_camera)
            {
                if (!device.isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_CONTROL))
                {
                _turnZAdd -= inputSystem.getMouseMovement().x() * 0.5f;
                }
            }
            // Normal camera.
            else if (device.isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_CONTROL))
            {
            _turnZAdd += inputSystem.getMouseMovement().x() / 3.0f;
            _zaddGoto += static_cast<float>(inputSystem.getMouseMovement().y()) / 3.0f;

                _turnTime = DEFAULT_TURN_TIME;  // Sticky turn ...
            }            
        }
        break;

        //TODO: Not implemented
        /*
        case INPUT_DEVICE_JOYSTICK:
        {
            // Joystick camera controls:

            int ijoy = type - INPUT_DEVICE_JOY;

            // Figure out which joystick this is.
        joystick_data_t *pjoy = inputSystem.joysticks[ijoy].get();

            // Autoturn camera.
            if (autoturn_camera)
            {
                if (!device.isButtonPressed(Ego::Input::InputDevice::InputButton::CONTROL_CAMERA))
                {
                    _turnZAdd -= pjoy->x * DEFAULT_TURN_JOY;
                }
            }
            // Normal camera.
        else if (input_device_t::control_active( pdevice, CONTROL_CAMERA))
            {
                _turnZAdd += pjoy->x * DEFAULT_TURN_JOY;
                _zaddGoto += pjoy->y * DEFAULT_TURN_JOY;

                _turnTime = DEFAULT_TURN_TIME;  // Sticky turn ...
            }
        }
        break;
        */

        // INPUT_DEVICE_KEYBOARD and any unknown device end up here
        case Ego::Input::InputDevice::InputDeviceType::KEYBOARD:
        default:
        {
            // Autoturn camera.
            if (autoturn_camera)
            {
                if (device.isButtonPressed( Ego::Input::InputDevice::InputButton::MOVE_LEFT))
                {
                    _turnZAdd += DEFAULT_TURN_KEY;
                }
                if (device.isButtonPressed( Ego::Input::InputDevice::InputButton::MOVE_RIGHT))
                {
                    _turnZAdd -= DEFAULT_TURN_KEY;
                }
            }
            // Normal camera.
            else
            {
                int _turn_z_diff = 0;

                // Rotation.
                if (device.isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_LEFT))
                {
                    _turn_z_diff += DEFAULT_TURN_KEY;
                }
                if (device.isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_RIGHT))
                {
                    _turn_z_diff -= DEFAULT_TURN_KEY;
                }

                // Sticky turn?
                if (0 != _turn_z_diff)
                {
                    _turnZAdd += _turn_z_diff;
                    _turnTime   = DEFAULT_TURN_TIME;
                }

                // Zoom.
                if (device.isButtonPressed(Ego::Input::InputDevice::InputButton::CAMERA_ZOOM_OUT))
                {
                    _zaddGoto += DEFAULT_TURN_KEY;
                }
                if (device.isButtonPressed( Ego::Input::InputDevice::InputButton::CAMERA_ZOOM_IN))
                {
                    _zaddGoto -= DEFAULT_TURN_KEY;
                }
            }
        }        
        break;
    }
}

void Camera::reset(const ego_mesh_t *mesh)
{
    // Defaults.
    _zoom = CAM_ZOOM_AVG;
    _zadd = CAM_ZADD_AVG;
    _zaddGoto = CAM_ZADD_AVG;
    _zGoto = CAM_ZADD_AVG;
    _turnZ_radians = Ego::Radians(-idlib::pi_over<float, 4>());
    _turnZAdd = 0.0f;
    _roll = 0.0f;

    // Derived values.
    _center.x()     = mesh->_tmem._edge_x * 0.5f;
    _center.y()     = mesh->_tmem._edge_y * 0.5f;
    _center.z()     = 0.0f;

    _trackPos = _center;
    m_position = _center;

    m_position.x() += _zoom * std::sin(_turnZ_radians);
    m_position.y() += _zoom * std::cos(_turnZ_radians);
    m_position.z() += CAM_ZADD_MAX;

    _turnZ_turns = idlib::semantic_cast<Ego::Turns>(_turnZ_radians);
    _ori.facing_z = TurnToFacing(_turnZ_turns);

    // Get optional parameters.
    _swing = _options.swing;
    _swingRate = _options.swingRate;
    _swingAmp = _options.swingAmp;
    _turnMode = _options.turnMode;

    // Make sure you are looking at the players.
    resetTarget(mesh);
}

void Camera::resetTarget(const ego_mesh_t *mesh)
{
    // Save some values.
    CameraTurnMode turnModeSave = _turnMode;
    CameraMovementMode moveModeSave = _moveMode;

    // Get back to the default view matrix.
    resetView();

    // Specify the modes that will make the camera point at the players.
    _turnMode = CameraTurnMode::Auto;
    _moveMode = CameraMovementMode::Reset;

    // If you use Camera::MoveMode::Reset,
    // Camera::update() automatically restores _moveMode to its default setting.
    update(mesh);

    // Fix the center position.
    _center.x() = _trackPos.x();
    _center.y() = _trackPos.y();

    // Restore the modes.
    _turnMode = turnModeSave;
    _moveMode = moveModeSave;

    // reset the turn time
    _turnTime = 0;
}

void Camera::setScreen( float xmin, float ymin, float xmax, float ymax )
{
    // Set the screen rectangle.
    m_viewport->setLeftPixels(xmin);
    m_viewport->setTopPixels(ymin);
    m_viewport->setWidthPixels(xmax - xmin);
    m_viewport->setHeightPixels(ymax - ymin);

    // Update projection after setting size.
    float aspect_ratio = m_viewport->getWidthPixels()
                       / m_viewport->getHeightPixels();
    // The nearest we will have to worry about is 1/2 of a tile.
    float frustum_near = Info<int>::Grid::Size() * 0.25f;
    // Set the maximum depth to be the "largest possible size" of a mesh.
    float frustum_far  = Info<int>::Grid::Size() * 256 * idlib::sqrt_two<float>();
    updateProjection(DEFAULT_FOV, aspect_ratio, frustum_near, frustum_far);
}

void Camera::setPosition(const Ego::Vector3f &position)
{
    _center = position;
}
