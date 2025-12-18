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

	/* Retrieve data from disk */
#define SAVEFILE SAVES_PATH WORLD_PATH
	void *save;
	size_t savesize;
	if ((save = usf_ftob(SAVEFILE, &savesize))) {
		fprintf(stderr, "Loading save file %s (%zu bytes)\n", SAVEFILE, savesize);

#define SAVESTRIDE (CHUNKSIZE*CHUNKSIZE*CHUNKSIZE * sizeof(uint64_t) + sizeof(int64_t))
		Chunkdata *chunkdata;
		Blockdata blockdata;
		uint64_t x, y, z, saveblock, i, chunkindex, (*savedata)[CHUNKSIZE][CHUNKSIZE];
		for (i = 0; i < savesize; i += SAVESTRIDE) {
			chunkindex = *((uint64_t *) (save + i));
			savedata = (uint64_t (*)[CHUNKSIZE][CHUNKSIZE]) (save + i + sizeof(int64_t));
			chunkdata = malloc(sizeof(Chunkdata));

			for (x = 0; x < CHUNKSIZE; x++) for (y = 0; y < CHUNKSIZE; y++) for (z = 0; z < CHUNKSIZE; z++) {
				saveblock = savedata[x][y][z];

				blockdata.id = saveblock >> RSM_SAVE_IDSHIFT & 0xFFFF;
				blockdata.variant = saveblock >> RSM_SAVE_VARIANTSHIFT & 0xFF;
				blockdata.rotation = saveblock >> RSM_SAVE_ROTATIONSHIFT & 0x7;
				blockdata.metadata = saveblock >> RSM_SAVE_METADATASHIFT & 0xFFFFFFFF;

				(*chunkdata)[x][y][z] = blockdata;
			}
			usf_inthmput(chunkmap, chunkindex, USFDATAP(chunkdata));
			cu_asyncRemeshChunk(chunkindex);
		}
		free(save);
	} else fprintf(stderr, "No save file loaded!\n");

	cu_generateMeshlist(); /* Subsequently called only on render distance change */
	gui_updateGUI(); /* Subsequently called only on GUI modification (from user input) */
}

void client_savedata(void) {
	/* Save world to disk */
	uint64_t x, y, z, i, n;
	usf_data *entry;
	uint64_t savedata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE]; /* Blockdata is 64 bits */
	Chunkdata *chunkdata;
	Blockdata blockdata;

/* Enough space for all chunks and their chunkindex */
#define SAVESIZE (chunkmap->size * SAVESTRIDE)
	void *save, *s;
	s = save = malloc(SAVESIZE);

	GLuint *chunkmesh; /* To check emptiness */
	n = i = 0;
	while ((entry = usf_inthmnext(chunkmap, &i))) { /* Normalize and save chunks */
		if ((chunkmesh = usf_inthmget(meshmap, entry[0].u).p) == NULL || chunkmesh[2] + chunkmesh[3] == 0)
			continue; /* Either no mesh or mesh is empty : chunk is empty */

		chunkdata = entry[1].p;
		for (x = 0; x < CHUNKSIZE; x++) for (y = 0; y < CHUNKSIZE; y++) for (z = 0; z < CHUNKSIZE; z++) {
			blockdata = (*chunkdata)[x][y][z];
			savedata[x][y][z] =
				(uint64_t) blockdata.id << RSM_SAVE_IDSHIFT |
				(uint64_t) blockdata.variant << RSM_SAVE_VARIANTSHIFT |
				(uint64_t) blockdata.rotation << RSM_SAVE_ROTATIONSHIFT |
				(uint64_t) blockdata.metadata << RSM_SAVE_METADATASHIFT;
		}

		memcpy(s, &entry[0], sizeof(int64_t)); s += sizeof(int64_t); /* Chunk index */
		memcpy(s, savedata, sizeof(savedata)); s += sizeof(savedata); /* Chunk data */
		n++; /* Saved chunk */
	}

	usf_btof(SAVEFILE, save, n * SAVESTRIDE); /* Only write chunks which exist */
	free(save);

	fprintf(stderr, "Saved %"PRIu64" chunks to disk.\n", n);
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
	/* Write world to disk */
	client_savedata();

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
