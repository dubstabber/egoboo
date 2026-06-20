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

/// @file egolib/Graphics/ModelDescriptor.hpp
/// @author Johan Jansen aka Zefz
#pragma once

#include "egolib/Graphics/ModelAnimationMetadata.hpp"

#include <memory>
#include <string>

//Forward declarations
namespace Ego { namespace Graphics { class AnimatedModel; } }

namespace Ego
{

class ModelDescriptor : private idlib::non_copyable
{
public:
    static const size_t FRAMELIP_COUNT = Graphics::ModelAnimationMetadata::FRAMELIP_COUNT;

    ModelDescriptor(const std::string &folderPath);

    const std::string& getName() const;

    const std::shared_ptr<Ego::Graphics::AnimatedModel>& getModel() const;

    /// @details translate the action that was given into a valid action for the model
    ///
    /// returns ACTION_COUNT on a complete failure, or the default ACTION_DA if it exists
    ModelAction getAction(int action) const;

    /**
    * @brief
    *   Gets all ModelFrameEffects that are in all of the animation frames in the specified action
    **/
    BIT_FIELD getMadFX(int action) const;

    /**
    * @return
    *   true if this model has a valid animation for the specified action
    **/
    bool isActionValid(int action) const;

    /// @details this function actually determines whether the action follows the
    ///               pattern of ACTION_?A, ACTION_?B, ACTION_?C, ACTION_?D, with
    ///               A and B being for the left hand, and C and D being for the right hand
    ModelAction randomizeAction(ModelAction action, int slot=0) const;

    void makeEquallyLit();

    int getFrameLipToWalkFrame(int lip, int framelip) const;

    bool isFrameValid(int action, int frame) const;

    int getFirstFrame(int action) const { return _animationMetadata.getFirstFrame(action); }
    int getLastFrame(int action) const { return _animationMetadata.getLastFrame(action); }

    /// @details This function changes a letter into an action code
    static ModelAction charToAction(char cTmp);

private:
    std::string _name;

    Graphics::ModelAnimationMetadata _animationMetadata;
    std::shared_ptr<Ego::Graphics::AnimatedModel> _model;   ///< actual animated model
};

} //Ego
