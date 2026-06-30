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

/// @file egolib/game/CharacterMatrix_grip.c
/// @brief Grip-vertex geometry helpers + weapon-grip / grip-collision-volume calculation,
///        split out of the character matrix-cache pipeline in CharacterMatrix.c

#include "egolib/game/CharacterMatrix.h"
#include "egolib/Entities/IObjectWorld.hpp"
#include "egolib/game/graphic_mad.h"
#include "egolib/game/renderer_3d.h"
#include "egolib/Entities/_Include.hpp"
#include "egolib/game/CharacterMatrix_internal.h"

namespace
{
auto& objectHandler()
{
    return Ego::Entities::activeObjectHandler();
}
}

static int convert_grip_to_local_points( Object * pholder, uint16_t grip_verts[], Ego::Vector4f   dst_point[] );


//--------------------------------------------------------------------------------------------
int get_grip_verts( uint16_t grip_verts[], const ObjectRef imount, int vrt_offset )
{
    /// @author BB
    /// @details Fill the grip_verts[] array from the mount's data.
    ///     Return the number of vertices found.

    if ( NULL == grip_verts ) return 0;

    // set all the vertices to a "safe" value
    for (size_t i = 0; i < GRIP_VERTS; i++ )
    {
        grip_verts[i] = 0xFFFF;
    }

    if ( !objectHandler().exists( imount ) ) return 0;
    Object *pmount = objectHandler().get( imount );

    if ( 0 == pmount->getVertexCount() ) return 0;

    //---- set the proper weapongrip vertices
    int tnc = ( int )pmount->getVertexCount() - ( int )vrt_offset;

    // if the starting vertex is less than 0, just take the first vertex
    if ( tnc < 0 )
    {
        grip_verts[0] = 0;
        return 1;
    }

    int vrt_count = 0;
    for (size_t i = 0; i < GRIP_VERTS; i++ )
    {
        if ( tnc + i < pmount->getVertexCount() )
        {
            grip_verts[i] = tnc + i;
            vrt_count++;
        }
        else
        {
            grip_verts[i] = 0xFFFF;
        }
    }

    return vrt_count;
}

//--------------------------------------------------------------------------------------------
int convert_grip_to_local_points( Object * pholder, uint16_t grip_verts[], Ego::Vector4f dst_point[] )
{
    /// @author ZZ
    /// @details a helper function for apply_one_weapon_matrix()

    int cnt, point_count;

    if ( NULL == grip_verts || NULL == dst_point ) return 0;

    if (!pholder || pholder->isTerminated()) return 0;

    // count the valid weapon connection dst_points
    point_count = 0;
    for ( cnt = 0; cnt < GRIP_VERTS; cnt++ )
    {
        if ( 0xFFFF != grip_verts[cnt] )
        {
            point_count++;
        }
    }

    // do the best we can
    if ( 0 == point_count )
    {
        // punt! attach to origin
        dst_point[0][kX] = pholder->getPosX();
        dst_point[0][kY] = pholder->getPosY();
        dst_point[0][kZ] = pholder->getPosZ();
        dst_point[0][kW] = 1;

        point_count = 1;
    }
    else
    {
        // update the grip vertices
        pholder->getGraphics().updateGripVertices(grip_verts, GRIP_VERTS);

        // copy the vertices into dst_point[]
        for ( point_count = 0, cnt = 0; cnt < GRIP_VERTS; cnt++, point_count++ )
        {
            uint16_t vertex = grip_verts[cnt];

            if ( 0xFFFF == vertex ) continue;

            dst_point[point_count][kX] = pholder->getVertex(vertex).pos[XX];
            dst_point[point_count][kY] = pholder->getVertex(vertex).pos[YY];
            dst_point[point_count][kZ] = pholder->getVertex(vertex).pos[ZZ];
            dst_point[point_count][kW] = 1.0f;
        }
    }

    return point_count;
}

//--------------------------------------------------------------------------------------------
int convert_grip_to_global_points( const ObjectRef iholder, uint16_t grip_verts[], Ego::Vector4f   dst_point[] )
{
    /// @author ZZ
    /// @details a helper function for apply_one_weapon_matrix()

	Ego::Vector4f  src_point[GRIP_VERTS];

    if ( !objectHandler().exists( iholder ) ) return 0;
    Object *pholder = objectHandler().get( iholder );

    // find the grip points in the character's "local" or "body-fixed" coordinates
    int point_count = convert_grip_to_local_points( pholder, grip_verts, src_point );

    if ( 0 == point_count ) return 0;

    // use the math function instead of rolling out own
	Utilities::transform(pholder->getMatrix(), src_point, dst_point, point_count);

    return point_count;
}

//--------------------------------------------------------------------------------------------
bool chr_calc_grip_cv( Object * pmount, int grip_offset, oct_bb_t * grip_cv_ptr, const bool shift_origin )
{
    /// @author BB
    /// @details use a standard size for the grip

    // take the character size from the adventurer model
    const float default_chr_height = 88.0f;
    const float default_chr_radius = 22.0f;

    int              cnt;

    oct_bb_t         tmp_cv;

    int      grip_count;
    uint16_t grip_verts[GRIP_VERTS];
	Ego::Vector4f grip_points[GRIP_VERTS];
	Ego::Vector4f grip_nupoints[GRIP_VERTS];
    bumper_t bmp;

	if (!pmount) {
		return false;
	}

    // tune the grip radius
    bmp.size     = default_chr_radius * pmount->getFat() * 0.75f;
    bmp.height   = default_chr_height * pmount->getFat() * 2.00f;
    bmp.size_big = bmp.size * idlib::sqrt_two<float>();

    tmp_cv.assign(bmp);

    // move the vertical bounding box down a little
    tmp_cv._mins[OCT_Z] -= bmp.height * 0.25f;
    tmp_cv._maxs[OCT_Z] -= bmp.height * 0.25f;

    // get appropriate vertices for this model's grip
    {
        // do the automatic vertex update
		int vert_stt = static_cast<int>(pmount->getVertexCount()) - static_cast<int>(grip_offset);
        if ( vert_stt < 0 ) return false;

		if (!pmount->getGraphics().updateVertices(vert_stt, vert_stt + grip_offset, false))
        {
            grip_count = 0;
            for ( cnt = 0; cnt < GRIP_VERTS; cnt++ )
            {
                grip_verts[cnt] = 0xFFFF;
            }
        }
        else
        {
            // calculate the grip vertices
			for (grip_count = 0, cnt = 0; cnt < GRIP_VERTS && (size_t)(vert_stt + cnt) < pmount->getVertexCount(); grip_count++, cnt++)
            {
                grip_verts[cnt] = vert_stt + cnt;
            }
            for ( /* nothing */ ; cnt < GRIP_VERTS; cnt++ )
            {
                grip_verts[cnt] = 0xFFFF;
            }
        }

        // calculate grip_origin and grip_up
        if ( 4 == grip_count )
        {
            // Calculate grip point locations with linear interpolation and other silly things
            convert_grip_to_local_points( pmount, grip_verts, grip_points );
        }
        else if ( grip_count > 0 )
        {
            // Calculate grip point locations with linear interpolation and other silly things
            convert_grip_to_local_points( pmount, grip_verts, grip_points );

            if ( grip_count < 2 )
            {
                grip_points[2] = idlib::zero<Ego::Vector4f>();
                grip_points[2][kY] = 1.0f;
            }

            if ( grip_count < 3 )
            {
                grip_points[3] = idlib::zero<Ego::Vector4f>();
                grip_points[3][kZ] = 1.0f;
            }
        }
        else if ( 0 == grip_count )
        {
            // choose the location point at the model's origin and axis aligned

            for ( cnt = 0; cnt < 4; cnt++ )
            {
                grip_points[cnt] = idlib::zero<Ego::Vector4f>();
            }

            grip_points[1][kX] = 1.0f;
            grip_points[2][kY] = 1.0f;
            grip_points[3][kZ] = 1.0f;
        }

        // fix the 4th component depending on the whether we shift the origin of the cv
        if ( !shift_origin )
        {
            for ( cnt = 0; cnt < grip_count; cnt++ )
            {
                grip_points[cnt][kW] = 0.0f;
            }
        }
    }

    // transform the vertices to calculate the grip_vecs[]
    // we only need one vertex
	Utilities::transform(pmount->getMatrix(),grip_points, grip_nupoints, 1);

    // add in the "origin" of the grip, if necessary
    if ( NULL != grip_cv_ptr )
    {
        *grip_cv_ptr = idlib::translate(tmp_cv, Ego::Vector3f(grip_nupoints[0][kX], grip_nupoints[0][kY], grip_nupoints[0][kZ]));
    }

    return true;
}

//--------------------------------------------------------------------------------------------
bool set_weapongrip( const ObjectRef iitem, const ObjectRef iholder, uint16_t vrt_off )
{
    uint16_t grip_verts[GRIP_VERTS];

    if ( !objectHandler().exists( iitem ) ) return false;
	Object *pitem = objectHandler().get( iitem );
	matrix_cache_t mcache = pitem->getGraphics().getMatrixCache();

    // is the item attached to this valid holder?
    if ( pitem->getHolderRef() != iholder ) return false;

    bool needs_update  = true;

    if ( GRIP_VERTS == get_grip_verts( grip_verts, iholder, vrt_off ) )
    {
        //---- detect any changes in the matrix_cache data

        needs_update  = false;

        if ( iholder != mcache.grip_chr || pitem->getHolderRef() != iholder )
        {
            needs_update  = true;
        }

        if ( pitem->getAttachmentSlot() != mcache.grip_slot )
        {
            needs_update  = true;
        }

        // check to see if any of the
        for (size_t i = 0; i < GRIP_VERTS; i++ )
        {
            if ( grip_verts[i] != mcache.grip_verts[i] )
            {
                needs_update = true;
                break;
            }
        }
    }

    if ( needs_update )
    {
        // cannot create the matrix, therefore the current matrix must be invalid
        mcache.matrix_valid = false;

        mcache.grip_chr  = iholder;
        mcache.grip_slot = pitem->getAttachmentSlot();

        for (size_t i = 0; i < GRIP_VERTS; i++ )
        {
            mcache.grip_verts[i] = grip_verts[i];
        }
    }

    if (needs_update)
    {
        pitem->getGraphics().setMatrixCache(mcache);
    }

    return true;
}
