#include "egolib/game/Graphics/ObjectGraphics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Core/EngineContext.hpp"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/Core/ISessionState.hpp"
#include "egolib/game/graphic.h"
#include "egolib/game/Module/IModuleEnvironment.hpp"
#include "egolib/game/game.h" // get_light() used by ObjectGraphics_internal.hpp
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/Graphics/ObjectGraphics_internal.hpp"

namespace Ego
{
namespace Graphics
{
namespace
{
const LocalPlayerPerceptionState& localPlayerPerception()
{
    if (ISessionState* session = tryActiveSessionState())
    {
        return session->localPlayerPerception();
    }

    return GameSessionContext::get().localPlayerPerception();
}
}

using namespace detail;    // shared file-local helpers (see ObjectGraphics_internal.hpp)

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
    auto mesh = activeModuleEnvironment().mesh();
    const uint32_t currentUpdateFrame = activeSessionState().worldUpdateCount();

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

bool ObjectGraphics::playAction(const ModelAction action, const bool action_ready)
{
    return startAnimation(getModelDescriptor()->getAction(action), action_ready, true);
}

int ObjectGraphics::getMaxLight() const
{
    return _maxLight;    
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

const AnimatedModelFrame& ObjectGraphics::getNextFrame() const
{
    assertFrameIndex(_targetFrameIndex);
    return getModelDescriptor()->getModel()->getFrames()[_targetFrameIndex];
}

const AnimatedModelFrame& ObjectGraphics::getLastFrame() const
{
    assertFrameIndex(_sourceFrameIndex);
    return getModelDescriptor()->getModel()->getFrames()[_sourceFrameIndex];
}

void ObjectGraphics::getTint(GLXvector4f tint, const bool reflection, const int type) const
{
    TintRenderState tintState = makeTintRenderState(_object, alpha, light, sheen, colorshift, reflection);
    applyLocalPlayerPerception(tintState, localPlayerPerception());
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

} //namespace Graphics
} //namespace Ego
