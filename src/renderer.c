#include "renderer.h"

#define COLOR_CLEAR (float [4]) {0.098f, 0.098f, 0.098f, 1.0f}
#define ZERO_CLEAR (float [4]) {0.0f, 0.0f, 0.0f, 0.0f}
#define ONE_CLEAR (float [4]) {1.0f, 1.0f, 1.0f, 1.0f}

void framebuffer_size_callback(GLFWwindow *window, int32_t width, int32_t height);

float compositionQuad[] = {
	/* Position		TexPos */
	-1.0f,  1.0f,	0.0f, 1.0f,
	-1.0f, -1.0f,	0.0f, 0.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	-1.0f,  1.0f,	0.0f, 1.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	1.0f,  1.0f,	1.0f, 1.0f
};

uint32_t screenWidth = 1080, screenHeight = 1920;

/* Shader programs */
GLuint vertexShader, opaqueFragmentShader, transFragmentShader, compositionVertexShader,
	   compositionFragmentShader, guiVertexShader, guiFragmentShader, opaqueShader,
	   transShader, compositionShader, guiShader;

/* Framebuffer stuff */
GLuint FBO, GUIFBO;
GLuint opaqueColorTex, accTex, revealTex, depthTex, guiColorTex, guiDepthTex;

void renderer_initShaders(void) {
	/* Compile shaders from GLSL programs and link them */
	vertexShader = ru_createShader(GL_VERTEX_SHADER, "shaders/vertexshader.glsl");
	opaqueFragmentShader = ru_createShader(GL_FRAGMENT_SHADER, "shaders/opaquefragmentshader.glsl");
	transFragmentShader = ru_createShader(GL_FRAGMENT_SHADER, "shaders/transfragmentshader.glsl");
	compositionVertexShader = ru_createShader(GL_VERTEX_SHADER, "shaders/compositionvertexshader.glsl");
	compositionFragmentShader = ru_createShader(GL_FRAGMENT_SHADER, "shaders/compositionfragmentshader.glsl");
	guiVertexShader = ru_createShader(GL_VERTEX_SHADER, "shaders/guivertexshader.glsl");
	guiFragmentShader = ru_createShader(GL_FRAGMENT_SHADER, "shaders/guifragmentshader.glsl");

	opaqueShader = ru_createShaderProgram(vertexShader, opaqueFragmentShader);
	transShader = ru_createShaderProgram(vertexShader, transFragmentShader);
	compositionShader = ru_createShaderProgram(compositionVertexShader, compositionFragmentShader);
	guiShader = ru_createShaderProgram(guiVertexShader, guiFragmentShader);
}

void renderer_render(GLFWwindow *window) {
	/* Setup renderer and start rendering loop */

	/* Set callback functions */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, io_processMouseMovement);
	glfwSetKeyCallback(window, io_processKey);
	glfwSetCharCallback(window, io_processChar);
	glfwSetMouseButtonCallback(window, io_processMouseInput);
	glfwSetScrollCallback(window, io_processMouseScroll);

	/* To render composition quad at the end of rendering */
	GLuint compositionVAO, compositionVBO;
	glGenVertexArrays(1, &compositionVAO);
	glGenBuffers(1, &compositionVBO);
	glBindVertexArray(compositionVAO);
	glBindBuffer(GL_ARRAY_BUFFER, compositionVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(compositionQuad), compositionQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));
	glBindVertexArray(0);

	/* Link textures with shader samplers ; the textures are set to their proper slots in renderer_initBuffers */
	glUseProgram(opaqueShader);
	glUniform1i(glGetUniformLocation(opaqueShader, "atlas"), 0);
	glUseProgram(transShader);
	glUniform1i(glGetUniformLocation(transShader, "atlas"), 0);

	/* Link textures with shader samplers. The reason for using attachment 4
	 * is purely because the GUI was introduced after code for the rest was implemented */
	glUseProgram(guiShader);
	glUniform1i(glGetUniformLocation(guiShader, "atlas"), 4);
	glUseProgram(compositionShader);
	glUniform1i(glGetUniformLocation(compositionShader, "opaqueBuffer"), 1);
	glUniform1i(glGetUniformLocation(compositionShader, "accBuffer"), 2);
	glUniform1i(glGetUniformLocation(compositionShader, "revealBuffer"), 3);
	glUniform1i(glGetUniformLocation(compositionShader, "guiBuffer"), 5);

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
	int32_t i;

	mat4 view, perspective; /* In World Space, the camera is at (0, 0, 0) */
	vec3 cameraPos = {0.0f, 0.0f, 0.0f};
	vec3 relativeViewTarget, viewTarget;

	/* Shader locations */
	GLint opaqueViewLocation, transViewLocation, opaqueProjLocation, transProjLocation;
	opaqueViewLocation = glGetUniformLocation(opaqueShader, "viewMatrix");
	transViewLocation = glGetUniformLocation(transShader, "viewMatrix");
	opaqueProjLocation = glGetUniformLocation(opaqueShader, "projectionMatrix");
	transProjLocation = glGetUniformLocation(transShader, "projectionMatrix");

	GLenum glError;

	/* Render loop */
	while (!glfwWindowShouldClose(window)) {
		if ((glError = glGetError())) {
			fprintf(stderr, "OpenGL Error %d\n", glError);
			exit(RSM_EXIT_GLERROR);
		}

		if (GUI_SCHEDULEDUPDATE) { /* Redraw requested by updateGUI */
			gui_renderGUI();
			GUI_SCHEDULEDUPDATE = 0;
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
		glm_perspective(glm_rad(RSM_FOV), (float) screenWidth / (float) screenHeight,
				RSM_NEARPLANE, RSM_RENDER_DISTANCE * CHUNKSIZE, perspective);

		/* Things we must keep track of during rendering (OpenGL state stuff)
		 * glDepthMask and glDepthTest
		 * glCullFace
		 * glBlend */

		/* Rendering */
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);

		glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
		glDisable(GL_BLEND);

		glClearBufferfv(GL_COLOR, 0, COLOR_CLEAR);
		glClearBufferfv(GL_COLOR, 1, ZERO_CLEAR);
		glClearBufferfv(GL_COLOR, 2, ONE_CLEAR);
		glClearBufferfv(GL_DEPTH, 0, ONE_CLEAR);

		/* Opaque rendering */
		glUseProgram(opaqueShader);
		glUniformMatrix4fv(opaqueViewLocation, 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(opaqueProjLocation, 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nmesh; i++) {
			glBindVertexArray(meshes[i][0]);
			glDrawElements(GL_TRIANGLES, meshes[i][2], GL_UNSIGNED_INT, 0);
		}

		/* Wireframe (highlighting) rendering */
		glBindVertexArray(wiremesh[0]);
		glDrawElements(GL_LINES, wiremesh[1], GL_UNSIGNED_INT, 0);

		/* Transparent rendering */
		glDepthMask(GL_FALSE); glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
		glEnable(GL_BLEND);

		glUseProgram(transShader);
		glUniformMatrix4fv(transViewLocation, 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(transProjLocation, 1, GL_FALSE, (float *) perspective);
		for (i = 0; i < nmesh; i++) {
			glBindVertexArray(meshes[i][1]);
			glDrawElements(GL_TRIANGLES, meshes[i][3], GL_UNSIGNED_INT, 0);
		}

		/* GUI rendering */
		glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);
		glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);

		glClearBufferfv(GL_COLOR, 0, ZERO_CLEAR);
		glClearBufferfv(GL_DEPTH, 0, ONE_CLEAR);

		glUseProgram(guiShader);
		for (i = MAX_GUI_PRIORITY - 1; i >= 0; i--) {
			glBindVertexArray(guiVAO[i]);
			if (nGUIIndices[i]) glDrawElements(GL_TRIANGLES, nGUIIndices[i], GL_UNSIGNED_INT, 0);
		}

		/* Composition */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		/* Do not clear as entire screen is overwritten with composed image */
		glUseProgram(compositionShader);
		glBindVertexArray(compositionVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6); /* Draw final quad */

		glfwSwapBuffers(window); /* Render what's been drawn */
	    glfwPollEvents(); /* Update state and call appropriate callback functions for user input */
	}
}

void renderer_initBuffers(void) {
	/* Create GL buffers for rendering */

	/* Draw opaque scene to the opaque color buffer, saving depth data
	 * then render transparent scene (with depth check) to both accTex
	 * and revealTex (for WB-OIT algorithm) and finally composite them
	 * both and draw to screen framebuffer */
	/* Opaque color buffer */
	glGenTextures(1, &opaqueColorTex);
	glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Accumulator buffer (nearest for interpolation) */
    glGenTextures(1, &accTex);
	glBindTexture(GL_TEXTURE_2D, accTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Reveal buffer (R32F precision) */
	glGenTextures(1, &revealTex);
	glBindTexture(GL_TEXTURE_2D, revealTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, screenWidth, screenHeight, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Depth buffer */
	glGenTextures(1, &depthTex);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Draw any GUI elements to the GUI buffer (will get blended as a transparent layer in the
	 * composition stage) with depth check (only draw "closest" (priority) GUI elements) */

	/* GUI color buffer */
	glGenTextures(1, &guiColorTex);
	glBindTexture(GL_TEXTURE_2D, guiColorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* GUI depth buffer */
	glGenTextures(1, &guiDepthTex);
	glBindTexture(GL_TEXTURE_2D, guiDepthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Framebuffer generation and linking */

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

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making rendering framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	glGenFramebuffers(1, &GUIFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);

	/* Attach color buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, guiColorTex, 0);

	/* Attach depth buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, guiDepthTex, 0);

	glUseProgram(guiShader);
	mat4 guiProject;
	glm_ortho(0.0f, screenWidth, 0.0f, screenHeight, 0.0f, MAX_GUI_PRIORITY, guiProject);
	glUniformMatrix4fv(glGetUniformLocation(guiShader, "projectionMatrix"), 1, GL_FALSE, (float *) guiProject);

	GLenum guiDrawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &guiDrawBuffer);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making GUI framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	/* Texture mapping */
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textureAtlas);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, opaqueColorTex);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, accTex);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, revealTex);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, guiColorTex);
	glActiveTexture(GL_TEXTURE15); /* Avoid disconnecting textures on subsequent calls */
	glViewport(0, 0, screenWidth, screenHeight);
}

void renderer_freeBuffers() {
	/* Free rendering GL buffers */
	/* Delete textures */
    glDeleteTextures(1, &opaqueColorTex);
    glDeleteTextures(1, &accTex);
    glDeleteTextures(1, &revealTex);
    glDeleteTextures(1, &depthTex);

    glDeleteTextures(1, &guiColorTex);
    glDeleteTextures(1, &guiDepthTex);

	/* Delete framebuffers */
	glDeleteFramebuffers(1, &FBO);
	glDeleteFramebuffers(1, &GUIFBO);
}

void framebuffer_size_callback(GLFWwindow *window, int32_t width, int32_t height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	screenWidth = width;
	screenHeight = height;

	renderer_freeBuffers();
	renderer_initBuffers();
	gui_updateGUI(); /* Re-render with new size */
}
