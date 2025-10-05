#include "renderer.h"

#define ACCUM_CLEAR (float []) {0.0f, 0.0f, 0.0f, 0.0f}
#define REVEAL_CLEAR (float []) {1.0f}

float compositionQuad[] = {
	/* Position		TexPos */
	-1.0f,  1.0f,	0.0f, 1.0f,
	-1.0f, -1.0f,	0.0f, 0.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	-1.0f,  1.0f,	0.0f, 1.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	1.0f,  1.0f,	1.0f, 1.0f
};

unsigned int screenWidth = WINDOW_WIDTH, screenHeight = WINDOW_HEIGHT;

int render(GLFWwindow* window) {
	/* Setup renderer within window window and start rendering loop */

	/* Set callback functions */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, client_mouseEvent);
	glfwSetKeyCallback(window, client_keyboardEvent);

	/* Compile shaders from GLSL programs and link them */
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
	GLuint **chunks;
	int nchunks;

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

	/* Link textures with shader samplers */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureAtlas);

	glUseProgram(opaqueShader);
	glUniform1i(glGetUniformLocation(opaqueShader, "atlas"), 0);
	glUseProgram(transShader);
	glUniform1i(glGetUniformLocation(transShader, "atlas"), 0);

	glUseProgram(compositionShader);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, accTex);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, revealTex);
	glUniform1i(glGetUniformLocation(compositionShader, "opaqueBuffer"), 1);
	glUniform1i(glGetUniformLocation(compositionShader, "accBuffer"), 2);
	glUniform1i(glGetUniformLocation(compositionShader, "revealBuffer"), 3);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	/* Add value in accumulation buffer; will divide by sum of alphas afterwards */
	glBlendFuncSeparatei(1, GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	/* Multiply reveal buffer by 1 - alpha of new source to get opacity factor */
	glBlendFuncSeparatei(2, GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_ZERO);

	glBlendEquation(GL_FUNC_ADD);
	glClearColor(0.098f, 0.098f, 0.098f, 1.0f); /* This is my terminal background color */
	glEnable(GL_CULL_FACE); glCullFace(GL_BACK);

	/* Rendering variables */
	float time, lastTime = 0.0f;
	int i;

	mat4 view, perspective; /* In World Space, the camera is at (0, 0, 0) */
	vec3 cameraPos = {0.0f, 0.0f, 0.0f};
	vec3 relativeViewTarget, viewTarget;

	/* Render loop */
	GLenum glError;

	while (!glfwWindowShouldClose(window)) {
		/* Game loop */

		if ((glError = glGetError())) {
			fprintf(stderr, "OpenGL Error %d\n", glError);
			return 1;
		}

		/* Framerate cap */
		time = glfwGetTime();
		if (time - lastTime < 1.0f/RSM_FPS)
			glfwWaitEventsTimeout(1.0/RSM_FPS - (time - lastTime));
		lastTime = glfwGetTime(); /* Finished drawing frame */

		/* Client communication */
		client_frameEvent(window);
		client_getOrientationVector(relativeViewTarget);
		client_getPosition(cameraPos);
		client_getChunks(&chunks, &nchunks);

		/* Create view translation matrix from cameraPos to origin:
		 * This matrix transforms world coordinates to be in front of
		 * our camera, effectively tilting the world to simulate having
		 * moved around. */
		glm_vec3_add(cameraPos, relativeViewTarget, viewTarget);
		glm_lookat(cameraPos, viewTarget, (vec3) {0.0f, 1.0f, 0.0f}, view);

		/* Perspective projection matrix */
		glm_perspective(glm_rad(RSM_FOV), (float) WINDOW_WIDTH / (float) WINDOW_HEIGHT,
				RENDER_DISTANCE_NEAR, (float) RENDER_DISTANCE * CHUNKSIZE, perspective);

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

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	screenWidth = width;
	screenHeight = height;
}
