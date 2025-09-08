#include "chunkutils.h"

void checkAndAddBlockmeshDataFloat(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		float **buffer, float *data, float **basebuffer);
void checkAndAddBlockmeshDataInteger(unsigned int basecount, unsigned int increment, unsigned int *buffersize,
		unsigned int **buffer, unsigned int *data, unsigned int **basebuffer);

/* Associates currently loaded meshes with their chunk coords, used for updating meshlist */
uint32_t **meshpositions;

void remeshChunk(uint64_t x, uint64_t y, uint64_t z) {
	/* Generate appropriate mesh array (VAO) for a given chunk and set it in meshmap */
	uint64_t chunkindex;
	GLuint *mesh, opaqueVAO, transVAO, opaqueVBO, transVBO, opaqueEBO, transEBO;
	unsigned int a, b, c;
	Chunkdata chunk;
	Blockdata block;

	chunkindex = x << 42 | y << 21 | z;
	chunk = (Chunkdata) usf_inthmget(chunkmap, chunkindex).p;

	mesh = (GLuint *) usf_inthmget(meshmap, chunkindex).p;
	if (mesh == NULL) {
		/* No mesh is generated for this chunk; initialize GL structures */
		mesh = malloc(4 * sizeof(GLuint));

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

	Blockmesh *blockmesh;

	/* Dynamically-resizing buffer */
	unsigned int opaqueVertBufferSize = BUFSIZ, transVertBufferSize = BUFSIZ,
				 opaqueIndBufferSize = BUFSIZ, transIndBufferSize = BUFSIZ;
	oVert = opaqueVertices = malloc(BUFSIZ * sizeof(float));
	tVert = transVertices = malloc(BUFSIZ * sizeof(float));
	oInd = opaqueIndices = malloc(BUFSIZ * sizeof(unsigned int));
	tInd = transIndices = malloc(BUFSIZ * sizeof(unsigned int));

	for (a = 0; a < CHUNKSIZE; a++) {
		for (b = 0; b < CHUNKSIZE; b++) {
			for (c = 0; c < CHUNKSIZE; c++) {
				/* Get blockmesh for block at this location
				 * blockmeshes will only regenerate their vertex
				 * arrays to apply rotation and translation to place them
				 * correctly in the world. The rest is static data. */
				block = chunk[a][b][c];

				if (block.id == 0) continue; /* Air */

				blockmesh = getBlockmesh(block.id, block.variant, block.rotation,
						x * CHUNKSIZE + a, y * CHUNKSIZE + b, z * CHUNKSIZE + c);

				/* Query mesh member counts */
				nMeshOpaqueVertex = blockmesh->count[0];
				nMeshTransVertex = blockmesh->count[1];
				nMeshOpaqueIndex = blockmesh->count[2];
				nMeshTransIndex = blockmesh->count[3];

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

	/* TODO: null out an empty chunk. Don't forget to remove it from **meshes ! */

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
	free(opaqueVertices);
	free(opaqueIndices);
	free(transVertices);
	free(transIndices);
}

void updateMeshlist(vec3 lastPosition) {
    /* Generate new required meshes since movement from lastPosition
     * and remove out of render distance ones */

	generateMeshlist(); /* TODO: Update instead of regenerating each time */

	//GLuint (*meshlist)[4] = meshes;
    //int i;

    //for (i = 0; i < nmesh; i++) {
        /* For all loaded meshes:
         * Remove if out of render distance
         * Replace with new mesh if there are any */
    //}
}

void generateMeshlist(void) {
    /* Regenerate entire meshes for all chunks in render distance */
    GLuint **meshlist, *mesh;
	uint32_t **poslist;
    unsigned int a, b, c;
    uint64_t x, y, z, meshindex, chunkvolume;

    /* Allocate enough mem for all chunks in cubic view.
     * Note that each cell is a pointer to a chunk mesh (array[4]) so even with
     * 100 chunk render distance, the size is only
     * (100 * 2 + 1) ^ 3 * 8 = 64964808 = 65 MB of memory */

	/* Cleanup old meshlist */
    free(meshes); /* OK since all mesh chunks are stored in meshmap */
	for (poslist = meshpositions; nmesh; poslist++, nmesh--)
		free(*poslist); /* Need to free old mesh positions */
	free(meshpositions); /* nmesh is at 0 after this */

	chunkvolume = pow(RENDER_DISTANCE * 2 + 1, 3) * sizeof(GLuint [4]);
    meshlist = meshes = (GLuint **) malloc(chunkvolume); /* Store meshes for all chunks */
	poslist = meshpositions = (uint32_t **) malloc(chunkvolume); /* Associate mesh to chunk coordinates */

    /* Populate meshlist with meshes of existing chunks (not empty) */
    for (a = 0; a < RENDER_DISTANCE * 2 + 1; a++) {
        x = ((int) position[0] / CHUNKSIZE + a - RENDER_DISTANCE);
        for (b = 0; b < RENDER_DISTANCE * 2 + 1; b++) {
            y = ((int) position[1] / CHUNKSIZE + b - RENDER_DISTANCE);
            for (c = 0; c < RENDER_DISTANCE * 2 + 1; c++) {
                z = ((int) position[2] / CHUNKSIZE + c - RENDER_DISTANCE);

				/* Set mesh for this chunk */
                meshindex = x << 42 | y << 21 | z;
				mesh = (GLuint *) usf_inthmget(meshmap, meshindex).p;

				if (mesh == NULL) continue; /* No mesh for this chunk */

                *meshlist++ = mesh;
                nmesh++;

				/* Set chunk coordinates for this mesh */
				*poslist = malloc(3 * sizeof(uint32_t));
				(*poslist)[0] = (uint32_t) x; (*poslist)[1] = (uint32_t) y; (*poslist)[2] = (uint32_t) z;
				poslist++;
            }
        }
	}
}

Blockmesh *getBlockmesh(unsigned int id, unsigned int variant, Rotation rotation,
		uint64_t x, uint64_t y, uint64_t z) {
	/* Return a valid blockmesh containing adjusted vertex positions for a block
	 * of type id of variant variant with rotation rotation at position xyz */
	(void) id;
	(void) variant;
	(void) rotation;
	(void) x; (void) y; (void) z;
	/* TESTBED */
	float *testopVert;
	unsigned int *testopInd, *count;
	testopVert = malloc(32 * sizeof(float));
	testopInd = malloc(6 * sizeof(unsigned int));
	count = malloc(4 * sizeof(unsigned int));

	count[0] = 32; count[1] = 0; count[2] = 6; count[3] = 0;

	float testBed[] = {
		-1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f
	};
	unsigned int testIndices[] = {
		0, 1, 2,
		1, 2, 3
	};

	memcpy(testopVert, testBed, sizeof(testBed));
	memcpy(testopInd, testIndices, sizeof(testIndices));

	Blockmesh *testmesh = (Blockmesh *) malloc(sizeof(Blockmesh));

	testmesh->opaqueVertices = testopVert;
	testmesh->transVertices = NULL;
	testmesh->opaqueIndices = testopInd;
	testmesh->transIndices = NULL;
	memcpy(testmesh->count, count, 4 * sizeof(unsigned int));

	return testmesh;
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
