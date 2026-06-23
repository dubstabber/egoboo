#include "egolib/game/Physics/ParticlePhysics.hpp"
#include "egolib/Entities/_Include.hpp"
#include "egolib/Entities/IObjectWorld.hpp"       // activeObjectWorld() object/team access (the entity-world seam)
#include "egolib/Physics/PhysicalConstants.hpp"
#include "egolib/Physics/ICollisionWorld.hpp"    // activeCollisionWorld() terrain queries (the mesh-query seam)
#include "egolib/Physics/MeshLookupTables.hpp"   // g_meshLookupTables
#include "egolib/FileFormats/map_fx.hpp"          // TWIST_FLAT, MAPFX_SLIPPY (was via Module.hpp -> mesh.h)
#include "egolib/game/CharacterMatrix.h"
#include "egolib/game/Physics/ParticlePhysics_internal.h"

namespace Ego
{
namespace Physics
{

void ParticlePhysics::updateMovement()
{
    Ego::prt_environment_t *penviro = &(_particle.enviro);

    //capture the position
    Vector3f tmp_pos = _particle.getPosition();

    bool hit_a_floor = false;
    bool hit_a_wall = false;
    bool touch_a_floor = false;
    bool touch_a_wall = false;

    Vector3f nrm_total = idlib::zero<Vector3f>();

    // Move the particle
    float ftmp = tmp_pos.z();
    tmp_pos.z() += _particle.getVelocity().z();
    LOG_NAN(tmp_pos.z());

    //Are we touching the floor?
    if (tmp_pos.z() < penviro->adj_level)
    {
        Vector3f floor_nrm = Vector3f(0.0f, 0.0f, 1.0f);

        touch_a_floor = true;

        uint8_t tmp_twist = collisionWorld().getFanTwist(_particle.getTile());

        if (TWIST_FLAT != tmp_twist)
        {
            floor_nrm = g_meshLookupTables.twist_nrm[penviro->twist];
        }

        float vel_dot = dot(floor_nrm, _particle.getVelocity());

        //Handle bouncing
        if (_particle.getVelocity().z() < -STOPBOUNCINGPART)
        {
            // the particle will bounce
            nrm_total += floor_nrm;

            // take reflection in the floor into account when computing the new level
            tmp_pos.z() = penviro->adj_level + (penviro->adj_level - ftmp) * _particle.getProfile()->dampen + 0.1f;

            _particle.setVelocity({ _particle.getVelocity().x(),
                                    _particle.getVelocity().y(),
                                   -_particle.getVelocity().z()});

            hit_a_floor = true;
        }
        else if (vel_dot > 0.0f)
        {
            // the particle is not bouncing, it is just at the wrong height
            tmp_pos.z() = penviro->adj_level + 0.1f;
        }
        else
        {
            // the particle is in the "stop bouncing zone"
            tmp_pos.z() = penviro->adj_level + 0.1f;
            _particle.setVelocity({_particle.getVelocity().x(),
                                   _particle.getVelocity().y(),
                                   0.0f});
        }
    }

    // handle the sounds
    if (hit_a_floor)
    {
        // Play the sound for hitting the floor [FSND]
        _particle.playSound(_particle.getProfile()->end_sound_floor);
    }

    // handle the collision
    if (touch_a_floor && _particle.getProfile()->end_ground)
    {
        _particle.requestTerminate();
        return;
    }

    // interaction with the mesh walls
    hit_a_wall = false;
    if (std::abs(_particle.getVelocity().x()) + std::abs(_particle.getVelocity().y()) > 0.0f)
    {
        tmp_pos.x() += _particle.getVelocity().x();
        tmp_pos.y() += _particle.getVelocity().y();

        //Hitting a wall?
        if (EMPTY_BIT_FIELD != _particle.test_wall(tmp_pos))
        {
            Vector2f nrm;
            float   pressure;

            // how is the character hitting the wall?
            if (EMPTY_BIT_FIELD != _particle.hit_wall(tmp_pos, nrm, &pressure))
            {
                touch_a_wall = true;

                nrm_total.x() += nrm.x();
                nrm_total.y() += nrm.y();

                hit_a_wall = (dot(xy(_particle.getVelocity()), nrm) < 0.0f);
            }
        }
    }

    // handle the sounds
    if (hit_a_wall)
    {
        // Play the sound for hitting the wall [WSND]
        _particle.playSound(_particle.getProfile()->end_sound_wall);
    }

    // handle the collision
    if (touch_a_wall)
    {
        //End particle if it hits a wall?
        if(_particle.getProfile()->end_wall)
        {
            _particle.requestTerminate();
            return;            
        }
    }

    // do the reflections off the walls and floors
    if (hit_a_wall || hit_a_floor)
    {

        if ((hit_a_wall && (_particle.getVelocity().x() * nrm_total.x() + _particle.getVelocity().y() * nrm_total.y()) < 0.0f) ||
            (hit_a_floor && (_particle.getVelocity().z() * nrm_total.z()) < 0.0f))
        {
            float vdot;
            Vector3f vpara, vperp;

            nrm_total = normalize(nrm_total).get_vector();

            vdot = dot(nrm_total, _particle.getVelocity());

            vperp = nrm_total * vdot;

            vpara = _particle.getVelocity() - vperp;

            // do the reflection
            vperp *= -_particle.getProfile()->dampen;

            // fake the friction, for now
            if (0.0f != nrm_total.y() || 0.0f != nrm_total.z())
            {
                vpara.x() *= _particle.getProfile()->dampen;
            }

            if (0.0f != nrm_total.x() || 0.0f != nrm_total.z())
            {
                vpara.y() *= _particle.getProfile()->dampen;
            }

            if (0.0f != nrm_total.x() || 0.0f != nrm_total.y())
            {
                vpara.z() *= _particle.getProfile()->dampen;
            }

            // add the components back together
            _particle.setVelocity(vpara + vperp);
        }

        if (nrm_total.z() != 0.0f && _particle.getVelocity().z() < STOPBOUNCINGPART)
        {
            // this is the very last bounce
            _particle.setVelocity({_particle.getVelocity().x(), _particle.getVelocity().y(), 0.0f});
            tmp_pos.z() = penviro->adj_level + 0.0001f;
        }

        if (hit_a_wall)
        {
            float fx, fy;

            // fix the facing
            facing_to_vec(_particle.facing, &fx, &fy);

            if (0.0f != nrm_total.x())
            {
                fx *= -1;
            }

            if (0.0f != nrm_total.y())
            {
                fy *= -1;
            }

            _particle.facing = Facing(vec_to_facing(fx, fy));
        }
    }

    //Don't fall in pits...
    if (_particle.isHoming()) {
        tmp_pos.z() = std::max(tmp_pos.z(), 0.0f);
    }

    //Rotate particle to the direction we are moving
    if (_particle.getProfile()->rotatetoface)
    {
        if (std::abs(_particle.getVelocity().x()) + std::abs(_particle.getVelocity().y()) > FLT_EPSILON)
        {
            // use velocity to find the angle
            _particle.facing = Facing(vec_to_facing(_particle.getVelocity().x(), _particle.getVelocity().y()));
        }
        else if (_particle.hasValidTarget())
        {
            const Object* ptarget = objectWorld().getObjectHandler().get(_particle.getTargetID());
            if (ptarget == nullptr) return;

            // face your target
            _particle.facing = Facing(vec_to_facing(ptarget->getPosX() - tmp_pos.x(), ptarget->getPosY() - tmp_pos.y()));
        }
    }

    _particle.setPosition(tmp_pos);
}

void ParticlePhysics::updateAttached()
{
    Ego::prt_environment_t *penviro = &(_particle.enviro);

    // if the particle is not still in "display mode" there is no point in going on
    if (_particle.isTerminated()) return;

    // handle floor collision
    if (_particle.getPosition().z() < penviro->adj_level)
    {
        // Play the sound for hitting the floor [FSND]
        _particle.playSound(_particle.getProfile()->end_sound_floor);

        if(_particle.getProfile()->end_ground)
        {
            _particle.requestTerminate();
        }
    }

    // interaction with the mesh walls
    if (std::abs(_particle.getVelocity().x()) + std::abs(_particle.getVelocity().y()) > 0.0f)
    {
        if (EMPTY_BIT_FIELD != _particle.test_wall(_particle.getPosition()))
        {
            Vector2f nrm;
            float   pressure;

            // how is the character hitting the wall?
            BIT_FIELD hit_bits = _particle.hit_wall(_particle.getPosition(), nrm, &pressure);

		    // handle the collision
            if (0 != hit_bits)
            {
		        if(_particle.getProfile()->end_wall || _particle.getProfile()->end_bump)
		        {
		            _particle.requestTerminate();
		        }
            }
        }
    }
}

} //Physics
} //Ego
