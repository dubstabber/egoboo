#include "egolib/game/Graphics/ObjectGraphics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/game.h" //only for character_swipe()

namespace Ego
{
namespace Graphics
{

// the flip tolerance is the default flip increment / 2
static constexpr float FLIP_TOLERANCE = 0.25f * 0.5f;

namespace
{
egoboo_config_t& config()
{
    return EngineContext::get().config();
}

struct TintRenderState
{
    int alpha;
    int light;
    int sheen;
    colorshift_t colorShift;
};

struct LocomotionAnimationDecision
{
    bool shouldApply = false;
    float animationRate = 1.0f;
    ModelAction action = ACTION_DA;
    int lip = 0;
};

bool isWalkTypeAnimation(ModelAction action)
{
    return action < ACTION_DD || ACTION_IS_TYPE(action, W);
}

LocomotionAnimationDecision makeLocomotionAnimationDecision(Object& object,
                                                            const ModelDescriptor& modelDescriptor,
                                                            ModelAction currentAnimation)
{
    LocomotionAnimationDecision decision;
    if (!isWalkTypeAnimation(currentAnimation))
    {
        return decision;
    }

    if (!object.isTouchingGround() && !object.isFlying())
    {
        return decision;
    }

    decision.shouldApply = true;

    float speed = 0.0f;
    if (object.isFlying())
    {
        speed = idlib::euclidean_norm(object.getVelocity());
    }
    else
    {
        speed = std::max(idlib::euclidean_norm(xy(object.getVelocity())),
                         idlib::euclidean_norm(object.getDesiredVelocity()));
        if (object.floorIsSlippy())
        {
            decision.animationRate = 2.0f;
            speed *= 2.0f;
        }
    }

    if (object.getFat() > 0.0f)
    {
        speed /= object.getFat();
    }

    if (speed <= 1.0f)
    {
        decision.action = ACTION_DA;
    }
    else if (object.isStealthed() && modelDescriptor.isActionValid(ACTION_WA))
    {
        decision.action = ACTION_WA;
        decision.lip = LIPWA;
    }
    else if (speed <= 4.0f && modelDescriptor.isActionValid(ACTION_WB))
    {
        decision.action = ACTION_WB;
        decision.lip = LIPWB;
    }
    else
    {
        decision.action = ACTION_WC;
        decision.lip = LIPWC;
    }

    if (object.isFlying())
    {
        switch (decision.action)
        {
            case ACTION_DA: decision.action = ACTION_WC; break;
            case ACTION_WA: decision.action = ACTION_WB; break;
            case ACTION_WB: decision.action = ACTION_WA; break;
            case ACTION_WC: decision.action = ACTION_DA; break;
            default: break;
        }
    }

    return decision;
}

uint8_t computeReflectionAlpha(const Object& object, uint8_t alpha)
{
    const float altitudeAboveGround = std::max(0.0f, object.getPosZ() - object.getFloorElevation());
    float alphaFade = (255.0f - altitudeAboveGround) * 0.5f;
    alphaFade = Ego::Math::constrain(alphaFade, 0.0f, 255.0f);

    return alpha * alphaFade * idlib::fraction<float, 1, 255>();
}

TintRenderState makeTintRenderState(const Object& object,
                                    uint8_t alpha,
                                    uint8_t light,
                                    uint8_t sheen,
                                    const colorshift_t& colorShift,
                                    bool reflection)
{
    if (!reflection)
    {
        return TintRenderState{alpha, light, sheen, colorShift};
    }

    const uint8_t reflectionAlpha = computeReflectionAlpha(object, alpha);
    const int reflectionLight = (light == 0xFF)
                              ? 0xFF
                              : light * reflectionAlpha * idlib::fraction<float, 1, 255>();

    return TintRenderState{
        reflectionAlpha,
        reflectionLight,
        sheen / 2,
        colorshift_t(static_cast<uint8_t>(colorShift.red + 1),
                     static_cast<uint8_t>(colorShift.green + 1),
                     static_cast<uint8_t>(colorShift.blue + 1))
    };
}

void applyLocalPlayerPerception(TintRenderState& state, const LocalPlayerPerceptionState& localPlayerPerception)
{
    if (localPlayerPerception.seeInvisibleLevel > 0.0f)
    {
        state.alpha = std::max(state.alpha, static_cast<int>(SEEINVISIBLE));
    }

    state.light = get_light(state.light, localPlayerPerception.seeDarkMagnitude);
}

void encodeTint(GLXvector4f tint, const TintRenderState& state, int type)
{
    tint[RR] = 1.0f / (1 << state.colorShift.red);
    tint[GG] = 1.0f / (1 << state.colorShift.green);
    tint[BB] = 1.0f / (1 << state.colorShift.blue);
    tint[AA] = 1.0f;

    switch (type)
    {
        case CHR_LIGHT:
        case CHR_ALPHA:
            tint[AA] = state.alpha * idlib::fraction<float, 1, 255>();
            tint[RR] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.red);
            tint[GG] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.green);
            tint[BB] = state.light * idlib::fraction<float, 1, 255>() / (1 << state.colorShift.blue);
            break;

        case CHR_PHONG:
        {
            const float amount = (Ego::Math::constrain(state.sheen, 0, 15) << 4) / 240.0f;

            tint[RR] += tint[RR] * 0.5f + amount;
            tint[GG] += tint[GG] * 0.5f + amount;
            tint[BB] += tint[BB] * 0.5f + amount;

            tint[RR] /= 2.0f;
            tint[GG] /= 2.0f;
            tint[BB] /= 2.0f;
            break;
        }

        case CHR_SOLID:
        case CHR_REFLECT:
        case CHR_UNKNOWN:
        default:
            break;
    }
}

} // namespace

ObjectGraphics::ObjectGraphics(Object &object) :
    matrix_cache(),

    alpha(0xFF),
    light(0),
    sheen(0),

    colorshift(),

    uoffset(0),
    voffset(0),

    _object(object),
    _vertexList(),
    _matrix(idlib::identity<Matrix4f4f>()),
    _reflectionMatrix(idlib::identity<Matrix4f4f>()),

    // graphical optimizations
    _vertexCache(),

    // lighting info
    _ambientColour(0),
    _maxLight(-0xFF),
    _lastLightingUpdateFrame(-1),

    _modelDescriptor(nullptr),
    _animationRate(1.0f),
    _animationProgress(0.0f),
    _animationProgressInteger(0),
    _targetFrameIndex(0),
    _sourceFrameIndex(0),

    _canBeInterrupted(true),
    _freezeAtLastFrame(false),
    _loopAnimation(false),
    _currentAnimation(ACTION_DA),
    _nextAnimation(ACTION_DA)    
{
    // initalize the character instance
    setObjectProfile(_object.getProfile());
}

ObjectGraphics::~ObjectGraphics() 
{
    //dtor
}

void ObjectGraphics::updateLighting()
{
    static constexpr uint32_t FRAME_SKIP = 1 << 2;
    static constexpr uint32_t FRAME_MASK = FRAME_SKIP - 1;
    auto mesh = GameSessionContext::get().mesh();
    const uint32_t currentUpdateFrame = GameSessionContext::get().worldUpdateCount();

    // make sure the matrix is valid
    //chr_update_matrix(&_object, true);

    // has this already been calculated in the last FRAME_SKIP update frames?
	if (_lastLightingUpdateFrame >= 0 && static_cast<uint32_t>(_lastLightingUpdateFrame) >= currentUpdateFrame) {
		return;
	}

    // reduce the amount of updates to one every FRAME_SKIP frames, but dither
    // the updating so that not all objects update on the same frame
    _lastLightingUpdateFrame = currentUpdateFrame + ((currentUpdateFrame + _object.getObjRef().get()) & FRAME_MASK);

    // interpolate the lighting for the origin of the object
	lighting_cache_t global_light;
    GridIllumination::grid_lighting_interpolate(*mesh, global_light, Vector2f(_object.getPosX(), _object.getPosY()));

    // rotate the lighting data to body_centered coordinates
	lighting_cache_t loc_light;
	lighting_cache_t::lighting_project_cache(loc_light, global_light, getMatrix());

    //Low-pass filter to smooth lighting transitions?
    //_ambientColour = 0.9f * _ambientColour + 0.1f * (loc_light.hgh._lighting[LVEC_AMB] + loc_light.low._lighting[LVEC_AMB]) * 0.5f;
    //_ambientColour = (loc_light.hgh._lighting[LVEC_AMB] + loc_light.low._lighting[LVEC_AMB]) * 0.5f;
    _ambientColour = get_ambient_level();

    _maxLight = -0xFF;
    for (size_t cnt = 0; cnt < _vertexList.size(); cnt++ )
    {
        float lite = 0.0f;

        GLvertex *pvert = &_vertexList[cnt];

        // a simple "height" measurement
        float hgt = pvert->pos[ZZ] * _matrix(3, 3) + _matrix(3, 3);

        if (pvert->nrm[0] == 0.0f && pvert->nrm[1] == 0.0f && pvert->nrm[2] == 0.0f)
        {
            // this is the "ambient only" index, but it really means to sum up all the light
            lite  = lighting_cache_t::lighting_evaluate_cache(loc_light, Vector3f(+1.0f,+1.0f,+1.0f), hgt, mesh->_tmem._bbox, nullptr, nullptr);
            lite += lighting_cache_t::lighting_evaluate_cache(loc_light, Vector3f(-1.0f,-1.0f,-1.0f), hgt, mesh->_tmem._bbox, nullptr, nullptr);

            // average all the directions
            lite /= 6.0f;
        }
        else
        {
            lite = lighting_cache_t::lighting_evaluate_cache(loc_light, Vector3f(pvert->nrm[0],pvert->nrm[1],pvert->nrm[2]), hgt, mesh->_tmem._bbox, nullptr, nullptr);
        }

        pvert->color_dir = lite;

        _maxLight = std::max(_maxLight, pvert->color_dir);
    }

    // ??coerce this to reasonable values in the presence of negative light??
    _maxLight = std::max(_maxLight, 0);
}

int ObjectGraphics::getAmbientColour() const
{
    return _ambientColour;
}

gfx_rv ObjectGraphics::needs_update(int vmin, int vmax, bool *verts_match, bool *frames_match)
{
	bool local_verts_match, local_frames_match;

    // ensure that the pointers point to something
    if ( NULL == verts_match ) verts_match  = &local_verts_match;
    if ( NULL == frames_match ) frames_match = &local_frames_match;

    // initialize the boolean pointers
    *verts_match  = false;
    *frames_match = false;

    // check to see if the _vertexCache has been marked as invalid.
    // in this case, everything needs to be updated
	if (!isVertexCacheValid()) {
		return gfx_success;
	}

    // get the last valid vertex from the chr_instance
    int maxvert = static_cast<int>(_vertexList.size()) - 1;

    // check to make sure the lower bound of the saved data is valid.
    // it is initialized to an invalid value (_vertexCache.vmin = _vertexCache.vmax = -1)
	if (_vertexCache.vmin < 0 || _vertexCache.vmax < 0) {
		return gfx_success;
	}
    // check to make sure the upper bound of the saved data is valid.
	if (_vertexCache.vmin > maxvert || _vertexCache.vmax > maxvert) {
		return gfx_success;
	}
    // make sure that the min and max vertices are in the correct order
	if (vmax < vmin) {
		std::swap(vmax, vmin);
	}
    // test to see if we have already calculated this data
    *verts_match = (vmin >= _vertexCache.vmin) && (vmax <= _vertexCache.vmax);

	bool flips_match = (std::abs(_vertexCache.flip - _animationProgress) < FLIP_TOLERANCE);

    *frames_match = (_targetFrameIndex == _sourceFrameIndex && _vertexCache.frame_nxt == _targetFrameIndex && _vertexCache.frame_lst == _sourceFrameIndex ) ||
                    (flips_match && _vertexCache.frame_nxt == _targetFrameIndex && _vertexCache.frame_lst == _sourceFrameIndex);

    return (!(*verts_match) || !( *frames_match )) ? gfx_success : gfx_fail;
}

void ObjectGraphics::interpolateVerticesRaw(const std::vector<MD2_Vertex> &lst_ary, const std::vector<MD2_Vertex> &nxt_ary, int vmin, int vmax, float flip )
{
    /// raw indicates no bounds checking, so be careful

    if ( 0.0f == flip )
    {
        for (size_t i = vmin; i <= vmax; i++)
        {
            GLvertex* dst = &_vertexList[i];
            const MD2_Vertex &srcLast = lst_ary[i];

			dst->pos[XX] = srcLast.pos[kX];
			dst->pos[YY] = srcLast.pos[kY];
			dst->pos[ZZ] = srcLast.pos[kZ];
            dst->pos[WW] = 1.0f;

			dst->nrm[XX] = srcLast.nrm[kX];
			dst->nrm[YY] = srcLast.nrm[kY];
			dst->nrm[ZZ] = srcLast.nrm[kZ];

            dst->env[XX] = indextoenvirox[srcLast.normal];
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
    else if ( 1.0f == flip )
    {
        for (size_t i = vmin; i <= vmax; i++ )
        {
            GLvertex* dst = &_vertexList[i];
            const MD2_Vertex &srcNext = nxt_ary[i];

			dst->pos[XX] = srcNext.pos[kX];
			dst->pos[YY] = srcNext.pos[kY];
			dst->pos[ZZ] = srcNext.pos[kZ];
            dst->pos[WW] = 1.0f;

            dst->nrm[XX] = srcNext.nrm[kX];
            dst->nrm[YY] = srcNext.nrm[kY];
            dst->nrm[ZZ] = srcNext.nrm[kZ];

            dst->env[XX] = indextoenvirox[srcNext.normal];
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
    else
    {
        uint16_t vrta_lst, vrta_nxt;

        for (size_t i = vmin; i <= vmax; i++)
        {
            GLvertex* dst = &_vertexList[i];
            const MD2_Vertex &srcLast = lst_ary[i];
            const MD2_Vertex &srcNext = nxt_ary[i];

            dst->pos[XX] = srcLast.pos[kX] + ( srcNext.pos[kX] - srcLast.pos[kX] ) * flip;
            dst->pos[YY] = srcLast.pos[kY] + ( srcNext.pos[kY] - srcLast.pos[kY] ) * flip;
            dst->pos[ZZ] = srcLast.pos[kZ] + ( srcNext.pos[kZ] - srcLast.pos[kZ] ) * flip;
            dst->pos[WW] = 1.0f;

            dst->nrm[XX] = srcLast.nrm[kX] + ( srcNext.nrm[kX] - srcLast.nrm[kX] ) * flip;
            dst->nrm[YY] = srcLast.nrm[kY] + ( srcNext.nrm[kY] - srcLast.nrm[kY] ) * flip;
            dst->nrm[ZZ] = srcLast.nrm[kZ] + ( srcNext.nrm[kZ] - srcLast.nrm[kZ] ) * flip;

            vrta_lst = srcLast.normal;
            vrta_nxt = srcNext.normal;

            dst->env[XX] = indextoenvirox[vrta_lst] + ( indextoenvirox[vrta_nxt] - indextoenvirox[vrta_lst] ) * flip;
            dst->env[YY] = 0.5f * ( 1.0f + dst->nrm[ZZ] );
        }
    }
}

gfx_rv ObjectGraphics::updateVertices(int vmin, int vmax, bool force)
{
    bool vertices_match, frames_match;
    float  loc_flip;

    int vdirty1_min = -1, vdirty1_max = -1;
    int vdirty2_min = -1, vdirty2_max = -1;

    // get the model
    const std::shared_ptr<MD2Model> &pmd2 = getModelDescriptor()->getMD2();

    // make sure we have valid data
    if (_vertexList.size() != pmd2->getVertexCount())
    {
        EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "character instance vertex data does not match its md2", Log::EndOfEntry);
        return gfx_error;
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
        gfx_rv retval = needs_update(vmin, vmax, &vertices_match, &frames_match );
        if ( gfx_error == retval ) return gfx_error;            // gfx_error == retval means some pointer or reference is messed up
        if ( gfx_fail  == retval ) return gfx_success;          // gfx_fail  == retval means we do not need to update this round

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
    const auto& frameList = pmd2->getFrames();
    if ( _targetFrameIndex >= frameList.size() || _sourceFrameIndex >= frameList.size() )
    {
		EngineContext::get().logTarget() << Log::Entry::create(Log::Level::Warning, __FILE__, __LINE__, "character instance frame is outside "
                                         "the range of its MD2", Log::EndOfEntry);
        return gfx_error;
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
    return updateVertexCache(vmax, vmin, force, vertices_match, frames_match);
}

gfx_rv ObjectGraphics::updateVertexCache(int vmax, int vmin, bool force, bool vertices_match, bool frames_match)
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
    const uint32_t currentUpdateFrame = GameSessionContext::get().worldUpdateCount();

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

    return ( verts_updated || frames_updated ) ? gfx_success : gfx_fail;
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
    return updateVertices(vmin, vmax, true) == gfx_success;
}

bool ObjectGraphics::playAction(const ModelAction action, const bool action_ready)
{
    return startAnimation(getModelDescriptor()->getAction(action), action_ready, true);
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

int ObjectGraphics::getMaxLight() const
{
    return _maxLight;    
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
    size_t vlst_size = getModelDescriptor()->getMD2()->getVertexCount();
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

const Matrix4f4f& ObjectGraphics::getMatrix() const
{
    return _matrix;
}

const Matrix4f4f& ObjectGraphics::getReflectionMatrix() const
{
    return _reflectionMatrix;
}

uint8_t ObjectGraphics::getReflectionAlpha() const
{
    return computeReflectionAlpha(_object, alpha);
}

void ObjectGraphics::resetProfileApplicationState()
{
    _matrix = idlib::identity<Matrix4f4f>();
    _reflectionMatrix = idlib::identity<Matrix4f4f>();
    uoffset = 0;
    voffset = 0;

    _ambientColour = 0;
    _maxLight = -0xFF;
    _vertexList.clear();
    clearCache();

    //Animation and 3D model
    _modelDescriptor = nullptr;
    _targetFrameIndex = 0;
    _sourceFrameIndex = 0;
    _animationProgressInteger = 0;
    _animationProgress = 0.0f;
    _animationRate = 1.0f;

    //Action specific variables
    _canBeInterrupted = true;
    _freezeAtLastFrame = false;
    _loopAnimation = false;
    _currentAnimation = ACTION_DA;
    _nextAnimation = ACTION_DA;
}

void ObjectGraphics::applyProfileRenderDefaults(const ObjectProfile& profile)
{
    alpha = profile.getAlpha();
    light = profile.getLight();
    sheen = profile.getSheen();

    setModel(profile.getModel());
}

void ObjectGraphics::initializeProfileAnimation(const ObjectProfile& profile)
{
    setActionReady(false);
    setActionLooped(false);
    if (_object.isAlive()) {
        playAction(ACTION_DA, false);
        setActionKeep(false);
    }
    else {
        playAction(profile.getModel()->randomizeAction(ACTION_KA), false);
        setActionKeep(true);
    }
}

void ObjectGraphics::setObjectProfile(const std::shared_ptr<ObjectProfile> &profile)
{
    resetProfileApplicationState();
    applyProfileRenderDefaults(*profile);
    initializeProfileAnimation(*profile);
}

BIT_FIELD ObjectGraphics::getFrameFX() const
{
    return getNextFrame().framefx;
}

const MD2_Frame& ObjectGraphics::getNextFrame() const
{
    assertFrameIndex(_targetFrameIndex);
    return getModelDescriptor()->getMD2()->getFrames()[_targetFrameIndex];
}

const MD2_Frame& ObjectGraphics::getLastFrame() const
{
    assertFrameIndex(_sourceFrameIndex);
    return getModelDescriptor()->getMD2()->getFrames()[_sourceFrameIndex];
}

bool ObjectGraphics::isVertexCacheValid() const
{
    if (_sourceFrameIndex != _vertexCache.frame_nxt) {
        return false;
    }

    if (_sourceFrameIndex != _vertexCache.frame_lst) {
        return false;
    }

    if ((_sourceFrameIndex != _vertexCache.frame_lst) && std::abs(_animationProgress - _vertexCache.flip) > Ego::Graphics::FLIP_TOLERANCE) {
        return false;
    }

    return true;
}

void ObjectGraphics::getTint(GLXvector4f tint, const bool reflection, const int type) const
{
    TintRenderState tintState = makeTintRenderState(_object, alpha, light, sheen, colorshift, reflection);
    applyLocalPlayerPerception(tintState, GameSessionContext::get().localPlayerPerception());
    encodeTint(tint, tintState, type);
}

void ObjectGraphics::flash(uint8_t value)
{
	const float flash_val = value * idlib::fraction<float, 1, 255>();

	// flash the ambient color
	_ambientColour = flash_val;

	// flash the directional lighting
	for (size_t i = 0; i < _vertexList.size(); ++i) {
		_vertexList[i].color_dir = flash_val;
	}
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

void ObjectGraphics::setMatrix(const Matrix4f4f& matrix)
{
    //Set the normal model matrix
    _matrix = matrix;

    //Compute the reflected matrix as well
    _reflectionMatrix = matrix;
    _reflectionMatrix(2, 0) = -_reflectionMatrix(0, 2);
    _reflectionMatrix(2, 1) = -_reflectionMatrix(1, 2);
    _reflectionMatrix(2, 2) = -_reflectionMatrix(2, 2);
    _reflectionMatrix(2, 3) = 2.0f * _object.getFloorElevation() - _object.getPosZ();
}

static void chr_invalidate_child_instances(Object &object)
{
    if(object.getLeftHandItem()) {
        object.getLeftHandItem()->setMatrixCacheValid(false);
    }
    if(object.getRightHandItem()) {
        object.getRightHandItem()->setMatrixCacheValid(false);
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
        if(_object.getLeftHandItem()) {
            _object.getLeftHandItem()->detatchFromHolder(false, true);
        }
    }

    if ( HAS_SOME_BITS( framefx, MADFX_DROPRIGHT ) )
    {
        if(_object.getRightHandItem()) {
            _object.getRightHandItem()->detatchFromHolder(false, true);
        }
    }

    if ( HAS_SOME_BITS( framefx, MADFX_POOF ) && !_object.isPlayer() )
    {
        _object.setAIPoofTime(GameSessionContext::get().worldUpdateCount());
    }

    //Do footfall sound effect
    if (config().sound_footfallEffects_enable.getValue() && HAS_SOME_BITS(framefx, MADFX_FOOTFALL))
    {
        AudioSystem::get().playSound(_object.getPosition(), _object.getProfile()->getFootFallSound());
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
    if (_object.getLeftHandItem() || _object.getRightHandItem())
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

    if (_object.getHolder()->isScenery()) {
        //This is a special case to make animation while in the Pot (which is actually a "mount") look better
        _animationRate = 0.0f;
    }
    else {
        // just copy the rate from the mount
        _animationRate = _object.getHolder()->getAnimationSpeed();
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
