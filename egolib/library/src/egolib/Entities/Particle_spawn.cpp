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

/// @file egolib/Entities/Particle_spawn.cpp
/// @brief Particle creation, initialization, and attachment helpers.

#include "egolib/Entities/Particle_internal.h"
#include "egolib/game/Core/EngineContext.hpp"

namespace Ego
{

namespace
{
IParticleHandler& particleHandler()
{
    return EngineContext::get().particleHandler();
}
}

void Particle::reset(ParticleRef ref)
{
    //We are terminated until we are initialized()
    _isTerminated = true;

    _particleID = ref;
    frame_count = 0;
    _collidedObjects.clear();

    _particleProfileID = INVALID_PIP_REF;
    _particleProfile = nullptr;

    _attachedTo = ObjectRef::Invalid;
    owner_ref = ObjectRef::Invalid;
    _target = ObjectRef::Invalid;
    parent_ref = ParticleRef::Invalid;
    _spawnerProfile = ObjectProfileRef::Invalid;

    attachedto_vrt_off = 0;
    type = SPRITE_LIGHT;
    facing = Facing(0);
    team = 0;

    vel_stt = idlib::zero<Vector3f>();
    offset = idlib::zero<Vector3f>();

    PhysicsData::reset(this);

    rotate = Facing(0);
    rotate_add = Facing(0);

    size_stt = 0;
    size = 0;
    size_add = 0;

    _image.reset();

    // "no lifetime" = "eternal"
    is_eternal = false;
    lifetime_total = std::numeric_limits<size_t>::max();
    lifetime_remaining = lifetime_total;
    frames_total = std::numeric_limits<size_t>::max();
    frames_remaining = frames_total;

    contspawn_timer = 0;

    // bumping
    bump_size_stt = 0;           ///< the starting size of the particle (8.8 fixed point)
    bumper_t::reset(&bump_real);
    bumper_t::reset(&bump_padded);
    prt_min_cv = oct_bb_t(oct_vec_v2_t());
    prt_max_cv = oct_bb_t(oct_vec_v2_t());

    // damage
    damagetype = DamageType::DAMAGE_SLASH;
    damage.base = 0;
    damage.rand = 0;
    lifedrain = 0;
    manadrain = 0;

    // bump effects
    is_bumpspawn = false;

    // motion effects
    buoyancy = 0.0f;
    air_resistance = 0.0f;
    _isHoming = false;
    no_gravity = false;

    ///if != SPAWNNOCHARACTER, then a character is spawned on end
    endspawn_characterstate = SPAWNNOCHARACTER;

    dynalight.reset();
    inst.reset();
    enviro.reset();
}

bool Particle::initialize(const ParticleRef particleID, const Vector3f& spawnPos, const Facing& spawnFacing, ObjectProfileRef spawnProfile,
                          const PIP_REF particleProfile, const ObjectRef spawnAttach, uint16_t vrt_offset, const TEAM_REF spawnTeam,
                          const ObjectRef spawnOrigin, const ParticleRef spawnParticleOrigin, const int multispawn, const ObjectRef spawnTarget,
                          const bool onlyOverWater)
{
    const int INFINITE_UPDATES = std::numeric_limits<int>::max();

    Vector3f vel;
    int offsetfacing = 0, newrand;

    //if(!isTerminated()) {
    //    throw std::logic_error("Tried to spawn an existing particle that was not terminated");
    //}

    //Clear any old data first
    reset(ParticleRef(particleID));

    //Load particle profile
    _spawnerProfile = spawnProfile.get();
    _particleProfileID = particleProfile;
    assert(EngineContext::get().profileSystem().isParticleProfileLoaded(_particleProfileID));
    _particleProfile = EngineContext::get().profileSystem().getParticleProfile(_particleProfileID);
    assert(_particleProfile != nullptr); //"Tried to spawn particle with invalid PIP_REF"

    team = spawnTeam;
    parent_ref = ParticleRef(spawnParticleOrigin);
    damagetype = getProfile()->damageType;
    lifedrain = getProfile()->lifeDrain;
    manadrain = getProfile()->manaDrain;

    //Mark particle as no longer terminated
    _isTerminated = false;

    // Save a version of the position for local use.
    // In cpp, will be passed by reference, so we do not want to alter the
    // components of the original vector.
    Vector3f tmp_pos = spawnPos;
    Facing loc_facing = idlib::canonicalize(spawnFacing);

    // try to get an idea of who our owner is even if we are
    // given bogus info
    ObjectRef loc_chr_origin = spawnOrigin;
    if (!activeModule().getObjectHandler().exists(spawnOrigin) && particleHandler()[spawnParticleOrigin])
    {
        loc_chr_origin = particleHandler()[spawnParticleOrigin]->getOwner();
    }
    owner_ref = loc_chr_origin;

    // Lighting and sound
    dynalight = getProfile()->dynalight;
    dynalight.on = false;
    if (0 == multispawn)
    {
        dynalight.on = getProfile()->dynalight.mode;
        if (DYNA_MODE_LOCAL == getProfile()->dynalight.mode)
        {
            dynalight.on = DYNA_MODE_OFF;
        }
    }

    // Set character attachments ( ObjectRef::Invalid means none )
    _attachedTo = spawnAttach;
    attachedto_vrt_off = vrt_offset;

    // Correct loc_facing
    loc_facing += Facing(getProfile()->getSpawnFacing().base);

    // Targeting...
    vel.z() = 0;

    offset.z() = generate_irand_pair(getProfile()->getSpawnPositionOffsetZ()) - (getProfile()->getSpawnPositionOffsetZ().rand / 2);
    tmp_pos.z() += offset.z();
    const int velocity = generate_irand_pair(getProfile()->getSpawnVelocityOffsetXY());

    //Set target
    _target = spawnTarget;
    if (getProfile()->newtargetonspawn)
    {
        if (getProfile()->targetcaster)
        {
            // Set the target to the caster
            _target = owner_ref;
        }
        else
        {
            const float PERFECT_AIM = 45.0f;   // 45 dex is perfect aim
            float attackerAgility = activeModule().getObjectHandler().get(owner_ref)->getAttribute(Ego::Attribute::AGILITY);

            //Sharpshooter Perk improves aim by 25%
            if(activeModule().getObjectHandler().get(owner_ref)->hasPerk(Ego::Perks::SHARPSHOOTER)) {
                attackerAgility = std::min(PERFECT_AIM, attackerAgility*1.25f);
            }

            // Find a target
            Facing targetAngle;
            _target = prt_find_target(spawnPos, idlib::canonicalize(loc_facing), _particleProfileID, spawnTeam, owner_ref, spawnTarget, &targetAngle);
            const std::shared_ptr<Object> &target = activeModule().getObjectHandler()[_target];

            if (target && !getProfile()->homing)
            {
                //Correct angle to new target
                loc_facing -= Facing(targetAngle);
            }

            //Agility determines how good we aim towards the target
            offsetfacing = 0;
            if ( attackerAgility < PERFECT_AIM)
            {
                //Add some random error (Apply 50% error at 10 Agility)
                float aimError = 0.5f;

                //Increase aim error by 5% for each Agility below 10 (up to a max of 100% error at 0 Agility)
                if(attackerAgility < 10.0f) {
                    aimError += 0.5f - (attackerAgility*0.05f);
                }
                else {
                    //Agility reduces aim error (convering towards 0% error at 45 Agility)
                    aimError -= (0.5f/PERFECT_AIM) * attackerAgility;
                }

                offsetfacing = Random::next(getProfile()->getSpawnFacing().rand) - (getProfile()->getSpawnFacing().rand / 2);
                offsetfacing *= aimError;
            }

            if (0.0f != getProfile()->zaimspd)
            {
                if (target)
                {
                    // These aren't velocities...  This is to do aiming on the Z axis
                    if (velocity > 0)
                    {
                        vel[kX] = target->getPosX() - spawnPos[kX];
                        vel[kY] = target->getPosY() - spawnPos[kY];
                        float distance = std::sqrt(vel[kX] * vel[kX] + vel[kY] * vel[kY]) / velocity;  // This is the number of steps...
                        if (distance > 0.0f)
                        {
                            // This is the vel[kZ] alteration
                            vel[kZ] = (target->getPosZ() + (target->getCurrentBump().height * 0.5f) - tmp_pos[kZ]) / distance;
                        }
                    }
                }
                else
                {
                    vel[kZ] = 0.5f * getProfile()->zaimspd;
                }

                vel[kZ] = Ego::Math::constrain(vel[kZ], -0.5f * getProfile()->zaimspd, getProfile()->zaimspd);
            }
        }

        const std::shared_ptr<Object> &target = activeModule().getObjectHandler()[_target];

        // Does it go away?
        if (!target && getProfile()->needtarget)
        {
            requestTerminate();
            return false;
        }

        // Start on top of target
        if (target && getProfile()->startontarget)
        {
            tmp_pos[kX] = target->getPosX();
            tmp_pos[kY] = target->getPosY();
        }
    }
    else
    {
        // Correct loc_facing for randomness
        offsetfacing = generate_irand_pair(getProfile()->getSpawnFacing()) - (getProfile()->getSpawnFacing().base + getProfile()->getSpawnFacing().rand / 2);
    }
    loc_facing += Facing(offsetfacing);
    facing = Facing(loc_facing);

    // this is actually pointing in the opposite direction?
    // Location data from arguments
    newrand = generate_irand_pair(getProfile()->getSpawnPositionOffsetXY());
    offset[kX] = -std::cos(loc_facing) * newrand;
    offset[kY] = -std::sin(loc_facing) * newrand;

    tmp_pos[kX] += offset[kX];
    tmp_pos[kY] += offset[kY];

    //Particles can only spawn inside the map bounds
    tmp_pos[kX] = Ego::Math::constrain(tmp_pos[kX], 0.0f, activeModule().getMeshPointer()->_tmem._edge_x - 2.0f);
    tmp_pos[kY] = Ego::Math::constrain(tmp_pos[kY], 0.0f, activeModule().getMeshPointer()->_tmem._edge_y - 2.0f);

    setPosition(tmp_pos);
    setSpawnPosition(tmp_pos);

    //Can this particle only spawn over water?
    if(onlyOverWater && !isOverWater()) {
        return false;
    }

    // Velocity data
    vel.x() = -std::cos(loc_facing) * velocity;
    vel.y() = -std::sin(loc_facing) * velocity;
    vel.z() += generate_irand_pair(getProfile()->getSpawnVelocityOffsetZ()) - (getProfile()->getSpawnVelocityOffsetZ().rand / 2);
    this->setVelocity(vel);
    this->setOldVelocity(vel);
    this->vel_stt = vel;

    // Template values
    bump_size_stt = getProfile()->bump_size;
    type = getProfile()->type;

    // Image data
    rotate = Facing((FACING_T)generate_irand_pair(getProfile()->rotate_pair));
    rotate_add = Facing(getProfile()->rotate_add);

    size_stt = getProfile()->size_base;
    size_add = getProfile()->size_add;

    _image._start = (getProfile()->image_stt)*EGO_ANIMATION_MULTIPLIER;
    _image._add = generate_irand_pair(getProfile()->image_add);
    _image._count = (getProfile()->image_max)*EGO_ANIMATION_MULTIPLIER;

    // a particle can EITHER end_lastframe or end_time.
    // if it ends after the last frame, end_time tells you the number of cycles through
    // the animation
    int prt_anim_frames_updates = 0;
    bool prt_anim_infinite = false;
    if (getProfile()->end_lastframe)
    {
        if (0 == _image._add)
        {
            prt_anim_frames_updates = INFINITE_UPDATES;
            prt_anim_infinite = true;
        }
        else
        {
            prt_anim_frames_updates = _image.getUpdateCount();

            if (getProfile()->end_time > 0)
            {
                // Part time is used to give number of cycles
                prt_anim_frames_updates *= getProfile()->end_time;
            }
        }
    }
    else
    {
        // no end to the frames
        prt_anim_frames_updates = INFINITE_UPDATES;
        prt_anim_infinite = true;
    }
    prt_anim_frames_updates = std::max(1, prt_anim_frames_updates);

    // estimate the number of frames
    int prt_life_frames_updates = 0;
    bool prt_life_infinite = false;
    if (getProfile()->end_lastframe)
    {
        // for end last frame, the lifetime is given by the number of animation frames
        prt_life_frames_updates = prt_anim_frames_updates;
        prt_life_infinite = prt_anim_infinite;
    }
    else if (getProfile()->end_time <= 0)
    {
        // zero or negative lifetime == infinite lifetime
        prt_life_frames_updates = INFINITE_UPDATES;
        prt_life_infinite = true;
    }
    else
    {
        prt_life_frames_updates = getProfile()->end_time;
    }
    prt_life_frames_updates = std::max(1, prt_life_frames_updates);

    // set lifetime counter
    if (prt_life_infinite)
    {
        lifetime_total = std::numeric_limits<size_t>::max();
        is_eternal = true;
    }
    else
    {
        // the lifetime is really supposed to be in terms of frames, but
        // to keep the number of updates stable, the frames could lag.
        // sooo... we just rescale the prt_life_frames_updates so that it will work with the
        // updates and cross our fingers
        //lifetime_total = std::ceil((float)prt_life_frames_updates * (float)GameEngine::GAME_TARGET_UPS / (float)GameEngine::GAME_TARGET_FPS);
        lifetime_total = prt_life_frames_updates;
    }

    // make the particle exists for AT LEAST one update
    lifetime_total = std::max<size_t>(1, lifetime_total);
    lifetime_remaining = lifetime_total;

    // set the frame counters
    // make the particle display AT LEAST one frame, regardless of how many updates
    // it has or when someone requests for it to terminate
    frames_total = std::max(1, prt_anim_frames_updates);
    frames_remaining = frames_total;

    // Damage stuff
    damage = range_to_pair(getProfile()->damage);

    //If it is a FIRE particle spawned by a Pyromaniac, increase damage by 25%
    const std::shared_ptr<Object> &owner = activeModule().getObjectHandler()[owner_ref];
    if(owner != nullptr && owner->hasPerk(Ego::Perks::PYROMANIAC)) {
        damage.base *= 1.25f;
        damage.rand *= 1.25f;
    }

    // Spawning data
    if (0 != getProfile()->contspawn._delay)
    {
        contspawn_timer = 1;

        // Because attachment takes an update before it happens
        if (isAttached()) {
            contspawn_timer++;
        }
    }

    // set up the particle transparency
    inst.alpha = 0xFF;
    switch (inst.type)
    {
        case SPRITE_SOLID: break;
        case SPRITE_ALPHA: inst.alpha = 0x80; break;    //#define PRT_TRANS 0x80
        case SPRITE_LIGHT: break;
    }

    // is the spawn location safe?
    Vector2f nrm;
    if (0 == hit_wall(tmp_pos, nrm, nullptr))
    {
        setSafePosition(tmp_pos);
    }

    // get an initial value for the _isHoming variable
    _isHoming = getProfile()->homing && !isAttached();

    //enable or disable gravity
    no_gravity = getProfile()->ignore_gravity;

    // estimate some parameters for buoyancy and air resistance
    {
        const float buoyancy_min = 0.0f;
        const float buoyancy_max = 2.0f * std::abs(Ego::Physics::g_environment.gravity);
        const float air_resistance_min = 0.0f;
        const float air_resistance_max = 1.0f;

        // find the buoyancy, assuming that the air_resistance of the particle
        // is equal to air_friction at standard gravity
        buoyancy = -getProfile()->spdlimit * (1.0f - Ego::Physics::g_environment.airfriction) - Ego::Physics::g_environment.gravity;
        buoyancy = Ego::Math::constrain(buoyancy, buoyancy_min, buoyancy_max);

        // reduce the buoyancy if the particle falls
        if (getProfile()->spdlimit > 0.0f) buoyancy *= 0.5f;

        // determine if there is any left-over air resistance
        if (std::abs(getProfile()->spdlimit) > 0.0001f)
        {
            air_resistance = 1.0f - (buoyancy + Ego::Physics::g_environment.gravity) / -getProfile()->spdlimit;
            air_resistance = Ego::Math::constrain(air_resistance, air_resistance_min, air_resistance_max);

            air_resistance /= Ego::Physics::g_environment.airfriction;
            air_resistance = Ego::Math::constrain(air_resistance, 0.0f, 1.0f);
        }
        else
        {
            air_resistance = 0.0f;
        }
    }

    endspawn_characterstate = SPAWNNOCHARACTER;

    //Set starting size
    setSize(getProfile()->size_base);

#if defined(_DEBUG) && defined(DEBUG_PRT_LIST)

    // some code to track all allocated particles, where they came from, how long they are going to last,
    // what they are being used for...
    const auto& spawnerProfile = EngineContext::get().profileSystem().getProfile(_spawnerProfile);
    log_debug( "spawn_one_particle() - spawned a particle %d\n"
        "\tupdate == %d, remaining life == %d\n"
        "\towner == %d (\"%s\")\n"
        "\tparticleProfile == %d(\"%s\")\n"
        "\t\t%s"
        "\tobjectProfile == %d(\"%s\")\n"
        "\n",
        _particleID,
        worldUpdateCount(), static_cast<int>(lifetime_remaining),
        loc_chr_origin, activeModule().getObjectHandler().exists( loc_chr_origin ) ? activeModule().getObjectHandler().get(loc_chr_origin)->Name : "INVALID",
        _particleProfileID, getProfile()->getName().c_str(),
        getProfile()->comment,
        _spawnerProfile, spawnerProfile ? spawnerProfile->getPathname().c_str() : "INVALID");
#endif

    //Attach ourselves to an Object if needed
    if (ObjectRef::Invalid != _attachedTo)
    {
        attach(_attachedTo);
    }

    //Spawn sound effect
    playSound(getProfile()->soundspawn);

    return true;
}

bool Particle::attach(const ObjectRef attach)
{
    const std::shared_ptr<Object> &pchr = activeModule().getObjectHandler()[attach];
    if(!pchr) {
        return false;
    }

    _attachedTo = attach;

    if(!placeAtVertex(pchr, attachedto_vrt_off)) {
        return false;
    }

    // Correct facing so swords knock characters in the right direction...
    if (getProfile()->hasBit(DAMFX_TURN))
    {
        facing = idlib::canonicalize(pchr->getFacingZ());
    }

    return true;
}

bool Particle::placeAtVertex(const std::shared_ptr<Object> &object, int vertex_offset)
{
    int vertex;
    Vector4f point[1], nupoint[1];

    // Check validity of attachment
    if (object->isInsideInventory()) {
        requestTerminate();
        return false;
    }

    // Do we have a matrix???
    if ( !chr_matrix_valid(object.get()) )
    {
        chr_update_matrix(object.get(), true);
    }

    // Do we have a matrix???
    if ( chr_matrix_valid(object.get()) )
    {
        // Transform the weapon vertex_offset from model to world space

        if ( vertex_offset == GRIP_ORIGIN )
        {
            Vector3f tmp_pos;
            tmp_pos[kX] = object->getMatrix()(0, 3);
            tmp_pos[kY] = object->getMatrix()(1, 3);
            tmp_pos[kZ] = object->getMatrix()(2, 3);

            setPosition(tmp_pos);

            return true;
        }

        if(vertex_offset > object->getVertexCount()) {
            throw std::invalid_argument("Particle::placeAtVertex() =  vertex_offset > object->inst.getVertexCount()");
        }

        vertex = object->getVertexCount() - vertex_offset;

        // do the automatic update
        object->updateVertices(vertex, vertex, false);

        // Calculate vertex_offset point locations with linear interpolation and other silly things
        point[0][kX] = object->getVertex(vertex).pos[XX];
        point[0][kY] = object->getVertex(vertex).pos[YY];
        point[0][kZ] = object->getVertex(vertex).pos[ZZ];
        point[0][kW] = 1.0f;

        // Do the transform
        Utilities::transform(object->getMatrix(), point, nupoint, 1);

        setPosition(Vector3f(nupoint[0][kX],nupoint[0][kY],nupoint[0][kZ]));
    }
    else
    {
        // No matrix, so just wing it...
        setPosition(object->getPosition());
    }

    return true;
}

} // namespace Ego
