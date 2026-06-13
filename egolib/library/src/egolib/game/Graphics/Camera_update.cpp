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
#include "egolib/game/Core/GameSessionContext.hpp"

#include "egolib/game/game.h" // TODO: remove only needed for mesh

#include "egolib/game/mesh.h"
#include "egolib/Entities/_Include.hpp"

namespace
{
Object* tryLiveTrackedObject(ObjectRef objectRef)
{
    Object* object = GameSessionContext::get().tryObject(objectRef);
    return object != nullptr && !object->isTerminated() && object->isAlive() ? object : nullptr;
}
}

void Camera::updatePosition()
{
    // Update the height.
    _zGoto = _zadd;

    // Update the turn.
#if 0
    if ( 0 != _turnTime )
    {
        _turnZRad = Ego::Math::Radians(std::atan2(_center[kY] - pos[kY], _center[kX] - pos[kX]));  // xgg
        _turnZOne = Ego::Math::Turns(_turnZRad);
        _ori.facing_z = TurnsToFacing(_turnZ_turns);
    }
#endif

    // Update the camera position.
    Ego::Vector3f pos_new = _center + Ego::Vector3f(_zoom * std::sin(_ori.facing_z), _zoom * std::cos(_ori.facing_z), _zGoto);

    if(idlib::euclidean_norm(m_position-pos_new) < Info<float>::Grid::Size()*8.0f) {
        // Make the camera motion smooth using a low-pass filter
        m_position = m_position * 0.9f + pos_new * 0.1f; /// @todo Use Ego::Math::lerp.
    }
    else {
        //Teleport camera if error becomes too large
        m_position = pos_new;
        _center = _trackPos;
    }
}

void Camera::updateZoom()
{
    // Update zadd.
    _zaddGoto = Ego::Math::constrain(_zaddGoto, CAM_ZADD_MIN, CAM_ZADD_MAX);
    _zadd = 0.9f * _zadd  + 0.1f * _zaddGoto; /// @todo Use Ego::Math::lerp.

    // Update zoom.
    float percentmax = (_zaddGoto - CAM_ZADD_MIN) / static_cast<float>(CAM_ZADD_MAX - CAM_ZADD_MIN);
    float percentmin = 1.0f - percentmax;
    _zoom = (CAM_ZOOM_MIN * percentmin) + (CAM_ZOOM_MAX * percentmax);

    // update _turn_z
    if (std::abs( _turnZAdd ) < 0.5f)
    {
        _turnZAdd = 0.0f;
    }
    else
    {
        //Make it wrap around
        int32_t newAngle = static_cast<int32_t>(static_cast<float>(FACING_T(_ori.facing_z)) +_turnZAdd);
        _ori.facing_z = idlib::canonicalize(Facing(newAngle));

        _turnZ_turns = FacingToTurn(Facing(_ori.facing_z));
        _turnZ_radians = idlib::semantic_cast<Ego::Radians>(_turnZ_turns);
    }
    _turnZAdd *= TURN_Z_SUSTAIN;
}

void Camera::updateCenter()
{
    // Center on target for doing rotation ...
    if (0 != _turnTime)
    {
        _center.x() = _center.x() * 0.9f + _trackPos.x() * 0.1f;
        _center.y() = _center.y() * 0.9f + _trackPos.y() * 0.1f;
    }
    else
    {
        // Determine tracking direction.
        Ego::Vector3f trackError = _trackPos - m_position;

        // Determine the size of the dead zone.
        Ego::Degrees track_fov = DEFAULT_FOV * 0.25f;
        float track_dist = idlib::euclidean_norm(trackError);
        float track_size = track_dist * std::tan(track_fov);
        float track_size_x = track_size;
        float track_size_y = track_size;  /// @todo adjust this based on the camera viewing angle

        // Calculate the difference between the center of the tracked characters
        // and the center of the camera look to look at.
        Ego::Vector2f diff = Ego::Vector2f(_trackPos.x(), _trackPos.y()) - Ego::Vector2f(_center.x(), _center.y());

        // Get 2d versions of the camera's right and up vectors.
        Ego::Vector2f vrt(m_right.x(), m_right.y());
        vrt = Ego::normalize(vrt).get_vector();

        Ego::Vector2f vup(m_up.x(), m_up.y());
        vup = Ego::normalize(vup).get_vector();

        // project the diff vector into this space
        float diff_rt = Ego::dot(vrt, diff);
        float diff_up = Ego::dot(vup, diff);

        // Get ready to scroll ...
        Ego::Vector2f scroll;
        if (diff_rt < -track_size_x)
        {
            // Scroll left
            scroll += vrt * (diff_rt + track_size_x);
        }
        if (diff_rt > track_size_x)
        {
            // Scroll right.
            scroll += vrt * (diff_rt - track_size_x);
        }

        if (diff_up > track_size_y)
        {
            // Scroll down.
            scroll += vup * (diff_up - track_size_y);
        }

        if (diff_up < -track_size_y)
        {
            // Scroll up.
            scroll += vup * (diff_up + track_size_y);
        }

        // Scroll.
        _center.x() += scroll.x();
        _center.y() += scroll.y();
    }

    // _center.z always approaches _trackPos.z
    _center.z() = _center.z() * 0.9f + _trackPos.z() * 0.1f; /// @todo Use Ego::Math::lerp
}

void Camera::updateFreeControl()
{
    auto& inputSystem = EngineContext::get().inputSystem();
    float moveSpeed = 25.0f;
    if(inputSystem.isKeyDown(SDLK_LSHIFT) || inputSystem.isKeyDown(SDLK_RSHIFT)) {
        moveSpeed += 25.0f;
    }

    //Forward and backwards
    if (inputSystem.isKeyDown(SDLK_KP_2) || inputSystem.isKeyDown(SDLK_DOWN)) {
        _center.x() += std::sin(_turnZ_radians) * moveSpeed;
        _center.y() += std::cos(_turnZ_radians) * moveSpeed;
    }
    else if (inputSystem.isKeyDown(SDLK_KP_8) || inputSystem.isKeyDown(SDLK_UP)) {
        _center.x() -= std::sin(_turnZ_radians) * moveSpeed;
        _center.y() -= std::cos(_turnZ_radians) * moveSpeed;
    }
    
    //Left and right
    if (inputSystem.isKeyDown(SDLK_KP_4) || inputSystem.isKeyDown(SDLK_LEFT)) {
        _center.x() -= std::sin(_turnZ_radians + Ego::Radians(idlib::pi<float>() * 0.5f)) * moveSpeed;
        _center.y() -= std::cos(_turnZ_radians + Ego::Radians(idlib::pi<float>() * 0.5f)) * moveSpeed;
    }
    else if (inputSystem.isKeyDown(SDLK_KP_6) || inputSystem.isKeyDown(SDLK_RIGHT)) {
        _center.x() += std::sin(_turnZ_radians + Ego::Radians(idlib::pi<float>() * 0.5f)) * moveSpeed;
        _center.y() += std::cos(_turnZ_radians + Ego::Radians(idlib::pi<float>() * 0.5f)) * moveSpeed;
    }
    
    //Rotate left or right
    if (inputSystem.isKeyDown(SDLK_KP_7)) {
        _turnZAdd += DEFAULT_TURN_KEY * 2.0f;
    }
    else if (inputSystem.isKeyDown(SDLK_KP_9)) {
        _turnZAdd -= DEFAULT_TURN_KEY * 2.0f;
    }

    //Up and down
    if (inputSystem.isKeyDown(SDLK_KP_PLUS) || inputSystem.isKeyDown(SDLK_SPACE)) {
        _center.z() -= moveSpeed * 0.2f;
    }
    else if (inputSystem.isKeyDown(SDLK_KP_MINUS) || inputSystem.isKeyDown(SDLK_LCTRL)) {
        _center.z() += moveSpeed * 0.2f;
    }

    //Pitch camera
    if(inputSystem.isKeyDown(SDLK_PAGEDOWN)) {
        _pitch += Ego::Math::degToRad(7.5f);
    }
    else if(inputSystem.isKeyDown(SDLK_PAGEUP)) {
        _pitch -= Ego::Math::degToRad(7.5f);
    }

    //Constrain between 0 and 90 degrees pitch (and a little extra to avoid singularities)
    _pitch = Ego::Math::constrain(_pitch, 0.05f, idlib::pi<float>() - 0.05f);

    //Prevent the camera target from being below the mesh
    auto& session = GameSessionContext::get();
    auto mesh = session.mesh();
    _center.z() = std::max(_center.z(), mesh->getElevation({ _center.x(), _center.y() }));

    //Calculate camera position from desired zoom and rotation
    m_position.x() = _center.x() + _zoom * std::sin(_turnZ_radians);
    m_position.y() = _center.y() + _zoom * std::cos(_turnZ_radians);
    m_position.z() = _center.z() + _zoom * _pitch;

    //Prevent the camera from being below the mesh
    m_position.z() = std::max(m_position.z(), 180.0f + mesh->getElevation(Ego::Vector2f(m_position.x(), m_position.y())));

    updateZoom();
    makeMatrix();

    _trackPos = _center;
}

void Camera::updateTrack()
{
    // The default camera motion is to do nothing.
    Ego::Vector3f new_track = _trackPos;

    switch(_moveMode)
    {

    // The camera is (re-)focuses in on a one or more objects.
    case CameraMovementMode::Reset:
        {
            float sum_wt    = 0.0f;
            Ego::Vector3f sum_pos = idlib::zero<Ego::Vector3f>();

            for(ObjectRef objectRef : _trackList)
            {
                Object* object = tryLiveTrackedObject(objectRef);
                if (!object) continue;

                sum_pos += object->getPosition() + Ego::Vector3f(0.0f, 0.0f, object->getMinCollisionVolume()._maxs[OCT_Z] * 0.9f);
                sum_wt += 1.0f;
            }

            // If any of the characters is doing anything.
            if (sum_wt > 0.0f)
            {
                new_track = sum_pos * (1.0f / sum_wt);
            }
        }

        // Reset the camera mode.
        _moveMode = CameraMovementMode::Player;
    break;

    // The camera is (re-)focuses in on a one or more player objects.
    // "Show me the drama!"
    case CameraMovementMode::Player:
        {
            Object* soleTrackedPlayer = nullptr;
            size_t trackedPlayerCount = 0;
            float sum_wt = 0.0f;
            Ego::Vector3f sum_pos = idlib::zero<Ego::Vector3f>();

            for(ObjectRef objectRef : _trackList)
            {
                Object* object = tryLiveTrackedObject(objectRef);
                if (!object) continue;

                ++trackedPlayerCount;
                soleTrackedPlayer = trackedPlayerCount == 1 ? object : nullptr;

                // Weight it by the character's velocity^2, so that
                // inactive characters don't control the camera.
                float weight1 = Ego::dot(object->getVelocity(), object->getVelocity());

                // Make another weight based on button-pushing.
                float weight2 = object->isAnyLatchButtonPressed() ? 127 : 0;

                // I would weight this by the amount of damage that the character just sustained,
                // but there is no real way to do this?

                // Get the maximum effect.
                float weight = std::max(weight1, weight2);

                // The character is on foot.
                sum_pos += object->getPosition() * weight;
                sum_wt += weight;
            }

            if (trackedPlayerCount == 0)
            {
                // Do nothing.
            }
            else if (trackedPlayerCount == 1 && soleTrackedPlayer != nullptr)
            {
                // Copy from the one character.
                _trackPos = soleTrackedPlayer->getPosition();
            }
            else if (sum_wt > 0.0f)
            {
                // Use the characer's "activity" to average the position the camera is viewing.
                new_track = sum_pos * (1.0f / sum_wt);
            }
            else
            {
                // If all live tracked players are idle, keep the current focal point.
            }
        }
        break;

    default:
        break;
    }


    if (CameraMovementMode::Player == _moveMode)
    {
        // Smoothly interpolate the camera tracking position.
        _trackPos = _trackPos * 0.9f + new_track * 0.1f;           /// @todo Use Ego::Math::lerp.
    }
    else
    {
        // Just set the position.
        _trackPos = new_track;
    }
}

void Camera::update(const ego_mesh_t *mesh)
{
    // Update the _turnTime counter.
    if (CameraTurnMode::None != _turnMode)
    {
        _turnTime = 255;
    }
    else if (_turnTime > 0)
    {
        _turnTime--;
    }

    // Camera controls.
    for(const std::shared_ptr<Ego::Player> &player : GameSessionContext::get().playerList()) {
        readInput(player->getInputDevice());
    }

    // Update the special camera effects like swinging and blur
    updateEffects();

    // Update the average position of the tracked characters
    updateTrack();

    // Move the camera center, if need be.
    updateCenter();

    // Make the zadd and zoom work together.
    updateZoom();

    // Update the position of the camera.
    updatePosition();

    // Set the view matrix.
    makeMatrix();
}

void Camera::updateEffects()
{
    float local_swingamp = _swingAmp;
    const LocalPlayerPerceptionState& localPlayerPerception = GameSessionContext::get().localPlayerPerception();

    _motionBlurOld = _motionBlur;

    // Fade out the motion blur
    if ( _motionBlur > 0 )
    {
        _motionBlur *= 0.99f; //Decay factor
        if ( _motionBlur < 0.001f ) _motionBlur = 0;
    }

    // Swing the camera if players are groggy
    if ( localPlayerPerception.grogLevel > 0 )
    {
        float zoom_add;
        _swing = (_swing + 120) & 0x3FFF;
        local_swingamp = std::max(local_swingamp, 0.175f);

        zoom_add = ( 0 == ((( int )localPlayerPerception.grogLevel ) % 2 ) ? 1 : - 1 ) * DEFAULT_TURN_KEY * localPlayerPerception.grogLevel * 0.35f;

        _zaddGoto   = _zaddGoto + zoom_add;

        _zaddGoto = Ego::Math::constrain(_zaddGoto, CAM_ZADD_MIN, CAM_ZADD_MAX);
    }

    //Rotate camera if they are dazed
    if ( localPlayerPerception.dazeLevel > 0 )
    {
        _turnZAdd = localPlayerPerception.dazeLevel * DEFAULT_TURN_KEY * 0.5f;
    }
    
    // Apply motion blur
    if ( localPlayerPerception.dazeLevel > 0 || localPlayerPerception.grogLevel > 0 ) {
        _motionBlur = std::min(0.95f, 0.5f + 0.03f * std::max(localPlayerPerception.dazeLevel, localPlayerPerception.grogLevel));
    }

    //Apply camera swinging
    //mat_Multiply( _mView.v, mat_Translate( tmp1.v, pos[kX], -pos[kY], pos[kZ] ), _mViewSave.v );  // xgg
    if ( local_swingamp > 0.001f )
    {
        _roll = TLT::get().sin(_swing) * local_swingamp;
        //mat_Multiply( _mView.v, mat_RotateY( tmp1.v, roll ), mat_Copy( tmp2.v, _mView.v ) );
    }
    // If the camera stops swinging for some reason, slowly return to _original position
    else if ( 0 != _roll )
    {
        _roll *= 0.9875f;            //Decay factor
        //mat_Multiply( _mView.v, mat_RotateY( tmp1.v, _roll ), mat_Copy( tmp2.v, _mView.v ) );

        // Come to a standstill at some point
        if ( std::abs( _roll ) < 0.001f )
        {
            _roll = 0;
            _swing = 0;
        }
    }

    _swing = ( _swing + _swingRate ) & 0x3FFF;
}

void Camera::addTrackTarget(ObjectRef targetRef)
{
    //Make sure the target is valid
    Object* object = GameSessionContext::get().tryObject(targetRef);
    if(!object) {
        return;
    }

    //Initialize camera position on spawn
    if(_trackList.empty()) {
        _trackPos = object->getPosition();
        _center = _trackPos;
        m_position = _center + Ego::Vector3f(_zoom * std::sin(_ori.facing_z), _zoom * std::cos(_ori.facing_z), _zGoto);
    }

    _trackList.push_front(targetRef);
}
