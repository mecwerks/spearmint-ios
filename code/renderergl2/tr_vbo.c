/*
===========================================================================
Copyright (C) 2007-2009 Robert Beckebans <trebor_7@users.sourceforge.net>

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
// tr_vbo.c
#include "tr_local.h"


uint32_t R_VboPackTangent(vec4_t v)
{
	if (glRefConfig.packedNormalDataType == GL_UNSIGNED_INT_2_10_10_10_REV)
	{
		return (((uint32_t)(v[3] * 1.5f   + 2.0f  )) << 30)
		     | (((uint32_t)(v[2] * 511.5f + 512.0f)) << 20)
		     | (((uint32_t)(v[1] * 511.5f + 512.0f)) << 10)
		     | (((uint32_t)(v[0] * 511.5f + 512.0f)));
	}
	else
	{
		return (((uint32_t)(v[3] * 127.5f + 128.0f)) << 24)
		     | (((uint32_t)(v[2] * 127.5f + 128.0f)) << 16)
		     | (((uint32_t)(v[1] * 127.5f + 128.0f)) << 8)
		     | (((uint32_t)(v[0] * 127.5f + 128.0f)));
	}
}

uint32_t R_VboPackNormal(vec3_t v)
{
	if (glRefConfig.packedNormalDataType == GL_UNSIGNED_INT_2_10_10_10_REV)
	{
		return (((uint32_t)(v[2] * 511.5f + 512.0f)) << 20)
		     | (((uint32_t)(v[1] * 511.5f + 512.0f)) << 10)
		     | (((uint32_t)(v[0] * 511.5f + 512.0f)));
	}
	else
	{
		return (((uint32_t)(v[2] * 127.5f + 128.0f)) << 16)
		     | (((uint32_t)(v[1] * 127.5f + 128.0f)) << 8)
		     | (((uint32_t)(v[0] * 127.5f + 128.0f)));
	}
}

void R_VboUnpackTangent(vec4_t v, uint32_t b)
{
	if (glRefConfig.packedNormalDataType == GL_UNSIGNED_INT_2_10_10_10_REV)
	{
		v[0] = ((b)       & 0x3ff) * 1.0f/511.5f - 1.0f;
		v[1] = ((b >> 10) & 0x3ff) * 1.0f/511.5f - 1.0f;
		v[2] = ((b >> 20) & 0x3ff) * 1.0f/511.5f - 1.0f;
		v[3] = ((b >> 30) & 0x3)   * 1.0f/1.5f   - 1.0f;
	}
	else
	{
		v[0] = ((b)       & 0xff) * 1.0f/127.5f - 1.0f;
		v[1] = ((b >> 8)  & 0xff) * 1.0f/127.5f - 1.0f;
		v[2] = ((b >> 16) & 0xff) * 1.0f/127.5f - 1.0f;
		v[3] = ((b >> 24) & 0xff) * 1.0f/127.5f - 1.0f;
	}
}

void R_VboUnpackNormal(vec3_t v, uint32_t b)
{
	if (glRefConfig.packedNormalDataType == GL_UNSIGNED_INT_2_10_10_10_REV)
	{
		v[0] = ((b)       & 0x3ff) * 1.0f/511.5f - 1.0f;
		v[1] = ((b >> 10) & 0x3ff) * 1.0f/511.5f - 1.0f;
		v[2] = ((b >> 20) & 0x3ff) * 1.0f/511.5f - 1.0f;
	}
	else
	{
		v[0] = ((b)       & 0xff) * 1.0f/127.5f - 1.0f;
		v[1] = ((b >> 8)  & 0xff) * 1.0f/127.5f - 1.0f;
		v[2] = ((b >> 16) & 0xff) * 1.0f/127.5f - 1.0f;
	}
}

/*
============
R_CreateVBO
============
*/
VBO_t          *R_CreateVBO(const char *name, byte * vertexes, int vertexesSize, vboUsage_t usage)
{
	VBO_t          *vbo;
	int				glUsage;

	switch (usage)
	{
		case VBO_USAGE_STATIC:
			glUsage = GL_STATIC_DRAW_ARB;
			break;

		case VBO_USAGE_DYNAMIC:
			glUsage = GL_DYNAMIC_DRAW_ARB;
			break;

		default:
			Com_Error(ERR_FATAL, "bad vboUsage_t given: %i", usage);
			return NULL;
	}

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateVBO: \"%s\" is too long", name);
	}

	if ( tr.numVBOs == MAX_VBOS ) {
		ri.Error( ERR_DROP, "R_CreateVBO: MAX_VBOS hit");
	}

	R_IssuePendingRenderCommands();

	vbo = tr.vbos[tr.numVBOs] = ri.Hunk_Alloc(sizeof(*vbo), h_low);
	tr.numVBOs++;

	memset(vbo, 0, sizeof(*vbo));

	Q_strncpyz(vbo->name, name, sizeof(vbo->name));

	vbo->vertexesSize = vertexesSize;

	qglGenBuffersARB(1, &vbo->vertexesVBO);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vertexesVBO);
	qglBufferDataARB(GL_ARRAY_BUFFER_ARB, vertexesSize, vertexes, glUsage);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	glState.currentVBO = NULL;

	GL_CheckErrors();

	return vbo;
}

/*
============
R_CreateVBO2
============
*/
VBO_t          *R_CreateVBO2(const char *name, int numVertexes, srfVert_t * verts, unsigned int stateBits, vboUsage_t usage)
{
	VBO_t          *vbo;
	int             i;

	byte           *data;
	int             dataSize;
	int             dataOfs;

	int				glUsage;

	switch (usage)
	{
		case VBO_USAGE_STATIC:
			glUsage = GL_STATIC_DRAW_ARB;
			break;

		case VBO_USAGE_DYNAMIC:
			glUsage = GL_DYNAMIC_DRAW_ARB;
			break;

		default:
			Com_Error(ERR_FATAL, "bad vboUsage_t given: %i", usage);
			return NULL;
	}

	if(!numVertexes)
		return NULL;

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateVBO2: \"%s\" is too long", name);
	}

	if ( tr.numVBOs == MAX_VBOS ) {
		ri.Error( ERR_DROP, "R_CreateVBO2: MAX_VBOS hit");
	}

	R_IssuePendingRenderCommands();

	vbo = tr.vbos[tr.numVBOs] = ri.Hunk_Alloc(sizeof(*vbo), h_low);
	tr.numVBOs++;

	memset(vbo, 0, sizeof(*vbo));

	Q_strncpyz(vbo->name, name, sizeof(vbo->name));

	if (usage == VBO_USAGE_STATIC)
	{
		// since these vertex attributes are never altered, interleave them
		vbo->ofs_xyz = 0;
		dataSize = sizeof(verts[0].xyz);

		if(stateBits & ATTR_NORMAL)
		{
			vbo->ofs_normal = dataSize;
			dataSize += sizeof(uint32_t);
		}

#ifdef USE_VERT_TANGENT_SPACE
		if(stateBits & ATTR_TANGENT)
		{
			vbo->ofs_tangent = dataSize;
			dataSize += sizeof(uint32_t);
		}
#endif

		if(stateBits & ATTR_TEXCOORD)
		{
			vbo->ofs_st = dataSize;
			dataSize += sizeof(verts[0].st);
		}

		if(stateBits & ATTR_LIGHTCOORD)
		{
			vbo->ofs_lightmap = dataSize;
			dataSize += sizeof(verts[0].lightmap);
		}

		if(stateBits & ATTR_COLOR)
		{
			vbo->ofs_vertexcolor = dataSize;
			dataSize += sizeof(verts[0].vertexColors);
		}

		if(stateBits & ATTR_LIGHTDIRECTION)
		{
			vbo->ofs_lightdir = dataSize;
			dataSize += sizeof(uint32_t);
		}

		vbo->stride_xyz         = dataSize;
		vbo->stride_normal      = dataSize;
#ifdef USE_VERT_TANGENT_SPACE
		vbo->stride_tangent     = dataSize;
#endif
		vbo->stride_st          = dataSize;
		vbo->stride_lightmap    = dataSize;
		vbo->stride_vertexcolor = dataSize;
		vbo->stride_lightdir    = dataSize;

		// create VBO
		dataSize *= numVertexes;
		data = ri.Hunk_AllocateTempMemory(dataSize);
		dataOfs = 0;

		//ri.Printf(PRINT_ALL, "CreateVBO: %d, %d %d %d %d %d, %d %d %d %d %d\n", dataSize, vbo->ofs_xyz, vbo->ofs_normal, vbo->ofs_st, vbo->ofs_lightmap, vbo->ofs_vertexcolor,
			//vbo->stride_xyz, vbo->stride_normal, vbo->stride_st, vbo->stride_lightmap, vbo->stride_vertexcolor);

		for (i = 0; i < numVertexes; i++)
		{
			// xyz
			memcpy(data + dataOfs, &verts[i].xyz, sizeof(verts[i].xyz));
			dataOfs += sizeof(verts[i].xyz);

			// normal
			if(stateBits & ATTR_NORMAL)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackNormal(verts[i].normal);

				dataOfs += sizeof(uint32_t);
			}

#ifdef USE_VERT_TANGENT_SPACE
			// tangent
			if(stateBits & ATTR_TANGENT)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackTangent(verts[i].tangent);

				dataOfs += sizeof(uint32_t);
			}
#endif

			// vertex texcoords
			if(stateBits & ATTR_TEXCOORD)
			{
				memcpy(data + dataOfs, &verts[i].st, sizeof(verts[i].st));
				dataOfs += sizeof(verts[i].st);
			}

			// feed vertex lightmap texcoords
			if(stateBits & ATTR_LIGHTCOORD)
			{
				memcpy(data + dataOfs, &verts[i].lightmap, sizeof(verts[i].lightmap));
				dataOfs += sizeof(verts[i].lightmap);
			}

			// feed vertex colors
			if(stateBits & ATTR_COLOR)
			{
				memcpy(data + dataOfs, &verts[i].vertexColors, sizeof(verts[i].vertexColors));
				dataOfs += sizeof(verts[i].vertexColors);
			}

			// feed vertex light directions
			if(stateBits & ATTR_LIGHTDIRECTION)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackNormal(verts[i].lightdir);

				dataOfs += sizeof(uint32_t);
			}
		}
	}
	else
	{
		// since these vertex attributes may be changed, put them in flat arrays
		dataSize = sizeof(verts[0].xyz);

		if(stateBits & ATTR_NORMAL)
		{
			dataSize += sizeof(uint32_t);
		}

#ifdef USE_VERT_TANGENT_SPACE
		if(stateBits & ATTR_TANGENT)
		{
			dataSize += sizeof(uint32_t);
		}
#endif

		if(stateBits & ATTR_TEXCOORD)
		{
			dataSize += sizeof(verts[0].st);
		}

		if(stateBits & ATTR_LIGHTCOORD)
		{
			dataSize += sizeof(verts[0].lightmap);
		}

		if(stateBits & ATTR_COLOR)
		{
			dataSize += sizeof(verts[0].vertexColors);
		}

		if(stateBits & ATTR_LIGHTDIRECTION)
		{
			dataSize += sizeof(uint32_t);
		}

		// create VBO
		dataSize *= numVertexes;
		data = ri.Hunk_AllocateTempMemory(dataSize);
		dataOfs = 0;

		vbo->ofs_xyz            = 0;
		vbo->ofs_normal         = 0;
#ifdef USE_VERT_TANGENT_SPACE
		vbo->ofs_tangent        = 0;
#endif
		vbo->ofs_st             = 0;
		vbo->ofs_lightmap       = 0;
		vbo->ofs_vertexcolor    = 0;
		vbo->ofs_lightdir       = 0;

		vbo->stride_xyz         = sizeof(verts[0].xyz);
		vbo->stride_normal      = sizeof(uint32_t);
#ifdef USE_VERT_TANGENT_SPACE
		vbo->stride_tangent     = sizeof(uint32_t);
#endif
		vbo->stride_vertexcolor = sizeof(verts[0].vertexColors);
		vbo->stride_st          = sizeof(verts[0].st);
		vbo->stride_lightmap    = sizeof(verts[0].lightmap);
		vbo->stride_lightdir    = sizeof(uint32_t);

		//ri.Printf(PRINT_ALL, "2CreateVBO: %d, %d %d %d %d %d, %d %d %d %d %d\n", dataSize, vbo->ofs_xyz, vbo->ofs_normal, vbo->ofs_st, vbo->ofs_lightmap, vbo->ofs_vertexcolor,
			//vbo->stride_xyz, vbo->stride_normal, vbo->stride_st, vbo->stride_lightmap, vbo->stride_vertexcolor);

		// xyz
		for (i = 0; i < numVertexes; i++)
		{
			memcpy(data + dataOfs, &verts[i].xyz, sizeof(verts[i].xyz));
			dataOfs += sizeof(verts[i].xyz);
		}

		// normal
		if(stateBits & ATTR_NORMAL)
		{
			vbo->ofs_normal = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackNormal(verts[i].normal);

				dataOfs += sizeof(uint32_t);
			}
		}

#ifdef USE_VERT_TANGENT_SPACE
		// tangent
		if(stateBits & ATTR_TANGENT)
		{
			vbo->ofs_tangent = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackTangent(verts[i].tangent);

				dataOfs += sizeof(uint32_t);
			}
		}
#endif

		// vertex texcoords
		if(stateBits & ATTR_TEXCOORD)
		{
			vbo->ofs_st = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				memcpy(data + dataOfs, &verts[i].st, sizeof(verts[i].st));
				dataOfs += sizeof(verts[i].st);
			}
		}

		// feed vertex lightmap texcoords
		if(stateBits & ATTR_LIGHTCOORD)
		{
			vbo->ofs_lightmap = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				memcpy(data + dataOfs, &verts[i].lightmap, sizeof(verts[i].lightmap));
				dataOfs += sizeof(verts[i].lightmap);
			}
		}

		// feed vertex colors
		if(stateBits & ATTR_COLOR)
		{
			vbo->ofs_vertexcolor = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				memcpy(data + dataOfs, &verts[i].vertexColors, sizeof(verts[i].vertexColors));
				dataOfs += sizeof(verts[i].vertexColors);
			}
		}

		// feed vertex lightdirs
		if(stateBits & ATTR_LIGHTDIRECTION)
		{
			vbo->ofs_lightdir = dataOfs;
			for (i = 0; i < numVertexes; i++)
			{
				uint32_t *p = (uint32_t *)(data + dataOfs);

				*p = R_VboPackNormal(verts[i].lightdir);

				dataOfs += sizeof(uint32_t);
			}
		}
	}


	vbo->vertexesSize = dataSize;

	qglGenBuffersARB(1, &vbo->vertexesVBO);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vertexesVBO);
	qglBufferDataARB(GL_ARRAY_BUFFER_ARB, dataSize, data, glUsage);

	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	glState.currentVBO = NULL;

	GL_CheckErrors();

	ri.Hunk_FreeTempMemory(data);

	return vbo;
}


/*
============
R_CreateIBO
============
*/
IBO_t          *R_CreateIBO(const char *name, byte * indexes, int indexesSize, vboUsage_t usage)
{
	IBO_t          *ibo;
	int				glUsage;

	switch (usage)
	{
		case VBO_USAGE_STATIC:
			glUsage = GL_STATIC_DRAW_ARB;
			break;

		case VBO_USAGE_DYNAMIC:
			glUsage = GL_DYNAMIC_DRAW_ARB;
			break;

		default:
			Com_Error(ERR_FATAL, "bad vboUsage_t given: %i", usage);
			return NULL;
	}

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateIBO: \"%s\" is too long", name);
	}

	if ( tr.numIBOs == MAX_IBOS ) {
		ri.Error( ERR_DROP, "R_CreateIBO: MAX_IBOS hit");
	}

	R_IssuePendingRenderCommands();

	ibo = tr.ibos[tr.numIBOs] = ri.Hunk_Alloc(sizeof(*ibo), h_low);
	tr.numIBOs++;

	Q_strncpyz(ibo->name, name, sizeof(ibo->name));

	ibo->indexesSize = indexesSize;

	qglGenBuffersARB(1, &ibo->indexesVBO);

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibo->indexesVBO);
	qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexesSize, indexes, glUsage);

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	glState.currentIBO = NULL;

	GL_CheckErrors();

	return ibo;
}

/*
============
R_CreateIBO2
============
*/
IBO_t          *R_CreateIBO2(const char *name, int numIndexes, glIndex_t * inIndexes, vboUsage_t usage)
{
	IBO_t          *ibo;
	int             i;

	glIndex_t       *indexes;
	int             indexesSize;

	int				glUsage;

	switch (usage)
	{
		case VBO_USAGE_STATIC:
			glUsage = GL_STATIC_DRAW_ARB;
			break;

		case VBO_USAGE_DYNAMIC:
			glUsage = GL_DYNAMIC_DRAW_ARB;
			break;

		default:
			Com_Error(ERR_FATAL, "bad vboUsage_t given: %i", usage);
			return NULL;
	}

	if(!numIndexes)
		return NULL;

	if(strlen(name) >= MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateIBO2: \"%s\" is too long", name);
	}

	if ( tr.numIBOs == MAX_IBOS ) {
		ri.Error( ERR_DROP, "R_CreateIBO2: MAX_IBOS hit");
	}

	R_IssuePendingRenderCommands();

	ibo = tr.ibos[tr.numIBOs] = ri.Hunk_Alloc(sizeof(*ibo), h_low);
	tr.numIBOs++;

	Q_strncpyz(ibo->name, name, sizeof(ibo->name));

	indexesSize = numIndexes * sizeof(glIndex_t);
	indexes = ri.Hunk_AllocateTempMemory(indexesSize);

	for(i = 0; i < numIndexes; i++)
	{
		indexes[i] = inIndexes[i];
	}

	ibo->indexesSize = indexesSize;

	qglGenBuffersARB(1, &ibo->indexesVBO);

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibo->indexesVBO);
	qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, indexesSize, indexes, glUsage);

	qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	glState.currentIBO = NULL;

	GL_CheckErrors();

	ri.Hunk_FreeTempMemory(indexes);

	return ibo;
}

/*
============
R_BindVBO
============
*/
void R_BindVBO(VBO_t * vbo)
{
	if(!vbo)
	{
		//R_BindNullVBO();
		ri.Error(ERR_DROP, "R_BindNullVBO: NULL vbo");
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- R_BindVBO( %s ) ---\n", vbo->name));
	}

	if(glState.currentVBO != vbo)
	{
		glState.currentVBO = vbo;
		glState.vertexAttribPointersSet = 0;

		glState.vertexAttribsInterpolation = 0;
		glState.vertexAttribsOldFrame = 0;
		glState.vertexAttribsNewFrame = 0;
		glState.vertexAnimation = qfalse;

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, vbo->vertexesVBO);

		backEnd.pc.c_vboVertexBuffers++;
	}
}

/*
============
R_BindNullVBO
============
*/
void R_BindNullVBO(void)
{
	GLimp_LogComment("--- R_BindNullVBO ---\n");

	if(glState.currentVBO)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glState.currentVBO = NULL;
	}

	GL_CheckErrors();
}

/*
============
R_BindIBO
============
*/
void R_BindIBO(IBO_t * ibo)
{
	if(!ibo)
	{
		//R_BindNullIBO();
		ri.Error(ERR_DROP, "R_BindIBO: NULL ibo");
		return;
	}

	if(r_logFile->integer)
	{
		// don't just call LogComment, or we will get a call to va() every frame!
		GLimp_LogComment(va("--- R_BindIBO( %s ) ---\n", ibo->name));
	}

	if(glState.currentIBO != ibo)
	{
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, ibo->indexesVBO);

		glState.currentIBO = ibo;

		backEnd.pc.c_vboIndexBuffers++;
	}
}

/*
============
R_BindNullIBO
============
*/
void R_BindNullIBO(void)
{
	GLimp_LogComment("--- R_BindNullIBO ---\n");

	if(glState.currentIBO)
	{
		qglBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glState.currentIBO = NULL;
		glState.vertexAttribPointersSet = 0;
	}
}

/*
============
R_InitVBOs
============
*/
void R_InitVBOs(void)
{
	int             dataSize;
	int             offset;

	ri.Printf(PRINT_ALL, "------- R_InitVBOs -------\n");

	tr.numVBOs = 0;
	tr.numIBOs = 0;

	dataSize  = sizeof(tess.xyz[0]);
	dataSize += sizeof(tess.normal[0]);
#ifdef USE_VERT_TANGENT_SPACE
	dataSize += sizeof(tess.tangent[0]);
#endif
	dataSize += sizeof(tess.vertexColors[0]);
	dataSize += sizeof(tess.texCoords[0][0]) * 2;
	dataSize += sizeof(tess.lightdir[0]);
	dataSize *= SHADER_MAX_VERTEXES;

	tess.vbo = R_CreateVBO("tessVertexArray_VBO", NULL, dataSize, VBO_USAGE_DYNAMIC);

	offset = 0;

	tess.vbo->ofs_xyz         = offset; offset += sizeof(tess.xyz[0])              * SHADER_MAX_VERTEXES;
	tess.vbo->ofs_normal      = offset; offset += sizeof(tess.normal[0])           * SHADER_MAX_VERTEXES;
#ifdef USE_VERT_TANGENT_SPACE
	tess.vbo->ofs_tangent     = offset; offset += sizeof(tess.tangent[0])          * SHADER_MAX_VERTEXES;
#endif
	// these next two are actually interleaved
	tess.vbo->ofs_st          = offset; 
	tess.vbo->ofs_lightmap    = offset + sizeof(tess.texCoords[0][0]);
	                                    offset += sizeof(tess.texCoords[0][0]) * 2 * SHADER_MAX_VERTEXES;

	tess.vbo->ofs_vertexcolor = offset; offset += sizeof(tess.vertexColors[0])     * SHADER_MAX_VERTEXES;
	tess.vbo->ofs_lightdir    = offset;

	tess.vbo->stride_xyz         = sizeof(tess.xyz[0]);
	tess.vbo->stride_normal      = sizeof(tess.normal[0]);
#ifdef USE_VERT_TANGENT_SPACE
	tess.vbo->stride_tangent     = sizeof(tess.tangent[0]);
#endif
	tess.vbo->stride_vertexcolor = sizeof(tess.vertexColors[0]);
	tess.vbo->stride_st          = sizeof(tess.texCoords[0][0]) * 2;
	tess.vbo->stride_lightmap    = sizeof(tess.texCoords[0][0]) * 2;
	tess.vbo->stride_lightdir    = sizeof(tess.lightdir[0]);

	dataSize = sizeof(tess.indexes[0]) * SHADER_MAX_INDEXES;

	tess.ibo = R_CreateIBO("tessVertexArray_IBO", NULL, dataSize, VBO_USAGE_DYNAMIC);

	R_BindNullVBO();
	R_BindNullIBO();

	GL_CheckErrors();
}

/*
============
R_ShutdownVBOs
============
*/
void R_ShutdownVBOs(void)
{
	int             i;
	VBO_t          *vbo;
	IBO_t          *ibo;

	ri.Printf(PRINT_ALL, "------- R_ShutdownVBOs -------\n");

	R_BindNullVBO();
	R_BindNullIBO();


	for(i = 0; i < tr.numVBOs; i++)
	{
		vbo = tr.vbos[i];

		if(vbo->vertexesVBO)
		{
			qglDeleteBuffersARB(1, &vbo->vertexesVBO);
		}

		//ri.Free(vbo);
	}

	for(i = 0; i < tr.numIBOs; i++)
	{
		ibo = tr.ibos[i];

		if(ibo->indexesVBO)
		{
			qglDeleteBuffersARB(1, &ibo->indexesVBO);
		}

		//ri.Free(ibo);
	}

	tr.numVBOs = 0;
	tr.numIBOs = 0;
}

/*
============
R_VBOList_f
============
*/
void R_VBOList_f(void)
{
	int             i;
	VBO_t          *vbo;
	IBO_t          *ibo;
	int             vertexesSize = 0;
	int             indexesSize = 0;

	ri.Printf(PRINT_ALL, " size          name\n");
	ri.Printf(PRINT_ALL, "----------------------------------------------------------\n");

	for(i = 0; i < tr.numVBOs; i++)
	{
		vbo = tr.vbos[i];

		ri.Printf(PRINT_ALL, "%d.%02d MB %s\n", vbo->vertexesSize / (1024 * 1024),
				  (vbo->vertexesSize % (1024 * 1024)) * 100 / (1024 * 1024), vbo->name);

		vertexesSize += vbo->vertexesSize;
	}

	for(i = 0; i < tr.numIBOs; i++)
	{
		ibo = tr.ibos[i];

		ri.Printf(PRINT_ALL, "%d.%02d MB %s\n", ibo->indexesSize / (1024 * 1024),
				  (ibo->indexesSize % (1024 * 1024)) * 100 / (1024 * 1024), ibo->name);

		indexesSize += ibo->indexesSize;
	}

	ri.Printf(PRINT_ALL, " %i total VBOs\n", tr.numVBOs);
	ri.Printf(PRINT_ALL, " %d.%02d MB total vertices memory\n", vertexesSize / (1024 * 1024),
			  (vertexesSize % (1024 * 1024)) * 100 / (1024 * 1024));

	ri.Printf(PRINT_ALL, " %i total IBOs\n", tr.numIBOs);
	ri.Printf(PRINT_ALL, " %d.%02d MB total triangle indices memory\n", indexesSize / (1024 * 1024),
			  (indexesSize % (1024 * 1024)) * 100 / (1024 * 1024));
}


/*
==============
RB_UpdateVBOs

Adapted from Tess_UpdateVBOs from xreal

Update the default VBO to replace the client side vertex arrays
==============
*/
void RB_UpdateVBOs(unsigned int attribBits)
{
	GLimp_LogComment("--- RB_UpdateVBOs ---\n");

	backEnd.pc.c_dynamicVboDraws++;

	// update the default VBO
	if(tess.numVertexes > 0 && tess.numVertexes <= SHADER_MAX_VERTEXES)
	{
		R_BindVBO(tess.vbo);

		// orphan old buffer so we don't stall on it
		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->vertexesSize, NULL, GL_DYNAMIC_DRAW_ARB);

		if(attribBits & ATTR_BITS)
		{
			if(attribBits & ATTR_POSITION)
			{
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_xyz, tess.numVertexes * sizeof(tess.xyz[0]));
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_xyz,         tess.numVertexes * sizeof(tess.xyz[0]),              tess.xyz);
			}

			if(attribBits & ATTR_TEXCOORD || attribBits & ATTR_LIGHTCOORD)
			{
				// these are interleaved, so we update both if either need it
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_st, tess.numVertexes * sizeof(tess.texCoords[0][0]) * 2);
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_st,          tess.numVertexes * sizeof(tess.texCoords[0][0]) * 2, tess.texCoords);
			}

			if(attribBits & ATTR_NORMAL)
			{
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_normal, tess.numVertexes * sizeof(tess.normal[0]));
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_normal,      tess.numVertexes * sizeof(tess.normal[0]),           tess.normal);
			}

#ifdef USE_VERT_TANGENT_SPACE
			if(attribBits & ATTR_TANGENT)
			{
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_tangent, tess.numVertexes * sizeof(tess.tangent[0]));
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_tangent,     tess.numVertexes * sizeof(tess.tangent[0]),          tess.tangent);
			}
#endif

			if(attribBits & ATTR_COLOR)
			{
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_vertexcolor, tess.numVertexes * sizeof(tess.vertexColors[0]));
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_vertexcolor, tess.numVertexes * sizeof(tess.vertexColors[0]),     tess.vertexColors);
			}

			if(attribBits & ATTR_LIGHTDIRECTION)
			{
				//ri.Printf(PRINT_ALL, "offset %d, size %d\n", tess.vbo->ofs_lightdir, tess.numVertexes * sizeof(tess.lightdir[0]));
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_lightdir,    tess.numVertexes * sizeof(tess.lightdir[0]),         tess.lightdir);
			}
		}
		else
		{
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_xyz,         tess.numVertexes * sizeof(tess.xyz[0]),              tess.xyz);
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_st,          tess.numVertexes * sizeof(tess.texCoords[0][0]) * 2, tess.texCoords);
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_normal,      tess.numVertexes * sizeof(tess.normal[0]),           tess.normal);
#ifdef USE_VERT_TANGENT_SPACE
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_tangent,     tess.numVertexes * sizeof(tess.tangent[0]),          tess.tangent);
#endif
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_vertexcolor, tess.numVertexes * sizeof(tess.vertexColors[0]),     tess.vertexColors);
			qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, tess.vbo->ofs_lightdir,    tess.numVertexes * sizeof(tess.lightdir[0]),         tess.lightdir);
		}

	}

	// update the default IBO
	if(tess.numIndexes > 0 && tess.numIndexes <= SHADER_MAX_INDEXES)
	{
		R_BindIBO(tess.ibo);

		// orphan old buffer so we don't stall on it
		qglBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, tess.ibo->indexesSize, NULL, GL_DYNAMIC_DRAW_ARB);

		qglBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, tess.numIndexes * sizeof(tess.indexes[0]), tess.indexes);
	}
}
