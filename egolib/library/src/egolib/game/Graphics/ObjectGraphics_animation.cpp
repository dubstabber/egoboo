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

/// @file egolib/game/Graphics/ObjectGraphics_animation.cpp
/// @brief ObjectGraphics animation state machine (frame/action advance, animation-rate policy,
///   FX, interpolation commit). Split out of ObjectGraphics.cpp (which keeps the vertex/lighting/
///   cache/matrix/profile/tint half); both TUs implement the same ObjectGraphics class and stay in
///   egolib-library. The file-static chr_invalidate_child_instances moved here with its sole caller;
///   the shared anon-namespace helpers live in ObjectGraphics_internal.hpp.

#include "egolib/game/Graphics/ObjectGraphics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h" //only for character_swipe()
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Graphics/ObjectGraphics_internal.hpp"

namespace Ego
{
namespace Graphics
{

using namespace detail;    // shared file-local helpers (see ObjectGraphics_internal.hpp)

static void chr_invalidate_child_instances(const IInventoryHolder &object)
{
    if (const std::shared_ptr<Object>& leftHandItem = heldItem(object, SLOT_LEFT)) {
        leftHandItem->setMatrixCacheValid(false);
    }
    if (const std::shared_ptr<Object>& rightHandItem = heldItem(object, SLOT_RIGHT)) {
        rightHandItem->setMatrixCacheValid(false);
    }
}

const std::shared_ptr<Ego::ModelDescriptor>& ObjectGraphics::getModelDescriptor() const {
    return _modelDescriptor;
}

void ObjectGraphics::assertFrameIndex(int frameIndex) const {
    if (frameIndex > getModelDescriptor()->getMD2()->getFrames().size()) {
        auto e = Log::Entry::create(Log::Level::Error, __FILE__, __LINE__, "invalid frame ", frameIndex, "/", 
                                    getModelDescriptor()->getMD2()->getFrames().size(), Log::EndOfEntry);
        EngineContext::get().logTarget() << e;
        throw idlib::runtime_error(__FILE__, __LINE__, e.getText());
    }
}

void ObjectGraphics::setAnimationSpeed(const float rate)
{
    _animationRate = Ego::Math::constrain(rate, 0.1f, 3.0f);
}

void ObjectGraphics::updateAnimation()
{
    float flip_diff  = 0.25f * _animationRate;
    float flip_next = getRemainingFlip();

    while ( flip_next > 0.0f && flip_diff >= flip_next )
    {
        flip_diff -= flip_next;

        publishInterpolationState(_animationProgressInteger + 1, 0.25f * (_animationProgressInteger + 1));
        if (!applyPublishedInterpolationStep())
        {
            break;
        }

        flip_next = getRemainingFlip();
    }

    if ( flip_diff > 0.0f )
    {
        const uint8_t ilip_old = _animationProgressInteger;
        const float updatedProgress = _animationProgress + flip_diff;
        const uint8_t updatedInteger = static_cast<uint8_t>(std::floor(updatedProgress * 4)) % 4;

        publishInterpolationState(updatedInteger, updatedProgress);

        if ( ilip_old != _animationProgressInteger )
        {
            applyPublishedInterpolationStep();
        }
    }

    updateAnimationRate();
}

float ObjectGraphics::getRemainingFlip() const
{
    return (_animationProgressInteger + 1) * 0.25f - _animationProgress;
}

bool ObjectGraphics::handleAnimationFX() const
{
    uint32_t framefx = getFrameFX();

    if ( 0 == framefx ) return true;

    // Check frame effects
    if ( HAS_SOME_BITS( framefx, MADFX_ACTLEFT ) )
    {
        character_swipe( _object.getObjRef(), SLOT_LEFT );
    }

    if ( HAS_SOME_BITS( framefx, MADFX_ACTRIGHT ) )
    {
        character_swipe( _object.getObjRef(), SLOT_RIGHT );
    }

    if ( HAS_SOME_BITS( framefx, MADFX_GRABLEFT ) )
    {
        _object.grabStuff(GRIP_LEFT, false);
    }

    if ( HAS_SOME_BITS( framefx, MADFX_GRABRIGHT ) )
    {
        _object.grabStuff(GRIP_RIGHT, false);
    }

    if ( HAS_SOME_BITS( framefx, MADFX_CHARLEFT ) )
    {
        _object.grabStuff(GRIP_LEFT, true);
    }

    if ( HAS_SOME_BITS( framefx, MADFX_CHARRIGHT ) )
    {
        _object.grabStuff(GRIP_RIGHT, true);
    }

    if ( HAS_SOME_BITS( framefx, MADFX_DROPLEFT ) )
    {
        if (const std::shared_ptr<Object>& leftHandItem = heldItem(_object, SLOT_LEFT)) {
            leftHandItem->detachFromHolder(false, true);
        }
    }

    if ( HAS_SOME_BITS( framefx, MADFX_DROPRIGHT ) )
    {
        if (const std::shared_ptr<Object>& rightHandItem = heldItem(_object, SLOT_RIGHT)) {
            rightHandItem->detachFromHolder(false, true);
        }
    }

    if ( HAS_SOME_BITS( framefx, MADFX_POOF ) && !_object.isPlayer() )
    {
        _object.setAIPoofTime(GameSessionContext::get().worldUpdateCount());
    }

    //Do footfall sound effect
    if (config().sound_footfallEffects_enable.getValue() && HAS_SOME_BITS(framefx, MADFX_FOOTFALL))
    {
        audioSystem().playSound(_object.getPosition(), _object.getProfile()->getFootFallSound());
    }

    return true;
}

void ObjectGraphics::incrementFrame()
{
    // fix the ilip and flip
    _animationProgressInteger %= 4;
    _animationProgress = fmod(_animationProgress, 1.0f);

    // Change frames
    int frame_lst = _targetFrameIndex;
    int frame_nxt = _targetFrameIndex + 1;

    // detect the end of the animation and handle special end conditions
    if (frame_nxt > getModelDescriptor()->getLastFrame(_currentAnimation))
    {
        if (_freezeAtLastFrame)
        {
            frame_nxt = handleFrozenAnimationEnd(frame_lst);
        }
        else if (_loopAnimation)
        {
            frame_nxt = handleLoopedAnimationEnd();
        }
        else
        {
            frame_nxt = handleQueuedAnimationEnd();
        }
    }

    _sourceFrameIndex = frame_lst;
    _targetFrameIndex = frame_nxt;

    // if the instance is invalid, invalidate everything that depends on this object
    invalidateChildInstancesIfCacheInvalid();
}

int ObjectGraphics::handleFrozenAnimationEnd(int frame_lst)
{
    // Freeze that animation at the last frame.
    _canBeInterrupted = true;
    return frame_lst;
}

int ObjectGraphics::handleLoopedAnimationEnd()
{
    // Convert the action into a riding action if the character is mounted.
    if (_object.isBeingHeld())
    {
        startAnimation(resolveMountedLoopAnimation(), true, true);
    }

    // Break a looped action at any time.
    _canBeInterrupted = true;

    // Set the frame to the beginning of the current action.
    return getModelDescriptor()->getFirstFrame(_currentAnimation);
}

int ObjectGraphics::handleQueuedAnimationEnd()
{
    // Go on to the next action. don't let just anything interrupt it?
    incrementAction();

    // incrementAction() actually sets this value properly. just grab the new value.
    return _targetFrameIndex;
}

ModelAction ObjectGraphics::resolveMountedLoopAnimation() const
{
    // ACTION_MH == "sitting"; use it when the rider is holding something.
    if (heldItem(_object, SLOT_LEFT) || heldItem(_object, SLOT_RIGHT))
    {
        return getModelDescriptor()->getAction(ACTION_MH);
    }

    return getModelDescriptor()->getAction(ACTION_MI);
}

bool ObjectGraphics::tryCommitActionState(const ModelAction action, const bool action_ready, const bool override_action)
{
    // is the chosen action valid?
    if (!getModelDescriptor()->isActionValid(action)) {
        return false;
    }

    // are we going to check action_ready?
    if (!override_action && !_canBeInterrupted) {
        return false;
    }

    // set up the action
    _currentAnimation = action;
    _nextAnimation = ACTION_DA;
    _canBeInterrupted = action_ready;

    return true;
}

void ObjectGraphics::publishFrameState(const uint16_t sourceFrameIndex,
                                       const uint16_t targetFrameIndex,
                                       const uint8_t animationProgressInteger)
{
    _sourceFrameIndex = sourceFrameIndex;
    _targetFrameIndex = targetFrameIndex;
    _animationProgressInteger = animationProgressInteger;
    _animationProgress = _animationProgressInteger * 0.25f;
}

void ObjectGraphics::publishInterpolationState(const uint8_t animationProgressInteger,
                                               const float animationProgress)
{
    _animationProgressInteger = animationProgressInteger;
    _animationProgress = animationProgress;
}

bool ObjectGraphics::applyPublishedInterpolationStep()
{
    if ( 3 == _animationProgressInteger )
    {
        handleAnimationFX();
    }

    if ( 4 == _animationProgressInteger )
    {
        incrementFrame();
    }

    if ( _animationProgressInteger > 4 )
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "invalid ilip", Log::EndOfEntry);
        _animationProgressInteger = 0;
        return false;
    }

    return true;
}

bool ObjectGraphics::tryCommitFrameState(const int frame)
{
    // is the frame within the valid range for this action?
    if (!getModelDescriptor()->isFrameValid(_currentAnimation, frame)) {
        return false;
    }

    // jump to the next frame
    publishFrameState(_targetFrameIndex, frame, 0);

    return true;
}

bool ObjectGraphics::tryRestartAnimationAtActionStart(const ModelAction action,
                                                      const bool action_ready,
                                                      const bool override_action)
{
    if (!tryCommitActionState(action, action_ready, override_action)) {
        return false;
    }

    return tryCommitFrameState(getModelDescriptor()->getFirstFrame(action));
}

bool ObjectGraphics::normalizeCurrentAnimationForFrameMutation()
{
    // we have to have a valid action range
    if (_currentAnimation > ACTION_COUNT) {
        return false;
    }

    // try to heal a bad action
    _currentAnimation = getModelDescriptor()->getAction(_currentAnimation);

    // reject the action if it cannot be made valid
    return _currentAnimation != ACTION_COUNT;
}

void ObjectGraphics::invalidateChildInstancesIfCacheInvalid()
{
    if (!isVertexCacheValid()) {
        chr_invalidate_child_instances(_object);
    }
}

bool ObjectGraphics::startAnimation(const ModelAction action, const bool action_ready, const bool override_action)
{
    if (!tryRestartAnimationAtActionStart(action, action_ready, override_action)) {
        return false;
    }

    // if the instance is invalid, invalidate everything that depends on this object
    invalidateChildInstancesIfCacheInvalid();

    return true;
}

bool ObjectGraphics::setFrame(int frame)
{
    return tryCommitFrameState(frame);
}

bool ObjectGraphics::incrementAction()
{
    // get the correct action
    ModelAction action = getModelDescriptor()->getAction(_nextAnimation);

    // determine if the action is one of the types that can be broken at any time
    // D == "dance" and "W" == walk
    // @note ZF> Can't use ACTION_IS_TYPE(action, D) because of GCC compile warning
    bool action_ready = action < ACTION_DD || ACTION_IS_TYPE(action, W);

    return startAnimation(action, action_ready, true);
}

bool ObjectGraphics::shouldSkipAnimationRateUpdate()
{
    if (_object.isAttacking()) {
        return true;
    }

    if (_freezeAtLastFrame) {
        return true;
    }

    if (!canBeInterrupted())
    {
        if (0.0f == _animationRate) {
            _animationRate = 1.0f;
        }
        return true;
    }

    return false;
}

bool ObjectGraphics::applyMountedAnimationRatePolicy()
{
    if (!_object.isBeingHeld() || ((ACTION_MI != _currentAnimation) && (ACTION_MH != _currentAnimation)))
    {
        return false;
    }

    const std::shared_ptr<Object>& holder = attachmentObject(_object.getHolderRef());
    if (!holder) {
        return false;
    }

    if (holder->isScenery()) {
        //This is a special case to make animation while in the Pot (which is actually a "mount") look better
        _animationRate = 0.0f;
    }
    else {
        // just copy the rate from the mount
        _animationRate = holder->getAnimationSpeed();
    }

    return true;
}

void ObjectGraphics::applyIdleAnimationPolicy()
{
    _object.setBoredTimer(_object.getBoredTimer() - 1);
    if (_object.getBoredTimer() < 0)
    {
        _object.resetBoredTimer();

        //Don't yell "im bored!" while stealthed!
        if(!_object.isStealthed())
        {
            _object.addAIAlertBits(ALERTIF_BORED);

            // set the action to "bored", which is ACTION_DB, ACTION_DC, or ACTION_DD
            const int rand_val = Random::next(std::numeric_limits<uint16_t>::max());
            const ModelAction boredAction = getModelDescriptor()->getAction(ACTION_DB + (rand_val % 3));
            startAnimation(boredAction, true, true);
        }
    }
    else if (_currentAnimation > ACTION_DD)
    {
        const ModelAction idleAction = getModelDescriptor()->getAction(ACTION_DA);
        startAnimation(idleAction, true, true);
    }
}

void ObjectGraphics::applyMovementAnimationPolicy(ModelAction action, int lip)
{
    const ModelAction resolvedAction = getModelDescriptor()->getAction(action);
    if (ACTION_COUNT == resolvedAction)
    {
        return;
    }

    if (_currentAnimation != resolvedAction)
    {
        restartMovementAnimation(resolvedAction, lip);
    }

    _nextAnimation = resolvedAction;
}

void ObjectGraphics::updateAnimationRate()
{
    if (shouldSkipAnimationRateUpdate()) {
        return;
    }

    // go back to a base animation rate, in case the next frame is not a
    // "variable speed frame"
    _animationRate = 1.0f;

    // if the character is mounted or sitting, base the rate off of the mount
    if (!applyMountedAnimationRatePolicy())
    {
        const auto decision = makeLocomotionAnimationDecision(_object, *getModelDescriptor(), _currentAnimation);
        if (!decision.shouldApply) {
            return;
        }

        _animationRate = decision.animationRate;

        if (ACTION_DA == decision.action)
        {
            applyIdleAnimationPolicy();
        }
        else
        {
            applyMovementAnimationPolicy(decision.action, decision.lip);
        }
    }

    //Limit final animation speed
    setAnimationSpeed(_animationRate);
}

bool ObjectGraphics::canBeInterrupted() const
{
    return _canBeInterrupted;    
}

bool ObjectGraphics::setAction(const ModelAction action, const bool action_ready, const bool override_action)
{
    return tryCommitActionState(action, action_ready, override_action);
}

bool ObjectGraphics::setFrameFull(int frame_along, int ilip)
{
    if (!normalizeCurrentAnimationForFrameMutation()) {
        return false;
    }

    // get some frame info
    int frame_stt   = getModelDescriptor()->getFirstFrame(_currentAnimation);
    int frame_end   = getModelDescriptor()->getLastFrame(_currentAnimation);
    int frame_count = 1 + ( frame_end - frame_stt );

    // try to heal an out of range value
    frame_along %= frame_count;

    // get the next frames
    int new_nxt = frame_stt + frame_along;
    new_nxt = std::min(new_nxt, frame_end);

    publishFrameState(_sourceFrameIndex, new_nxt, static_cast<uint8_t>(ilip));

    // set the validity of the cache
    return true;
}

void ObjectGraphics::restartMovementAnimation(ModelAction action, int lip)
{
    if (!tryCommitActionState(action, true, true)) {
        return;
    }

    const int walkFrame = getModelDescriptor()->getFrameLipToWalkFrame(lip, getNextFrame().framelip);
    if (!tryCommitFrameState(walkFrame)) {
        return;
    }

    if (!tryRestartAnimationAtActionStart(action, true, true)) {
        return;
    }

    invalidateChildInstancesIfCacheInvalid();
}

ModelAction ObjectGraphics::getCurrentAnimation() const
{
    return _currentAnimation;
}

void ObjectGraphics::removeInterpolation()
{
    if (_sourceFrameIndex != _targetFrameIndex ) {
        publishFrameState(_targetFrameIndex, _targetFrameIndex, 0);
    }
}

oct_bb_t ObjectGraphics::getBoundingBox() const
{
    //Beginning of a frame animation
    if (_targetFrameIndex == _sourceFrameIndex || _animationProgress == 0.0f) {
        return getLastFrame().bb;
    } 

    //Finished frame animation
    if (_animationProgress == 1.0f) {
        return getNextFrame().bb;
    } 

    //We are middle between two animation frames
    return oct_bb_t::interpolate(getLastFrame().bb, getNextFrame().bb, _animationProgress);
}

} //namespace Graphics
} //namespace Ego
