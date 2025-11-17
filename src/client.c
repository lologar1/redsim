#include "client.h"

/* Global client player variables */
float pitch;
float yaw;
vec3 orientation;
vec3 position;

usf_hashmap *chunkmap; /* Maps XYZ (21 bits) to corresponding chunk pointer */
usf_hashmap *meshmap; /* Maps XYZ (21 bits) to corresponding chunk mesh (array of 4), automatically maintained */

GLuint **meshes; /* Stores current meshmap pointers for rendering */
int nmesh; /* How many meshes to render */

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
	parseBlockdata();

	/* Allocate buffers for GUI and load its assets from disk */
	parseGUIdata();
	initGUI();

	/* Allocate buffers for wiremesh */
	initWiremesh();

	/* TODO: Retrieve data from disk
	 * And remesh all chunks */

	fprintf(stderr, "Initialization success!\n");

	/* TESTBED */
	Blockdata b0 = {
		.id = 1,
		.variant = 0,
		.rotation = NONE,
		.metadata = 1
	};

	Blockdata t0 = {
		.id = 2,
		.variant = 0,
		.rotation = NORTH,
		.metadata = 0
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
		.id = 2,
		.variant = 0,
		.rotation = WEST,
		.metadata = 0
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
		.variant = 1,
		.rotation = DOWN,
		.metadata = 1
	};

	Chunkdata *c0 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c1 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c2 = calloc(1, sizeof(Chunkdata));

	//(*c0)[0][0][0] = t0;
	(*c0)[5][5][5] = b0;
	//(*c0)[1][0][0] = t6;
	//(*c0)[1][1][1] = b0;
	(*c0)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;

	(*c1)[0][0][0] = b0;
	//(*c1)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;
	//(*c1)[CHUNKSIZE-1][CHUNKSIZE-2][CHUNKSIZE-1] = b0;

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

	remeshChunk(0);
	remeshChunk(TOCHUNKINDEX(-1, -1, -1));
	remeshChunk(TOCHUNKINDEX(0, 0, 1));

	/* END TESTBED */

	generateMeshlist(); /* Subsequently called only on render distance change */
	renderGUI(); /* Subsequently called only on GUI modification (from user input) */
}

/* View stuff */
void client_frameEvent(GLFWwindow *window) {
	/* Called each frame */
	(void) window;

	/* Meshlist maintenance */
	static unsigned int lastRenderDistance = 0;
	if (lastRenderDistance != RENDER_DISTANCE) {
		/* Whenever user changes render distance or world is first
		 * loaded, regenerate whole meshlist */
		generateMeshlist();
		lastRenderDistance = RENDER_DISTANCE;
	}

	/* Keep last position since chunk border crossing ; update meshlist on border crossing */
	static vec3 lastPosition = {0.0f, 0.0f, 0.0f};
	if ((int) floor(lastPosition[0] / CHUNKSIZE) != (int) floor(position[0] / CHUNKSIZE)
			|| (int) floor(lastPosition[1] / CHUNKSIZE) != (int) floor(position[1] / CHUNKSIZE)
			|| (int) floor(lastPosition[2] / CHUNKSIZE) != (int) floor(position[2] / CHUNKSIZE)) {
		/* If position changed by at least a chunk, add new meshes and
		 * remove old ones */

		updateMeshlist();
		glm_vec3_copy(position, lastPosition);
	}

	/* Set camera direction to appropriate pitch/yaw */
	orientation[0] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
	orientation[1] = sin(glm_rad(pitch));
	orientation[2] = -cos(glm_rad(yaw)) * cos(glm_rad(pitch));
	glm_vec3_normalize(orientation);

	rsm_move(position);
	rsm_updateWiremesh();
}

void client_getOrientationVector(vec3 ori) {
	glm_vec3_copy(orientation, ori);
}

void client_getPosition(vec3 pos) {
	glm_vec3_copy(position, pos);
}
