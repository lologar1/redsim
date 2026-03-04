#include "client.h"

u64 NPROCS;
char SAVEFILE[RSM_MAX_PATH_NAME_LENGTH];
u64 NPLAYERBBOFFSETS;
vec3 *PLAYERBBOFFSETS;

f32 pitch_;
f32 yaw_;
vec3 orientation_;
vec3 position_;

u8 sspower_ = 1;

usf_hashmap *chunkmap_; /* Maps XYZ (21 bits) to corresponding chunk pointer */
usf_hashmap *meshmap_; /* Maps XYZ (21 bits) to corresponding chunk mesh (array of 4 + remeshing flag) */
usf_hashmap *datamap_; /* Maps UID to default metadata values */
usf_hashmap *namemap_; /* Maps name (string) to its corresponding UID */
usf_queue *meshqueue_; /* Asynchronously remeshed chunks to be sent to GL buffers */

Mesh **meshes_; /* Stores current meshmap pointers for rendering */
u64 nmeshes_; /* How many meshes to render */
GLuint wiremesh_[2]; /* Block highlight and selection (equivalent to one member of meshes_) */

static usf_thread simthread_; /* Thread ID for the simulation coroutine */

void client_init(void) {
	fprintf(stderr, "Initializing client...\n");

	chunkmap_ = usf_newhm_ts(); /* Accessed async by remeshing */
	meshqueue_ = usf_newqueue_ts();
	meshmap_ = usf_newhm_ts(); /* Accessed async by remeshing (check flag) */
	datamap_ = usf_newhm();
	namemap_ = usf_newhm();

	pio_parseBlockdata(); /* Parse all block data from disk */
	pio_parseGUIdata(); /* Parse all GUI data from disk */

	gu_initGUI(); /* Build GUI GL buffers */
	cmd_init(); /* Build alias table */
	sim_init(); /* Build simulation structures & start sim process */

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
	if (usf_fexists(SAVEFILE)) client_loaddata();
	else fprintf(stderr, "No save file loaded!\n");

	gui_updateGUI(); /* Rescale and properly show GUI */

	NPROCS = USF_MIN(usf_nprocsonln(), RSM_MAX_PROCESSORS); /* Number of logical cores */
	fprintf(stderr, "Concurrency: using %"PRIu64" threads for simulation.\n", NPROCS);

	usf_atmflagtry(&simstop_, MEMORDER_RELAXED); /* Flag is set */

	if (usf_thrdcreate(&simthread_, &sim_run, NULL) == THRD_ERROR) {
		fprintf(stderr, "Couldn't start simulation thread!\n");
		exit(RSM_EXIT_THREADFAIL);
	}
}

void client_loaddata(void) {
	/* Load world from disk, then remesh and register it */

	u8 *save;
	u64 savesize;
	save = usf_ftob(SAVEFILE, &savesize);
	fprintf(stderr, "Loading save file %s (%"PRIu64" bytes)\n", SAVEFILE, savesize);

	/* Read header */
	memcpy(position_, save, sizeof(vec3));
	memcpy(hotbar_, save + sizeof(vec3), sizeof(hotbar_));

	usf_listptr *toregister; /* Pending component graph registration */
	toregister = usf_newlistptr();

	u64 i;
	for (i = RSM_SAVE_HEADERSZ; i < savesize; i += RSM_SAVE_CHUNKSTRIDE) {
		u64 chunkindex;
		memcpy(&chunkindex, save + i, sizeof(u64)); /* Get chunkindex header */

		u32 savedata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE]; /* From disk */
		memcpy(savedata, save + i + sizeof(u64), sizeof(savedata)); /* Skip chunkindex */

		Chunkdata *chunkdata;
		chunkdata = malloc(sizeof(Chunkdata)); /* Alloc chunk to be loaded */

		/* Copy data to chunk and defer registration */
		i64 x, y, z;
		for (x = 0; x < CHUNKSIZE; x++)
		for (y = 0; y < CHUNKSIZE; y++)
		for (z = 0; z < CHUNKSIZE; z++) {
			u64 saveblock;
			saveblock = savedata[x][y][z];

			Blockdata blockdata;
			blockdata.id = saveblock >> RSM_SAVE_IDSHIFT & RSM_SAVE_IDMASK;
			blockdata.variant = saveblock >> RSM_SAVE_VARIANTSHIFT & RSM_SAVE_VARIANTMASK;
			blockdata.rotation = saveblock >> RSM_SAVE_ROTATIONSHIFT & RSM_SAVE_ROTATIONMASK;
			blockdata.metadata = saveblock >> RSM_SAVE_METADATASHIFT & RSM_SAVE_METADATAMASK;
			(*chunkdata)[x][y][z] = blockdata; /* Set */

			if (blockdata.id == RSM_BLOCK_AIR) continue; /* Don't bother registering air */

			/* Get world coordinates to add to component graph */
			vec3 *coords;
			coords = malloc(sizeof(vec3));
			(*coords)[0] = SIGNED21CAST64(chunkindex >> 42) * CHUNKSIZE + x;
			(*coords)[1] = SIGNED21CAST64((chunkindex >> 21) & LOW21MASK) * CHUNKSIZE + y;
			(*coords)[2] = SIGNED21CAST64(chunkindex & LOW21MASK) * CHUNKSIZE + z;
			usf_listptradd(toregister, coords); /* Defer registration */
		}
		free(usf_inthmget(chunkmap_, chunkindex).p); /* Free if present (NULL free if not, OK) */
		usf_inthmput(chunkmap_, chunkindex, USFDATAP(chunkdata));
	}
	free(save); /* Free disk data buffer */

	/* Remesh after loading (batched access to chunkmap_, so manually acquire) */
	usf_mtxlock(chunkmap_->lock); /* Thread-safe lock */
	usf_data *entry; /* Jobs will need to wait until all are queued */
	for (i = 0; (entry = usf_inthmnext(chunkmap_, &i));)
		cu_asyncRemeshChunk(entry[0].u);
	usf_mtxunlock(chunkmap_->lock); /* Thread-safe unlock */

	/* Register graph from loaded world */
	f64 timestart;
	fprintf(stderr, "Building component graph... ");
	timestart = glfwGetTime();

	Fillcontext *afcontext;
	afcontext = wf_newcontext(RSM_DISCARD_VISUAL_INFO);

	/* Batched registering */
	usf_mtxlock(graphmap_->lock); /* Thread-safe lock */
	for (i = 0; i < toregister->size; i++)
		wf_findaffected(*(vec3 *) toregister->array[i], afcontext);
	wf_registercontext(afcontext);
	usf_mtxunlock(graphmap_->lock); /* Thread-safe unlock */

	usf_freelistptrfunc(toregister, free); /* Free pending coordinates */
	wf_freecontext(afcontext); /* Free batch context */
	fprintf(stderr, "Done! Took %f seconds.\n", glfwGetTime() - timestart);
}

void client_savedata(void) {
	/* Save world to disk */

	usf_mtxlock(chunkmap_->lock); /* Thread-safe lock */

	u8 *save, *s;
	s = save = malloc(RSM_SAVE_HEADERSZ + chunkmap_->size * RSM_SAVE_CHUNKSTRIDE);

	/* Save header */
	memcpy(s, position_, sizeof(vec3)); s += sizeof(vec3);
	memcpy(s, hotbar_, sizeof(hotbar_)); s += sizeof(hotbar_);

	/* Save chunks */
	u64 i, n;
	usf_data *entry;
	for (i = n = 0; (entry = usf_inthmnext(chunkmap_, &i));) {
		GLuint *chunkmesh;
		if ((chunkmesh = usf_inthmget(meshmap_, entry[0].u).p) == NULL || chunkmesh[2] + chunkmesh[3] == 0)
			continue; /* Empty or uninitialized mesh (no blocks in chunk) */

		Chunkdata *chunkdata;
		chunkdata = entry[1].p;

		u64 x, y, z;
		u32 savedata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE]; /* To disk */
		for (x = 0; x < CHUNKSIZE; x++) for (y = 0; y < CHUNKSIZE; y++) for (z = 0; z < CHUNKSIZE; z++) {
			Blockdata blockdata;
			blockdata = (*chunkdata)[x][y][z];

			savedata[x][y][z] = 0
				| (u32) blockdata.id << RSM_SAVE_IDSHIFT
				| (u32) blockdata.variant << RSM_SAVE_VARIANTSHIFT
				| (u32) blockdata.rotation << RSM_SAVE_ROTATIONSHIFT
				| (u32) blockdata.metadata << RSM_SAVE_METADATASHIFT;
		}
		memcpy(s, &entry[0], sizeof(u64)); s += sizeof(u64); /* Chunkindex */
		memcpy(s, savedata, sizeof(savedata)); s += sizeof(savedata); /* Chunk data */
		n++; /* Saved chunk */
	}

	usf_mtxunlock(chunkmap_->lock); /* Thread-safe unlock */

	usf_btof(SAVEFILE, save, RSM_SAVE_HEADERSZ + n * RSM_SAVE_CHUNKSTRIDE);
	free(save); /* Free RAM representation of disk file */

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
	/* Destroy all non-GL RSM allocations and processes */

	u64 i, j;

	/* Stop simulation */
	i32 simreturn;
	usf_atmflagclr(&simstop_, MEMORDER_RELAXED);
	usf_thrdjoin(simthread_, &simreturn);
	if (simreturn) fprintf(stderr, "Simulation returned with abnormal exit code %"PRId32".\n", simreturn);

	/* Write world to disk */
	client_savedata();

	/* Wait for remeshing */
	usf_data *meshentry;
	for (i = 0; (meshentry = usf_inthmnext(meshmap_, &i));) {
		Mesh *mesh;
		mesh = meshentry[1].p;

		/* Relaxed since no more calls will be issued; wait until existing ones have finished */
		while (usf_atmflagtry(&mesh->remeshing, MEMORDER_ACQ_REL));
	}

	/* Drain remaining */
	Rawmesh *leftover;
	while ((leftover = usf_dequeue(meshqueue_).p)) {
		free(leftover->opaqueVertexBuffer);
		free(leftover);
	}

	/* Free */
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
	free(TEXTCHARS);

	free(meshes_); /* Meshlist components free'd in hashmap deallocation */

	/* Free structures */
	usf_freeinthmfunc(chunkmap_, free);
	usf_freeinthmfunc(meshmap_, free);
	usf_freeinthmfunc(graphmap_, sim_freecomponent);
	usf_freeinthm(datamap_);
	usf_freestrhm(namemap_);
	usf_freestrhm(cmdmap_); /* Don't dealloc func pointers */
	usf_freestrhm(varmap_); /* Idem rsmlayout pointers */
	usf_freestrhm(aliasmap_); /* Idem string literals */
	usf_freequeuefunc(meshqueue_, free);
}
