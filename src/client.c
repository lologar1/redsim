#include "client.h"


/* RENDERER COMMUNICATION */

int inited = 1;
unsigned int things[4];
void init() {
	/* Opaque scene POS_POS_POS+NORM_NORM_NORM+TEXPOS_TEXPOS*/
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

	float transTestBed[] = {
		-1.0f, 4.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 4.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
		-1.0f, 2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
		1.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f
	};

	unsigned int transTestIndices[] = {
		0, 1, 2,
		1, 2, 3,
		4, 5, 6,
		5, 6, 7
	};
	GLuint testOVAO, testOVBO, testOEBO, testTVAO, testTVBO, testTEBO;
	glGenVertexArrays(1, &testOVAO);
	glGenBuffers(1, &testOVBO);
	glGenBuffers(1, &testOEBO);
	glBindVertexArray(testOVAO);
	glBindBuffer(GL_ARRAY_BUFFER, testOVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testOEBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(testBed), testBed, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(testIndices), testIndices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

	glGenVertexArrays(1, &testTVAO);
	glGenBuffers(1, &testTVBO);
	glGenBuffers(1, &testTEBO);
	glBindVertexArray(testTVAO);
	glBindBuffer(GL_ARRAY_BUFFER, testTVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, testTEBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(transTestBed), transTestBed, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(transTestIndices), transTestIndices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

	things[0] = testOVAO;
	things[1] = testTVAO;
	things[2] = 6;
	things[3] = 12;
}

/* Chunk stuff */
void client_getChunks(GLuint (**chunks)[4], int *nchunks) {
	/* TESTBED */
	if (inited) {
		init();
		inited = 0;
	}
	*nchunks = 1;
	*chunks = &things;
}

/* View stuff */
float pitch;
float yaw;
vec3 orientation;
vec3 position;

void client_frameEvent(GLFWwindow *window) {
	/* Called each frame */
	static float lastTime = 0.0f;
	float deltaTime, cameraSpeedX, cameraSpeedY, cameraSpeedZ;

	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	cameraSpeedX = RSM_FLY_X_SPEED * deltaTime; /* Configurable in layout */
	cameraSpeedY = RSM_FLY_Y_SPEED * deltaTime;
	cameraSpeedZ = RSM_FLY_Z_SPEED * deltaTime;

	vec3 temp;
	vec3 cameraMovement = {0.0f, 0.0f, 0.0f};

	/* Movement; orientation should always be normalized */
	if (glfwGetKey(window, RSM_KEY_FORWARD) == GLFW_PRESS) {
		glm_vec3_copy(orientation, temp);
		temp[1] = 0.0f; /* Lock vertical movement */
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, cameraSpeedZ, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_BACKWARD) == GLFW_PRESS) {
		glm_vec3_copy(orientation, temp);
		temp[1] = 0.0f; /* Lock vertical movement */
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, -cameraSpeedZ, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_RIGHT) == GLFW_PRESS) {
		glm_vec3_cross(orientation, upvector, temp);
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, cameraSpeedX, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_LEFT) == GLFW_PRESS) {
		glm_vec3_cross(orientation, upvector, temp);
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, -cameraSpeedX, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_UP) == GLFW_PRESS) {
		glm_vec3_scale(upvector, cameraSpeedY, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if(glfwGetKey(window, RSM_KEY_DOWN) == GLFW_PRESS) {
		glm_vec3_scale(upvector, -cameraSpeedY, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	/* Update camera position */
	glm_vec3_add(position, cameraMovement, position);

	/* Set camera direction to appropriate pitch/yaw */
	orientation[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
	orientation[1] = sin(glm_rad(pitch));
	orientation[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
	glm_vec3_normalize(orientation);
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
