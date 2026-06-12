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

/// @file egolib/game/Physics/ObjectPhysics_attachment.cpp
/// @brief grabStuff (quad-tree + Shop gate), attachToObject (grip/mount/team/animation),
///        and updateCollisionSize (vertex-cloud to oct_bb) for ObjectPhysics.
/// @details Split from ObjectPhysics.cpp to reduce TU size.
/// @author Johan Jansen aka Zefz
#include "egolib/game/Physics/ObjectPhysics_internal.h"
#include "egolib/game/Shop.hpp"
#include "egolib/game/CharacterMatrix.h"
#include "egolib/FileFormats/map_file.h"

namespace Ego
{
namespace Physics
{

bool ObjectPhysics::grabStuff(grip_offset_t grip_off, bool grab_people)
{
    //Max search distance in quad tree relative to object position
    static constexpr float MAX_SEARCH_DIST = 3.0f * Info<float>::Grid::Size();

    //Max grab distance is 2/3rds of a tile
    static constexpr float MAX_DIST_GRAB = Info<float>::Grid::Size() * 0.66f;

    // find the slot from the grip
    slot_t slot = grip_offset_to_slot( grip_off );
    if ( slot >= SLOT_COUNT ) return false;

    // Make sure the character doesn't have something already, and that it has hands
    if (objectWorld().getObjectHandler().exists( _object.getHeldObject(slot) ) || !_object.getProfile()->isSlotValid(slot)) {
        return false;
    }

    //Determine the position of the grip
    oct_vec_v2_t mids = _object.getSlotCollisionVolume(slot).getMid();

    Vector3f   slot_pos = Vector3f(mids[OCT_X], mids[OCT_Y], mids[OCT_Z]) + _object.getPosition();

    //The object that we grab
    Object* bestMatch = nullptr;
    float bestMatchDistance = std::numeric_limits<float>::max();

    // Go through all nearby objects to find the best match
    std::vector<ObjectRef> nearbyObjectRefs;
    objectWorld().getObjectHandler().findObjectRefs(slot_pos.x(), slot_pos.y(), MAX_SEARCH_DIST, nearbyObjectRefs, false);
    for (const ObjectRef& objectRef : nearbyObjectRefs)
    {
        Object* pchr_c = objectWorld().getObjectHandler().get(objectRef);
        if (pchr_c == nullptr) {
            continue;
        }

        //Skip invalid objects
        if(pchr_c->isTerminated()) {
            continue;
        }

        // do nothing to yourself
        if (_object.getObjRef() == pchr_c->getObjRef()) continue;

        // Dont do hidden objects
        if (pchr_c->isHidden()) continue;

        // disarm and pickpocket not allowed yet
        if (pchr_c->isBeingHeld()) continue;

        // do not pick up your mount
        if ( pchr_c->getHeldObject(SLOT_LEFT) == _object.getObjRef() ||
             pchr_c->getHeldObject(SLOT_RIGHT) == _object.getObjRef() ) continue;

        // do not notice completely broken items?
        if (pchr_c->isItem() && !pchr_c->isAlive()) continue;

        // reasonable carrying capacity
        if (pchr_c->phys.weight > _object.phys.weight + FLOAT_TO_FP8(_object.getAttribute(Ego::Attribute::MIGHT)) * idlib::fraction<float, 1, 255>()) {
            continue;
        }

        // grab_people == true allows you to pick up living non-items
        // grab_people == false allows you to pick up living (functioning) items
        if (!grab_people && !pchr_c->isItem()) {
            continue;
        }

        // calculate the distance
        const float horizontalDistance = idlib::euclidean_norm(pchr_c->getPosition() - slot_pos);
        const float verticalDistance = std::sqrt(idlib::sq(_object.getPosZ() - pchr_c->getPosZ()));

        //Figure out if the character is looking towards the object
        const bool isFacingObject = _object.isFacingLocation(pchr_c->getPosX(), pchr_c->getPosY());

        // Is it too far away to interact with?
        if (horizontalDistance > MAX_SEARCH_DIST || verticalDistance > MAX_SEARCH_DIST) {
            continue;
        }

        // visibility affects the max grab distance.
        // if it is not visible then we have to be touching it.
        float maxHorizontalGrabDistance = MAX_DIST_GRAB;

        //Halve grab distance for items behind us
        if(!isFacingObject && !grab_people) {
            maxHorizontalGrabDistance *= 0.5f;
        }

        //Bigger characters have bigger grab size
        maxHorizontalGrabDistance += _object.getCurrentBump().size / 4.0f;

        //Double grab distance for monsters that are trying to grapple
        if(grab_people) {
            maxHorizontalGrabDistance *= 2.0f;
        }

        // is it too far away to grab?
        if (horizontalDistance > maxHorizontalGrabDistance + _object.getCurrentBump().size / 4.0f &&
            horizontalDistance > _object.getCurrentBump().size) {
            continue;
        }

        //Check vertical distance as well
        else
        {
            float maxVerticalGrabDistance = _object.getCurrentBump().height / 2.0f;

            if(grab_people) {
                //This allows very flat creatures like the Carpet Mimics grab people
                maxVerticalGrabDistance = std::max(maxVerticalGrabDistance, MAX_DIST_GRAB);
            }

            if (verticalDistance > maxVerticalGrabDistance) {
                continue;
            }
        }

        //Is this one better to grab than any previous matches?
        if(horizontalDistance < bestMatchDistance) {
            bestMatchDistance = horizontalDistance;
            bestMatch = pchr_c;

            //Prioritize items in front of us over those behind us
            if(!isFacingObject) {
                bestMatchDistance *= 2.0f;
            }
        }
    }

    if(bestMatch != nullptr) {
        const std::shared_ptr<Object> &grabber = objectWorld().getObjectHandler()[_object.getObjRef()];
        if (Shop::canGrabItem(grabber, bestMatch->shared_from_this()))
        {
            // Stick 'em together and quit
            if(bestMatch->attachToObject(grabber->getObjRef(), grip_off))
            {
                if (grab_people)
                {
                    // Start the slam animation...  ( Be sure to drop!!! )
                    _object.getGraphics().playAction(static_cast<ModelAction>(ACTION_MC + slot), false);
                }
            }
            return true;
        }
        else
        {
            // Lift the item a little and quit...
            bestMatch->setVelocity({bestMatch->getVelocity().x(),
                                    bestMatch->getVelocity().y(),
                                    Object::DROPZVEL});
            bestMatch->setHitReady(true);
            scriptable(*bestMatch).addAIAlertBits(ALERTIF_DROPPED);
        }
    }

    return false;
}

bool ObjectPhysics::attachToObject(ObjectRef holderRef, grip_offset_t grip_off)
{
    const std::shared_ptr<Object>& holder = objectByRef(holderRef);
    if (!holder) {
        return false;
    }

    /// @author ZZ
    /// @details This function attaches one object (rider) to another object (mount)
    ///          at a certain vertex offset ( grip_off )
    ///   - This function is called as a part of spawning a module, so the rider or the mount may not
    ///     be fully instantiated
    ///   - This function should do very little testing to see if attachment is allowed.
    ///     Most of that checking should be done by the calling function

    //Don't attach a character to itself!
    if (_object.getObjRef() == holder->getObjRef()) {
        return false;
    }

    // do not deal with packed items at this time
    // this would have to be changed to allow for pickpocketing
    if (_object.isBeingHeld()) {
       return false;
    }

    // This is a small fix that allows special grabbable mounts not to be mountable while
    // held by another character (such as the magic carpet for example)
    if(holder->isBeingHeld()) {
        return false;
    }

    // make a reasonable time for the character to remount something
    // for characters jumping out of pots, etc
    if (holder->getObjRef() == _object.getDismountObject() && _object.getDismountTimer() > 0) {
        return false;
    }

    // Figure out which slot this grip_off relates to
    slot_t slot = grip_offset_to_slot(grip_off);

    // Make sure the the slot is valid
    if (!holder->getProfile()->isSlotValid(slot)) {
        return false;
    }

    //Make sure it is no longer attach to a platform
    detachFromPlatform();

    // Put 'em together
    _object.setAttachmentSlot(slot);
    _object.setHolderRef(holder->getObjRef());
    holder->setHeldObject(slot, _object.getObjRef());

    // set the grip vertices for the irider
    set_weapongrip(_object.getObjRef(), holder->getObjRef(), grip_off);

    chr_update_matrix(_object, true);

    _object.setPosition(mat_getTranslate(_object.getMatrix()));

    _object.setInWater(false);
    _object.setJumpTimer(Object::JUMPDELAY * 4);

    // Run the held animation
    if (holder->isMount() && (GRIP_ONLY == grip_off))
    {
        // Riding imount
        if (objectByRef(_object.getHeldObject(SLOT_LEFT)) || objectByRef(_object.getHeldObject(SLOT_RIGHT)))
        {
            // if the character is holding anything, make the animation
            // ACTION_MH == "sitting" so that it does not look so silly
            _object.getGraphics().playAction(ACTION_MH, true);
        }
        else
        {
            // if it is not holding anything, go for the riding animation
            _object.getGraphics().playAction(ACTION_MI, true);
        }

        // set this action to loop
        _object.getGraphics().setActionLooped(true);
    }
    else if (_object.isAlive())
    {
        _object.getGraphics().playAction(static_cast<ModelAction>(ACTION_MM + slot), false);

        /// @note ZF@> hmm, here is the torch holding bug. Removing
        /// the interpolation seems to fix it...
        //_object.inst.removeInterpolation();
        /// @note ZF@> Reverted this hack, no longer needed? (01.09.2016)

        // set the action to keep for items
        if (_object.isItem()) {
            _object.setActionKeep(true);
        }
    }

    // Set the team
    if (_object.isItem())
    {
        _object.setTeamRef(holder->getTeamRef());

        // Set the alert
        if (_object.isAlive()) {
            scriptable(_object).addAIAlertBits(ALERTIF_GRABBED);
        }

        // Lore Master perk identifies everything
        if (holder->hasPerk(Ego::Perks::LORE_MASTER)) {
            _object.getProfile()->makeUsageKnown();
            _object.setNameKnown(true);
            _object.setAmmoKnown(true);
        }
    }

    if (holder->isMount())
    {
        holder->setTeamRef(_object.getTeamRef());

        // Set the alert
        if (!holder->isItem() && holder->isAlive())
        {
            scriptable(*holder).addAIAlertBits(ALERTIF_GRABBED);
        }
    }

    // It's not gonna hit the floor
    _object.setHitReady(false);

    return true;
}

void ObjectPhysics::updateCollisionSize(bool update_matrix)
{
    oct_bb_t minCollisionVolume;
    oct_bb_t maxCollisionVolume;
    std::array<oct_bb_t, SLOT_COUNT> slotCollisionVolumes;
    slotCollisionVolumes.fill(oct_bb_t());

    // make sure the matrix is updated properly
    if (update_matrix) {
        // call chr_update_matrix() but pass in a false value to prevent a recursive call
        chr_update_matrix(_object, false);
    }

    // convert the point cloud in the GLvertex array (_object.inst._vertexList) to
    // a level 1 bounding box. Subtract off the position of the character
    oct_bb_t bsrc = _object.getGraphics().getBoundingBox();

    Vector4f  src[16];  // for the upper and lower octagon points
    Vector4f  dst[16];  // for the upper and lower octagon points

    // convert the corners of the level 1 bounding box to a point cloud
    // keep track of the actual number of vertices, in case the object is square
    int vcount = oct_bb_t::to_points(bsrc, src, 16);

    // transform the new point cloud
    Utilities::transform(_object.getMatrix(), src, dst, vcount);

    // convert the new point cloud into a level 1 bounding box
    oct_bb_t bdst;
    oct_bb_t::points_to_oct_bb(bdst, dst, vcount);

    //---- set the bounding boxes
    minCollisionVolume = bdst;
    maxCollisionVolume = bdst;

    oct_bb_t bmin;
    bmin.assign(_object.getCurrentBump());

    // only use the current bump size if it was overridden in data.txt through the [MODL] expansion
    if ( _object.getProfile()->getBumpOverrideSize() )
    {
        minCollisionVolume.cut(bmin, OCT_X);
        minCollisionVolume.cut(bmin, OCT_Y);

        maxCollisionVolume.join(bmin, OCT_X);
        maxCollisionVolume.join(bmin, OCT_Y);
    }

    // only use the current bump size_big if it was overridden in data.txt through the [MODL] expansion
    if (_object.getProfile()->getBumpOverrideSizeBig()) {
        minCollisionVolume.cut(bmin, OCT_XY);
        minCollisionVolume.cut(bmin, OCT_YX);

        maxCollisionVolume.join(bmin, OCT_XY);
        maxCollisionVolume.join(bmin, OCT_YX);
    }

    // only use the current bump height if it was overridden in data.txt through the [MODL] expansion
    if (_object.getProfile()->getBumpOverrideHeight()) {
        minCollisionVolume.cut(bmin, OCT_Z);
        maxCollisionVolume.join(bmin, OCT_Z);
    }

    //// raise the upper bound for platforms
    //if ( _object.platform )
    //{
    //    _object.chr_max_cv.maxs[OCT_Z] += PLATTOLERANCE;
    //}

    //This makes it easier to jump on top of mounts
    if(_object.isMount()) {
       maxCollisionVolume._maxs[OCT_Z] = Ego::Math::constrain<float>(maxCollisionVolume._maxs[OCT_Z] - MOUNTTOLERANCE,
                                                                     MOUNTTOLERANCE,
                                                                     MOUNTTOLERANCE * 3.0f);
       minCollisionVolume._maxs[OCT_Z] = Ego::Math::constrain<float>(minCollisionVolume._maxs[OCT_Z] - MOUNTTOLERANCE,
                                                                     MOUNTTOLERANCE,
                                                                     MOUNTTOLERANCE * 3.0f);
    }

    // calculate collision volumes for various slots
    for (size_t cnt = 0; cnt < SLOT_COUNT; cnt++)
    {
        if (!_object.getProfile()->isSlotValid(static_cast<slot_t>(cnt))) continue;

        oct_bb_t slotCollisionVolume;
        chr_calc_grip_cv(&_object, GRIP_LEFT, &slotCollisionVolume, false);
        slotCollisionVolumes[cnt] = slotCollisionVolume;

        maxCollisionVolume.join(slotCollisionVolume);
    }

    // convert the level 1 bounding box to a level 0 bounding box
    bumper_t looseBump;
    oct_bb_t::downgrade(bdst, _object.getInitialBump(), _object.getCurrentBump(), looseBump);
    _object.setLooseBump(looseBump);
    _object.setCollisionVolumes(minCollisionVolume, maxCollisionVolume, slotCollisionVolumes);

    //Recalculate the fast 2D collision box
    _aabb2D = AxisAlignedBox2f(Point2f(_object.getPosX() + minCollisionVolume.getMin()[OCT_X],
                               _object.getPosY() + minCollisionVolume.getMin()[OCT_Y]),
                               Point2f(_object.getPosX() + minCollisionVolume.getMax()[OCT_X],
                               _object.getPosY() + minCollisionVolume.getMax()[OCT_Y]));
}

} //Physics
} //Ego
