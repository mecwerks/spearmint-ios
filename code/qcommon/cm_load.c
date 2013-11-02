/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// cmodel.c -- model loading

#include "cm_local.h"

#ifdef BSPC

#include "../bspc/l_qfiles.h"

void SetPlaneSignbits (cplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}
#endif //BSPC

// to allow boxes to be treated as brush models, we allocate
// some extra indexes along with those needed by the map
#define BOX_LEAF_BRUSHES	1
#define	BOX_BRUSHES		1
#define	BOX_SIDES		6
#define	BOX_LEAFS		2
#define	BOX_PLANES		12

#define	LL(x) x=LittleLong(x)


clipMap_t	cm;
int			c_pointcontents;
int			c_traces, c_brush_traces, c_patch_traces;


byte		*cmod_base;

#ifndef BSPC
cvar_t		*cm_noAreas;
cvar_t		*cm_noCurves;
cvar_t		*cm_playerCurveClip;
cvar_t		*cm_betterIbspSurfaceNums;
#endif

cmodel_t	box_model;
cplane_t	*box_planes;
cbrush_t	*box_brush;

int			capsule_contents;


void	CM_InitBoxHull (void);
void	CM_FloodAreaConnections (void);


/*
===============================================================================

					MAP LOADING

===============================================================================
*/

/*
=================
CMod_LoadShaders
=================
*/
void CMod_LoadShaders( lump_t *l ) {
	dshader_t	*in, *out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "CMod_LoadShaders: funny lump size");
	}
	count = l->filelen / sizeof(*in);

	if (count < 1) {
		Com_Error (ERR_DROP, "Map with no shaders");
	}
	cm.shaders = Hunk_Alloc( count * sizeof( *cm.shaders ), h_high );
	cm.numShaders = count;

	Com_Memcpy( cm.shaders, in, count * sizeof( *cm.shaders ) );

	if ( LittleLong( 1 ) != 1 ) {
		out = cm.shaders;
		for ( i=0 ; i<count ; i++, in++, out++ ) {
			out->contentFlags = LittleLong( out->contentFlags );
			out->surfaceFlags = LittleLong( out->surfaceFlags );
		}
	}
}


/*
=================
CMod_LoadSubmodels
=================
*/
void CMod_LoadSubmodels( lump_t *l ) {
	dmodel_t	*in;
	cmodel_t	*out;
	int			i, j, count;
	int			*indexes;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "CMod_LoadSubmodels: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no models");
	cm.cmodels = Hunk_Alloc( count * sizeof( *cm.cmodels ), h_high );
	cm.numSubModels = count;

	for ( i=0 ; i<count ; i++, in++)
	{
		out = &cm.cmodels[i];

		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
		}

		if ( i == 0 ) {
			continue;	// world model doesn't need other info
		}

		// make a "leaf" just to hold the model's brushes and surfaces
		out->leaf.numLeafBrushes = LittleLong( in->numBrushes );
		indexes = Hunk_Alloc( out->leaf.numLeafBrushes * 4, h_high );
		out->leaf.firstLeafBrush = indexes - cm.leafbrushes;
		for ( j = 0 ; j < out->leaf.numLeafBrushes ; j++ ) {
			indexes[j] = LittleLong( in->firstBrush ) + j;
		}

		out->leaf.numLeafSurfaces = LittleLong( in->numSurfaces );
		indexes = Hunk_Alloc( out->leaf.numLeafSurfaces * 4, h_high );
		out->leaf.firstLeafSurface = indexes - cm.leafsurfaces;
		for ( j = 0 ; j < out->leaf.numLeafSurfaces ; j++ ) {
			indexes[j] = LittleLong( in->firstSurface ) + j;
		}
	}
}


/*
=================
CMod_LoadNodes

=================
*/
void CMod_LoadNodes( lump_t *l ) {
	dnode_t		*in;
	int			child;
	cNode_t		*out;
	int			i, j, count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map has no nodes");
	cm.nodes = Hunk_Alloc( count * sizeof( *cm.nodes ), h_high );
	cm.numNodes = count;

	out = cm.nodes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->plane = cm.planes + LittleLong( in->planeNum );
		for (j=0 ; j<2 ; j++)
		{
			child = LittleLong (in->children[j]);
			out->children[j] = child;
		}
	}

}

/*
=================
CM_BoundBrush

=================
*/
void CM_BoundBrush( cbrush_t *b ) {
	b->bounds[0][0] = -b->sides[0].plane->dist;
	b->bounds[1][0] = b->sides[1].plane->dist;

	b->bounds[0][1] = -b->sides[2].plane->dist;
	b->bounds[1][1] = b->sides[3].plane->dist;

	b->bounds[0][2] = -b->sides[4].plane->dist;
	b->bounds[1][2] = b->sides[5].plane->dist;
}


/*
=================
CMod_LoadBrushes

=================
*/
void CMod_LoadBrushes( lump_t *l ) {
	dbrush_t	*in;
	cbrush_t	*out;
	int			i, count;

	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	}
	count = l->filelen / sizeof(*in);

	cm.brushes = Hunk_Alloc( ( BOX_BRUSHES + count ) * sizeof( *cm.brushes ), h_high );
	cm.numBrushes = count;

	out = cm.brushes;

	for ( i=0 ; i<count ; i++, out++, in++ ) {
		out->sides = cm.brushsides + LittleLong(in->firstSide);
		out->numsides = LittleLong(in->numSides);

		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushes: bad shaderNum: %i", out->shaderNum );
		}
		out->contents = cm.shaders[out->shaderNum].contentFlags;

		CM_BoundBrush( out );
	}

}

/*
=================
CMod_LoadLeafs
=================
*/
void CMod_LoadLeafs (lump_t *l)
{
	int			i;
	cLeaf_t		*out;
	dleaf_t 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no leafs");

	cm.leafs = Hunk_Alloc( ( BOX_LEAFS + count ) * sizeof( *cm.leafs ), h_high );
	cm.numLeafs = count;

	out = cm.leafs;	
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->cluster = LittleLong (in->cluster);
		out->area = LittleLong (in->area);
		out->firstLeafBrush = LittleLong (in->firstLeafBrush);
		out->numLeafBrushes = LittleLong (in->numLeafBrushes);
		out->firstLeafSurface = LittleLong (in->firstLeafSurface);
		out->numLeafSurfaces = LittleLong (in->numLeafSurfaces);

		if (out->cluster >= cm.numClusters)
			cm.numClusters = out->cluster + 1;
		if (out->area >= cm.numAreas)
			cm.numAreas = out->area + 1;
	}

	cm.areas = Hunk_Alloc( cm.numAreas * sizeof( *cm.areas ), h_high );
	cm.areaPortals = Hunk_Alloc( cm.numAreas * cm.numAreas * sizeof( *cm.areaPortals ), h_high );
}

/*
=================
CMod_LoadPlanes
=================
*/
void CMod_LoadPlanes (lump_t *l)
{
	int			i, j;
	cplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	if (count < 1)
		Com_Error (ERR_DROP, "Map with no planes");
	cm.planes = Hunk_Alloc( ( BOX_PLANES + count ) * sizeof( *cm.planes ), h_high );
	cm.numPlanes = count;

	out = cm.planes;	

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = PlaneTypeForNormal( out->normal );
		out->signbits = bits;
	}
}

/*
=================
CMod_LoadLeafBrushes
=================
*/
void CMod_LoadLeafBrushes (lump_t *l)
{
	int			i;
	int			*out;
	int		 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	cm.leafbrushes = Hunk_Alloc( (BOX_LEAF_BRUSHES + count) * sizeof( *cm.leafbrushes ), h_high );
	cm.numLeafBrushes = count;

	out = cm.leafbrushes;

	for ( i=0 ; i<count ; i++, in++, out++) {
		*out = LittleLong (*in);
	}
}

/*
=================
CMod_LoadLeafSurfaces
=================
*/
void CMod_LoadLeafSurfaces( lump_t *l )
{
	int			i;
	int			*out;
	int		 	*in;
	int			count;
	
	in = (void *)(cmod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	count = l->filelen / sizeof(*in);

	cm.leafsurfaces = Hunk_Alloc( count * sizeof( *cm.leafsurfaces ), h_high );
	cm.numLeafSurfaces = count;

	out = cm.leafsurfaces;

	for ( i=0 ; i<count ; i++, in++, out++) {
		*out = LittleLong (*in);
	}
}

/*
=================
CMod_LoadBrushSides
=================
*/
void CMod_LoadBrushSides (lump_t *l)
{
	int				i;
	cbrushside_t	*out;
	dbrushside_t 	*in;
	int				count;
	int				num;

	in = (void *)(cmod_base + l->fileofs);
	if ( l->filelen % sizeof(*in) ) {
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	}
	count = l->filelen / sizeof(*in);

	cm.brushsides = Hunk_Alloc( ( BOX_SIDES + count ) * sizeof( *cm.brushsides ), h_high );
	cm.numBrushSides = count;

	out = cm.brushsides;	

	for ( i=0 ; i<count ; i++, in++, out++) {
		num = LittleLong( in->planeNum );
		out->planeNum = num;
		out->plane = &cm.planes[num];
		out->shaderNum = LittleLong( in->shaderNum );
		if ( out->shaderNum < 0 || out->shaderNum >= cm.numShaders ) {
			Com_Error( ERR_DROP, "CMod_LoadBrushSides: bad shaderNum: %i", out->shaderNum );
		}
		out->surfaceFlags = cm.shaders[out->shaderNum].surfaceFlags;

		// this gets set to our best guess later using CMod_GetBestSurfaceNumForBrushSide
		out->surfaceNum = -1;
	}
}

/*
=================
CMod_GetBestSurfaceNumForBrushSide

Based on NetRadiant's GetBestSurfaceTriangleMatchForBrushside in tools/quake3/q3map2/convert_map.c
=================
*/
static int CMod_GetBestSurfaceNumForBrushSide( const cbrushside_t *buildSide, dsurface_t *bspDrawSurfaces, drawVert_t *bspDrawVerts, int *bspDrawIndexes ) {
#ifdef BSPC
	return -1;
#else
	const float		normalEpsilon = 0.00001f;
	const float		distanceEpsilon = 0.01f;
	dsurface_t		*s;
	int				i;
	int				t;
	vec_t			best = 0;
	vec_t			thisarea;
	vec3_t			normdiff;
	vec3_t			v1v0, v2v0, norm;
	drawVert_t		*vert[3];
	winding_t		*polygon;
	cplane_t		*buildPlane = &cm.planes[buildSide->planeNum];
	//dshader_t		*shaderInfo = &cm.shaders[buildSide->shaderNum];
	//int			matches = 0;
	int				bestSurfaceNum = -1;
	int				surfnum;

	// ZTM: NOTE: Unfortunately this takes longer than is acceptable on a lot of maps,
	//      and it's only needed for trap_R_SetSurfaceShader/trap_R_GetSurfaceShader.
	if ( !cm_betterIbspSurfaceNums->integer ) {
		return -1;
	}

	// brute force through all surfaces
	for ( s = bspDrawSurfaces, surfnum = 0; surfnum < cm.numSurfaces; ++s, ++surfnum )
	{
		if ( s->surfaceType != MST_PLANAR && s->surfaceType != MST_TRIANGLE_SOUP ) {
			continue;
		}
		if ( buildSide->shaderNum != s->shaderNum ) {
			continue;
		}
		for ( t = 0; t + 3 <= s->numIndexes; t += 3 )
		{
			vert[0] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 0]];
			vert[1] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 1]];
			vert[2] = &bspDrawVerts[s->firstVert + bspDrawIndexes[s->firstIndex + t + 2]];
			if ( s->surfaceType == MST_PLANAR && VectorCompare( vert[0]->normal, vert[1]->normal ) && VectorCompare( vert[1]->normal, vert[2]->normal ) ) {
				VectorSubtract( vert[0]->normal, buildPlane->normal, normdiff ); if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
				VectorSubtract( vert[1]->normal, buildPlane->normal, normdiff ); if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
				VectorSubtract( vert[2]->normal, buildPlane->normal, normdiff ); if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
			}
			else
			{
				// this is more prone to roundoff errors, but with embedded
				// models, there is no better way
				VectorSubtract( vert[1]->xyz, vert[0]->xyz, v1v0 );
				VectorSubtract( vert[2]->xyz, vert[0]->xyz, v2v0 );
				CrossProduct( v2v0, v1v0, norm );
				VectorNormalize( norm );
				VectorSubtract( norm, buildPlane->normal, normdiff ); if ( VectorLength( normdiff ) >= normalEpsilon ) {
					continue;
				}
			}
			if ( abs( DotProduct( vert[0]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			if ( abs( DotProduct( vert[1]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			if ( abs( DotProduct( vert[2]->xyz, buildPlane->normal ) - buildPlane->dist ) >= distanceEpsilon ) {
				continue;
			}
			// Okay. Correct surface type, correct shader, correct plane. Let's start with the business...
			polygon = CopyWinding( buildSide->winding );
			for ( i = 0; i < 3; ++i )
			{
				// 0: 1, 2
				// 1: 2, 0
				// 2; 0, 1
				vec3_t *v1 = &vert[( i + 1 ) % 3]->xyz;
				vec3_t *v2 = &vert[( i + 2 ) % 3]->xyz;
				vec3_t triNormal;
				vec_t triDist;
				vec3_t sideDirection;
				// we now need to generate triNormal and triDist so that they represent the plane spanned by normal and (v2 - v1).
				VectorSubtract( *v2, *v1, sideDirection );
				CrossProduct( sideDirection, buildPlane->normal, triNormal );
				triDist = DotProduct( *v1, triNormal );
				ChopWindingInPlace( &polygon, triNormal, triDist, distanceEpsilon );
				if ( !polygon ) {
					goto exwinding;
				}
			}
			thisarea = WindingArea( polygon );
			//if ( thisarea > 0 ) {
			//	++matches;
			//}
			if ( thisarea > best ) {
				best = thisarea;
				bestSurfaceNum = surfnum;
			}
			FreeWinding( polygon );
exwinding:
			;
		}
	}
	//if(Q_stricmpn(shaderInfo->shader, "textures/common/", 16) && Q_stricmp(shaderInfo->shader, "noshader"))
	//	Com_Printf("brushside with %s: %d matches (%f area)\n", shaderInfo->shader, matches, best);

	return bestSurfaceNum;
#endif
}

#define CM_EDGE_VERTEX_EPSILON 0.1f

/*
=================
CMod_BrushEdgesAreTheSame
=================
*/
static qboolean CMod_BrushEdgesAreTheSame( const vec3_t p0, const vec3_t p1,
		const vec3_t q0, const vec3_t q1 )
{
	if( VectorCompareEpsilon( p0, q0, CM_EDGE_VERTEX_EPSILON ) &&
			VectorCompareEpsilon( p1, q1, CM_EDGE_VERTEX_EPSILON ) )
		return qtrue;

	if( VectorCompareEpsilon( p1, q0, CM_EDGE_VERTEX_EPSILON ) &&
			VectorCompareEpsilon( p0, q1, CM_EDGE_VERTEX_EPSILON ) )
		return qtrue;

	return qfalse;
}

/*
=================
CMod_AddEdgeToBrush
=================
*/
static qboolean CMod_AddEdgeToBrush( const vec3_t p0, const vec3_t p1,
		cbrushedge_t *edges, int *numEdges )
{
	int i;

	if( !edges || !numEdges )
		return qfalse;

	for( i = 0; i < *numEdges; i++ )
	{
		if( CMod_BrushEdgesAreTheSame( p0, p1,
					edges[ i ].p0, edges[ i ].p1 ) )
			return qfalse;
	}

	VectorCopy( p0, edges[ *numEdges ].p0 );
	VectorCopy( p1, edges[ *numEdges ].p1 );
	(*numEdges)++;

	return qtrue;
}

/*
=================
CMod_CreateBrushSideWindings
=================
*/
static void CMod_CreateBrushSideWindings( lump_t *surfLump, lump_t *vertsLump, lump_t *indexLump )
{
	int						i, j, k;
	winding_t			*w;
	cbrushside_t	*side, *chopSide;
	cplane_t			*plane;
	cbrush_t			*brush;
	cbrushedge_t	*tempEdges;
	int						numEdges;
	int						edgesAlloc;
	int						totalEdgesAlloc = 0;
	int						totalEdges = 0;
	qboolean				isIBSP;
	dsurface_t				*dsurfaces;
	drawVert_t				*verts;
	int						*indexes;

	isIBSP = ( surfLump && vertsLump && indexLump );

	if ( isIBSP ) {
		dsurfaces = (void *)(cmod_base + surfLump->fileofs);
		if (surfLump->filelen % sizeof(*dsurfaces))
			Com_Error (ERR_DROP, "CMod_SetBrushSideSurfaceNums: funny lump size");

		verts = (void *)(cmod_base + vertsLump->fileofs);
		if (vertsLump->filelen % sizeof(*verts))
			Com_Error (ERR_DROP, "CMod_SetBrushSideSurfaceNums: funny lump size");

		indexes = (void *)(cmod_base + indexLump->fileofs);
		if (indexLump->filelen % sizeof(*indexes))
			Com_Error (ERR_DROP, "CMod_SetBrushSideSurfaceNums: funny lump size");
	}

	for( i = 0; i < cm.numBrushes; i++ )
	{
		brush = &cm.brushes[ i ];
		numEdges = 0;

		// walk the list of brush sides
		for( j = 0; j < brush->numsides; j++ )
		{
			// get side and plane
			side = &brush->sides[ j ];
			plane = side->plane;

			w = BaseWindingForPlane( plane->normal, plane->dist );

			// walk the list of brush sides
			for( k = 0; k < brush->numsides && w != NULL; k++ )
			{
				chopSide = &brush->sides[ k ];

				if( chopSide == side )
					continue;

				if( chopSide->planeNum == ( side->planeNum ^ 1 ) )
					continue;		// back side clipaway

				plane = &cm.planes[ chopSide->planeNum ^ 1 ];
				ChopWindingInPlace( &w, plane->normal, plane->dist, 0 );
			}

			if( w )
				numEdges += w->numpoints;

			// set side winding
			side->winding = w;

			if ( side->winding && isIBSP ) {
				side->surfaceNum = CMod_GetBestSurfaceNumForBrushSide( side, dsurfaces, verts, indexes );
			}
		}

		// Allocate a temporary buffer of the maximal size
		tempEdges = (cbrushedge_t *)Z_Malloc( sizeof( cbrushedge_t ) * numEdges );
		brush->numEdges = 0;

		// compose the points into edges
		for( j = 0; j < brush->numsides; j++ )
		{
			side = &brush->sides[ j ];

			if( side->winding )
			{
				for( k = 0; k < side->winding->numpoints - 1; k++ )
				{
					if( brush->numEdges == numEdges )
						Com_Error( ERR_FATAL,
								"Insufficient memory allocated for collision map edges" );

					CMod_AddEdgeToBrush( side->winding->p[ k ],
							side->winding->p[ k + 1 ], tempEdges, &brush->numEdges );
				}

				FreeWinding( side->winding );
				side->winding = NULL;
			}
		}

		// Allocate a buffer of the actual size
		edgesAlloc = sizeof( cbrushedge_t ) * brush->numEdges;
		totalEdgesAlloc += edgesAlloc;
		brush->edges = (cbrushedge_t *)Hunk_Alloc( edgesAlloc, h_low );

		// Copy temporary buffer to permanent buffer
		Com_Memcpy( brush->edges, tempEdges, edgesAlloc );

		// Free temporary buffer
		Z_Free( tempEdges );

		totalEdges += brush->numEdges;
	}

	Com_DPrintf( "Allocated %d bytes for %d collision map edges...\n",
			totalEdgesAlloc, totalEdges );
}

/*
=================
CMod_LoadEntityString
=================
*/
void CMod_LoadEntityString( lump_t *l ) {
	cm.entityString = Hunk_Alloc( l->filelen, h_high );
	cm.numEntityChars = l->filelen;
	Com_Memcpy (cm.entityString, cmod_base + l->fileofs, l->filelen);
}

/*
=================
CMod_LoadVisibility
=================
*/
#define	VIS_HEADER	8
void CMod_LoadVisibility( lump_t *l ) {
	int		len;
	byte	*buf;

    len = l->filelen;
	if ( !len ) {
		cm.clusterBytes = ( cm.numClusters + 31 ) & ~31;
		cm.visibility = Hunk_Alloc( cm.clusterBytes, h_high );
		Com_Memset( cm.visibility, 255, cm.clusterBytes );
		return;
	}
	buf = cmod_base + l->fileofs;

	cm.vised = qtrue;
	cm.visibility = Hunk_Alloc( len, h_high );
	cm.numClusters = LittleLong( ((int *)buf)[0] );
	cm.clusterBytes = LittleLong( ((int *)buf)[1] );
	Com_Memcpy (cm.visibility, buf + VIS_HEADER, len - VIS_HEADER );
}

//==================================================================


/*
=================
CMod_LoadPatches
=================
*/
#define	MAX_PATCH_VERTS		1024
void CMod_LoadPatches( lump_t *surfs, lump_t *verts ) {
	drawVert_t	*dv, *dv_p;
	dsurface_t	*in;
	int			count;
	int			i, j;
	int			c;
	cPatch_t	*patch;
	vec3_t		points[MAX_PATCH_VERTS];
	int			width, height;
	int			shaderNum;

	in = (void *)(cmod_base + surfs->fileofs);
	if (surfs->filelen % sizeof(*in))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");
	cm.numSurfaces = count = surfs->filelen / sizeof(*in);
	cm.surfaces = Hunk_Alloc( cm.numSurfaces * sizeof( cm.surfaces[0] ), h_high );

	dv = (void *)(cmod_base + verts->fileofs);
	if (verts->filelen % sizeof(*dv))
		Com_Error (ERR_DROP, "MOD_LoadBmodel: funny lump size");

	// scan through all the surfaces, but only load patches,
	// not planar faces
	for ( i = 0 ; i < count ; i++, in++ ) {
		if ( LittleLong( in->surfaceType ) != MST_PATCH ) {
			continue;		// ignore other surfaces
		}
		// FIXME: check for non-colliding patches

		cm.surfaces[ i ] = patch = Hunk_Alloc( sizeof( *patch ), h_high );

		// load the full drawverts onto the stack
		width = LittleLong( in->patchWidth );
		height = LittleLong( in->patchHeight );
		c = width * height;
		if ( c > MAX_PATCH_VERTS ) {
			Com_Error( ERR_DROP, "ParseMesh: MAX_PATCH_VERTS" );
		}

		dv_p = dv + LittleLong( in->firstVert );
		for ( j = 0 ; j < c ; j++, dv_p++ ) {
			points[j][0] = LittleFloat( dv_p->xyz[0] );
			points[j][1] = LittleFloat( dv_p->xyz[1] );
			points[j][2] = LittleFloat( dv_p->xyz[2] );
		}

		shaderNum = LittleLong( in->shaderNum );
		patch->contents = cm.shaders[shaderNum].contentFlags;
		patch->surfaceFlags = cm.shaders[shaderNum].surfaceFlags;

		// create the internal facet structure
		patch->pc = CM_GeneratePatchCollide( width, height, points );
	}
}

//==================================================================

unsigned CM_LumpChecksum(lump_t *lump) {
	return LittleLong (Com_BlockChecksum (cmod_base + lump->fileofs, lump->filelen));
}

unsigned CM_Checksum(dheader_t *header) {
	unsigned checksums[16];
	checksums[0] = CM_LumpChecksum(&header->lumps[LUMP_SHADERS]);
	checksums[1] = CM_LumpChecksum(&header->lumps[LUMP_LEAFS]);
	checksums[2] = CM_LumpChecksum(&header->lumps[LUMP_LEAFBRUSHES]);
	checksums[3] = CM_LumpChecksum(&header->lumps[LUMP_LEAFSURFACES]);
	checksums[4] = CM_LumpChecksum(&header->lumps[LUMP_PLANES]);
	checksums[5] = CM_LumpChecksum(&header->lumps[LUMP_BRUSHSIDES]);
	checksums[6] = CM_LumpChecksum(&header->lumps[LUMP_BRUSHES]);
	checksums[7] = CM_LumpChecksum(&header->lumps[LUMP_MODELS]);
	checksums[8] = CM_LumpChecksum(&header->lumps[LUMP_NODES]);
	checksums[9] = CM_LumpChecksum(&header->lumps[LUMP_SURFACES]);
	checksums[10] = CM_LumpChecksum(&header->lumps[LUMP_DRAWVERTS]);

	return LittleLong(Com_BlockChecksum(checksums, 11 * 4));
}

/*
==================
CM_LoadMap

Loads in the map and all submodels
==================
*/
void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) {
	union {
		int				*i;
		void			*v;
	} buf;
	int				i;
	dheader_t		header;
	int				length;
	static unsigned	last_checksum;

	if ( !name || !name[0] ) {
		Com_Error( ERR_DROP, "CM_LoadMap: NULL name" );
	}

#ifndef BSPC
	cm_noAreas = Cvar_Get ("cm_noAreas", "0", CVAR_CHEAT);
	cm_noCurves = Cvar_Get ("cm_noCurves", "0", CVAR_CHEAT);
	cm_playerCurveClip = Cvar_Get ("cm_playerCurveClip", "1", CVAR_ARCHIVE|CVAR_CHEAT );
	cm_betterIbspSurfaceNums = Cvar_Get ("cm_betterIbspSurfaceNums", "0", CVAR_LATCH );
#endif
	Com_DPrintf( "CM_LoadMap( %s, %i )\n", name, clientload );

	if ( !strcmp( cm.name, name ) && clientload ) {
		*checksum = last_checksum;
		return;
	}

	// free old stuff
	Com_Memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();

	if ( !name[0] ) {
		cm.numLeafs = 1;
		cm.numClusters = 1;
		cm.numAreas = 1;
		cm.cmodels = Hunk_Alloc( sizeof( *cm.cmodels ), h_high );
		*checksum = 0;
		return;
	}

	//
	// load the file
	//
#ifndef BSPC
	length = FS_ReadFile( name, &buf.v );
#else
	length = LoadQuakeFile((quakefile_t *) name, &buf.v);
#endif

	if ( !buf.i ) {
		Com_Error (ERR_DROP, "Couldn't load %s", name);
	}

	last_checksum = LittleLong (Com_BlockChecksum (buf.i, length));
	*checksum = last_checksum;

	header = *(dheader_t *)buf.i;
	for (i=0 ; i<sizeof(dheader_t)/4 ; i++) {
		((int *)&header)[i] = LittleLong ( ((int *)&header)[i]);
	}

	if ( header.version != Q3_BSP_VERSION && header.version != WOLF_BSP_VERSION ) {
		Com_Error (ERR_DROP, "CM_LoadMap: %s has wrong version number (%i should be %i or %i)",
			name, header.version, Q3_BSP_VERSION, WOLF_BSP_VERSION );
	}

	cmod_base = (byte *)buf.i;

	// load into heap
	CMod_LoadShaders( &header.lumps[LUMP_SHADERS] );
	CMod_LoadLeafs (&header.lumps[LUMP_LEAFS]);
	CMod_LoadLeafBrushes (&header.lumps[LUMP_LEAFBRUSHES]);
	CMod_LoadLeafSurfaces (&header.lumps[LUMP_LEAFSURFACES]);
	CMod_LoadPlanes (&header.lumps[LUMP_PLANES]);
	CMod_LoadBrushSides (&header.lumps[LUMP_BRUSHSIDES]);
	CMod_LoadBrushes (&header.lumps[LUMP_BRUSHES]);
	CMod_LoadSubmodels (&header.lumps[LUMP_MODELS]);
	CMod_LoadNodes (&header.lumps[LUMP_NODES]);
	CMod_LoadEntityString (&header.lumps[LUMP_ENTITIES]);
	CMod_LoadVisibility( &header.lumps[LUMP_VISIBILITY] );
	CMod_LoadPatches( &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS] );

	CMod_CreateBrushSideWindings( &header.lumps[LUMP_SURFACES], &header.lumps[LUMP_DRAWVERTS], &header.lumps[LUMP_DRAWINDEXES] );

	// we are NOT freeing the file, because it is cached for the ref
	FS_FreeFile (buf.v);

	CM_InitBoxHull ();

	CM_FloodAreaConnections ();

	// allow this to be cached if it is loaded by the server
	if ( !clientload ) {
		Q_strncpyz( cm.name, name, sizeof( cm.name ) );
	}
}

/*
==================
CM_ClearMap
==================
*/
void CM_ClearMap( void ) {
	Com_Memset( &cm, 0, sizeof( cm ) );
	CM_ClearLevelPatches();
}

/*
==================
CM_ClipHandleToModel
==================
*/
cmodel_t	*CM_ClipHandleToModel( clipHandle_t handle ) {
	if ( handle < 0 ) {
		Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i", handle );
	}
	if ( handle < cm.numSubModels ) {
		return &cm.cmodels[handle];
	}
	if ( handle == BOX_MODEL_HANDLE || handle == CAPSULE_MODEL_HANDLE ) {
		return &box_model;
	}
	Com_Error( ERR_DROP, "CM_ClipHandleToModel: bad handle %i (max %d)", handle, cm.numSubModels );

	return NULL;

}

/*
==================
CM_InlineModel
==================
*/
clipHandle_t	CM_InlineModel( int index ) {
	if ( index < 0 || index >= cm.numSubModels ) {
		Com_Error (ERR_DROP, "CM_InlineModel: bad number");
	}
	return index;
}

int		CM_NumClusters( void ) {
	return cm.numClusters;
}

int		CM_NumInlineModels( void ) {
	return cm.numSubModels;
}

char	*CM_EntityString( void ) {
	return cm.entityString;
}

int		CM_LeafCluster( int leafnum ) {
	if (leafnum < 0 || leafnum >= cm.numLeafs) {
		Com_Error (ERR_DROP, "CM_LeafCluster: bad number");
	}
	return cm.leafs[leafnum].cluster;
}

int		CM_LeafArea( int leafnum ) {
	if ( leafnum < 0 || leafnum >= cm.numLeafs ) {
		Com_Error (ERR_DROP, "CM_LeafArea: bad number");
	}
	return cm.leafs[leafnum].area;
}

//=======================================================================


/*
===================
CM_InitBoxHull

Set up the planes and nodes so that the six floats of a bounding box
can just be stored out and get a proper clipping hull structure.
===================
*/
void CM_InitBoxHull (void)
{
	int			i;
	int			side;
	cplane_t	*p;
	cbrushside_t	*s;

	box_planes = &cm.planes[cm.numPlanes];

	box_brush = &cm.brushes[cm.numBrushes];
	box_brush->numsides = 6;
	box_brush->sides = cm.brushsides + cm.numBrushSides;
	box_brush->contents = 0; // Will be set to CONTENTS_SOLID, CONTENTS_BODY, etc
	box_brush->edges = (cbrushedge_t *)Hunk_Alloc(
			sizeof( cbrushedge_t ) * 12, h_low );
	box_brush->numEdges = 12;

	box_model.leaf.numLeafBrushes = 1;
//	box_model.leaf.firstLeafBrush = cm.numBrushes;
	box_model.leaf.firstLeafBrush = cm.numLeafBrushes;
	cm.leafbrushes[cm.numLeafBrushes] = cm.numBrushes;

	for (i=0 ; i<6 ; i++)
	{
		side = i&1;

		// brush sides
		s = &cm.brushsides[cm.numBrushSides+i];
		s->plane = 	cm.planes + (cm.numPlanes+i*2+side);
		s->surfaceFlags = 0;

		// planes
		p = &box_planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = 1;

		p = &box_planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear (p->normal);
		p->normal[i>>1] = -1;

		SetPlaneSignbits( p );
	}	
}

/*
===================
CM_TempBoxModel

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
Capsules are handled differently though.
===================
*/
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule, int contents ) {

	VectorCopy( mins, box_model.mins );
	VectorCopy( maxs, box_model.maxs );

	if ( capsule ) {
		capsule_contents = contents;
		return CAPSULE_MODEL_HANDLE;
	}

	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	// First side
	VectorSet( box_brush->edges[ 0 ].p0,  mins[ 0 ], mins[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 0 ].p1,  mins[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 1 ].p0,  mins[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 1 ].p1,  mins[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 2 ].p0,  mins[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 2 ].p1,  mins[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 3 ].p0,  mins[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 3 ].p1,  mins[ 0 ], mins[ 1 ], mins[ 2 ] );

	// Opposite side
	VectorSet( box_brush->edges[ 4 ].p0,  maxs[ 0 ], mins[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 4 ].p1,  maxs[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 5 ].p0,  maxs[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 5 ].p1,  maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 6 ].p0,  maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 6 ].p1,  maxs[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 7 ].p0,  maxs[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 7 ].p1,  maxs[ 0 ], mins[ 1 ], mins[ 2 ] );

	// Connecting edges
	VectorSet( box_brush->edges[ 8 ].p0,  mins[ 0 ], mins[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 8 ].p1,  maxs[ 0 ], mins[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 9 ].p0,  mins[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 9 ].p1,  maxs[ 0 ], maxs[ 1 ], mins[ 2 ] );
	VectorSet( box_brush->edges[ 10 ].p0, mins[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 10 ].p1, maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 11 ].p0, mins[ 0 ], mins[ 1 ], maxs[ 2 ] );
	VectorSet( box_brush->edges[ 11 ].p1, maxs[ 0 ], mins[ 1 ], maxs[ 2 ] );

	VectorCopy( mins, box_brush->bounds[0] );
	VectorCopy( maxs, box_brush->bounds[1] );

	box_brush->contents = contents;

	return BOX_MODEL_HANDLE;
}

/*
===================
CM_ModelBounds
===================
*/
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	cmodel_t	*cmod;

	cmod = CM_ClipHandleToModel( model );
	VectorCopy( cmod->mins, mins );
	VectorCopy( cmod->maxs, maxs );
}


