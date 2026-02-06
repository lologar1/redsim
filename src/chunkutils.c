#include "chunkutils.h"

static i32 getBlockmesh(Blockmesh *blockmesh, u64 id, u64 variant, Rotation rotation, i64 x, i64 y, i64 z);
static usf_compatibility_int pushRawmesh(void *chunkindexptr);

void cu_asyncRemeshChunk(u64 chunkindex) {
	/* Asynchronously pushes a new rawmesh to be sent to the GPU */

	u64 *arg;
	arg = malloc(sizeof(u64)); /* Allow thread to operate separately; it cleans up its argument after */
	*arg = chunkindex;

	usf_thread id;
	if ((usf_thrdcreate(&id, pushRawmesh, arg)) == THRD_ERROR) {
		fprintf(stderr, "Error creating thread (async remesh), aborting.\n");
		exit(RSM_EXIT_THREADFAIL);
	}

	if (usf_thrddetach(id) == THRD_ERROR) {
		fprintf(stderr, "Error detaching thread (async remesh), aborting.\n");
		exit(RSM_EXIT_THREADFAIL);
	}
}

void cu_updateMeshlist(void) {
    /* Generate new required meshes since movement from lastPosition
     * and remove out of render distance ones */

	GLuint **meshlist;
	meshlist = meshes_;
	nmeshes_ = 0; /* Reset mesh count */

	GLuint *mesh;
	u64 chunkindex;
	i64 i, j, k, chunk[3];
	i64 pos[3] = {position_[0]/CHUNKSIZE, position_[1]/CHUNKSIZE, position_[2]/CHUNKSIZE };
	for (i = -RSM_LOADING_DISTANCE; i < RSM_LOADING_DISTANCE + 1; i++) { chunk[0] = pos[0] + i;
	for (j = -RSM_LOADING_DISTANCE; j < RSM_LOADING_DISTANCE + 1; j++) { chunk[1] = pos[1] + j;
	for (k = -RSM_LOADING_DISTANCE; k < RSM_LOADING_DISTANCE + 1; k++) { chunk[2] = pos[2] + k;
		chunkindex = TOCHUNKINDEX(chunk[0], chunk[1], chunk[2]);

		if ((mesh = usf_inthmget(meshmap_, chunkindex).p) == NULL) continue; /* Uninitialized */

		*meshlist++ = mesh;
		nmeshes_++;
	}}} /* Is this a Minecraft command block one-block mod ?!? */
}

void cu_generateMeshlist(void) {
    /* Regenerate the render distance meshlist */

	free(meshes_); /* Free old meshlist; first free(NULL) is OK */

	u64 chunkvolume;
	chunkvolume = (u64) pow(RSM_LOADING_DISTANCE * 2 + 1, 3);
	meshes_ = malloc(chunkvolume * sizeof(GLuint *));
	nmeshes_ = 0;

	cu_updateMeshlist();
}

void cu_translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation) {
	/* Makes an appropriate translocation (translation + rotation) matrix given
	 * a translation vector (position) and a rotation */

	mat4 rotAdjust;
	cu_rotationMatrix(rotAdjust, rotation, MESHCENTER);

	glm_mat4_identity(translocation);
	glm_translate(translocation, translation);
	glm_mat4_mul(translocation, rotAdjust, translocation);
}

void cu_rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter) {
	/* Makes an appropriate rotation matrix for a given rotation */

	glm_mat4_identity(rotAdjust);
	switch (rotation) {
		case EAST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), GLM_YUP);
			break;
		case SOUTH:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(180), GLM_YUP);
			break;
		case WEST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), GLM_YUP);
			break;
		case UP:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), GLM_XUP);
			break;
		case DOWN:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), GLM_XUP);
			break;
		case NONE:
		case NORTH:
		case COMPLEX:
			/* A block is defined as NORTH by default. Complex is TODO */
			break;
	}
}

i64 cu_chunkOffsetConvertFloat(f32 absoluteComponent) {
	/* Converts a float absoluteComponent to its chunk offset equivalent */

	return floorf(absoluteComponent / (f32) CHUNKSIZE);
}

u64 cu_blockOffsetConvertFloat(f32 absoluteComponent) {
	/* Converts a float absoluteComponent to its block offset (within a chunk) equivalent */

	return absoluteComponent < 0 ?
		CHUNKSIZE - 1 - ((u64) -floorf(absoluteComponent) - 1) % CHUNKSIZE :
		(u64) absoluteComponent % CHUNKSIZE;
}

Blockdata *cu_coordsToBlock(vec3 coords, u64 *chunkindex) {
	/* Returns a pointer to the Blockdata at these coordinates.
	 * Also initializes the chunk if it did not exist. */

	u64 index;
	index = TOCHUNKINDEX(cu_chunkOffsetConvertFloat(coords[0]),
			cu_chunkOffsetConvertFloat(coords[1]),
			cu_chunkOffsetConvertFloat(coords[2]));

	Chunkdata *chunkdata;
	if ((chunkdata = usf_inthmget(chunkmap_, index).p) == NULL) /* Alloc empty if uninitialized */
		usf_inthmput(chunkmap_, index, USFDATAP(chunkdata = calloc(1, sizeof(Chunkdata))));

	if (chunkindex) *chunkindex = index;

	return &(*chunkdata)[cu_blockOffsetConvertFloat(coords[0])]
		[cu_blockOffsetConvertFloat(coords[1])]
		[cu_blockOffsetConvertFloat(coords[2])];
}

Blockdata *cu_posToBlock(i64 x, i64 y, i64 z, u64 *chunkindex) {
	/* Position (integer) wrapper for cu_coordsToBlock */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	return cu_coordsToBlock(coords, chunkindex);
}

i32 cu_AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2) {
	/* Returns whether or not two 3D boxes intersect on all 3 axes */

	/* First normalize dimensions and corner */
#define _NORMALIZEDIM(_DIM, _CORNER, _I) \
	if (_DIM[_I] < 0.0f) { _DIM[_I] = fabsf(_DIM[_I]); _CORNER[_I] -= _DIM[_I]; }
	_NORMALIZEDIM(dim1, corner1, 0); _NORMALIZEDIM(dim1, corner1, 1); _NORMALIZEDIM(dim1, corner1, 2);
	_NORMALIZEDIM(dim2, corner2, 0); _NORMALIZEDIM(dim2, corner2, 1); _NORMALIZEDIM(dim2, corner2, 2);
#undef _NORMALIZED

#define _AXISCOMPARE(_I) \
	if ((corner1[_I]) + (dim1[_I]) < (corner2[_I]) || (corner1[_I]) > (corner2[_I]) + (dim2[_I])) return 0;
	_AXISCOMPARE(0); _AXISCOMPARE(1); _AXISCOMPARE(2);
#undef _AXISCOMPARE

	return 1;
}

void cu_deferRemesh(usf_skiplist *toremesh, u64 chunkindex) {
	/* Add a chunk index to the list, deferring its remeshing call to when all
	 * chunks are known (avoids duplicates). The skiplist must exist. */

	usf_skset(toremesh, chunkindex, USFTRUE);
}

void cu_deferArea(usf_skiplist *toremesh, i64 x, i64 y, i64 z) {
	/* Defers a 3x3 box around the specified coordinates for remeshing.
	 * The skiplist must exist. */

	i32 a, b, c;
	u64 chunkindex;
	for (a = -1; a < 2; a++) for (b = -1; b < 2; b++) for (c = -1; c < 2; c++) {
		cu_posToBlock(x + a, y + b, z + c, &chunkindex);
		cu_deferRemesh(toremesh, chunkindex);
	}
}

void cu_doRemesh(usf_skiplist *toremesh) {
	/* Remeshes all unique chunk indices in the deferred list. The list is not free'd. */

	u64 i;
	usf_skipnode *node;
	for (node = toremesh->base[0], i = 0; i < toremesh->size; node = node->nextnodes[0], i++)
		cu_asyncRemeshChunk(node->index);
}

static i32 getBlockmesh(Blockmesh *blockmesh, u64 id, u64 variant, Rotation rotation, i64 x, i64 y, i64 z) {
	/* Return a valid blockmesh containing adjusted vertex positions for a block
	 * of type id of variant variant with rotation rotation at position xyz */

	if (id >= MAX_BLOCK_ID) return -1; /* Illegal ID */
	if (variant >= MAX_BLOCK_VARIANT[id]) variant = 0; /* Default to 0; used by software-determined variants */

	Blockmesh *template;
	template = &BLOCKMESHES[id][variant];

	/* Copy state to scratchpad ; safe as buffer sizes are checked in parsing (renderutils) */
	memcpy(blockmesh->opaqueVertices, template->opaqueVertices, sizeof(f32) * template->count[0]);
	memcpy(blockmesh->transVertices, template->transVertices, sizeof(f32) * template->count[1]);
	memcpy(blockmesh->opaqueIndices, template->opaqueIndices, sizeof(u32) * template->count[2]);
	memcpy(blockmesh->transIndices, template->transIndices, sizeof(u32) * template->count[3]);
	memcpy(blockmesh->count, template->count, sizeof(template->count)); /* Get counts (never changes) */

	mat4 adjust, rotAdjust;
	vec3 posAdjust = {(f32) x, (f32) y, (f32) z};
	cu_translocationMatrix(adjust, posAdjust, rotation);
	cu_rotationMatrix(rotAdjust, rotation, MESHCENTER);

	/* Rotate, translate and adjust vertex coords and normals */
	u64 i;
	f32 *vertex;
#define _TRANSLOCATE(_COUNT, _VERTEX) \
	for (i = 0; i < template->count[_COUNT] / NMEMB_VERTEX; i++) { \
		vertex = blockmesh->_VERTEX + i * NMEMB_VERTEX; \
		glm_mat4_mulv3(adjust, vertex, 1.0f, vertex); \
		glm_mat4_mulv3(rotAdjust, vertex + 3, 1.0f, vertex + 3); /* Rotate normals */ \
	}
	_TRANSLOCATE(0, opaqueVertices);
	_TRANSLOCATE(1, transVertices);
#undef _TRANSLOCATE

	return 0;
}

static usf_compatibility_int pushRawmesh(void *chunkindexptr) {
	/* Push a new rawmesh to meshqueue for transfer to the GPU. Called asynchronously from cu_asyncRemeshChunk */

	u64 chunkindex; /* Retrieve argument */
	chunkindex = (* (u64 *) chunkindexptr);

	i64 x, y, z; /* Get chunk position */
#define _SIGNED21CAST64(_N) ((i64) ((_N) | (_N & (1 << 20) ? (u64) ~CHUNKCOORDMASK : 0)))
	x = _SIGNED21CAST64(chunkindex >> 42);
	y = _SIGNED21CAST64((chunkindex >> 21) & CHUNKCOORDMASK);
	z = _SIGNED21CAST64(chunkindex & CHUNKCOORDMASK);
#undef _SIGNED21CAST64

	Chunkdata chunk, *chunkptr;
	if ((chunkptr = (Chunkdata *) usf_inthmget(chunkmap_, chunkindex).p) == NULL) {
		fprintf(stderr, "Chunk at %"PRId64" %"PRId64" %"PRId64" does not exist, aborting.\n", x, y, z);
		exit(RSM_EXIT_NOCHUNK);
	}
	/* Get local copy independent of chunkmap_ */
	usf_mtxlock(chunkmap_->lock);
	memcpy(&chunk, chunkptr, sizeof(Chunkdata)); /* Safe since chunks cannot be deleted */
	usf_mtxunlock(chunkmap_->lock);

	f32 culled[4 * NMEMB_VERTEX * 6], *cullbuf; /* culled is misnomer since it holds not-culled faces */
	static const Rotation FROMNORTH[7] = { NORTH, NORTH, EAST, SOUTH, WEST, DOWN, UP };
	static const Rotation FROMWEST[7] = { WEST, WEST, NORTH, EAST, SOUTH, WEST, WEST };
	static const Rotation FROMSOUTH[7] = { SOUTH, SOUTH, WEST, NORTH, EAST, UP, DOWN };
	static const Rotation FROMEAST[7] = { EAST, EAST, SOUTH, WEST, NORTH, EAST, EAST };
	static const Rotation FROMUP[7] = { UP, UP, UP, UP, UP, NORTH, SOUTH };
	static const Rotation FROMDOWN[7] = { DOWN, DOWN, DOWN, DOWN, DOWN, SOUTH, NORTH };
	u32 culltrans;

	/* Template blockmesh */
	f32 ov_buf[RSM_MAX_BLOCKMESH_VERTICES], tv_buf[RSM_MAX_BLOCKMESH_VERTICES];
	u32 oi_buf[RSM_MAX_BLOCKMESH_INDICES], ti_buf[RSM_MAX_BLOCKMESH_INDICES];
	Blockmesh blockmesh = {
		.opaqueVertices = ov_buf, .transVertices = tv_buf,
		.opaqueIndices = oi_buf, .transIndices = ti_buf
	};

	/* Index offsets for each blockmesh added to the rawmesh */
	u32 runningOpaqueIndexOffset, runningTransIndexOffset, i;
	runningOpaqueIndexOffset = runningTransIndexOffset = 0;

	/* Scratchpad buffers are alloc'd with maximum possible size. Cannot make static due to multithreading. */
	Rawmesh *rawmesh;
	rawmesh = malloc(sizeof(Rawmesh));
	rawmesh->chunkindex = chunkindex;

	u8 *buffers; /* Single allocation */
	buffers = malloc(OV_BUFSZ + TV_BUFSZ + OI_BUFSZ + TI_BUFSZ);
	f32 *ov_bufptr, *tv_bufptr;
	ov_bufptr = rawmesh->opaqueVertexBuffer = (f32 *) (buffers);
	tv_bufptr = rawmesh->transVertexBuffer = (f32 *) (buffers += OV_BUFSZ);
	u32 *oi_bufptr, *ti_bufptr;
	oi_bufptr = rawmesh->opaqueIndexBuffer = (u32 *) (buffers += TV_BUFSZ);
	ti_bufptr = rawmesh->transIndexBuffer = (u32 *) (buffers += OI_BUFSZ);

	i64 a, b, c;
	Blockdata block;
	Blockdata *neighbor, *neighborup, *neighbordown, *neighbortop;
	for (a = 0; a < CHUNKSIZE; a++)
	for (b = 0; b < CHUNKSIZE; b++)
	for (c = 0; c < CHUNKSIZE; c++) {
		block = chunk[a][b][c];

		if (block.id == 0) continue; /* Air; do not remesh */

		if (getBlockmesh(&blockmesh, block.id, block.variant, block.rotation,
				x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c)) {
			fprintf(stderr, "Warning: deleting illegal block ID %"PRIu16" variant %"PRIu8
					" at coordinates %"PRId64" %"PRId64" %"PRId64".\n",
					block.id, block.variant, x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);
			memset(cu_posToBlock(x * CHUNKSIZE + a, y*CHUNKSIZE + b, z * CHUNKSIZE + c, NULL),
					0, sizeof(Blockdata));
			continue;
		}

		/* For fullblock (must be all opaque/trans) culling, meshdata must be four vertices per face and
		 * respect the order : front, left, back, right, top, bottom */
		cullbuf = NULL;
		if (block.metadata & RSM_BIT_CULLFACES) {
			if ((culltrans = !(block.metadata & RSM_BIT_CONDUCTOR))) cullbuf = blockmesh.transVertices;
			else cullbuf = blockmesh.opaqueVertices;

			i = 0; /* Valid faces processed */

			/* Checks the block at X, Y, Z, and cull vertices at index FACE if it CULLFACES too. */
#define _CHECKFACE(_X, _Y, _Z, _FACE) \
			neighbor = cu_posToBlock(_X, _Y, _Z, NULL); \
			if (!(neighbor->metadata & RSM_BIT_CULLFACES) \
					|| (!(neighbor->metadata & RSM_BIT_CONDUCTOR)) != culltrans) { \
				memcpy(culled + 4 * NMEMB_VERTEX * i, /* Next free spot */ \
						cullbuf + 4 * NMEMB_VERTEX * (_FACE-1), /* Culled face */ \
						4 * NMEMB_VERTEX * sizeof(f32)); /* Size of face */ \
				i++; \
			}
			/* Rotation is decremented by 1 in CHECKFACE to yield true face index */
#define xpos (x * CHUNKSIZE + a)
#define ypos (y * CHUNKSIZE + b)
#define zpos (z * CHUNKSIZE + c)
			_CHECKFACE(xpos, ypos, zpos + 1, FROMNORTH[block.rotation]);
			_CHECKFACE(xpos + 1, ypos, zpos, FROMWEST[block.rotation]);
			_CHECKFACE(xpos, ypos, zpos - 1, FROMSOUTH[block.rotation]);
			_CHECKFACE(xpos - 1, ypos, zpos, FROMEAST[block.rotation]);
			_CHECKFACE(xpos, ypos + 1, zpos, FROMUP[block.rotation]);
			_CHECKFACE(xpos, ypos - 1, zpos, FROMDOWN[block.rotation]);
			memcpy(cullbuf, culled, 4 * NMEMB_VERTEX * i * sizeof(f32));
#undef _CHECKFACE

			/* Adjust counts */
			blockmesh.count[culltrans ? 3 : 2] = 6 * i;
			blockmesh.count[culltrans ? 1 : 0] = NMEMB_VERTEX * 4 * i;
		}

		/* Special case for software-variants */
		f32 *uadjust, u, v;
		u32 j, k, digit[3];
		switch (block.id) {
			case RSM_BLOCK_RESISTOR:
			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				if (cullbuf == NULL) {
					fprintf(stderr, "Block ID %"PRIu16" at %"PRId64", %"PRId64", %"PRId64" has "
							"no face culling (illegal state); Cannot display variant.\n",
							block.id, xpos, ypos, zpos);
					break;
				}

				/* I am a bit lazy, so all of these have the same type of numerical display */
				blockmesh.count[culltrans ? 3 : 2] += 6 * 12; /* Re-use indices */
				blockmesh.count[culltrans ? 1 : 0] += NMEMB_VERTEX * 4 * 12; /* Now vertices */

				memmove(cullbuf + 4 * NMEMB_VERTEX * i, /* After rendered faces */
						cullbuf + 4 * NMEMB_VERTEX * 6, /* After all possible faces */
						4 * NMEMB_VERTEX * 12 * sizeof(f32)); /* Get display triangles */

				uadjust = cullbuf + 4 * NMEMB_VERTEX * i + 6;
				digit[0] = (block.variant / 100) % 10;
				digit[1] = (block.variant / 10) % 10;
				digit[2] = (block.variant / 1) % 10;
				for (j = 0; j < 4; j++) {
#define _ADJUSTU(OFFSET) \
	*uadjust = (f32) (digit[k] + OFFSET) * (3.0f/RSM_BLOCK_TEXTURE_SIZE_PIXELS); \
	uadjust += NMEMB_VERTEX;
					for (k = 0; k < 3; k++) { _ADJUSTU(0); _ADJUSTU(1); _ADJUSTU(1); _ADJUSTU(0); }
#undef _ADJUSTU
				}
				break;
			case RSM_BLOCK_WIRE:
				/* 3 steps to render a wire
				 * First: display signal strength
				 * Second: Adjust wire color
				 * Third: remove unnecessary indices */

				/* Display signal strength */
				digit[0] = (block.variant / 100) % 10;
				digit[1] = (block.variant / 10) % 10;
				digit[2] = (block.variant / 1) % 10;
				for (j = 0; j < 3; j++) {
#define _ADJUSTU(VERTEX, OFFSET) \
	blockmesh.transVertices[(j * 8 + VERTEX) * 8 + 6] = \
			(f32) (digit[j] + OFFSET) * (3.0f/RSM_BLOCK_TEXTURE_SIZE_PIXELS);
					/* Pattern here matches wire.mesh */
					_ADJUSTU(0, 0); _ADJUSTU(1, 1); _ADJUSTU(2, 0); _ADJUSTU(3, 1);
					_ADJUSTU(4, 1); _ADJUSTU(5, 0); _ADJUSTU(6, 1); _ADJUSTU(7, 0);
#undef _ADJUSTU
				}

				/* Adjust wire color */
				u = (block.variant % 32) * (1.0f/32.0f) + (0.5f/32.0f); /* UVs are offsets from (0,0) */
				v = (block.variant / 32) * ((1.0f / RSM_BLOCK_TEXTURE_SIZE_PIXELS) / (f32) NBLOCKTEXTURES);
				for (j = 0; j < 64; j++) { /* Can bunch together everything in 64 vertices */
					blockmesh.opaqueVertices[j * 8 + 6] += u;
					blockmesh.opaqueVertices[j * 8 + 7] += v;
				}

				/* Remove unnecessary indices */
				j = 36; k = 240; /* Starting at index 36, 240 indices left */
				u8 connect[4] = {0, 0, 0, 0}; /* North West South West */
				u8 straighten[2] = {0, 0}; /* Defer */

#define WIRECONNECT_ALL (neighbor->metadata & RSM_BIT_WIRECONNECT_ALL) /* ROT 1 = NS, ROT 0 = WE */
#define WIRECONNECT_LINE(R) ((neighbor->metadata&RSM_BIT_WIRECONNECT_LINE) && ((neighbor->rotation&1)==((R)&1)))
#define _BLOCKAT(_X, _Y, _Z) (cu_posToBlock(_X, _Y, _Z, NULL))
				neighbortop = _BLOCKAT(xpos, ypos + 1, zpos);
#define _NEIGHBORS(_X, _Y, _Z) \
	neighbor = _BLOCKAT(_X, _Y, _Z); \
	neighborup = _BLOCKAT(_X, _Y + 1, _Z); \
	neighbordown = _BLOCKAT(_X, _Y - 1, _Z);
#define CONNECT(I) \
	connect[I] = (WIRECONNECT_ALL || WIRECONNECT_LINE(I + 1) \
		|| (neighborup->id == RSM_BLOCK_WIRE && !(neighbortop->metadata & RSM_BIT_CONDUCTOR)) \
		|| (neighbordown->id == RSM_BLOCK_WIRE && !(neighbor->metadata & RSM_BIT_CONDUCTOR)));
				_NEIGHBORS(xpos, ypos, zpos + 1); CONNECT(0); _NEIGHBORS(xpos + 1, ypos, zpos); CONNECT(1);
				_NEIGHBORS(xpos, ypos, zpos - 1); CONNECT(2); _NEIGHBORS(xpos - 1, ypos, zpos); CONNECT(3);

				/* Straighten wire if it doesn't turn */
				if (!(connect[0] || connect[2])) straighten[0] = 1;
				if (!(connect[1] || connect[3])) straighten[1] = 1;
				if (straighten[0]) connect[1] = connect[3] = 1;
				if (straighten[1]) connect[0] = connect[2] = 1;

				/* Ground connections */
				for (i = 0; i < 4; i++) {
					if (!connect[i])
						memmove(blockmesh.opaqueIndices + j, blockmesh.opaqueIndices + j + 30,
								(k -= 30) * sizeof(u32));
					else j += 30;
				}
#undef _NEIGHBORS
#define _NEIGHBORS(_X, _Y, _Z) neighbor = cu_posToBlock(_X, _Y + 1, _Z, NULL);
#define _CULLINDICES \
	if (!(neighbor->id == RSM_BLOCK_WIRE) || (neighbortop->metadata & RSM_BIT_CONDUCTOR)) \
		memmove(blockmesh.opaqueIndices + j, blockmesh.opaqueIndices + j + 30, (k -= 30) * sizeof(u32)); \
	else j += 30;
				/* Upwards connections */
				_NEIGHBORS(xpos, ypos, zpos + 1); _CULLINDICES; /* North */
				_NEIGHBORS(xpos + 1, ypos, zpos); _CULLINDICES; /* West */
				_NEIGHBORS(xpos, ypos, zpos - 1); _CULLINDICES; /* South */
				_NEIGHBORS(xpos - 1, ypos, zpos); _CULLINDICES; /* East */
#undef _CULLINDICES
#undef _NEIGHBORS
#undef CONNECT
#undef _BLOCKAT
#undef WIRECONNECT_LINE
#undef WIRECONNECT_ALL
				blockmesh.count[2] = j; /* Adjust index count */
				break;
		}

		/* Adjust indices with running total. All vertices assumed to be used at least once */
		for (i = 0; i < blockmesh.count[2]; i++) blockmesh.opaqueIndices[i] += runningOpaqueIndexOffset;
		for (i = 0; i < blockmesh.count[3]; i++) blockmesh.transIndices[i] += runningTransIndexOffset;
		runningOpaqueIndexOffset += (blockmesh.count[0] / NMEMB_VERTEX);
		runningTransIndexOffset += (blockmesh.count[1] / NMEMB_VERTEX);

		/* Copy to rawmesh while updating pointers */
		memcpy(ov_bufptr, blockmesh.opaqueVertices, blockmesh.count[0] * sizeof(f32));
		memcpy(tv_bufptr, blockmesh.transVertices, blockmesh.count[1] * sizeof(f32));
		memcpy(oi_bufptr, blockmesh.opaqueIndices, blockmesh.count[2] * sizeof(u32));
		memcpy(ti_bufptr, blockmesh.transIndices, blockmesh.count[3] * sizeof(u32));

		/* Offset pointers */
		ov_bufptr += blockmesh.count[0]; tv_bufptr += blockmesh.count[1];
		oi_bufptr += blockmesh.count[2]; ti_bufptr += blockmesh.count[3];
	}

	/* Set member counts; will always be positive */
	rawmesh->nOV = (u64) (ov_bufptr - rawmesh->opaqueVertexBuffer);
	rawmesh->nTV = (u64) (tv_bufptr - rawmesh->transVertexBuffer);
	rawmesh->nOI = (u64) (oi_bufptr - rawmesh->opaqueIndexBuffer);
	rawmesh->nTI = (u64) (ti_bufptr - rawmesh->transIndexBuffer);

	usf_enqueue(meshqueue_, USFDATAP(rawmesh));

	free(chunkindexptr); /* Cleanup argument; it was allocated to allow for thread detachment from callee */
	return 0;
}
