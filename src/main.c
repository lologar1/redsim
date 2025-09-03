#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "cglm/cglm.h"

/* utils and usflib2 includes */
#include "shaderutils.h"
#include "textureutils.h"
#include "rsmlayout.h"

/* C Standard Library include */
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_NAME "Redsim V0.1"
#define FPS 240.0f
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define upvector (vec3) {0.0f, 1.0f, 0.0f}
#define ACCUM_CLEAR (float []) {0.0f, 0.0f, 0.0f, 0.0f}
#define REVEAL_CLEAR (float []) {1.0f}

void framebuffer_size_callback(GLFWwindow* window, int width, int height); /* Function to remap viewport whenever window is resized */
void processInput(GLFWwindow *window, vec3 cameraPos, vec3 relativeViewTarget); /* Check for user input each frame */
void mouse_callback(GLFWwindow* window, double xpos, double ypos); /* Adjust pitch/yaw on mouse movement */
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods); /* Single keypresses */

float compositionQuad[] = {
	/* Position		TexPos */
	-1.0f,  1.0f,	0.0f, 1.0f,
	-1.0f, -1.0f,	0.0f, 0.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	-1.0f,  1.0f,	0.0f, 1.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	1.0f,  1.0f,	1.0f, 1.0f
};

float pitch, yaw; /* Camera pitch and yaw values */
unsigned int screenWidth = WINDOW_WIDTH, screenHeight = WINDOW_HEIGHT;

int main(void) {
	glfwInit();

	/* Set OpenGL version 4.6 */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	/* Use OpenGL core-profile */
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/* Create window */
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME, NULL, NULL);

	if (window == NULL) {
		fprintf(stderr, "Fatal error creating GLFW window, exiting.\n");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(0); /* Disable VSync to avoid stuttering, FPS cap is set elsewhere */

	/* Callback functions */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetKeyCallback(window, key_callback);

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "Fatal error initializing GLAD, exiting.\n");
		glfwTerminate();
		return -1;
	}

	/* Compile shaders from GLSL programs and link them */
	/* TODO: entities+particles */
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, "shaders/vertexshader.glsl");
	GLuint opaqueFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/opaquefragmentshader.glsl");
	GLuint transFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/transfragmentshader.glsl");
	GLuint compositionVertexShader = createShader(GL_VERTEX_SHADER, "shaders/compositionvertexshader.glsl");
	GLuint compositionFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/compositionfragmentshader.glsl");

	GLuint opaqueShader = createShaderProgram(vertexShader, opaqueFragmentShader);
	GLuint transShader = createShaderProgram(vertexShader, transFragmentShader);
	GLuint compositionShader = createShaderProgram(compositionVertexShader, compositionFragmentShader);

	/* Each chunk holds separate opaque/transparent meshes
	 * chunks is a list of VAOs queried each frame from the game.
	 * Whenever a chunk changes, the CPU regenerates the mesh and chunks is
	 * automatically updated.
	 * The last two unsigned ints hold the number of elements for their
	 * respective chunks. */
	unsigned int (*chunks)[4];
	int nchunks;

	/* TESTBED */
	nchunks = 1;

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

	unsigned int things[4] = {testOVAO, testTVAO, 6, 12};
	chunks = &things;

	/* To render composition quad at the end of rendering */
	GLuint compositionVAO, compositionVBO;
	glGenVertexArrays(1, &compositionVAO);
	glGenBuffers(1, &compositionVBO);
	glBindVertexArray(compositionVAO);
	glBindBuffer(GL_ARRAY_BUFFER, compositionVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(compositionQuad), &compositionQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

	/* Draw opaque scene to the opaque color buffer, saving depth data
	 * then render transparent scene (with depth check) to both accTex
	 * and revealTex (for WB-OIT algorithm) and finally composite them
	 * both and draw to screen framebuffer */
	GLuint opaqueColorTex, depthTex, accTex, revealTex;

	/* Opaque color buffer */
	glGenTextures(1, &opaqueColorTex);
	glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Accumulator buffer (nearest for interpolation) */
    glGenTextures(1, &accTex);
	glBindTexture(GL_TEXTURE_2D, accTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Reveal buffer (R32F precision) */
	glGenTextures(1, &revealTex);
	glBindTexture(GL_TEXTURE_2D, revealTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Depth buffer */
	glGenTextures(1, &depthTex);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLuint FBO;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	/* Attach color buffers */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueColorTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, accTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, revealTex, 0);

	/* Attach depth buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

	GLenum drawBuffers[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
	glDrawBuffers(3, drawBuffers); /* Set fragment shader output to proper buffers */

	/* Texture atlas */
	GLuint opaqueTextureAtlas = createTexture("textures/opaqueTextureAtlas.jpg");
	GLuint transTextureAtlas = createTexture("textures/transTextureAtlas.png");

	/* Link textures with shader samplers */
	glUseProgram(opaqueShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, opaqueTextureAtlas);
	glUniform1i(glGetUniformLocation(opaqueShader, "atlas"), 0);

	glUseProgram(transShader);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, transTextureAtlas);
	glUniform1i(glGetUniformLocation(transShader, "atlas"), 1);

	glUseProgram(compositionShader);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, accTex);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, revealTex);
	glUniform1i(glGetUniformLocation(compositionShader, "opaqueBuffer"), 2);
	glUniform1i(glGetUniformLocation(compositionShader, "accBuffer"), 3);
	glUniform1i(glGetUniformLocation(compositionShader, "revealBuffer"), 4);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	/* Add value in accumulation buffer; will divide by sum of alphas afterwards */
	glBlendFuncSeparatei(1, GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	/* Multiply reveal buffer by 1 - alpha of new source to get opacity factor */
	glBlendFuncSeparatei(2, GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_ZERO);

	glBlendEquation(GL_FUNC_ADD);
	glClearColor(0.05f, 0.0f, 0.05f, 1.0f);
	//glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
	//TODO: enable this

	/* Rendering variables */
	float time, lastTime = 0.0f;
	int i;

	mat4 view, perspective; /* In World Space, the camera is at (0, 0, 0) */
	vec3 sunlight = {500000.0f, 1000000.0f, 0.0f}; /* The Sun */
	vec3 cameraPos = {0.0f, 0.0f, 0.0f};
	vec3 relativeViewTarget, viewTarget;

	/* Render loop */
	GLenum glError;

	while (!glfwWindowShouldClose(window)) {
		/* Game loop */

		if ((glError = glGetError())) {
			fprintf(stderr, "OpenGL Error %d\n", glError);
			exit(1);
		}

		time = glfwGetTime();

		if (time - lastTime < 1.0f/FPS)
			glfwWaitEventsTimeout(1.0/FPS - (time - lastTime));

		lastTime = glfwGetTime(); /* Finished drawing frame */

		/* Process user inputs for this frame */
		processInput(window, cameraPos, relativeViewTarget);

		/* Set camera direction to appropriate pitch/yaw */
		relativeViewTarget[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
		relativeViewTarget[1] = sin(glm_rad(pitch));
		relativeViewTarget[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
		glm_vec3_normalize(relativeViewTarget);

		/* Create view translation matrix from cameraPos to origin:
		 * This matrix transforms world coordinates to be in front of
		 * our camera, effectively tilting the world to simulate having
		 * moved around. */
		glm_vec3_add(cameraPos, relativeViewTarget, viewTarget);
		glm_lookat(cameraPos, viewTarget, upvector, view);

		/* Perspective projection matrix */
		glm_perspective(glm_rad(RSM_FOV), (float) WINDOW_WIDTH / (float) WINDOW_HEIGHT,
				RENDER_DISTANCE_NEAR, RENDER_DISTANCE_FAR, perspective);

		/* Rendering */
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		/* Opaque rendering */
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_BLEND);
		glUseProgram(opaqueShader);
		glUniformMatrix4fv(glGetUniformLocation(opaqueShader, "viewMatrix"), 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(glGetUniformLocation(opaqueShader, "projectionMatrix"), 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nchunks; i++) {
			glBindVertexArray(chunks[i][0]);
			glDrawElements(GL_TRIANGLES, chunks[i][2], GL_UNSIGNED_INT, 0);
		}

		/* Transparent rendering */
		glClearBufferfv(GL_COLOR, 1, ACCUM_CLEAR);
		glClearBufferfv(GL_COLOR, 2, REVEAL_CLEAR); /* Set reveal buffer to 1; others are reset to 0 by glClear */
		glDepthMask(GL_FALSE); /* Don't affect depth buffer */
		glEnable(GL_BLEND);
		glUseProgram(transShader);
		glUniformMatrix4fv(glGetUniformLocation(transShader, "viewMatrix"), 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(glGetUniformLocation(transShader, "projectionMatrix"), 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nchunks; i++) {
			glBindVertexArray(chunks[i][1]);
			glDrawElements(GL_TRIANGLES, chunks[i][3], GL_UNSIGNED_INT, 0);
		}

		/* Composition */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenWidth, screenHeight);

		/* Composite rendering and aftereffects */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glUseProgram(compositionShader);
		glBindVertexArray(compositionVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6); /* Draw final quad */

		glfwSwapBuffers(window); /* Render what's been drawn */
	    glfwPollEvents(); /* Update state and call appropriate callback functions for user input */
	}

	/* Window has been closed */
	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow *window, vec3 cameraPos, vec3 relativeViewTarget) {
	/* Check for user input each frame */

	static float lastTime = 0.0f; /* Time since GLFW init; 0 at start */
	float deltaTime, cameraSpeedX, cameraSpeedY, cameraSpeedZ;

	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	cameraSpeedX = RSM_FLY_X_SPEED * deltaTime; /* Configurable in layout */
	cameraSpeedY = RSM_FLY_Y_SPEED * deltaTime;
	cameraSpeedZ = RSM_FLY_Z_SPEED * deltaTime;

	vec3 temp;
	vec3 cameraMovement = {0.0f, 0.0f, 0.0f};

	/* Movement; relativeViewTarget should always be normalized */
	if (glfwGetKey(window, RSM_KEY_FORWARD) == GLFW_PRESS) {
		glm_vec3_copy(relativeViewTarget, temp);
		temp[1] = 0.0f; /* Lock vertical movement */
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, cameraSpeedZ, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_BACKWARD) == GLFW_PRESS) {
		glm_vec3_copy(relativeViewTarget, temp);
		temp[1] = 0.0f; /* Lock vertical movement */
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, -cameraSpeedZ, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_RIGHT) == GLFW_PRESS) {
		glm_vec3_cross(relativeViewTarget, upvector, temp);
		glm_vec3_normalize(temp);
		glm_vec3_scale(temp, cameraSpeedX, temp);
		glm_vec3_add(cameraMovement, temp, cameraMovement);
	}

	if (glfwGetKey(window, RSM_KEY_LEFT) == GLFW_PRESS) {
		glm_vec3_cross(relativeViewTarget, upvector, temp);
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

	/* Update camera */
	glm_vec3_add(cameraPos, cameraMovement, cameraPos);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	screenWidth = width;
	screenHeight = height;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	/* Handle mouse movements and adjust pitch/yaw accordingly */

	(void) window; /* Prototype */

	static float lastX = WINDOW_WIDTH/2;
	static float lastY = WINDOW_HEIGHT/2;

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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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
