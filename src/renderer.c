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

void render(GLFWwindow *window) {
	/* Setup renderer within window window and start rendering loop */

	/* Set callback functions */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, processMouseMovement);
	glfwSetKeyCallback(window, processKey);
	glfwSetMouseButtonCallback(window, processMouseInput);
	glfwSetScrollCallback(window, processMouseScroll);

	/* Compile shaders from GLSL programs and link them */
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, "shaders/vertexshader.glsl");
	GLuint opaqueFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/opaquefragmentshader.glsl");
	GLuint transFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/transfragmentshader.glsl");
	GLuint compositionVertexShader = createShader(GL_VERTEX_SHADER, "shaders/compositionvertexshader.glsl");
	GLuint compositionFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/compositionfragmentshader.glsl");
	GLuint guiVertexShader = createShader(GL_VERTEX_SHADER, "shaders/guivertexshader.glsl");
	GLuint guiFragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/guifragmentshader.glsl");

	GLuint opaqueShader = createShaderProgram(vertexShader, opaqueFragmentShader);
	GLuint transShader = createShaderProgram(vertexShader, transFragmentShader);
	GLuint compositionShader = createShaderProgram(compositionVertexShader, compositionFragmentShader);
	GLuint guiShader = createShaderProgram(guiVertexShader, guiFragmentShader);

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

	glBindVertexArray(0);

	/* Draw opaque scene to the opaque color buffer, saving depth data
	 * then render transparent scene (with depth check) to both accTex
	 * and revealTex (for WB-OIT algorithm) and finally composite them
	 * both and draw to screen framebuffer */
	GLuint opaqueColorTex, depthTex, accTex, revealTex;

	/* Opaque color buffer */
	glGenTextures(1, &opaqueColorTex);
	glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Accumulator buffer (nearest for interpolation) */
    glGenTextures(1, &accTex);
	glBindTexture(GL_TEXTURE_2D, accTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Reveal buffer (R32F precision) */
	glGenTextures(1, &revealTex);
	glBindTexture(GL_TEXTURE_2D, revealTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
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
	glUseProgram(opaqueShader);
	glUniform1i(glGetUniformLocation(opaqueShader, "atlas"), 0);
	glUseProgram(transShader);
	glUniform1i(glGetUniformLocation(transShader, "atlas"), 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making rendering framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	/* Draw any GUI elements to the GUI buffer (will get blended as a transparent layer in the
	 * composition stage) with depth check (only draw "closest" (priority) GUI elements) */
	GLuint guiColorTex, guiDepthTex;

	/* GUI color buffer */
	glGenTextures(1, &guiColorTex);
	glBindTexture(GL_TEXTURE_2D, guiColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* GUI depth buffer */
	glGenTextures(1, &guiDepthTex);
	glBindTexture(GL_TEXTURE_2D, guiDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLuint GUIFBO;
	glGenFramebuffers(1, &GUIFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);

	/* Attach color buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, guiColorTex, 0);

	/* Attach depth buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, guiDepthTex, 0);

	/* Link textures with shader samplers. The reason for using attachment 4
	 * is purely because the GUI was introduced after code for the rest was implemented */
	glUseProgram(guiShader);
	glUniform1i(glGetUniformLocation(guiShader, "atlas"), 4);
	mat4 guiProject;
	glm_ortho(0.0f, WINDOW_WIDTH, 0.0f, WINDOW_HEIGHT, 0.0f, MAX_GUI_PRIORITY, guiProject);
	glUniformMatrix4fv(glGetUniformLocation(guiShader, "projectionMatrix"), 1, GL_FALSE, (float *) guiProject);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making GUI framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	glUseProgram(compositionShader);
	glUniform1i(glGetUniformLocation(compositionShader, "opaqueBuffer"), 1);
	glUniform1i(glGetUniformLocation(compositionShader, "accBuffer"), 2);
	glUniform1i(glGetUniformLocation(compositionShader, "revealBuffer"), 3);
	glUniform1i(glGetUniformLocation(compositionShader, "guiBuffer"), 5);

	/* Texture mapping */
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textureAtlas);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, accTex);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, revealTex);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, guiColorTex);

	/* Add value in accumulation buffer; will divide by sum of alphas afterwards */
	glBlendFuncSeparatei(1, GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	/* Multiply reveal buffer by new source to get opacity factor */
	glBlendFuncSeparatei(2, GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ZERO);

	/* For GUI (writes to its default color buffer of 0) do ordered transparency */
	glBlendFuncSeparatei(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glBlendEquation(GL_FUNC_ADD);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glFrontFace(GL_CW);

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
			exit(RSM_EXIT_GLERROR);
		}

		/* Framerate cap */
#if FPS_CAP == true
		time = glfwGetTime();
		if (time - lastTime < 1.0f/RSM_FPS)
			glfwWaitEventsTimeout(1.0/RSM_FPS - (time - lastTime));
		lastTime = glfwGetTime(); /* Finished drawing frame */
#else
		(void) time;
		(void) lastTime;
#endif

		/* Client communication */
		client_frameEvent(window);
		client_getOrientationVector(relativeViewTarget);
		client_getPosition(cameraPos);

		/* Create view translation matrix from cameraPos to origin:
		 * This matrix transforms world coordinates to be in front of
		 * our camera, effectively tilting the world to simulate having
		 * moved around. */
		glm_vec3_add(cameraPos, relativeViewTarget, viewTarget);
		glm_lookat(cameraPos, viewTarget, UPVECTOR, view);

		/* Perspective projection matrix */
		glm_perspective(glm_rad(RSM_FOV), (float) WINDOW_WIDTH / (float) WINDOW_HEIGHT,
				RENDER_DISTANCE_NEAR, (float) RENDER_DISTANCE * CHUNKSIZE, perspective);

		/* Rendering */
		glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

		/* Opaque rendering */
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glClearColor(0.098f, 0.098f, 0.098f, 1.0f); /* This is my terminal background color */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDisable(GL_BLEND);
		glUseProgram(opaqueShader);
		glUniformMatrix4fv(glGetUniformLocation(opaqueShader, "viewMatrix"), 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(glGetUniformLocation(opaqueShader, "projectionMatrix"), 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nmesh; i++) {
			glBindVertexArray(meshes[i][0]);
			glDrawElements(GL_TRIANGLES, meshes[i][2], GL_UNSIGNED_INT, 0);
		}

		/* Wireframe (highlighting) rendering */
		glDisable(GL_CULL_FACE);
		glBindVertexArray(wiremesh[0]);
		glDrawElements(GL_LINES, wiremesh[1], GL_UNSIGNED_INT, 0);

		/* Transparent rendering */
		glEnable(GL_CULL_FACE);
		glDepthMask(GL_FALSE); /* Don't affect depth buffer */
		glClearBufferfv(GL_COLOR, 1, ACCUM_CLEAR);
		glClearBufferfv(GL_COLOR, 2, REVEAL_CLEAR); /* Set reveal buffer to 1; others are reset to 0 by glClear */
		glEnable(GL_BLEND);
		glUseProgram(transShader);
		glUniformMatrix4fv(glGetUniformLocation(transShader, "viewMatrix"), 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(glGetUniformLocation(transShader, "projectionMatrix"), 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nmesh; i++) {
			glBindVertexArray(meshes[i][1]);
			glDrawElements(GL_TRIANGLES, meshes[i][3], GL_UNSIGNED_INT, 0);
		}

		/* GUI rendering */
		glDisable(GL_CULL_FACE);
		glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);
		glDisable(GL_DEPTH_TEST); /* Already ordered for transparency */
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_BLEND);
		glUseProgram(guiShader);

		for (i = MAX_GUI_PRIORITY - 1; i >= 0; i--) {
			glBindVertexArray(guiVAO[i]);
			if (nGUIIndices[i])
				glDrawElements(GL_TRIANGLES, nGUIIndices[i], GL_UNSIGNED_INT, 0);
		}

		/* Composition */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screenWidth, screenHeight);

		/* Composite rendering and aftereffects */
		glDisable(GL_DEPTH_TEST); /* Don't clear as screen is overwritten and no depth test */
		glDisable(GL_BLEND);
		glUseProgram(compositionShader);
		glBindVertexArray(compositionVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6); /* Draw final quad */

	    glfwPollEvents(); /* Update state and call appropriate callback functions for user input */
		glfwSwapBuffers(window); /* Render what's been drawn */
	}

	/* Window has been closed */
	glfwTerminate();
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	screenWidth = width;
	screenHeight = height;
}
