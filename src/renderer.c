#include "renderer.h"

#define COLOR_CLEAR (f32 [4]) {0.098f, 0.098f, 0.098f, 1.0f}
#define ZERO_CLEAR (f32 [4]) {0.0f, 0.0f, 0.0f, 0.0f}
#define ONE_CLEAR (f32 [4]) {1.0f, 1.0f, 1.0f, 1.0f}

u64 screenWidth_ = DEFAULT_SCREEN_WIDTH;
u64 screenHeight_ = DEFAULT_SCREEN_HEIGHT;

static f32 compositionQuad[] = {
	/* Position		TexPos */
	-1.0f,  1.0f,	0.0f, 1.0f,
	-1.0f, -1.0f,	0.0f, 0.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	-1.0f,  1.0f,	0.0f, 1.0f,
	1.0f, -1.0f,	1.0f, 0.0f,
	1.0f,  1.0f,	1.0f, 1.0f
};

static void framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height);

/* Shader programs; not static since client_terminate imports them */
GLuint vertexShader_, opaqueFragmentShader_, transFragmentShader_, compositionVertexShader_,
	   compositionFragmentShader_, guiVertexShader_, guiFragmentShader_, opaqueShader_,
	   transShader_, compositionShader_, guiShader_;

/* Framebuffer stuff */
static GLuint FBO, GUIFBO;
static GLuint opaqueColorTex_, accTex_, revealTex_, depthTex_, guiColorTex_, guiDepthTex_;

void renderer_initShaders(void) {
	/* Compile shaders from GLSL programs and link them */
	vertexShader_ = ru_createShader(GL_VERTEX_SHADER, "shaders/vertexshader.glsl");
	opaqueFragmentShader_ = ru_createShader(GL_FRAGMENT_SHADER, "shaders/opaquefragmentshader.glsl");
	transFragmentShader_ = ru_createShader(GL_FRAGMENT_SHADER, "shaders/transfragmentshader.glsl");
	compositionVertexShader_ = ru_createShader(GL_VERTEX_SHADER, "shaders/compositionvertexshader.glsl");
	compositionFragmentShader_ = ru_createShader(GL_FRAGMENT_SHADER, "shaders/compositionfragmentshader.glsl");
	guiVertexShader_ = ru_createShader(GL_VERTEX_SHADER, "shaders/guivertexshader.glsl");
	guiFragmentShader_ = ru_createShader(GL_FRAGMENT_SHADER, "shaders/guifragmentshader.glsl");

	opaqueShader_ = ru_createShaderProgram(vertexShader_, opaqueFragmentShader_);
	transShader_ = ru_createShaderProgram(vertexShader_, transFragmentShader_);
	compositionShader_ = ru_createShaderProgram(compositionVertexShader_, compositionFragmentShader_);
	guiShader_ = ru_createShaderProgram(guiVertexShader_, guiFragmentShader_);
}

void renderer_render(GLFWwindow *window) {
	/* Setup renderer and start rendering loop; assumes GL buffers are properly initialized */

	/* Set callback functions */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, io_processMouseMovement);
	glfwSetKeyCallback(window, io_processKey);
	glfwSetCharCallback(window, io_processChar);
	glfwSetMouseButtonCallback(window, io_processMouseInput);
	glfwSetScrollCallback(window, io_processMouseScroll);

	/* Composition quad */
	GLuint compositionVAO, compositionVBO;
	glGenVertexArrays(1, &compositionVAO);
	glGenBuffers(1, &compositionVBO);
	glBindVertexArray(compositionVAO);
	glBindBuffer(GL_ARRAY_BUFFER, compositionVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(compositionQuad), compositionQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *) (2 * sizeof(f32)));
	glBindVertexArray(0);

	/* Link textures with shader samplers ; the textures are set to their proper slots in renderer_initBuffers */
	glUseProgram(opaqueShader_);
	glUniform1i(glGetUniformLocation(opaqueShader_, "atlas"), 0);
	glUseProgram(transShader_);
	glUniform1i(glGetUniformLocation(transShader_, "atlas"), 0);

	/* Link textures with shader samplers. The reason for using attachment 4
	 * is purely because the GUI was introduced after code for the rest was implemented */
	glUseProgram(guiShader_);
	glUniform1i(glGetUniformLocation(guiShader_, "atlas"), 4);
	glUseProgram(compositionShader_);
	glUniform1i(glGetUniformLocation(compositionShader_, "opaqueBuffer"), 1);
	glUniform1i(glGetUniformLocation(compositionShader_, "accBuffer"), 2);
	glUniform1i(glGetUniformLocation(compositionShader_, "revealBuffer"), 3);
	glUniform1i(glGetUniformLocation(compositionShader_, "guiBuffer"), 5);

	glBlendEquation(GL_FUNC_ADD);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glFrontFace(GL_CW);

	mat4 view, perspective; /* In World Space, the camera is at (0, 0, 0) */
	vec3 cameraPos = {0.0f, 0.0f, 0.0f};
	vec3 relativeViewTarget, viewTarget;

	/* Shader locations */
	GLint opaqueViewLocation, transViewLocation, opaqueProjLocation, transProjLocation;
	opaqueViewLocation = glGetUniformLocation(opaqueShader_, "viewMatrix");
	transViewLocation = glGetUniformLocation(transShader_, "viewMatrix");
	opaqueProjLocation = glGetUniformLocation(opaqueShader_, "projectionMatrix");
	transProjLocation = glGetUniformLocation(transShader_, "projectionMatrix");

	GLenum glError;

	/* Render loop */
	f32 time, lastTime;
	u64 i;

	lastTime = 0.0f;
	while (!glfwWindowShouldClose(window)) {
		if ((glError = glGetError())) {
			fprintf(stderr, "OpenGL Error %"PRIu32"\n", glError);
			exit(RSM_EXIT_GLERROR);
		}

		if (gui_scheduleUpdate_) { /* Redraw requested by updateGUI */
			gui_renderGUI();
			gui_scheduleUpdate_ = 0;
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
		glm_vec3_copy(orientation_, relativeViewTarget);
		glm_vec3_copy(position_, cameraPos);

		/* Create view translation matrix from cameraPos to origin:
		 * This matrix transforms world coordinates to be in front of
		 * our camera, effectively tilting the world to simulate having
		 * moved around. */
		glm_vec3_add(cameraPos, relativeViewTarget, viewTarget);
		glm_lookat(cameraPos, viewTarget, GLM_YUP, view);

		/* Perspective projection matrix */
		glm_perspective(glm_rad(RSM_FOV), (f32) screenWidth_ / (f32) screenHeight_,
				RSM_NEARPLANE, RSM_RENDER_DISTANCE * CHUNKSIZE, perspective);

		/* Things we must keep track of during rendering (OpenGL state)
		 * glDepthMask and glDepthTest
		 * glCullFace
		 * glBlend
		 * Draw buffer targets & their blend functions (if applicable) */

		/* Rendering */
		glBindFramebuffer(GL_FRAMEBUFFER, FBO);

		/* Opaque rendering */
		glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
		glDisable(GL_BLEND);

		static GLenum opaqueDrawTargets[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, opaqueDrawTargets); /* Set fragment shader output to proper buffers */
		glClearBufferfv(GL_COLOR, 0, COLOR_CLEAR);
		glClearBufferfv(GL_DEPTH, 0, ONE_CLEAR);

		glUseProgram(opaqueShader_);
		glUniformMatrix4fv(opaqueViewLocation, 1, GL_FALSE, (f32 *) view);
		glUniformMatrix4fv(opaqueProjLocation, 1, GL_FALSE, (f32 *) perspective);
		for (i = 0; i < nmeshes_; i++) {
			glBindVertexArray(meshes_[i][0]);
			if (meshes_[i][2]) glDrawElements(GL_TRIANGLES, (GLsizei) meshes_[i][2], GL_UNSIGNED_INT, 0);
		}

		/* Wireframe (highlighting) rendering */
		glBindVertexArray(wiremesh_[0]);
		glDrawElements(GL_LINES, (GLsizei) wiremesh_[1], GL_UNSIGNED_INT, 0);

		/* Transparent rendering */
		glDepthMask(GL_FALSE); glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
		glEnable(GL_BLEND);
		/* Add value in accumulation buffer; will divide by sum of alphas afterwards */
		glBlendFuncSeparatei(0, GL_ONE, GL_ONE, GL_ONE, GL_ONE);
		/* Multiply reveal buffer by new source to get opacity factor */
		glBlendFuncSeparatei(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR, GL_ZERO, GL_ZERO);

		static GLenum transDrawTargets[2] = {GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
		glDrawBuffers(2, transDrawTargets); /* Set fragment shader output to proper buffers */
		glClearBufferfv(GL_COLOR, 0, ZERO_CLEAR); /* Goddamn GL state + indirection */
		glClearBufferfv(GL_COLOR, 1, ONE_CLEAR);

		glUseProgram(transShader_);
		glUniformMatrix4fv(transViewLocation, 1, GL_FALSE, (f32 *) view);
		glUniformMatrix4fv(transProjLocation, 1, GL_FALSE, (f32 *) perspective);
		for (i = 0; i < nmeshes_; i++) {
			glBindVertexArray(meshes_[i][1]);
			if (meshes_[i][3]) glDrawElements(GL_TRIANGLES, (GLsizei) meshes_[i][3], GL_UNSIGNED_INT, 0);
		}

		/* GUI rendering */
		glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);
		glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		/* For GUI, do ordered transparency */
		glBlendFuncSeparatei(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		static GLenum guiDrawTargets[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, guiDrawTargets); /* Set fragment shader output to proper buffers */
		glClearBufferfv(GL_COLOR, 0, ZERO_CLEAR);
		glClearBufferfv(GL_DEPTH, 0, ONE_CLEAR);

		glUseProgram(guiShader_);
		for (i = MAX_GUI_MESHID; i > 0; i--) {
			glBindVertexArray(guiVAOs_[i - 1]);
			if (nGUIIndices_[i - 1]) glDrawElements(GL_TRIANGLES, nGUIIndices_[i - 1], GL_UNSIGNED_INT, 0);
		}

		/* Composition */
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		/* No glDrawBuffers call as this is the default framebuffer */
		/* Do not clear as entire screen is overwritten with composed image */

		glUseProgram(compositionShader_);
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
	glGenTextures(1, &opaqueColorTex_);
	glBindTexture(GL_TEXTURE_2D, opaqueColorTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth_, screenHeight_, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Accumulator buffer (nearest for interpolation) */
    glGenTextures(1, &accTex_);
	glBindTexture(GL_TEXTURE_2D, accTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth_, screenHeight_, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Reveal buffer (R32F precision) */
	glGenTextures(1, &revealTex_);
	glBindTexture(GL_TEXTURE_2D, revealTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, screenWidth_, screenHeight_, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Depth buffer */
	glGenTextures(1, &depthTex_);
	glBindTexture(GL_TEXTURE_2D, depthTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth_, screenHeight_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Draw any GUI elements to the GUI buffer (will get blended as a transparent layer in the
	 * composition stage) with depth check (only draw "closest" (priority) GUI elements) */

	/* GUI color buffer */
	glGenTextures(1, &guiColorTex_);
	glBindTexture(GL_TEXTURE_2D, guiColorTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenWidth_, screenHeight_, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* GUI depth buffer */
	glGenTextures(1, &guiDepthTex_);
	glBindTexture(GL_TEXTURE_2D, guiDepthTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth_, screenHeight_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Framebuffer generation and linking */

	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	/* Attach color buffers */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueColorTex_, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, accTex_, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, revealTex_, 0);

	/* Attach depth buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex_, 0);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making rendering framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	glGenFramebuffers(1, &GUIFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, GUIFBO);

	/* Attach color buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, guiColorTex_, 0);

	/* Attach depth buffer */
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, guiDepthTex_, 0);

	glUseProgram(guiShader_);
	mat4 guiProject;
	glm_ortho(0.0f, screenWidth_, 0.0f, screenHeight_, 0.0f, MAX_GUI_MESHID, guiProject);
	glUniformMatrix4fv(glGetUniformLocation(guiShader_, "projectionMatrix"), 1, GL_FALSE, (f32 *) guiProject);

	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		fprintf(stderr, "Error making GUI framebuffer (GL_FRAMEBUFFER_COMPLETE)\n");

	/* Texture mapping */
	glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, blockTextureAtlas_);
	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, opaqueColorTex_);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, accTex_);
	glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, revealTex_);
	glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, guiTextureAtlas_);
	glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, guiColorTex_);
	glActiveTexture(GL_TEXTURE15); /* Avoid disconnecting textures on subsequent calls */
	glViewport(0, 0, screenWidth_, screenHeight_);
}

void renderer_freeBuffers() {
	/* Free rendering GL buffers */

	/* Delete textures */
    glDeleteTextures(1, &opaqueColorTex_);
    glDeleteTextures(1, &accTex_);
    glDeleteTextures(1, &revealTex_);
    glDeleteTextures(1, &depthTex_);

    glDeleteTextures(1, &guiColorTex_);
    glDeleteTextures(1, &guiDepthTex_);

	/* Delete framebuffers */
	glDeleteFramebuffers(1, &FBO);
	glDeleteFramebuffers(1, &GUIFBO);
}

static void framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	/* Need to accomodate width/height being signed from GLFW */
	screenWidth_ = (u64) width;
	screenHeight_ = (u64) height;

	renderer_freeBuffers();
	renderer_initBuffers();
	gui_updateGUI(); /* Re-render with new size */
}
