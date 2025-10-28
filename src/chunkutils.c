#include "chunkutils.h"

extern Blockmesh ***blockmeshes;

void checkAndAddBlockmeshDataFloat(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		float **buffer, float *data, float **basebuffer);
void checkAndAddBlockmeshDataInteger(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		unsigned int **buffer, unsigned int *data, unsigned int **basebuffer);

/* Temporary buffers for vertices/indices scratchpad */
float *opaqueVertexBuffer, *transVertexBuffer;
unsigned int *opaqueIndexBuffer, *transIndexBuffer;
void remeshChunk(int64_t x, int64_t y, int64_t z) {
	/* Generate appropriate mesh array (VAO) for a given chunk and set it in meshmap */
	uint64_t chunkindex;
	GLuint *mesh, opaqueVAO, transVAO, opaqueVBO, transVBO, opaqueEBO, transEBO;
	unsigned int a, b, c;
	Chunkdata *chunk;
	Blockdata block;

	chunkindex = TOCHUNKINDEX(x, y, z);

	chunk = (Chunkdata *) usf_inthmget(chunkmap, chunkindex).p;

	if (chunk == NULL) {
		fprintf(stderr, "Chunk at %lu %lu %lu does not exist ; aborting.\n", x, y, z);
		exit(RSM_EXIT_NOCHUNK);
	}

	mesh = (GLuint *) usf_inthmget(meshmap, chunkindex).p;

	if (mesh == NULL) {
		/* No mesh is generated for this chunk; initialize GL structures */
		mesh = (GLuint *) malloc(4 * sizeof(GLuint));

		/* Generate buffers */
		glGenVertexArrays(1, &opaqueVAO);
		glGenVertexArrays(1, &transVAO);
		glGenBuffers(1, &opaqueVBO);
		glGenBuffers(1, &transVBO);
		glGenBuffers(1, &opaqueEBO);
		glGenBuffers(1, &transEBO);

		/* Set attributes */
		glBindVertexArray(opaqueVAO);
		glBindBuffer(GL_ARRAY_BUFFER, opaqueVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opaqueEBO);
		glEnableVertexAttribArray(0); /* Vertex position */
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
		glEnableVertexAttribArray(1); /* Normals */
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
		glEnableVertexAttribArray(2); /* Texture mappings */
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

		glBindVertexArray(transVAO);
		glBindBuffer(GL_ARRAY_BUFFER, transVBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, transEBO);
		glEnableVertexAttribArray(0); /* Vertex position */
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
		glEnableVertexAttribArray(1); /* Normals */
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
		glEnableVertexAttribArray(2); /* Texture mappings */
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

		glBindVertexArray(0); /* Unbind to avoid modification */
		mesh[0] = opaqueVAO;
		mesh[1] = transVAO;
		usf_inthmput(meshmap, chunkindex, USFDATAP(mesh)); /* Set mesh */
	}

	/* Scratchpad for getting correctly-translated blockmeshes */
	static float ov_buf[RSM_MAX_BLOCKMESH_VERTICES], tv_buf[RSM_MAX_BLOCKMESH_VERTICES];
	static unsigned int oi_buf[RSM_MAX_BLOCKMESH_INDICES], ti_buf[RSM_MAX_BLOCKMESH_INDICES];
	static Blockmesh blockmesh = {
		.opaqueVertices = ov_buf, .transVertices = tv_buf,
		.opaqueIndices = oi_buf, .transIndices = ti_buf
	};

	/* Adjust indices for each blockmesh added */
	unsigned int runningOpaqueIndexOffset, runningTransIndexOffset, i;
	runningOpaqueIndexOffset = runningTransIndexOffset = 0;

	float *ov_bufptr = opaqueVertexBuffer, *tv_bufptr = transVertexBuffer;
	unsigned int *oi_bufptr = opaqueIndexBuffer, *ti_bufptr = transIndexBuffer;

	for (a = 0; a < CHUNKSIZE; a++) {
		for (b = 0; b < CHUNKSIZE; b++) {
			for (c = 0; c < CHUNKSIZE; c++) {
				/* Get blockmesh for block at this location blockmeshes will regenerate their
				 * vertex arrays to apply rotation and translation to place them correctly in the world.
				 * Indices must be offset according to the running total for this opaque/trans mesh */
				block = (*chunk)[a][b][c];

				if (block.id == 0) continue; /* Air */

				getBlockmesh(&blockmesh, block.id, block.variant, block.rotation,
						x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);

				/* Adjust indices with running total. All vertices assumed to be used at least once */
				for (i = 0; i < blockmesh.count[2]; i++) blockmesh.opaqueIndices[i] += runningOpaqueIndexOffset;
				for (i = 0; i < blockmesh.count[3]; i++) blockmesh.transIndices[i] += runningTransIndexOffset;
				runningOpaqueIndexOffset += blockmesh.count[0] / (sizeof(Vertex) / sizeof(float));
				runningTransIndexOffset += blockmesh.count[1] / (sizeof(Vertex) / sizeof(float));

				/* Copy to scratchpad while updating pointers */
				/* TODO: cull if block is present on one side */
				memcpy(ov_bufptr, blockmesh.opaqueVertices, blockmesh.count[0] * sizeof(float));
				memcpy(tv_bufptr, blockmesh.transVertices, blockmesh.count[1] * sizeof(float));
				memcpy(oi_bufptr, blockmesh.opaqueIndices, blockmesh.count[2] * sizeof(unsigned int));
				memcpy(ti_bufptr, blockmesh.transIndices, blockmesh.count[3] * sizeof(unsigned int));

				/* Offset pointers */
				ov_bufptr += blockmesh.count[0]; tv_bufptr += blockmesh.count[1];
				oi_bufptr += blockmesh.count[2]; ti_bufptr += blockmesh.count[3];
			}
		}
	}

	unsigned int nOV = ov_bufptr - opaqueVertexBuffer, nTV = tv_bufptr - transVertexBuffer,
				 nOI = oi_bufptr - opaqueIndexBuffer, nTI = ti_bufptr - transIndexBuffer;

	GLint buffer; /* VAOs don't store VBO bindings, but only attributes. This is why we have to query it manually */
	glBindVertexArray(mesh[0]); /* Opaque VAO */
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer); /* Query buffer for this VAO */
	glBindBuffer(GL_ARRAY_BUFFER, buffer); /* We need the VBO to be able to copy vertex data */
	glBufferData(GL_ARRAY_BUFFER, nOV * sizeof(float), opaqueVertexBuffer, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, nOI * sizeof(unsigned int), opaqueIndexBuffer, GL_DYNAMIC_DRAW);

	glBindVertexArray(mesh[1]); /* Trans VAO */
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, nTV * sizeof(float), transVertexBuffer, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, nTI * sizeof(unsigned int), transIndexBuffer, GL_DYNAMIC_DRAW);

	glBindVertexArray(0); /* Unbind when done */

	/* Set number of elements to draw for this chunk */
	mesh[2] = nOI; mesh[3] = nTI;
}

void updateMeshlist(void) {
    /* Generate new required meshes since movement from lastPosition
     * and remove out of render distance ones */

	GLuint **meshlist, *mesh;
	unsigned int a, b, c;
	uint64_t meshindex;
	int64_t x, y, z;

	meshlist = meshes;
	nmesh = 0;

    for (a = 0; a < RENDER_DISTANCE * 2 + 1; a++) {
        x = ((int) position[0] / CHUNKSIZE + a - RENDER_DISTANCE);
        for (b = 0; b < RENDER_DISTANCE * 2 + 1; b++) {
            y = ((int) position[1] / CHUNKSIZE + b - RENDER_DISTANCE);
            for (c = 0; c < RENDER_DISTANCE * 2 + 1; c++) {
                z = ((int) position[2] / CHUNKSIZE + c - RENDER_DISTANCE);

				/* Set mesh for this chunk */
				meshindex = TOCHUNKINDEX(x, y, z);
				mesh = (GLuint *) usf_inthmget(meshmap, meshindex).p;

				if (mesh == NULL) continue; /* No mesh for this chunk; empty */

                *meshlist++ = mesh;
                nmesh++;
            }
        }
	}
}

void generateMeshlist(void) {
    /* Regenerate entire meshes for all chunks in render distance */
	uint64_t chunkvolume;

    /* Allocate enough mem for all chunks in cubic view.
     * Note that each cell is a pointer to a chunk mesh so even with
     * 100 chunk render distance, the size is only
     * (100 * 2 + 1) ^ 3 * 8 = 64964808 = 65 MB of memory */

	/* Cleanup old meshlist ; on startup it is null but free(NULL) is safe */
    free(meshes); /* OK since all mesh chunks are stored in meshmap */

	chunkvolume = pow(RENDER_DISTANCE * 2 + 1, 3);
    meshes = malloc(chunkvolume * sizeof(GLuint *)); /* Store meshes for all chunks */

	updateMeshlist();
}

void getBlockmesh(Blockmesh *blockmesh, unsigned int id, unsigned int variant, Rotation rotation,
		int64_t x, int64_t y, int64_t z) {
	/* Return a valid blockmesh containing adjusted vertex positions for a block
	 * of type id of variant variant with rotation rotation at position xyz */

	Blockmesh *template;
	template = blockmeshes[id][variant];

	/* Copy state to scratchpad ; safe as buffer sizes are checked in parsing (renderutils) */
	memcpy(blockmesh->opaqueVertices, template->opaqueVertices, sizeof(float) * template->count[0]);
	memcpy(blockmesh->transVertices, template->transVertices, sizeof(float) * template->count[1]);
	memcpy(blockmesh->opaqueIndices, template->opaqueIndices, sizeof(unsigned int) * template->count[2]);
	memcpy(blockmesh->transIndices, template->transIndices, sizeof(unsigned int) * template->count[3]);

	memcpy(blockmesh->count, template->count, sizeof(unsigned int) * 4); /* Get counts (never changes) */

	unsigned int i;
	mat4 adjust, rotAdjust;
	vec3 posAdjust = {(float) x, (float) y, (float) z};

	translocationMatrix(adjust, posAdjust, rotation);
	rotationMatrix(rotAdjust, rotation, MESHCENTER); /* Needed for normal adjustment */

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
}

void translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation) {
	/* Makes an appropriate translocation (translation + rotation) matrix given
	 * a translation vector (position) and a rotation */
	mat4 rotAdjust;
	rotationMatrix(rotAdjust, rotation, MESHCENTER);

	glm_mat4_identity(translocation);
	glm_translate(translocation, translation);
	glm_mat4_mul(translocation, rotAdjust, translocation);
}

void rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter) {
	/* Makes an appropriate rotation matrix for a given rotation */
	glm_mat4_identity(rotAdjust);

	switch (rotation) {
		case EAST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), UPVECTOR);
			break;
		case SOUTH:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(180), UPVECTOR);
			break;
		case WEST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), UPVECTOR);
			break;
		case UP:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), RIGHTVECTOR);
			break;
		case DOWN:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), RIGHTVECTOR);
			break;
		case NONE:
		case NORTH:
		case COMPLEX:
			/* A block is defined as NORTH by default. Complex is TODO */
			break;
	}
}

void pathcat(char *destination, int n, ...) {
	size_t size = 0;

	va_list sizes, args;
	va_start(args, n);
	va_copy(sizes, args);

	for (int i = 0; i < n; i++)
		size += strlen(va_arg(sizes, char *));
	va_end(sizes);

	if (size + 1 > RSM_MAX_PATH_NAME_LENGTH) {
		fprintf(stderr, "Concatenation of %d paths exceeds max buffer length %u, aborting.\n",
				n, RSM_MAX_PATH_NAME_LENGTH);
		exit(RSM_EXIT_EXCBUF);
	}

	strcpy(destination, va_arg(args, char *));
	for (int i = 1; i < n; i++)
		strcat(destination, va_arg(args, char *));

	va_end(args);
}
