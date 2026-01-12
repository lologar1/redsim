#include "chunkutils.h"

Blockmesh **blockmeshes; /* Populated by parseBlockdata */

int32_t getBlockmesh(Blockmesh *blockmesh, uint32_t id, uint32_t variant,
		Rotation rotation, int64_t x, int64_t y, int64_t z);
void *pushRawmesh(void *chunkindexptr);

void cu_asyncRemeshChunk(uint64_t chunkindex) {
	/* Asynchronously pushes a new rawmesh to be sent to the GPU */
	int32_t rc;
	pthread_t id;
	uint64_t *arg;

	arg = malloc(sizeof(uint64_t)); /* Allow thread to operate separately; it cleans up its argument after */
	*arg = chunkindex;

	if ((rc = pthread_create(&id, NULL, pushRawmesh, arg))) {
		fprintf(stderr, "Error creating thread (async remesh): error code %d, aborting.\n", rc);
		exit(RSM_EXIT_THREADFAIL);
	}

	if ((rc = pthread_detach(id))) { /* Since thread is detached, cleanup is automatic */
		fprintf(stderr, "Error detaching thread (async remesh): error code %d, aborting.\n", rc);
		exit(RSM_EXIT_THREADFAIL);
	}
}

void cu_updateMeshlist(void) {
    /* Generate new required meshes since movement from lastPosition
     * and remove out of render distance ones */

	GLuint **meshlist, *mesh;
	uint64_t meshindex;
	int64_t i[3], chunk[3], pos[3];

	meshlist = meshes;
	nmesh = 0;
	pos[0] = position[0] / CHUNKSIZE; pos[1] = position[1] / CHUNKSIZE; pos[2] = position[2] / CHUNKSIZE;

	for (i[0] = -RSM_LOADING_DISTANCE; i[0] < RSM_LOADING_DISTANCE + 1; i[0]++) {
		chunk[0] = pos[0] + i[0];
		for (i[1] = -RSM_LOADING_DISTANCE; i[1] < RSM_LOADING_DISTANCE + 1; i[1]++) {
			chunk[1] = pos[1] + i[1];
			for (i[2] = -RSM_LOADING_DISTANCE; i[2] < RSM_LOADING_DISTANCE + 1; i[2]++) {
				chunk[2] = pos[2] + i[2];

				/* Set mesh for this chunk */
				meshindex = TOCHUNKINDEX(chunk[0], chunk[1], chunk[2]);
				mesh = (GLuint *) usf_inthmget(meshmap, meshindex).p;

				if (mesh == NULL) continue; /* Uninitialized chunk */

                *meshlist++ = mesh;
                nmesh++;
            }
        }
	}
}

void cu_generateMeshlist(void) {
    /* Regenerate entire meshes for all chunks in render distance */
	uint64_t chunkvolume;

    /* Allocate enough mem for all chunks in cubic view.
     * Note that each cell is a pointer to a chunk mesh so even with
     * 100 chunk render distance, the size is only
     * (100 * 2 + 1) ^ 3 * 8 = 64964808 = 65 MB of memory */

	/* Cleanup old meshlist ; on startup it is null but free(NULL) is safe */
    free(meshes); /* OK since all mesh chunks are stored in meshmap */

	chunkvolume = pow(RSM_LOADING_DISTANCE * 2 + 1, 3);
    meshes = malloc(chunkvolume * sizeof(GLuint *)); /* Store meshes for all chunks */

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

Blockdata *cu_coordsToBlock(vec3 coords, uint64_t *chunkindex) {
	/* Return the blockdata matching these absolute world offsets. If it is provided, also set chunk to
	 * the chunk of the block. The chunk is created as empty if it doesn't exist.
	 * Important: this is the _proper_ way of safely accessing a block. Directly querying chunkmap with
	 * a chunk index then checking for an offset within it may yield an uninitialized chunk */
#define COORDSTOCHUNKINDEX(V) \
	TOCHUNKINDEX(cu_chunkOffsetConvertFloat(V[0]), \
			cu_chunkOffsetConvertFloat(V[1]), \
			cu_chunkOffsetConvertFloat(V[2]))

#define COORDSTOBLOCKDATA(V, CHUNK) \
	 (&(*CHUNK)[cu_blockOffsetConvertFloat(V[0])] \
	  [cu_blockOffsetConvertFloat(V[1])] \
	  [cu_blockOffsetConvertFloat(V[2])])

	uint64_t index;
	Chunkdata *chunkdata;

	index = COORDSTOCHUNKINDEX(coords);

	/* Get chunk and allocate it if null */
	if ((chunkdata = (Chunkdata *) usf_inthmget(chunkmap, index).p) == NULL)
		usf_inthmput(chunkmap, index, USFDATAP(chunkdata = calloc(1, sizeof(Chunkdata))));

	if (chunkindex) *chunkindex = index; /* Pass chunk index */

	return COORDSTOBLOCKDATA(coords, chunkdata); /* Return blockdata */
#undef COORDSTOCHUNKINDEX
#undef COORDSTOBLOCKDATA
}

int64_t cu_chunkOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its chunk offset equivalent */
	return (int64_t) floorf(absoluteComponent / CHUNKSIZE);
}

uint64_t cu_blockOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its block offset (within a chunk) equivalent */
	return absoluteComponent < 0 ?
		CHUNKSIZE - 1 - ((uint64_t) -floorf(absoluteComponent) - 1) % CHUNKSIZE :
		(uint64_t) absoluteComponent % CHUNKSIZE;
}

int32_t cu_AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2) {
	/* Returns whether or not two 3D boxes intersect on all 3 axes */

	/* First normalize dimensions and corner */
#define NORMALIZEDIM(dim, corner, i) if (dim[i] < 0.0f) { dim[i] = fabsf(dim[i]); corner[i] -= dim[i]; }
	NORMALIZEDIM(dim1, corner1, 0); NORMALIZEDIM(dim1, corner1, 1); NORMALIZEDIM(dim1, corner1, 2);
	NORMALIZEDIM(dim2, corner2, 0); NORMALIZEDIM(dim2, corner2, 1); NORMALIZEDIM(dim2, corner2, 2);

#define AXISCOMPARE(i) \
	if ((corner1[i]) + (dim1[i]) < (corner2[i]) || (corner1[i]) > (corner2[i]) + (dim2[i])) return 0;
	AXISCOMPARE(0); AXISCOMPARE(1); AXISCOMPARE(2);

	return 1;
}

void pathcat(char *destination, int32_t n, ...) {
	/* Concatenates a path into destination */
	va_list args;
	va_start(args, n);

	int32_t success;
	success = usf_vstrcat(destination, RSM_MAX_PATH_NAME_LENGTH, n, args);
	va_end(args);

	if (!success) {
		fprintf(stderr, "Concatenation of %d paths exceeds max buffer length %u, aborting.\n",
				n, RSM_MAX_PATH_NAME_LENGTH);
		exit(RSM_EXIT_EXCBUF);
	}
}

int32_t getBlockmesh(Blockmesh *blockmesh, uint32_t id, uint32_t variant, Rotation rotation,
		int64_t x, int64_t y, int64_t z) {
	/* Return a valid blockmesh containing adjusted vertex positions for a block
	 * of type id of variant variant with rotation rotation at position xyz */
	if (id >= MAX_BLOCK_ID) return -1; /* Illegal ID */
	if (variant >= MAX_BLOCK_VARIANT[id]) variant = 0; /* Default to 0; used by software-determined variants */

	Blockmesh *template;
	template = &blockmeshes[id][variant];

	/* Copy state to scratchpad ; safe as buffer sizes are checked in parsing (renderutils) */
	memcpy(blockmesh->opaqueVertices, template->opaqueVertices, sizeof(float) * template->count[0]);
	memcpy(blockmesh->transVertices, template->transVertices, sizeof(float) * template->count[1]);
	memcpy(blockmesh->opaqueIndices, template->opaqueIndices, sizeof(uint32_t) * template->count[2]);
	memcpy(blockmesh->transIndices, template->transIndices, sizeof(uint32_t) * template->count[3]);

	memcpy(blockmesh->count, template->count, sizeof(uint32_t) * 4); /* Get counts (never changes) */

	uint32_t i;
	mat4 adjust, rotAdjust;
	vec3 posAdjust = {(float) x, (float) y, (float) z};

	cu_translocationMatrix(adjust, posAdjust, rotation);
	cu_rotationMatrix(rotAdjust, rotation, MESHCENTER); /* Needed for normal adjustment */

	float *vertexPos;

	/* Rotate, translate and adjust vertex coords (and normals for rotation) for opaque vertices */
#define TRANSLOCATEVERTICES(COUNTSECTION, VERTEXSECTION) \
	for (i = 0; i < template->count[COUNTSECTION] / (sizeof(Vertex)/sizeof(float)); i++) { \
		vertexPos = blockmesh->VERTEXSECTION + i * (sizeof(Vertex)/sizeof(float)); \
		glm_mat4_mulv3(adjust, vertexPos, 1.0f, vertexPos); \
		glm_mat4_mulv3(rotAdjust, vertexPos + 3, 1.0f, vertexPos + 3); /* Rotate normals */ \
	}

	TRANSLOCATEVERTICES(0, opaqueVertices);
	TRANSLOCATEVERTICES(1, transVertices);
#undef TRANSLOCATEVERTICES

	return 0;
}

void *pushRawmesh(void *chunkindexptr) {
	/* Push a new rawmesh to meshqueue for transfer to the GPU. Called asynchronously with pthread */
	uint64_t chunkindex;
	int64_t x, y, z, a, b, c;
	Chunkdata *chunk;
	Blockdata block;

	chunkindex = (* (uint64_t *) chunkindexptr);

	/* Get chunk index */
#define SIGNED21CAST64(N) ((N) | (N & (1 << 20) ? (uint64_t) ~CHUNKCOORDMASK : 0))
	x = SIGNED21CAST64(chunkindex >> 42);
	y = SIGNED21CAST64((chunkindex >> 21) & CHUNKCOORDMASK);
	z = SIGNED21CAST64(chunkindex & CHUNKCOORDMASK);

	if ((chunk = (Chunkdata *) usf_inthmget(chunkmap, chunkindex).p) == NULL) {
		fprintf(stderr, "Chunk at %"PRId64" %"PRId64" %"PRId64" does not exist, aborting.\n", x, y, z);
		exit(RSM_EXIT_NOCHUNK);
	}

	float culled[4 * NMEMB_VERTEX * 6], *cullbuf; /* Face culling buffer and rotation information */
	static const Rotation FROMNORTH[7] = { NORTH, NORTH, EAST, SOUTH, WEST, DOWN, UP };
	static const Rotation FROMWEST[7] = { WEST, WEST, NORTH, EAST, SOUTH, WEST, WEST };
	static const Rotation FROMSOUTH[7] = { SOUTH, SOUTH, WEST, NORTH, EAST, UP, DOWN };
	static const Rotation FROMEAST[7] = { EAST, EAST, SOUTH, WEST, NORTH, EAST, EAST };
	static const Rotation FROMUP[7] = { UP, UP, UP, UP, UP, NORTH, SOUTH };
	static const Rotation FROMDOWN[7] = { DOWN, DOWN, DOWN, DOWN, DOWN, SOUTH, NORTH };
	uint32_t culltrans;

	/* Template blockmesh */
	float ov_buf[RSM_MAX_BLOCKMESH_VERTICES], tv_buf[RSM_MAX_BLOCKMESH_VERTICES];
	uint32_t oi_buf[RSM_MAX_BLOCKMESH_INDICES], ti_buf[RSM_MAX_BLOCKMESH_INDICES];
	Blockmesh blockmesh = {
		.opaqueVertices = ov_buf, .transVertices = tv_buf,
		.opaqueIndices = oi_buf, .transIndices = ti_buf
	};

	/* Index offsets for each blockmesh added to the rawmesh */
	uint32_t runningOpaqueIndexOffset, runningTransIndexOffset, i;
	runningOpaqueIndexOffset = runningTransIndexOffset = 0;

	/* Scratchpad buffers for the entire mesh. Allocating maximum buffer size to avoid dynamic handling, as
	 * although it is a large allocation, it is always the same size, avoiding fragmentation */
	Rawmesh *rawmesh = malloc(sizeof(Rawmesh));
	void *buffers = malloc(ov_bufsiz + tv_bufsiz + oi_bufsiz + ti_bufsiz);
	rawmesh->chunkindex = chunkindex;
	float *ov_bufptr = rawmesh->opaqueVertexBuffer = (float *) buffers;
	float *tv_bufptr = rawmesh->transVertexBuffer = (float *) (buffers += ov_bufsiz);
	uint32_t *oi_bufptr = rawmesh->opaqueIndexBuffer = (uint32_t *) (buffers += tv_bufsiz);
	uint32_t *ti_bufptr = rawmesh->transIndexBuffer = (uint32_t *) (buffers += oi_bufsiz);

	for (a = 0; a < CHUNKSIZE; a++) {
		for (b = 0; b < CHUNKSIZE; b++) {
			for (c = 0; c < CHUNKSIZE; c++) {
				/* Get blockmesh for block at this location blockmeshes will regenerate their
				 * vertex arrays to apply rotation and translation to place them correctly in the world.
				 * Indices must be offset according to the running total for this opaque/trans mesh */
				block = (*chunk)[a][b][c];

				if (block.id == 0) continue; /* Air */

				if (getBlockmesh(&blockmesh, block.id, block.variant, block.rotation,
						x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c)) {
					fprintf(stderr, "Warning: deleting illegal block ID %"PRIu16" variant %"PRIu8
							" at coordinates %"PRIu64" %"PRIu64" %"PRIu64".\n",
							block.id, block.variant, x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);
					memset(cu_coordsToBlock(VEC3(x * CHUNKSIZE + a, y*CHUNKSIZE + b, z * CHUNKSIZE + c), NULL),
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
#define CHECKFACE(X, Y, Z, FACE) \
					neighbor = cu_coordsToBlock(VEC3(X, Y, Z), NULL); \
					if (!(neighbor->metadata & RSM_BIT_CULLFACES) \
							|| (!(neighbor->metadata & RSM_BIT_CONDUCTOR)) != culltrans) { \
						memcpy(culled + 4 * NMEMB_VERTEX * i, /* Next free spot */ \
								cullbuf + 4 * NMEMB_VERTEX * (FACE-1), /* Culled face */ \
								4 * NMEMB_VERTEX * sizeof(float)); /* Size of face */ \
						i++; \
					}

					/* Note that the rotation is decremented by 1 in CHECKFACE to yield true face index */
					Blockdata *neighbor;
					CHECKFACE(x*CHUNKSIZE + a, y*CHUNKSIZE + b, z*CHUNKSIZE + c + 1, FROMNORTH[block.rotation]);
					CHECKFACE(x*CHUNKSIZE + a + 1, y*CHUNKSIZE + b, z*CHUNKSIZE + c, FROMWEST[block.rotation]);
					CHECKFACE(x*CHUNKSIZE + a, y*CHUNKSIZE + b, z*CHUNKSIZE + c - 1, FROMSOUTH[block.rotation]);
					CHECKFACE(x*CHUNKSIZE + a - 1, y*CHUNKSIZE + b, z*CHUNKSIZE + c, FROMEAST[block.rotation]);
					CHECKFACE(x*CHUNKSIZE + a, y*CHUNKSIZE + b + 1, z*CHUNKSIZE + c, FROMUP[block.rotation]);
					CHECKFACE(x*CHUNKSIZE + a, y*CHUNKSIZE + b - 1, z*CHUNKSIZE + c, FROMDOWN[block.rotation]);
					memcpy(cullbuf, culled, 4 * NMEMB_VERTEX * i * sizeof(float));
#undef CHECKFACE

					/* Adjust counts */
					blockmesh.count[culltrans ? 3 : 2] = 6 * i;
					blockmesh.count[culltrans ? 1 : 0] = NMEMB_VERTEX * 4 * i;
				}

				/* Special case for software-variants */
				float *uadjust, u, v;
				uint32_t j, k, digit[3];
				switch (block.id) {
					case RSM_BLOCK_RESISTOR:
					case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
					case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
						if (cullbuf == NULL) {
							fprintf(stderr, "Block ID %"PRIu16" at %"PRIu64", %"PRIu64", %"PRIu64" has "
									"no face culling (illegal state); Cannot display variant.\n",
									block.id, x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);
							break;
						}

						/* I am a bit lazy, so all of these have the same numerical display */
						blockmesh.count[culltrans ? 3 : 2] += 6 * 12; /* Re-use indices */
						blockmesh.count[culltrans ? 1 : 0] += NMEMB_VERTEX * 4 * 12; /* Now vertices */

						memmove(cullbuf + 4 * NMEMB_VERTEX * i, /* After rendered faces */
								cullbuf + 4 * NMEMB_VERTEX * 6, /* After all possible faces */
								4 * NMEMB_VERTEX * 12 * sizeof(float)); /* Get display triangles */

						uadjust = cullbuf + 4 * NMEMB_VERTEX * i + 6;
						digit[0] = (block.variant / 100) % 10;
						digit[1] = (block.variant / 10) % 10;
						digit[2] = (block.variant / 1) % 10;
						for (j = 0; j < 4; j++) {
#define ADJUSTU(OFFSET) \
	*uadjust = (digit[k] + OFFSET) * (3.0f/RSM_BLOCK_TEXTURE_SIZE_PIXELS); \
	uadjust += NMEMB_VERTEX;
							for (k = 0; k < 3; k++) { ADJUSTU(0); ADJUSTU(1); ADJUSTU(1); ADJUSTU(0); }
#undef ADJUSTU
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
#define ADJUSTU(VERTEX, OFFSET) \
	blockmesh.transVertices[(j * 8 + VERTEX) * 8 + 6] = \
		(digit[j] + OFFSET) * (3.0f/RSM_BLOCK_TEXTURE_SIZE_PIXELS);
							/* Pattern here matches wire.mesh */
							ADJUSTU(0, 0); ADJUSTU(1, 1); ADJUSTU(2, 0); ADJUSTU(3, 1);
							ADJUSTU(4, 1); ADJUSTU(5, 0); ADJUSTU(6, 1); ADJUSTU(7, 0);
#undef ADJUSTU
						}

						/* Adjust wire color */
						u = (block.variant % 32) * (1.0f/32.0f) + (0.5f/32.0f); /* UVs are offsets from (0,0) */
						v = (block.variant / 32) * ((1.0f / RSM_BLOCK_TEXTURE_SIZE_PIXELS) / ntextures);
						for (j = 0; j < 64; j++) { /* Can bunch together everything in 64 vertices */
							blockmesh.opaqueVertices[j * 8 + 6] += u;
							blockmesh.opaqueVertices[j * 8 + 7] += v;
						}

						/* Remove unnecessary indices */

						break;
				}

				/* Adjust indices with running total. All vertices assumed to be used at least once */
				for (i = 0; i < blockmesh.count[2]; i++) blockmesh.opaqueIndices[i] += runningOpaqueIndexOffset;
				for (i = 0; i < blockmesh.count[3]; i++) blockmesh.transIndices[i] += runningTransIndexOffset;
				runningOpaqueIndexOffset += blockmesh.count[0] / NMEMB_VERTEX;
				runningTransIndexOffset += blockmesh.count[1] / NMEMB_VERTEX;

				/* Copy to rawmesh while updating pointers */
				memcpy(ov_bufptr, blockmesh.opaqueVertices, blockmesh.count[0] * sizeof(float));
				memcpy(tv_bufptr, blockmesh.transVertices, blockmesh.count[1] * sizeof(float));
				memcpy(oi_bufptr, blockmesh.opaqueIndices, blockmesh.count[2] * sizeof(uint32_t));
				memcpy(ti_bufptr, blockmesh.transIndices, blockmesh.count[3] * sizeof(uint32_t));

				/* Offset pointers */
				ov_bufptr += blockmesh.count[0]; tv_bufptr += blockmesh.count[1];
				oi_bufptr += blockmesh.count[2]; ti_bufptr += blockmesh.count[3];
			}
		}
	}

	/* Set member counts */
	rawmesh->nOV = ov_bufptr - rawmesh->opaqueVertexBuffer;
	rawmesh->nTV = tv_bufptr - rawmesh->transVertexBuffer;
	rawmesh->nOI = oi_bufptr - rawmesh->opaqueIndexBuffer;
	rawmesh->nTI = ti_bufptr - rawmesh->transIndexBuffer;

	pthread_mutex_lock(&meshlock);
	usf_enqueue(meshqueue, USFDATAP(rawmesh));
	pthread_mutex_unlock(&meshlock);

	free(chunkindexptr); /* Cleanup argument; it was allocated to allow for thread detachment from callee */
	return NULL;
}
