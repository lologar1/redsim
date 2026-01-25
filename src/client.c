#include "client.h"

u64 NPLAYERBBOFFSETS;
vec3 *PLAYERBBOFFSETS;

f32 pitch_;
f32 yaw_;
vec3 orientation_;
vec3 position_;

u8 sspower_ = 1;

usf_hashmap *chunkmap_; /* Maps XYZ (21 bits) to corresponding chunk pointer */
usf_hashmap *meshmap_; /* Maps XYZ (21 bits) to corresponding chunk mesh (array of 4) */
usf_hashmap *datamap_; /* Maps UID to default metadata values */
usf_hashmap *namemap_; /* Maps name (string) to its corresponding UID */
usf_queue *meshqueue_; /* Asynchronously remeshed chunks to be sent to GL buffers */

GLuint **meshes_; /* Stores current meshmap pointers for rendering */
u64 nmeshes_; /* How many meshes to render */
GLuint wiremesh_[2]; /* Block highlight and selection (equivalent to one member of meshes_) */

void client_init(void) {
	fprintf(stderr, "Initializing client...\n");

	chunkmap_ = usf_newhm_ts(); /* Accessed async by remeshing */
	meshqueue_ = usf_newqueue_ts();
	meshmap_ = usf_newhm();
	datamap_ = usf_newhm();
	namemap_ = usf_newhm();

	pio_parseBlockdata(); /* Parse all block data from disk */
	pio_parseGUIdata(); /* Parse all GUI data from disk */

	gu_initGUI(); /* Build GUI GL buffers */
	cmd_init(); /* Build alias table */

	/* Build wiremesh GL buffer */
	GLuint wiremeshVBO, wiremeshEBO;
	glGenVertexArrays(1, wiremesh_); /* VAO at index 0 */
	glGenBuffers(1, &wiremeshVBO);
	glGenBuffers(1, &wiremeshEBO);
	glBindVertexArray(*wiremesh_);
	glBindBuffer(GL_ARRAY_BUFFER, wiremeshVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wiremeshEBO);
	/* Need to match attributes with normal rendering as the same shaders are used */
	glEnableVertexAttribArray(0); /* Vertex position */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (0 * sizeof(f32)));
	glEnableVertexAttribArray(1); /* Normals */
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (3 * sizeof(f32)));
	glEnableVertexAttribArray(2); /* Texture mappings */
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (6 * sizeof(f32)));
	glBindVertexArray(0);

	/* Calculate player bounding box offsets to check for block collision.
	 * Cast and add 2 to simulate ceil and account for last corner */
	NPLAYERBBOFFSETS = ((u64) PLAYER_BOUNDINGBOX_DIMENSIONS[0] + 2)
		* ((u64) PLAYER_BOUNDINGBOX_DIMENSIONS[1] + 2)
		* 2 + (u64) PLAYER_BOUNDINGBOX_DIMENSIONS[2];
	PLAYERBBOFFSETS = malloc(NPLAYERBBOFFSETS * sizeof(vec3));

	/* Volumetric iteration and skip inside. A bit inefficient, but only done once. */
	vec3 offset, *bboffset;
	bboffset = PLAYERBBOFFSETS;
	for (offset[0] = PLAYER_BOUNDINGBOX_DIMENSIONS[0]; offset[0] > -1.0f; offset[0]--)
	for (offset[1] = PLAYER_BOUNDINGBOX_DIMENSIONS[1]; offset[1] > -1.0f; offset[1]--)
	for (offset[2] = PLAYER_BOUNDINGBOX_DIMENSIONS[2]; offset[2] > -1.0f; offset[2]--) {
		if (!(offset[0] == PLAYER_BOUNDINGBOX_DIMENSIONS[0] || offset[0] <= 0.0f
					|| offset[1] == PLAYER_BOUNDINGBOX_DIMENSIONS[1] || offset[1] <= 0.0f
					|| offset[2] == PLAYER_BOUNDINGBOX_DIMENSIONS[2] || offset[2] <= 0.0f))
			continue; /* Only want points across the boundary */
		memcpy(bboffset++, offset, sizeof(vec3));
	}

	/* Retrieve data from disk */
#define SAVEFILE SAVES_PATH WORLD_PATH
	void *save;
	u64 savesize;
	if ((save = usf_ftob(SAVEFILE, &savesize))) {
		fprintf(stderr, "Loading save file %s (%"PRIu64" bytes)\n", SAVEFILE, savesize);

#define SAVESTRIDE (CHUNKSIZE*CHUNKSIZE*CHUNKSIZE * sizeof(u64) + sizeof(u64))
		Chunkdata *chunkdata;
		u64 i, chunkindex, (*savedata)[CHUNKSIZE][CHUNKSIZE];
		for (i = 0; i < savesize; i += SAVESTRIDE) {
			chunkindex = *((u64 *) (save + i)); /* Get chunkindex header */
			savedata = (u64 (*)[CHUNKSIZE][CHUNKSIZE]) (save + i + sizeof(u64)); /* Skip chunkindex header */
			chunkdata = malloc(sizeof(Chunkdata)); /* Alloc chunk to be loaded */

			Blockdata blockdata;
			u64 x, y, z, saveblock;
			for (x = 0; x < CHUNKSIZE; x++)
			for (y = 0; y < CHUNKSIZE; y++)
			for (z = 0; z < CHUNKSIZE; z++) {
				saveblock = savedata[x][y][z];

				blockdata.id = saveblock >> RSM_SAVE_IDSHIFT & RSM_SAVE_IDMASK;
				blockdata.variant = saveblock >> RSM_SAVE_VARIANTSHIFT & RSM_SAVE_VARIANTMASK;
				blockdata.rotation = saveblock >> RSM_SAVE_ROTATIONSHIFT & RSM_SAVE_ROTATIONMASK;
				blockdata.metadata = saveblock >> RSM_SAVE_METADATASHIFT & RSM_SAVE_METADATAMASK;

				(*chunkdata)[x][y][z] = blockdata;
			}
			usf_inthmput(chunkmap_, chunkindex, USFDATAP(chunkdata));
		}

		/* Remesh after loading */
		pthread_mutex_lock(chunkmap_->lock); /* Lock (working with inthmnext) */
		usf_data *entry;
		i = 0;
		while ((entry = usf_inthmnext(chunkmap_, &i))) cu_asyncRemeshChunk(entry[0].u);
		pthread_mutex_unlock(chunkmap_->lock); /* Unlock */

		free(save);
	} else fprintf(stderr, "No save file loaded!\n");

	gui_updateGUI(); /* Subsequently called only on GUI modification (from user input) */
}

void client_savedata(void) {
	/* Save world to disk */

	u64 x, y, z, i, n;
	usf_data *entry;
	u64 savedata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE]; /* Blockdata is 64 bits */
	Chunkdata *chunkdata;
	Blockdata blockdata;

	pthread_mutex_lock(chunkmap_->lock); /* Lock (working with inthmnext) */

/* Enough space for all chunks and their chunkindex */
#define SAVESIZE (chunkmap_->size * SAVESTRIDE)
	void *save, *s;
	s = save = malloc(SAVESIZE);

	GLuint *chunkmesh; /* To check emptiness */
	n = i = 0;
	while ((entry = usf_inthmnext(chunkmap_, &i))) { /* Normalize and save chunks */
		if ((chunkmesh = usf_inthmget(meshmap_, entry[0].u).p) == NULL || chunkmesh[2] + chunkmesh[3] == 0)
			continue; /* Either no mesh or mesh is empty : chunk is empty */

		chunkdata = entry[1].p;
		for (x = 0; x < CHUNKSIZE; x++) for (y = 0; y < CHUNKSIZE; y++) for (z = 0; z < CHUNKSIZE; z++) {
			blockdata = (*chunkdata)[x][y][z];
			savedata[x][y][z] =
				(u64) blockdata.id << RSM_SAVE_IDSHIFT |
				(u64) blockdata.variant << RSM_SAVE_VARIANTSHIFT |
				(u64) blockdata.rotation << RSM_SAVE_ROTATIONSHIFT |
				(u64) blockdata.metadata << RSM_SAVE_METADATASHIFT;
		}

		memcpy(s, &entry[0], sizeof(i64)); s += sizeof(i64); /* Chunk index */
		memcpy(s, savedata, sizeof(savedata)); s += sizeof(savedata); /* Chunk data */
		n++; /* Saved chunk */
	}
	pthread_mutex_unlock(chunkmap_->lock); /* Unlock */

	usf_btof(SAVEFILE, save, n * SAVESTRIDE); /* Only write chunks which exist */
	free(save);

	fprintf(stderr, "Saved %"PRIu64" chunks to disk.\n", n);
}

/* View stuff */
void client_frameEvent(GLFWwindow *window) {
	/* Called each frame */
	(void) window;

	/* Meshlist maintenance */
	static f32 lastRenderDistance = 0.0f;
	if (lastRenderDistance != RSM_RENDER_DISTANCE) {
		cu_generateMeshlist();
		lastRenderDistance = RSM_RENDER_DISTANCE;
	}

	/* Keep last position since chunk border crossing ; update meshlist on border crossing */
	static vec3 lastPosition = {0.0f, 0.0f, 0.0f};
	if ((i32) floor(lastPosition[0] / CHUNKSIZE) != (i32) floor(position_[0] / CHUNKSIZE)
			|| (i32) floor(lastPosition[1] / CHUNKSIZE) != (i32) floor(position_[1] / CHUNKSIZE)
			|| (i32) floor(lastPosition[2] / CHUNKSIZE) != (i32) floor(position_[2] / CHUNKSIZE)) {
		cu_updateMeshlist();
		glm_vec3_copy(position_, lastPosition);
	}

	/* Set camera direction to appropriate pitch/yaw */
	orientation_[0] = sinf(glm_rad(yaw_)) * cosf(glm_rad(pitch_));
	orientation_[1] = sinf(glm_rad(pitch_));
	orientation_[2] = -cosf(glm_rad(yaw_)) * cosf(glm_rad(pitch_));
	glm_vec3_normalize(orientation_);

	rsm_move(position_); /* Move & collision */
	rsm_updateWiremesh(); /* Update block outline & selection */
	rsm_checkMeshes(); /* Check if new meshes have been remeshed asynchronously */
}

void client_terminate(void) {
	/* Destroy all non-GL RSM allocations */

	/* Write world to disk */
	client_savedata();

	/* Free */
	u64 i, j;
	for (i = 0; i < MAX_BLOCK_ID; i++) {
		/* Free blockmesh templates */
		Blockmesh *bm;
		for (j = 0; j < MAX_BLOCK_VARIANT[i]; j++) {
			if ((bm = &BLOCKMESHES[i][j]) == NULL) continue;

			free(bm->opaqueVertices);
			free(bm->transVertices);
			free(bm->opaqueIndices);
			free(bm->transIndices);
		}

		/* Free lookup entry */
		free(BLOCKMESHES[i]);
		free(SPRITEIDS[i]);
		free(BOUNDINGBOXES[i]);
	}

	/* Free lookup arrays */
	free(MAX_BLOCK_VARIANT);
	free(BLOCKMESHES);
	free(SPRITEIDS);
	free(BOUNDINGBOXES);
	free(PLAYERBBOFFSETS);

	free(meshes_); /* Meshlist components free'd in hashmap deallocation */

	/* Free structures */
	usf_freeinthmptr(chunkmap_);
	usf_freeinthmptr(meshmap_);
	usf_freeinthm(datamap_);
	usf_freestrhm(namemap_);
	usf_freestrhm(cmdmap_); /* Don't dealloc func pointers */
	usf_freestrhm(varmap_); /* Idem rsmlayout pointers */
	usf_freestrhm(aliasmap_); /* Idem string literals */
	usf_freequeueptr(meshqueue_);
}
