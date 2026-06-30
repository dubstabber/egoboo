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

/// @file egolib/game/Graphics/ObjectGraphics_vertices.cpp
/// @brief ObjectGraphics vertex interpolation/cache and model-binding methods. Split out of
///   ObjectGraphics.cpp; the residual keeps construction, lighting, profile, tint, and matrix
///   state, while ObjectGraphics_animation.cpp keeps the animation state machine.

#include "egolib/game/Graphics/ObjectGraphics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"

namespace Ego
{
namespace Graphics
{
namespace
{
uint32_t vertexCacheWorldUpdateCount()
{
    if (ISessionState* session = tryActiveSessionState())
    {
        return session->worldUpdateCount();
    }

    return GameSessionContext::get().worldUpdateCount();
}
}

// the flip tolerance is the default flip increment / 2
static constexpr float FLIP_TOLERANCE = 0.25f * 0.5f;

ObjectGraphics::VertexUpdateNeed ObjectGraphics::needsUpdate(int vmin, int vmax) const
{
    VertexUpdateNeed result;

    // check to see if the _vertexCache has been marked as invalid.
    // in this case, everything needs to be updated
	if (!isVertexCacheValid()) {
		return result;
	}

    // get the last valid vertex from the chr_instance
    int maxvert = static_cast<int>(_vertexList.size()) - 1;

    // check to make sure the lower bound of the saved data is valid.
    // it is initialized to an invalid value (_vertexCache.vmin = _vertexCache.vmax = -1)
	if (_vertexCache.vmin < 0 || _vertexCache.vmax < 0) {
		return result;
	}
    // check to make sure the upper bound of the saved data is valid.
	if (_vertexCache.vmin > maxvert || _vertexCache.vmax > maxvert) {
		return result;
	}
    // make sure that the min and max vertices are in the correct order
	if (vmax < vmin) {
		std::swap(vmax, vmin);
	}
    // test to see if we have already calculated this data
    result.verticesMatch = (vmin >= _vertexCache.vmin) && (vmax <= _vertexCache.vmax);

	bool flips_match = (std::abs(_vertexCache.flip - _animationProgress) < FLIP_TOLERANCE);

    result.framesMatch = (_targetFrameIndex == _sourceFrameIndex && _vertexCache.frame_nxt == _targetFrameIndex && _vertexCache.frame_lst == _sourceFrameIndex ) ||
                         (flips_match && _vertexCache.frame_nxt == _targetFrameIndex && _vertexCache.frame_lst == _sourceFrameIndex);
    result.updateNeeded = !result.verticesMatch || !result.framesMatch;

    return result;
}

void ObjectGraphics::interpolateVerticesRaw(const std::vector<AnimatedModelVertex> &lst_ary, const std::vector<AnimatedModelVertex> &nxt_ary, int vmin, int vmax, float flip )
{
    /// raw indicates no bounds checking, so be careful

    if ( 0.0f == flip )
    {
        for (size_t i = vmin; i <= vmax; i++)
        {
            GLvertex* dst = &_vertexList[i];
            const AnimatedModelVertex &srcLast = lst_ary[i];

			dst->pos[XX] = srcLast.pos[kX];
			dst->pos[YY] = srcLast.pos[kY];
			dst->pos[ZZ] = srcLast.pos[kZ];
            dst->pos[WW] = 1.0f;

			dst->nrm[XX] = srcLast.nrm[kX];
			dst->nrm[YY] = srcLast.nrm[kY];
			dst->nrm[ZZ] = srcLast.nrm[kZ];

            dst->env[XX] = srcLast.envU;
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
    else if ( 1.0f == flip )
    {
        for (size_t i = vmin; i <= vmax; i++ )
        {
            GLvertex* dst = &_vertexList[i];
            const AnimatedModelVertex &srcNext = nxt_ary[i];

			dst->pos[XX] = srcNext.pos[kX];
			dst->pos[YY] = srcNext.pos[kY];
			dst->pos[ZZ] = srcNext.pos[kZ];
            dst->pos[WW] = 1.0f;

            dst->nrm[XX] = srcNext.nrm[kX];
            dst->nrm[YY] = srcNext.nrm[kY];
            dst->nrm[ZZ] = srcNext.nrm[kZ];

            dst->env[XX] = srcNext.envU;
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
    else
    {
        for (size_t i = vmin; i <= vmax; i++)
        {
            GLvertex* dst = &_vertexList[i];
            const AnimatedModelVertex &srcLast = lst_ary[i];
            const AnimatedModelVertex &srcNext = nxt_ary[i];

            dst->pos[XX] = srcLast.pos[kX] + ( srcNext.pos[kX] - srcLast.pos[kX] ) * flip;
            dst->pos[YY] = srcLast.pos[kY] + ( srcNext.pos[kY] - srcLast.pos[kY] ) * flip;
            dst->pos[ZZ] = srcLast.pos[kZ] + ( srcNext.pos[kZ] - srcLast.pos[kZ] ) * flip;
            dst->pos[WW] = 1.0f;

            dst->nrm[XX] = srcLast.nrm[kX] + ( srcNext.nrm[kX] - srcLast.nrm[kX] ) * flip;
            dst->nrm[YY] = srcLast.nrm[kY] + ( srcNext.nrm[kY] - srcLast.nrm[kY] ) * flip;
            dst->nrm[ZZ] = srcLast.nrm[kZ] + ( srcNext.nrm[kZ] - srcLast.nrm[kZ] ) * flip;

            dst->env[XX] = srcLast.envU + ( srcNext.envU - srcLast.envU ) * flip;
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
}

bool ObjectGraphics::updateVertices(int vmin, int vmax, bool force)
{
    bool vertices_match = false;
    bool frames_match = false;
    float  loc_flip;

    int vdirty1_min = -1, vdirty1_max = -1;
    int vdirty2_min = -1, vdirty2_max = -1;

    // get the model
    const std::shared_ptr<AnimatedModel> &model = getModelDescriptor()->getModel();

    // make sure we have valid data
    if (_vertexList.size() != model->getVertexCount())
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "character instance vertex data does not match its model", Log::EndOfEntry);
        return false;
    }

    // get the vertex list size from the chr_instance
    int maxvert = static_cast<int>(_vertexList.size()) - 1;

    // handle the default parameters
    if ( vmin < 0 ) vmin = 0;
    if ( vmax < 0 ) vmax = maxvert;

    // are they in the right order?
    if ( vmax < vmin ) std::swap(vmax, vmin);

    // make sure that the vertices are within the max range
    vmin = Ego::Math::constrain(vmin, 0, maxvert);
    vmax = Ego::Math::constrain(vmax, 0, maxvert);

    if (force)
    {
        // force an update of vertices

        // select a range that encompases the requested vertices and the saved vertices
        // if this is the 1st update, the saved vertices may be set to invalid values, as well

        // grab the dirty vertices
        vdirty1_min = vmin;
        vdirty1_max = vmax;

        // force the routine to update
        vertices_match = false;
        frames_match   = false;
    }
    else
    {
        // do we need to update?
        const VertexUpdateNeed updateNeed = needsUpdate(vmin, vmax);
        vertices_match = updateNeed.verticesMatch;
        frames_match = updateNeed.framesMatch;
        if (!updateNeed.updateNeeded) {
            return true;
        }

        if ( !frames_match )
        {
            // the entire frame is dirty
            vdirty1_min = vmin;
            vdirty1_max = vmax;
        }
        else
        {
            // grab the dirty vertices
            if ( vmin < _vertexCache.vmin )
            {
                vdirty1_min = vmin;
                vdirty1_max = _vertexCache.vmin - 1;
            }

            if ( vmax > _vertexCache.vmax )
            {
                vdirty2_min = _vertexCache.vmax + 1;
                vdirty2_max = vmax;
            }
        }
    }

    // make sure the frames are in the valid range
    const auto& frameList = model->getFrames();
    if ( _targetFrameIndex >= frameList.size() || _sourceFrameIndex >= frameList.size() )
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "character instance frame is outside "
                                         "the range of its model", Log::EndOfEntry);
        return false;
    }

    // grab the frame data from the correct model
    const auto& nextFrame = frameList[_targetFrameIndex];
    const auto& lastFrame = frameList[_sourceFrameIndex];

    // fix the flip for objects that are not animating
    loc_flip = _animationProgress;
    if ( _targetFrameIndex == _sourceFrameIndex ) {
        loc_flip = 0.0f;
    }

    // interpolate the 1st dirty region
    if ( vdirty1_min >= 0 && vdirty1_max >= 0 )
    {
		interpolateVerticesRaw(lastFrame.vertexList, nextFrame.vertexList, vdirty1_min, vdirty1_max, loc_flip);
    }

    // interpolate the 2nd dirty region
    if ( vdirty2_min >= 0 && vdirty2_max >= 0 )
    {
		interpolateVerticesRaw(lastFrame.vertexList, nextFrame.vertexList, vdirty2_min, vdirty2_max, loc_flip);
    }

    // update the saved parameters
    updateVertexCache(vmin, vmax, force, vertices_match, frames_match);
    return true;
}

bool ObjectGraphics::updateVertexCache(int vmin, int vmax, bool force, bool vertices_match, bool frames_match)
{
    // this is getting a bit ugly...
    // we need to do this calculation as little as possible, so it is important that the
    // _vertexCache.* values be tested and stored properly

	int maxvert = static_cast<int>(_vertexList.size()) - 1;

    // the save_vmin and save_vmax is the most complex
    bool verts_updated = false;
    if ( force )
    {
        // to get here, either the specified range was outside the clean range or
        // the animation was updated. In any case, the only vertices that are
        // clean are in the range [vmin, vmax]

        _vertexCache.vmin   = vmin;
        _vertexCache.vmax   = vmax;
        verts_updated = true;
    }
    else if ( vertices_match && frames_match )
    {
        // everything matches, so there is nothing to do
    }
    else if ( vertices_match )
    {
        // The only way to get here is to fail the frames_match test, and pass vertices_match

        // This means that all of the vertices were SUPPOSED TO BE updated,
        // but only the ones in the range [vmin, vmax] actually were.
        _vertexCache.vmin = vmin;
        _vertexCache.vmax = vmax;
        verts_updated = true;
    }
    else if ( frames_match )
    {
        // The only way to get here is to fail the vertices_match test, and pass frames_match test

        // There was no update to the animation,  but there was an update to some of the vertices
        // The clean verrices should be the union of the sets of the vertices updated this time
        // and the oned updated last time.
        //
        // If these ranges are disjoint, then only one of them can be saved. Choose the larger set

        if ( vmax >= _vertexCache.vmin && vmin <= _vertexCache.vmax )
        {
            // the old list [save_vmin, save_vmax] and the new list [vmin, vmax]
            // overlap, so we can merge them
            _vertexCache.vmin = std::min( _vertexCache.vmin, vmin );
            _vertexCache.vmax = std::max( _vertexCache.vmax, vmax );
            verts_updated = true;
        }
        else
        {
            // the old list and the new list are disjoint sets, so we are out of luck
            // save the set with the largest number of members
            if (( _vertexCache.vmax - _vertexCache.vmin ) >= ( vmax - vmin ) )
            {
                // obviously no change...
                //_vertexCache.vmin = _vertexCache.vmin;
                //_vertexCache.vmax = _vertexCache.vmax;
                verts_updated = true;
            }
            else
            {
                _vertexCache.vmin = vmin;
                _vertexCache.vmax = vmax;
                verts_updated = true;
            }
        }
    }
    else
    {
        // The only way to get here is to fail the vertices_match test, and fail the frames_match test

        // everything was dirty, so just save the new vertex list
        _vertexCache.vmin = vmin;
        _vertexCache.vmax = vmax;
        verts_updated = true;
    }

    _vertexCache.frame_nxt = _targetFrameIndex;
    _vertexCache.frame_lst = _sourceFrameIndex;
    _vertexCache.flip      = _animationProgress;
    const uint32_t currentUpdateFrame = vertexCacheWorldUpdateCount();

    // store the last time there was an update to the animation
    bool frames_updated = false;
    if ( !frames_match )
    {
        _vertexCache.frame_wld = currentUpdateFrame;
        frames_updated   = true;
    }

    // store the time of the last full update
    if ( 0 == vmin && maxvert == vmax )
    {
        _vertexCache.vert_wld  = currentUpdateFrame;
    }

    return verts_updated || frames_updated;
}

bool ObjectGraphics::updateGripVertices(const uint16_t vrt_lst[], const size_t vrt_count)
{
    if ( nullptr == vrt_lst || 0 == vrt_count ) {
        return false;
    }

    // count the valid attachment points
    int vmin = 0xFFFF;
    int vmax = 0;
    size_t count = 0;
    for (size_t cnt = 0; cnt < vrt_count; cnt++ )
    {
        if ( 0xFFFF == vrt_lst[cnt] ) continue;

        vmin = std::min<uint16_t>(vmin, vrt_lst[cnt]);
        vmax = std::max<uint16_t>(vmax, vrt_lst[cnt]);
        count++;
    }

    // if there are no valid points, there is nothing to do
    if (0 == count) {
        return false;
    }

    // force the vertices to update
    return updateVertices(vmin, vmax, true);
}

void ObjectGraphics::clearCache()
{
    /// @author BB
    /// @details force chr_instance_update_vertices() recalculate the vertices the next time
    ///     the function is called

    _vertexCache.clear();
    this->matrix_cache = matrix_cache_t();

    _lastLightingUpdateFrame = -1;
}

const GLvertex& ObjectGraphics::getVertex(const size_t index) const
{
    return _vertexList[index];
}

bool ObjectGraphics::setModel(const std::shared_ptr<Ego::ModelDescriptor> &model)
{
    bool updated = false;

    if (getModelDescriptor() != model) {
        updated = true;
        _modelDescriptor = model;
    }

    // set the vertex size
    size_t vlst_size = getModelDescriptor()->getModel()->getVertexCount();
    if (_vertexList.size() != vlst_size) {
        updated = true;
        _vertexList.resize(vlst_size);
    }

    // set the frames to frame 0 of this object's data
    if (0 != _targetFrameIndex || 0 != _sourceFrameIndex) {
        updated = true;
        _sourceFrameIndex = 0;
        _targetFrameIndex = 0;
    }

    if (updated) {
        // update the vertex and lighting cache
        clearCache();
        updateVertices(-1, -1, true);
    }

    return updated;
}

bool ObjectGraphics::isVertexCacheValid() const
{
    if (_sourceFrameIndex != _vertexCache.frame_nxt) {
        return false;
    }

    if (_sourceFrameIndex != _vertexCache.frame_lst) {
        return false;
    }

    if ((_sourceFrameIndex != _vertexCache.frame_lst) && std::abs(_animationProgress - _vertexCache.flip) > FLIP_TOLERANCE) {
        return false;
    }

    return true;
}

size_t ObjectGraphics::getVertexCount() const
{
    return _vertexList.size();
}

void ObjectGraphics::flashVariableHeight(const uint8_t valuelow, const int16_t low, const uint8_t valuehigh, const int16_t high)
{
    for (size_t cnt = 0; cnt < _vertexList.size(); cnt++)
    {
        int16_t z = _vertexList[cnt].pos[ZZ];

        if ( z < low )
        {
            _vertexList[cnt].col[RR] =
                _vertexList[cnt].col[GG] =
                    _vertexList[cnt].col[BB] = valuelow;
        }
        else if ( z > high )
        {
            _vertexList[cnt].col[RR] =
                _vertexList[cnt].col[GG] =
                    _vertexList[cnt].col[BB] = valuehigh;
        }
        else if ( high != low )
        {
            uint8_t valuemid = ( valuehigh * ( z - low ) / ( high - low ) ) +
                             ( valuelow * ( high - z ) / ( high - low ) );

            _vertexList[cnt].col[RR] =
                _vertexList[cnt].col[GG] =
                    _vertexList[cnt].col[BB] =  valuemid;
        }
        else
        {
            // z == high == low
            uint8_t valuemid = ( valuehigh + valuelow ) * 0.5f;

            _vertexList[cnt].col[RR] =
                _vertexList[cnt].col[GG] =
                    _vertexList[cnt].col[BB] =  valuemid;
        }
    }
}

} //namespace Graphics
} //namespace Ego
