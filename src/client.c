#include "client.h"

/* Global client player variables */
float pitch;
float yaw;
vec3 orientation;
vec3 position;

usf_hashmap *chunkmap; /* Maps XYZ (21 bits) to corresponding chunk pointer */
usf_hashmap *meshmap; /* Maps XYZ (21 bits) to corresponding chunk mesh (array of 4), automatically maintained */
usf_hashmap *datamap; /* Maps UID to default metadata values */
usf_hashmap *namemap; /* Maps name (string) to its corresponding UID */

GLuint **meshes; /* Stores current meshmap pointers for rendering */
int32_t nmesh; /* How many meshes to render */

pthread_mutex_t meshlock;
usf_queue *meshqueue;

void client_init(void) {
	fprintf(stderr, "Initializing client...\n");

	/* Generate template blockmeshes from disk and parse bounding boxes */
	/* Generate world and mesh lookup tables */
	chunkmap = usf_newhm();
	meshmap = usf_newhm();

	/* Generate datamap for default metadata values from disk */
	datamap = usf_newhm();

	/* Get block, mesh and bounding box data from disk */
	namemap = usf_newhm(); /* Init namemap before populating it */
	ru_parseBlockdata();

	/* Allocate buffers for GUI and load its assets from disk */
	gu_parseGUIdata();
	gu_initGUI();

	/* Init command processor */
	cmd_init();

	/* Allocate buffers for wiremesh */
	rsm_initWiremesh();

	/* Allocate stuff for asynchronous remeshing */
	pthread_mutex_init(&meshlock, NULL); /* TODO: learn what the attributes here do */
	meshqueue = usf_newqueue();

	/* Calculate player bounding box offsets to check for block collision.
	 * Cast and add 2 to simulate ceil and account for last corner */
	nPlayerBBOffsets = ((size_t) PLAYER_BOUNDINGBOX_DIMENSIONS[0] + 2) *
		((size_t) PLAYER_BOUNDINGBOX_DIMENSIONS[1] + 2) * 2 + (size_t) PLAYER_BOUNDINGBOX_DIMENSIONS[2];
	playerBBOffsets = malloc(nPlayerBBOffsets * sizeof(vec3));

	/* Volumetric iteration and skip inside. A bit inefficient, but only done once. */
	vec3 offset, *offsetPtr;
	offsetPtr = playerBBOffsets;
	for (offset[0] = PLAYER_BOUNDINGBOX_DIMENSIONS[0]; offset[0] > -1.0f; offset[0]--) {
		for (offset[1] = PLAYER_BOUNDINGBOX_DIMENSIONS[1]; offset[1] > -1.0f; offset[1]--) {
			for (offset[2] = PLAYER_BOUNDINGBOX_DIMENSIONS[2]; offset[2] > -1.0f; offset[2]--) {
				if (!(offset[0] == PLAYER_BOUNDINGBOX_DIMENSIONS[0] || offset[0] <= 0.0f
							|| offset[1] == PLAYER_BOUNDINGBOX_DIMENSIONS[1] || offset[1] <= 0.0f
							|| offset[2] == PLAYER_BOUNDINGBOX_DIMENSIONS[2] || offset[2] <= 0.0f))
					continue; /* Only want points across the boundary */

				memcpy(offsetPtr++, offset, sizeof(vec3));
			}
		}
	}

	/* TODO: Retrieve data from disk
	 * And remesh all chunks */

	/* TESTBED */
	Blockdata b0 = {
		.id = 1,
		.variant = 4,
		.rotation = NONE,
		.metadata = 7
	};

	Blockdata t0 = {
		.id = 2,
		.variant = 0,
		.rotation = NORTH,
		.metadata = 7
	};
	Blockdata t1 = {
		.id = 2,
		.variant = 0,
		.rotation = EAST,
		.metadata = 0
	};
	Blockdata t2 = {
		.id = 2,
		.variant = 0,
		.rotation = SOUTH,
		.metadata = 0
	};
	Blockdata t3 = {
		.id = 1,
		.variant = 6,
		.rotation = WEST,
		.metadata = 7
	};
	Blockdata t4 = {
		.id = 2,
		.variant = 0,
		.rotation = UP,
		.metadata = 0
	};
	Blockdata t5 = {
		.id = 2,
		.variant = 0,
		.rotation = DOWN,
		.metadata = 0
	};
	Blockdata t6 = {
		.id = 1,
		.variant = 8,
		.rotation = DOWN,
		.metadata = 1
	};

	Chunkdata *c0 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c1 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c2 = calloc(1, sizeof(Chunkdata));

	Chunkdata *p0 = calloc(1, sizeof(Chunkdata));
	Chunkdata *p1 = calloc(1, sizeof(Chunkdata));
	Chunkdata *p2 = calloc(1, sizeof(Chunkdata));
	Chunkdata *p3 = calloc(1, sizeof(Chunkdata));
	Chunkdata *p4 = calloc(1, sizeof(Chunkdata));

	//(*c0)[0][0][0] = t0;
	(*c0)[5][5][5] = b0;
	//(*c0)[1][0][0] = t6;
	//(*c0)[1][1][1] = b0;
	(*c0)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;

	(*c1)[0][0][0] = b0;
	//(*c1)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;
	//(*c1)[CHUNKSIZE-1][CHUNKSIZE-2][CHUNKSIZE-1] = b0;

	for (int x = 0; x < CHUNKSIZE; x++) {
		for (int y = 0; y < CHUNKSIZE; y++) {
			for (int z = 0; z < CHUNKSIZE; z++) {
				(*c2)[x][y][z] = t0;
			}
		}
	}
	for (int x = 0; x < CHUNKSIZE; x++) {
		for (int y = 0; y < CHUNKSIZE; y++) {
			for (int z = 0; z < CHUNKSIZE; z++) {
				(*c1)[x][y][z] = b0;
			}
		}
	}

	(*c2)[0][0][0] = t0;
	(*c2)[0][0][2] = t1;
	(*c2)[0][0][4] = t2;
	(*c2)[0][0][6] = t3;
	(*c2)[0][0][8] = t4;
	(*c2)[0][0][10] = t5;
	(*c2)[0][2][4] = t6;
	(*c2)[0][2][2] = t6;
	(*c2)[0][2][0] = t6;

	usf_inthmput(chunkmap, TOCHUNKINDEX(0L, 0L, 0L), USFDATAP(c0));
	usf_inthmput(chunkmap, TOCHUNKINDEX(-1L, -1L, -1L), USFDATAP(c1));
	usf_inthmput(chunkmap, TOCHUNKINDEX(0L, 0L, 1L), USFDATAP(c2));
	for (int x = 0; x < CHUNKSIZE; x++) {
		for (int y = 0; y < CHUNKSIZE; y += 2) {
			for (int z = 0; z < CHUNKSIZE; z++) {
				(*p0)[x][y][z] = t3;
				(*p1)[x][y][z] = b0;
				(*p2)[x][y][z] = t6;
				(*p3)[x][y][z] = b0;
				(*p4)[x][y][z] = b0;
			}
		}
	}
	usf_inthmput(chunkmap, TOCHUNKINDEX(1L, 1L, 1L), USFDATAP(p0));
	usf_inthmput(chunkmap, TOCHUNKINDEX(2L, 1L, 1L), USFDATAP(p1));
	usf_inthmput(chunkmap, TOCHUNKINDEX(3L, 1L, 1L), USFDATAP(p2));
	usf_inthmput(chunkmap, TOCHUNKINDEX(4L, 1L, 1L), USFDATAP(p3));
	usf_inthmput(chunkmap, TOCHUNKINDEX(5L, 1L, 1L), USFDATAP(p4));

	cu_asyncRemeshChunk(0);
	cu_asyncRemeshChunk(TOCHUNKINDEX(-1, -1, -1));
	cu_asyncRemeshChunk(TOCHUNKINDEX(0, 0, 1));

	cu_asyncRemeshChunk(TOCHUNKINDEX(1, 1, 1));
	cu_asyncRemeshChunk(TOCHUNKINDEX(2, 1, 1));
	cu_asyncRemeshChunk(TOCHUNKINDEX(3, 1, 1));
	cu_asyncRemeshChunk(TOCHUNKINDEX(4, 1, 1));
	cu_asyncRemeshChunk(TOCHUNKINDEX(5, 1, 1));
	printf("Test scene loaded\n");

	/* END TESTBED */

	cu_generateMeshlist(); /* Subsequently called only on render distance change */
	gui_updateGUI(); /* Subsequently called only on GUI modification (from user input) */
}

/* View stuff */
void client_frameEvent(GLFWwindow *window) {
	/* Called each frame */
	(void) window;

	/* Meshlist maintenance */
	static float lastRenderDistance = 0.0f;
	if (lastRenderDistance != RSM_RENDER_DISTANCE) {
		/* Whenever user changes render distance or world is first
		 * loaded, regenerate whole meshlist */
		cu_generateMeshlist();
		lastRenderDistance = RSM_RENDER_DISTANCE;
	}

	/* Keep last position since chunk border crossing ; update meshlist on border crossing */
	static vec3 lastPosition = {0.0f, 0.0f, 0.0f};
	if ((int32_t) floor(lastPosition[0] / CHUNKSIZE) != (int32_t) floor(position[0] / CHUNKSIZE)
			|| (int32_t) floor(lastPosition[1] / CHUNKSIZE) != (int32_t) floor(position[1] / CHUNKSIZE)
			|| (int32_t) floor(lastPosition[2] / CHUNKSIZE) != (int32_t) floor(position[2] / CHUNKSIZE)) {
		/* If position changed by at least a chunk, add new meshes and
		 * remove old ones */

		cu_updateMeshlist();
		glm_vec3_copy(position, lastPosition);
	}

	/* Set camera direction to appropriate pitch/yaw */
	orientation[0] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
	orientation[1] = sin(glm_rad(pitch));
	orientation[2] = -cos(glm_rad(yaw)) * cos(glm_rad(pitch));
	glm_vec3_normalize(orientation);

	rsm_move(position);
	rsm_updateWiremesh();
	rsm_checkMeshes();
}

void client_getOrientationVector(vec3 ori) {
	glm_vec3_copy(orientation, ori);
}

void client_getPosition(vec3 pos) {
	glm_vec3_copy(position, pos);
}

/* Access renderer stuff for destruction */
extern GLuint opaqueShader, transShader, compositionShader, guiShader, FBO, GUIFBO;
extern GLuint opaqueColorTex, accTex, revealTex, depthTex, guiColorTex, guiDepthTex;
void client_terminate(void) {
	/* Free RSM resources before program exit */
	uint64_t i, j;
	GLuint *mesh;
	usf_data *entry;

	/* Free GL memory */
	for (i = 0; i < meshmap->capacity; i++) {
		if ((entry = meshmap->array[i]) == NULL || entry == (usf_data *) meshmap) continue;
		mesh = (GLuint *) entry[1].p;
		ru_deallocateVAO(mesh[0]);
		ru_deallocateVAO(mesh[1]);
	}

	glDeleteTextures(1, &textureAtlas);
	glDeleteTextures(1, &guiAtlas);

	ru_deallocateVAO(wiremesh[0]);

	for (i = 0; i < MAX_GUI_PRIORITY; i++) ru_deallocateVAO(guiVAO[i]);

	glDeleteProgram(opaqueShader);
	glDeleteProgram(transShader);
	glDeleteProgram(compositionShader);
	glDeleteProgram(guiShader);

	renderer_freeBuffers();

	for (i = 0; i < MAX_BLOCK_ID; i++) {
		/* Free blockmesh templates */
		Blockmesh *bm;
		for (j = 0; j < MAX_BLOCK_VARIANT[i]; j++) {
			if ((bm = &blockmeshes[i][j]) == NULL) continue;

			free(bm->opaqueVertices);
			free(bm->transVertices);
			free(bm->opaqueIndices);
			free(bm->transIndices);
		}

		free(blockmeshes[i]);
		free(spriteids[i]);
		free(boundingboxes[i]);
	}
	free(MAX_BLOCK_VARIANT);

	free(blockmeshes);
	free(spriteids);
	free(boundingboxes);

	free(meshes); /* Meshlist components free'd in hashmap deallocation */

	usf_freehmptr(chunkmap);
	usf_freehmptr(meshmap);
	usf_freehm(datamap);
	usf_freestrhm(namemap);
	usf_freestrhm(cmdmap); /* Don't dealloc func pointers */
	usf_freestrhm(varmap); /* Idem rsmlayout pointers */
	usf_freestrhm(aliasmap); /* Idem string literals */

	usf_freequeueptr(meshqueue);
	pthread_mutex_destroy(&meshlock);

	free(playerBBOffsets);
}
