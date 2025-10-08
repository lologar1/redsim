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
	chunkmap = usf_newhm();
	meshmap = usf_newhm();
	parseBlockmeshes();

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
		.metadata = 0
	};

	Chunkdata *c0 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c1 = calloc(1, sizeof(Chunkdata));
	Chunkdata *c2 = calloc(1, sizeof(Chunkdata));

	(*c0)[0][0][0] = t0;
	(*c0)[5][5][5] = b0;
	(*c0)[1][0][0] = t6;
	(*c0)[1][1][1] = b0;
	(*c0)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;

	(*c1)[0][0][0] = b0;
	(*c1)[CHUNKSIZE-1][CHUNKSIZE-1][CHUNKSIZE-1] = t0;
	(*c1)[CHUNKSIZE-1][CHUNKSIZE-2][CHUNKSIZE-1] = b0;

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

	remeshChunk(0, 0, 0);
	remeshChunk(-1, -1, -1);
	remeshChunk(0, 0, 1);

	/* END TESTBED */

	generateMeshlist();
}

/* RENDERER COMMUNICATION */

/* Chunk stuff */
void client_getChunks(GLuint ***chunks, int *nchunks) {
	/* Sets appropriate chunk mesh pointers for renderer */
	*nchunks = nmesh;
	*chunks = meshes;
}

/* View stuff */
void client_frameEvent(GLFWwindow *window) {
	/* Called each frame */

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
	orientation[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
	orientation[1] = sin(glm_rad(pitch));
	orientation[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
	glm_vec3_normalize(orientation);

	/* Movement handling */
	static float lastTime = 0.0f;
	static float momentumX = 0.0f, momentumY = 0.0f, momentumZ = 0.0f;

	float deltaTime, cameraSpeedX, cameraSpeedY, cameraSpeedZ;

	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	cameraSpeedX = momentumX * deltaTime;
	cameraSpeedY = momentumY * deltaTime;
	cameraSpeedZ = momentumZ * deltaTime;

	vec3 zMovement, xMovement, yMovement;

	/* Movement key polling */
	bool forward, backward, right, left, up, down;
	forward = glfwGetKey(window, RSM_KEY_FORWARD) == GLFW_PRESS;
	backward = glfwGetKey(window, RSM_KEY_BACKWARD) == GLFW_PRESS;
	right = glfwGetKey(window, RSM_KEY_RIGHT) == GLFW_PRESS;
	left = glfwGetKey(window, RSM_KEY_LEFT) == GLFW_PRESS;
	up = glfwGetKey(window, RSM_KEY_UP) == GLFW_PRESS;
	down = glfwGetKey(window, RSM_KEY_DOWN) == GLFW_PRESS;

	/* Movement; orientation should always be normalized */
	if (forward) momentumZ = fmin(momentumZ + RSM_FLY_Z_ACCELERATION * deltaTime, RSM_FLY_Z_CAP);
	if (backward) momentumZ = fmax(momentumZ - RSM_FLY_Z_ACCELERATION * deltaTime, -RSM_FLY_Z_CAP);
	if (!(forward && backward)) momentumZ = momentumZ < 0.0f ?
		fmin(0.0f, momentumZ + RSM_FLY_Z_DECELERATION * deltaTime) :
		fmax(0.0f, momentumZ - RSM_FLY_Z_DECELERATION * deltaTime);

	/* Z movement */
	glm_vec3_copy(orientation, zMovement);
	zMovement[1] = 0.0f; /* Lock vertical movement */
	glm_vec3_normalize(zMovement);
	glm_vec3_scale(zMovement, cameraSpeedZ, zMovement);

	if (right) momentumX = fmin(momentumX + RSM_FLY_X_ACCELERATION * deltaTime, RSM_FLY_X_CAP);
	if (left) momentumX = fmax(momentumX - RSM_FLY_X_ACCELERATION * deltaTime, -RSM_FLY_X_CAP);
	if (!(right && left)) momentumX = momentumX < 0.0f ?
		fmin(0.0f, momentumX + RSM_FLY_X_DECELERATION * deltaTime) :
		fmax(0.0f, momentumX - RSM_FLY_X_DECELERATION * deltaTime);

	/* X movement */
	glm_vec3_cross(orientation, upvector, xMovement);
	glm_vec3_normalize(xMovement);
	glm_vec3_scale(xMovement, cameraSpeedX, xMovement);

	if (up) momentumY = fmin(momentumY + RSM_FLY_Y_ACCELERATION * deltaTime, RSM_FLY_Y_CAP);
	if (down) momentumY = fmax(momentumY - RSM_FLY_Y_ACCELERATION * deltaTime, -RSM_FLY_Y_CAP);
	if (!(up && down)) momentumY = momentumY < 0.0f ?
		fmin(0.0f, momentumY + RSM_FLY_Y_DECELERATION * deltaTime) :
		fmax(0.0f, momentumY - RSM_FLY_Y_DECELERATION * deltaTime);

	/* Y movement */
	glm_vec3_scale(upvector, cameraSpeedY, yMovement);

	glm_vec3_addadd(xMovement, yMovement, zMovement); /* zMovement used for final position change */

	/* Move with collision checks and all */
	rsm_move(position, zMovement);
}

void client_mouseEvent(GLFWwindow *window, double xpos, double ypos) {
	/* Handle mouse movements and adjust pitch/yaw accordingly */

	(void) window; /* Prototype */

	static float lastX = 0;
	static float lastY = 0;

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; /* Reversed because Y axis is swapped */

	lastX = xpos;
	lastY = ypos;

	xoffset *= RSM_MOUSE_SENSITIVITY;
	yoffset *= RSM_MOUSE_SENSITIVITY;

	yaw += xoffset;
	pitch += yoffset;

	/* Guardrails to avoid angle swaps */
	if(pitch > 89.0f) pitch = 89.0f;
	if(pitch < -89.0f) pitch = -89.0f;
}

void client_keyboardEvent(GLFWwindow *window, int key, int scancode, int action, int mods) {
	 /* Called whenever a key is first pressed */

	(void) scancode;
	(void) mods;

	if (key == RSM_KEY_ESCAPE && action == GLFW_PRESS) {
		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); /* Release cursor */
		else
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); /* Capture cursor */
	}
}

void client_getOrientationVector(vec3 ori) {
	glm_vec3_copy(orientation, ori);
}

void client_getPosition(vec3 pos) {
	glm_vec3_copy(position, pos);
}
