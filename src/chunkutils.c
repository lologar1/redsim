#include "chunkutils.h"

void checkAndAddBlockmeshDataFloat(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		float **buffer, float *data, float **basebuffer);
void checkAndAddBlockmeshDataInteger(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		unsigned int **buffer, unsigned int *data, unsigned int **basebuffer);

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
		/* Newly created chunk, empty. */
		chunk = (Chunkdata *) calloc(1, sizeof(Chunkdata));
		usf_inthmput(chunkmap, chunkindex, USFDATAP(chunk));
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

	/* Temporary buffers for vertex data */
	float *opaqueVertices, *transVertices, *oVert, *tVert;
	unsigned int *opaqueIndices, *transIndices, *oInd, *tInd;

	/* Member count for vertex data */
	unsigned int opaqueVertexCount = 0, transVertexCount = 0, opaqueIndexCount = 0, transIndexCount = 0;
	unsigned int nMeshOpaqueVertex, nMeshTransVertex, nMeshOpaqueIndex, nMeshTransIndex;

	/* Scratchpad for getting correctly-translated blockmeshes */
	Blockmesh *blockmesh;
	blockmesh = (Blockmesh *) malloc(sizeof(Blockmesh));
	blockmesh->opaqueVertices = (float *) malloc(sizeof(float) * RSM_MAX_BLOCKMESH_VERTICES);
	blockmesh->transVertices = (float *) malloc(sizeof(float) * RSM_MAX_BLOCKMESH_VERTICES);
	blockmesh->opaqueIndices = (unsigned int *) malloc(sizeof(unsigned int) * RSM_MAX_BLOCKMESH_INDICES);
	blockmesh->transIndices = (unsigned int *) malloc(sizeof(unsigned int) * RSM_MAX_BLOCKMESH_INDICES);

	/* Dynamically-resizing buffer */
	unsigned int opaqueVertBufferSize = BUFSIZ, transVertBufferSize = BUFSIZ,
				 opaqueIndBufferSize = BUFSIZ, transIndBufferSize = BUFSIZ;
	oVert = opaqueVertices = malloc(BUFSIZ * sizeof(float));
	tVert = transVertices = malloc(BUFSIZ * sizeof(float));
	oInd = opaqueIndices = malloc(BUFSIZ * sizeof(unsigned int));
	tInd = transIndices = malloc(BUFSIZ * sizeof(unsigned int));

	unsigned int runningOpaqueIndexOffset, runningTransIndexOffset, i;
	runningOpaqueIndexOffset = runningTransIndexOffset = 0;

	for (a = 0; a < CHUNKSIZE; a++) {
		for (b = 0; b < CHUNKSIZE; b++) {
			for (c = 0; c < CHUNKSIZE; c++) {
				/* Get blockmesh for block at this location blockmeshes will regenerate their
				 * vertex arrays to apply rotation and translation to place them correctly in the world.
				 * Indices must be offset according to the running total for this opaque/trans mesh */
				block = (*chunk)[a][b][c];

				if (block.id == 0) continue; /* Air */

				getBlockmesh(blockmesh, block.id, block.variant, block.rotation,
						x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);

				/* Query mesh member counts */
				nMeshOpaqueVertex = blockmesh->count[0];
				nMeshTransVertex = blockmesh->count[1];
				nMeshOpaqueIndex = blockmesh->count[2];
				nMeshTransIndex = blockmesh->count[3];

				/* Adjust indices with running total */
				for (i = 0; i < nMeshOpaqueIndex; i++)
					blockmesh->opaqueIndices[i] += runningOpaqueIndexOffset;
				for (i = 0; i < nMeshTransIndex; i++)
					blockmesh->transIndices[i] += runningTransIndexOffset;
				runningOpaqueIndexOffset += nMeshOpaqueVertex / 8; /* Get number of vertices and offset */
				runningTransIndexOffset += nMeshTransVertex / 8; /* All vertices must be used at least once */

				/* Add mesh data to dynamic buffers */
				checkAndAddBlockmeshDataFloat(opaqueVertexCount, nMeshOpaqueVertex, &opaqueVertBufferSize,
						&oVert, blockmesh->opaqueVertices, &opaqueVertices);
				checkAndAddBlockmeshDataFloat(transVertexCount, nMeshTransVertex, &transVertBufferSize, &tVert,
						blockmesh->transVertices, &transVertices);
				/* Indices */
				checkAndAddBlockmeshDataInteger(opaqueIndexCount, nMeshOpaqueIndex, &opaqueIndBufferSize, &oInd,
						blockmesh->opaqueIndices, &opaqueIndices);
				checkAndAddBlockmeshDataInteger(transIndexCount, nMeshTransIndex, &transIndBufferSize, &tInd,
						blockmesh->transIndices, &transIndices);

				/* Add counts */
				opaqueVertexCount += nMeshOpaqueVertex;
				transVertexCount += nMeshTransVertex;
				opaqueIndexCount += nMeshOpaqueIndex;
				transIndexCount += nMeshTransIndex;
			}
		}
	}

	GLint buffer; /* VAOs don't store VBO bindings, but only attributes. This is why we have to query it manually */
	glBindVertexArray(mesh[0]); /* Opaque VAO */
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer); /* Query buffer for this VAO */
	glBindBuffer(GL_ARRAY_BUFFER, buffer); /* We need the VBO to be able to copy vertex data */
	glBufferData(GL_ARRAY_BUFFER, opaqueVertexCount * sizeof(float), opaqueVertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, opaqueIndexCount * sizeof(unsigned int), opaqueIndices, GL_DYNAMIC_DRAW);

	glBindVertexArray(mesh[1]); /* Trans VAO */
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, transVertexCount * sizeof(float), transVertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, transIndexCount * sizeof(unsigned int), transIndices, GL_DYNAMIC_DRAW);

	glBindVertexArray(0); /* Unbind when done */

	/* Set number of elements to draw for this chunk */
	mesh[2] = opaqueIndexCount;
	mesh[3] = transIndexCount;

	/* Free CPU-side buffers */
	free(blockmesh->opaqueVertices);
	free(blockmesh->transVertices);
	free(blockmesh->opaqueIndices);
	free(blockmesh->transIndices);
	free(blockmesh);
	free(opaqueVertices);
	free(opaqueIndices);
	free(transVertices);
	free(transIndices);
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

	/* Copy state to scratchpad */
	if (template->count[0] > RSM_MAX_BLOCKMESH_VERTICES || template->count[1] > RSM_MAX_BLOCKMESH_VERTICES) {
		fprintf(stderr, "Error: Blockmesh for ID %u variant %u exceeds maximum blockmesh vertex count (%u or %u > %u), aborting.\n", id, variant, template->count[0], template->count[1], RSM_MAX_BLOCKMESH_VERTICES);
		exit(RSM_EXIT_EXCBUF);
	} else {
		/* Copy to scratchpad data to be adjusted */
		memcpy(blockmesh->opaqueVertices, template->opaqueVertices, sizeof(float) * template->count[0]);
		memcpy(blockmesh->transVertices, template->transVertices, sizeof(float) * template->count[1]);
	}

	if (template->count[2] > RSM_MAX_BLOCKMESH_INDICES || template->count[3] > RSM_MAX_BLOCKMESH_INDICES) {
		fprintf(stderr, "Error: Blockmesh for ID %u variant %u exceeds maximum blockmesh index count (%u or %u > %u), aborting.\n", id, variant, template->count[2], template->count[3], RSM_MAX_BLOCKMESH_INDICES);
		exit(RSM_EXIT_EXCBUF);
	} else {
		memcpy(blockmesh->opaqueIndices, template->opaqueIndices, sizeof(unsigned int) * template->count[2]);
		memcpy(blockmesh->transIndices, template->transIndices, sizeof(unsigned int) * template->count[3]);
	}

	memcpy(blockmesh->count, template->count, sizeof(unsigned int) * 4); /* Get counts (never changes) */

	unsigned int i;
	mat4 adjust, rotAdjust;
	vec3 posAdjust = {(float) x, (float) y, (float) z};

	translocationMatrix(adjust, posAdjust, rotation);
	rotationMatrix(rotAdjust, rotation); /* Needed for normal adjustment */

	float *vertexPos;

	/* Rotate, translate and adjust vertex coords (and normals for rotation) for opaque vertices */
	for (i = 0; i < template->count[0] / 8; i++) {
		vertexPos = blockmesh->opaqueVertices + i * 8;
		glm_mat4_mulv3(adjust, vertexPos, 1.0f, vertexPos);
		glm_mat4_mulv3(rotAdjust, vertexPos + 3, 1.0f, vertexPos + 3); /* Rotate normals */
	}

	/* Now trans */
	for (i = 0; i < template->count[1] / 8; i++) {
		vertexPos = blockmesh->transVertices + i * 8;
		glm_mat4_mulv3(adjust, vertexPos, 1.0f, vertexPos);
		glm_mat4_mulv3(rotAdjust, vertexPos + 3, 1.0f, vertexPos + 3); /* Rotate normals */
	}
}

void translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation) {
	/* Makes an appropriate translocation (translation + rotation) matrix given
	 * a translation vector (position) and a rotation */
	mat4 rotAdjust;
	rotationMatrix(rotAdjust, rotation);

	glm_mat4_identity(translocation);
	glm_translate(translocation, translation);
	glm_mat4_mul(translocation, rotAdjust, translocation);
}

void rotationMatrix(mat4 rotAdjust, Rotation rotation) {
	/* Makes an appropriate rotation matrix for a given rotation */
	glm_mat4_identity(rotAdjust);

	switch (rotation) {
		case EAST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), upvector);
			break;
		case SOUTH:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(180), upvector);
			break;
		case WEST:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), upvector);
			break;
		case UP:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(270), rightvector);
			break;
		case DOWN:
			glm_rotate_at(rotAdjust, meshcenter, glm_rad(90), rightvector);
			break;
		case NONE:
		case NORTH:
		case COMPLEX:
			/* A block is defined as NORTH by default. Complex is TODO */
			break;
	}
}

void checkAndAddBlockmeshDataFloat(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
	float **buffer, float *data, float **basebuffer) {
	/* Automatically expand buffer size if needed and copy blockmesh data data into
	 * buffer buffer, incrementing it automatically */
	if (basecount + increment >= *buffersize) {
		/* Resize buffer */
		*basebuffer = realloc(*basebuffer, *buffersize * 2 * sizeof(float));
		*buffersize *= 2;
	}

	memcpy(*buffer, data, increment * sizeof(float));
	*buffer += increment;
}

void checkAndAddBlockmeshDataInteger(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
	unsigned int **buffer, unsigned int *data, unsigned int **basebuffer) {
	/* Same thing as checkAndAddBlockmeshDataFloat, but for unsigned integers (indices) */
	if (basecount + increment >= *buffersize) {
		*basebuffer = realloc(*basebuffer, *buffersize * 2 * sizeof(unsigned int));
		*buffersize *= 2;
	}

	memcpy(*buffer, data, increment * sizeof(unsigned int));
	*buffer += increment;
}
