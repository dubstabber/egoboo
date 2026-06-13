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

/// @file egolib/game/CharacterMatrix.c

#include "egolib/game/CharacterMatrix.h"
#include "egolib/game/Core/GameSessionContext.hpp"
#include "egolib/game/graphic_mad.h"
#include "egolib/game/renderer_3d.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/Module/Module.hpp"
#include "egolib/game/CharacterMatrix_internal.h"

namespace
{
auto& objectHandler()
{
    return GameSessionContext::get().activeModule().getObjectHandler();
}
}

static bool matrix_cache_needs_update(Object& object, matrix_cache_t& pmc);
static bool apply_matrix_cache( Object * pchr, matrix_cache_t& mc_tmp );
static bool chr_get_matrix_cache( Object * pchr, matrix_cache_t& mc_tmp );

static bool apply_one_character_matrix( Object * pchr, matrix_cache_t& mcache );
static bool apply_one_weapon_matrix( Object * pweap, matrix_cache_t& mcache );


//--------------------------------------------------------------------------------------------
bool matrix_cache_t::isValid() const {
    return valid && matrix_valid;
}

//--------------------------------------------------------------------------------------------
 bool matrix_cache_t::equal_to(const matrix_cache_t &rhs) const {

    // handle problems with pointers
    if (this == &rhs) {
        return true;
    }

    // handle one of both if the matrix caches being invalid
    if (!this->valid || !rhs.valid) {
        return false;
    }

    // handle differences in the type
    int itmp = this->type_bits - rhs.type_bits;
    if (0 != itmp) return false;

    //---- check for differences in the MAT_WEAPON data
    if (HAS_SOME_BITS(this->type_bits, MAT_WEAPON)) {
        itmp = (signed)REF_TO_INT(this->grip_chr.get()) - (signed)REF_TO_INT(rhs.grip_chr.get());
        if (0 != itmp) return false;

        itmp = (signed)this->grip_slot - (signed)rhs.grip_slot;
        if (0 != itmp) return false;

        for (int cnt = 0; cnt < GRIP_VERTS; cnt++) {
            itmp = (signed)this->grip_verts[cnt] - (signed)rhs.grip_verts[cnt];
            if (0 != itmp) return false;
        }

        // handle differences in the scale of our mount
        for (int cnt = 0; cnt < 3; cnt++) {
            float ftmp = this->grip_scale[cnt] - rhs.grip_scale[cnt];
            if (0.0f != ftmp) { return false; }
        }
    }

    //---- check for differences in the MAT_CHARACTER data
    if (HAS_SOME_BITS(this->type_bits, MAT_CHARACTER)) {
        // handle differences in the "Euler" rotation angles in 16-bit form
        for (int cnt = 0; cnt < 3; cnt++) {
            Facing ftmp = this->rotate[cnt] - rhs.rotate[cnt];
            if (Facing(0) != ftmp) { return false; }
        }

        // handle differences in the translate vector
        for (int cnt = 0; cnt < 3; cnt++) {
            float ftmp = this->pos[cnt] - rhs.pos[cnt];
            if (0.0f != ftmp) { return false; }
        }
    }

    //---- check for differences in the shared data
    if (HAS_SOME_BITS(this->type_bits, MAT_WEAPON) || HAS_SOME_BITS(this->type_bits, MAT_CHARACTER)) {
        // handle differences in our own scale
        for (int cnt = 0; cnt < 3; cnt++) {
            float ftmp = this->self_scale[cnt] - rhs.self_scale[cnt];
            if (0.0f != ftmp) { return false; }
        }
    }

    // if it got here, the data is all the same
    return true;
}

//--------------------------------------------------------------------------------------------
/// @brief Determine wether the object has a valid matrix cache.
/// @param pobj a pointer to the object
/// @return @a false if a @a pobj is a null pointer or if the object's matrix cache is invalid,
/// @a true otherwise
bool chr_matrix_valid(const Object *pobj) {
    /// @author BB
    /// @details Determine whether the character has a valid matrix

    if (!pobj) return false;
    return pobj->getGraphics().getMatrixCache().isValid();
}

//--------------------------------------------------------------------------------------------
bool chr_get_matrix_cache( Object * pchr, matrix_cache_t& mc_tmp )
{
    /// @author BB
    /// @details grab the matrix cache data for a given character and put it into mc_tmp.
    if ( nullptr == pchr ) return false;
    auto ichr = GET_INDEX_PCHR( pchr );

    bool handled = false;
    auto itarget = ObjectRef::Invalid;

    // initialize xome parameters in case we fail
    mc_tmp.valid     = false;
    mc_tmp.type_bits = MAT_UNKNOWN;

    mc_tmp.self_scale = Ego::Vector3f(pchr->getFat(), pchr->getFat(), pchr->getFat());

    // handle the overlay first of all
    if ( !handled && pchr->isOverlay() && ichr != pchr->getAITarget() && objectHandler().exists( pchr->getAITarget() ) )
    {
        // this will pretty much fail the cmp_matrix_cache() every time...

        Object * ptarget = objectHandler().get( pchr->getAITarget() );

        // make sure we have the latst info from the target
        chr_update_matrix(*ptarget, true);

        // grab the matrix cache into from the character we are overlaying
        mc_tmp = ptarget->getGraphics().getMatrixCache();

        // just in case the overlay's matrix cannot be corrected
        // then treat it as if it is not an overlay
        handled = mc_tmp.valid;
    }

    // this will happen if the overlay "failed" or for any non-overlay character
    if ( !handled )
    {
        // assume that the "target" of the MAT_CHARACTER data will be the character itself
        itarget = GET_INDEX_PCHR( pchr );

        //---- update the MAT_WEAPON data
        if ( objectHandler().exists( pchr->getHolderRef() ) )
        {
            Object * pmount = objectHandler().get( pchr->getHolderRef() );

            // make sure we have the latst info from the target
            chr_update_matrix(*pmount, true);

            // just in case the mounts's matrix cannot be corrected
            // then treat it as if it is not mounted... yuck
            if ( pmount->getGraphics().hasValidMatrixValue() )
            {
                mc_tmp.valid     = true;
                SET_BIT( mc_tmp.type_bits, MAT_WEAPON );        // add in the weapon data

                mc_tmp.grip_chr  = pchr->getHolderRef();
                mc_tmp.grip_slot = pchr->getAttachmentSlot();
                get_grip_verts( mc_tmp.grip_verts.data(), pchr->getHolderRef(), slot_to_grip_offset( pchr->getAttachmentSlot() ) );

                itarget = pchr->getHolderRef();
            }
        }

        //---- update the MAT_CHARACTER data
        if ( objectHandler().exists( itarget ) )
        {
            Object * ptarget = objectHandler().get( itarget );

            mc_tmp.valid   = true;
            SET_BIT( mc_tmp.type_bits, MAT_CHARACTER );  // add in the MAT_CHARACTER-type data for the object we are "connected to"

            mc_tmp.rotate[kX] = ptarget->getMapTwistFacingX() - orientation_t::MAP_TURN_OFFSET;
            mc_tmp.rotate[kY] = ptarget->getMapTwistFacingY() - orientation_t::MAP_TURN_OFFSET;
            mc_tmp.rotate[kZ] = ptarget->getFacingZ();

            mc_tmp.pos = ptarget->getPosition();

            mc_tmp.grip_scale = Ego::Vector3f(ptarget->getFat(), ptarget->getFat(), ptarget->getFat());
        }
    }

    return mc_tmp.valid;
}

//--------------------------------------------------------------------------------------------
bool apply_one_weapon_matrix( Object * pweap, matrix_cache_t& mc_tmp )
{
    /// @author ZZ
    /// @details Request that the data in the matrix cache be used to create a "character matrix".
    ///               i.e. a matrix that is not being held by anything.

	Ego::Vector4f  nupoint[GRIP_VERTS];
    int       iweap_points;

    if ( !mc_tmp.valid || 0 == ( MAT_WEAPON & mc_tmp.type_bits ) ) return false;

    if ( nullptr == pweap ) return false;
    matrix_cache_t pweap_mcache = pweap->getGraphics().getMatrixCache();

    if ( !objectHandler().exists( mc_tmp.grip_chr ) ) return false;

    // make sure that the matrix is invalid incase of an error
    pweap_mcache.matrix_valid = false;
    pweap->getGraphics().setMatrixCache(pweap_mcache);

    // grab the grip points in world coordinates
    iweap_points = convert_grip_to_global_points( mc_tmp.grip_chr, mc_tmp.grip_verts.data(), nupoint );

    if ( 4 == iweap_points )
    {
        // Calculate weapon's matrix based on positions of grip points
        // chrscale is recomputed at time of attachment
        pweap->getGraphics().setMatrix(Utilities::fromFourPoints(Ego::Vector3f(nupoint[0][kX], nupoint[0][kY], nupoint[0][kZ]),
                                                   Ego::Vector3f(nupoint[1][kX], nupoint[1][kY], nupoint[1][kZ]),
                                                   Ego::Vector3f(nupoint[2][kX], nupoint[2][kY], nupoint[2][kZ]),
                                                   Ego::Vector3f(nupoint[3][kX], nupoint[3][kY], nupoint[3][kZ]),
                                                   mc_tmp.self_scale[kZ]));

        // update the weapon position
        pweap->setPosition(Ego::Vector3f(nupoint[3][kX],nupoint[3][kY],nupoint[3][kZ]));
        pweap->getGraphics().setMatrixCache(mc_tmp);
        pweap->getGraphics().setMatrixValueValid(true);
    }
    else if ( iweap_points > 0 )
    {
        // cannot find enough vertices. punt.
        // ignore the shape of the grip and just stick the character to the single mount point

        // update the character position
        pweap->setPosition(Ego::Vector3f(nupoint[0][kX],nupoint[0][kY],nupoint[0][kZ]));

        // make sure we have the right data
        chr_get_matrix_cache( pweap, mc_tmp );

        // add in the appropriate mods
        // this is a hybrid character and weapon matrix
        SET_BIT( mc_tmp.type_bits, MAT_CHARACTER );

        // treat it like a normal character matrix
        apply_one_character_matrix( pweap, mc_tmp );
    }

    return pweap->getGraphics().hasValidMatrixValue();
}

//--------------------------------------------------------------------------------------------
bool apply_one_character_matrix( Object * pchr, matrix_cache_t& mc_tmp )
{
    /// @author ZZ
    /// @details Request that the matrix cache data be used to create a "weapon matrix".
    ///               i.e. a matrix that is attached to a specific grip.

    // only apply character matrices using this function
    if ( 0 == ( MAT_CHARACTER & mc_tmp.type_bits ) ) return false;

    if ( nullptr == pchr ) return false;

    pchr->getGraphics().setMatrixValueValid(false);

    if ( pchr->getProfile()->hasStickyButt() )
    {
        pchr->getGraphics().setMatrix(
        mat_ScaleXYZ_RotateXYZ_TranslateXYZ_SpaceFixed(
            mc_tmp.self_scale,
            mc_tmp.rotate[kZ],
            mc_tmp.rotate[kX],
            mc_tmp.rotate[kY],
            mc_tmp.pos));
    }
    else
    {
        pchr->getGraphics().setMatrix(
        mat_ScaleXYZ_RotateXYZ_TranslateXYZ_BodyFixed(
            mc_tmp.self_scale,
            mc_tmp.rotate[kZ],
            mc_tmp.rotate[kX],
            mc_tmp.rotate[kY],
            mc_tmp.pos));
    }

    pchr->getGraphics().setMatrixCache(mc_tmp);
    pchr->getGraphics().setMatrixValueValid(true);

    return true;
}

//--------------------------------------------------------------------------------------------
bool apply_matrix_cache( Object * pchr, matrix_cache_t& mc_tmp )
{
    /// @author BB
    /// @details request that the info in the matrix cache mc_tmp, be used to
    ///               make a matrix for the character pchr.

    bool applied = false;

    if ( nullptr == pchr ) return false;
    if ( !mc_tmp.valid ) return false;

    if ( 0 != ( MAT_WEAPON & mc_tmp.type_bits ) )
    {
        if ( objectHandler().exists( mc_tmp.grip_chr ) )
        {
            applied = apply_one_weapon_matrix( pchr, mc_tmp );
        }
        else
        {
            // !!!the mc_tmp was mis-labeled as a MAT_WEAPON!!!
            make_one_character_matrix(pchr->getObjRef());

            // recover the matrix_cache values from the character
            matrix_cache_t mcache = pchr->getGraphics().getMatrixCache();
            SET_BIT( mcache.type_bits, MAT_CHARACTER );
            if ( mcache.matrix_valid )
            {
                mcache.valid     = true;
                mcache.type_bits = MAT_CHARACTER;

                mcache.self_scale = Ego::Vector3f(pchr->getFat(), pchr->getFat(), pchr->getFat());

                mcache.grip_scale = mcache.self_scale;

                mcache.rotate[kX] = pchr->getMapTwistFacingX() - orientation_t::MAP_TURN_OFFSET;
                mcache.rotate[kY] = pchr->getMapTwistFacingY() - orientation_t::MAP_TURN_OFFSET;
                mcache.rotate[kZ] = pchr->getFacingZ();

                mcache.pos = pchr->getPosition();

                pchr->getGraphics().setMatrixCache(mcache);
                applied = true;
            }
        }
    }
    else if ( 0 != ( MAT_CHARACTER & mc_tmp.type_bits ) )
    {
        applied = apply_one_character_matrix( pchr, mc_tmp );
    }

    return applied;
}

//--------------------------------------------------------------------------------------------
bool matrix_cache_needs_update(Object& object, matrix_cache_t& pmc)
{
    /// @author BB
    /// @details determine whether a matrix cache has become invalid and needs to be updated

    // get the matrix data that is supposed to be used to make the matrix
    chr_get_matrix_cache(&object, pmc);

    // compare that data to the actual data used to make the matrix
    return !(pmc == object.getGraphics().getMatrixCache());
}

//--------------------------------------------------------------------------------------------
bool chr_update_matrix(Object& object, bool update_size)
{
    /// @author BB
    /// @details Do everything necessary to set the current matrix for this character.
    ///     This might include recursively going down the list of this character's mounts, etc.
    ///
    ///     Return true if a new matrix is applied to the character, false otherwise.

    bool         needs_update = false;

    // recursively make sure that any mount matrices are updated
    const ObjectRef holderRef = object.getHolderRef();
    if (objectHandler().exists(holderRef))
    {
        if (chr_update_matrix(*objectHandler().get(holderRef), true))
        {
            // the holder/mount matrix has changed.
            // this matrix is no longer valid.
            object.getGraphics().setMatrixValueValid(false);
        }
    }

    // does the matrix cache need an update at all?
    matrix_cache_t mc_tmp;
    needs_update = matrix_cache_needs_update(object, mc_tmp);

    // Update the grip vertices no matter what (if they are used)
    const std::shared_ptr<Object> &heldItem = objectHandler()[mc_tmp.grip_chr];
    if ( HAS_SOME_BITS(mc_tmp.type_bits, MAT_WEAPON) && heldItem)
    {
        // has that character changes its animation?
        if(heldItem->getGraphics().updateGripVertices(mc_tmp.grip_verts.data(), GRIP_VERTS)) {
            needs_update = true;
        }
    }

    // if it is not the same, make a new matrix with the new data
    if (needs_update)
    {
        // we know the matrix is not valid
        object.getGraphics().setMatrixValueValid(false);

        if(apply_matrix_cache(&object, mc_tmp)) {
            if(update_size) {
                // call chr_update_collision_size() but pass in a false value to prevent a recursize call
                object.updateCollisionSize(false);
            }
            return true;
        }

    }

    return false;
}

//--------------------------------------------------------------------------------------------
bool chr_getMatUp(Object *object_ptr, Ego::Vector3f& up)
{
	if (!object_ptr) return false;

	if (!chr_matrix_valid(object_ptr))
	{
		chr_update_matrix(*object_ptr, true);
	}

	if (chr_matrix_valid(object_ptr))
	{
        up = mat_getChrUp(object_ptr->getMatrix());
	}
    else
	{
		// assume default Up is +z
		up[kZ] = 1.0f;
		up[kX] = up[kY] = 0.0f;
	}

	return true;
}

//--------------------------------------------------------------------------------------------
bool chr_getMatRight(Object *object_ptr, Ego::Vector3f& right)
{
	if (!object_ptr) return false;

	if (!chr_matrix_valid(object_ptr))
	{
		chr_update_matrix(*object_ptr, true);
	}

	if (chr_matrix_valid(object_ptr))
	{
        right = mat_getChrRight(object_ptr->getMatrix());
	}
    else
	{
		// assume default Right is +y
		right[kY] = 1.0f;
		right[kX] = right[kZ] = 0.0f;
	}

	return true;
}

//--------------------------------------------------------------------------------------------
bool chr_getMatForward(Object *object_ptr, Ego::Vector3f& forward)
{
	if (!object_ptr) return false;

	if (!chr_matrix_valid(object_ptr))
	{
		chr_update_matrix(*object_ptr, true);
	}

	if (chr_matrix_valid(object_ptr))
	{
        forward = mat_getChrForward(object_ptr->getMatrix());
	}
    else
	{
		// assume default Forward is +x
		forward[kX] = 1.0f;
		forward[kY] = forward[kZ] = 0.0f;
	}

	return true;
}

//--------------------------------------------------------------------------------------------
bool chr_getMatTranslate(Object *object_ptr, Ego::Vector3f& translate)
{
	if (!object_ptr) return false;

	if (!chr_matrix_valid(object_ptr))
	{
		chr_update_matrix(*object_ptr, true);
	}

	if (chr_matrix_valid(object_ptr))
	{
        translate = mat_getTranslate(object_ptr->getMatrix());
	}
    else
	{
		translate = object_ptr->getPosition();
	}

	return true;
}

//--------------------------------------------------------------------------------------------
void make_one_character_matrix( const ObjectRef ichr )
{
    /// @author ZZ
    /// @details This function sets one character's matrix

    if ( !objectHandler().exists( ichr ) ) return;
    Object * pchr = objectHandler().get( ichr );

    // invalidate this matrix
    pchr->getGraphics().setMatrixValueValid(false);

    if ( pchr->isOverlay() )
    {
        // This character is an overlay and its ai.target points to the object it is overlaying
        // Overlays are kept with their target...
        if ( objectHandler().exists( pchr->getAITarget() ) )
        {
            Object * ptarget = objectHandler().get( pchr->getAITarget() );

            pchr->setPosition(ptarget->getPosition());

            // copy the matrix
            pchr->getGraphics().setMatrix(ptarget->getMatrix());

            // copy the matrix data
            pchr->getGraphics().setMatrixCache(ptarget->getGraphics().getMatrixCache());
        }
    }
    else
    {
        if ( pchr->getProfile()->hasStickyButt() )
        {
            pchr->getGraphics().setMatrix(
                mat_ScaleXYZ_RotateXYZ_TranslateXYZ_SpaceFixed(
    				Ego::Vector3f(pchr->getFat(), pchr->getFat(), pchr->getFat()),
                    pchr->getFacingZ(),
                    pchr->getMapTwistFacingX() - orientation_t::MAP_TURN_OFFSET,
                    pchr->getMapTwistFacingY() - orientation_t::MAP_TURN_OFFSET,
                    pchr->getPosition())
            );
        }
        else
        {
            pchr->getGraphics().setMatrix(
                mat_ScaleXYZ_RotateXYZ_TranslateXYZ_BodyFixed(
    				Ego::Vector3f(pchr->getFat(), pchr->getFat(), pchr->getFat()),
                    pchr->getFacingZ(),
                    pchr->getMapTwistFacingX() - orientation_t::MAP_TURN_OFFSET,
                    pchr->getMapTwistFacingY() - orientation_t::MAP_TURN_OFFSET,
                    pchr->getPosition())
            );
        }

        matrix_cache_t cache = pchr->getGraphics().getMatrixCache();
        cache.valid        = true;
        cache.matrix_valid = true;
        cache.type_bits    = MAT_CHARACTER;
        cache.self_scale   = Ego::Vector3f(pchr->getFat(), pchr->getFat(), pchr->getFat());
        cache.rotate[kX]   = pchr->getMapTwistFacingX() - orientation_t::MAP_TURN_OFFSET;
        cache.rotate[kY]   = pchr->getMapTwistFacingY() - orientation_t::MAP_TURN_OFFSET;
        cache.rotate[kZ]   = pchr->getFacingZ();
        cache.pos          = pchr->getPosition();
        pchr->getGraphics().setMatrixCache(cache);
    }
}
