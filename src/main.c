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

void framebuffer_size_callback(GLFWwindow* window, int width, int height); /* Function to remap viewport whenever window is resized */
void processInput(GLFWwindow *window, vec3 cameraPos, vec3 relativeViewTarget); /* Check for user input each frame */
void mouse_callback(GLFWwindow* window, double xpos, double ypos); /* Adjust pitch/yaw on mouse movement */
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods); /* Single keypresses */

float pitch, yaw; /* Camera pitch and yaw values */

int main(int args, char *argv[]) {
	(void) args;
	(void) argv;

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

	glfwMakeContextCurrent(window); /* Set current context to this window */
	glfwSwapInterval(0);

	/* Callbacks */
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
	GLuint vertexShader = createShader(GL_VERTEX_SHADER, "shaders/vertexshader.glsl");
	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, "shaders/fragmentshader.glsl");

	GLuint shaderProgram = createShaderProgram(vertexShader, fragmentShader);

	glUseProgram(shaderProgram); /* Use shader program */

	float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
	};

	unsigned int indices[] = {
		0, 1, 2,   // first triangle
		2, 0, 3
	};

	vec3 cubePositions[] = {
		{ 0.0f,  0.0f,  0.0f },
		{ 2.0f,  5.0f, -15.0f },
		{-1.5f, -2.2f, -2.5f },
		{-3.8f, -2.0f, -12.3f },
		{ 2.4f, -0.4f, -3.5f },
		{-1.7f,  3.0f, -7.5f },
		{ 1.3f, -2.0f, -2.5f },
		{ 1.5f,  2.0f, -2.5f },
		{ 1.5f,  0.2f, -1.5f },
		{-1.3f,  1.0f, -1.5f }
	};

	unsigned int VAO; /* A VAO contains the VBO and current attribute configuration */
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO); /* Bind it so the VBO will save to it */

	unsigned int VBO; /* A VBO is a memory block (typically in the GPU) */
	glGenBuffers(1, &VBO); /* Create one buffer and assign its id to VBO */
	glBindBuffer(GL_ARRAY_BUFFER, VBO); /* Bind to ARRAY_BUFFER so we can write to it */
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); /* Copy vertex data to VBO */

	unsigned int EBO; /* An EBO stores indices to avoid duplicate vertices */
	glGenBuffers(1, &EBO); /* EBOs get saved to the VAO too */
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO); /* Bind to element buffer */
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW); /* Copy indices */

	/* Link vertex attributes to how our data is formatted.
	 * First param binds the attribute. In this case 0 is bound to 'in vec3 aPos'
	 * Second param is size (in this case sizeof(xyz) is 3)
	 * Third param specifies datatype, in this case float (32 bits)
	 * Fourth param is if we want the data normalized to [-1, 1]. No, it is already.
	 * Fifth is stride (space between 2 consecutive same attributes). Beware when non tightly-packed!
	 * Last is offset for the first attribute of the first vertex. In this case, none. */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0); //Pos
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float))); //Texture

	/* Enable attributes */
	glEnableVertexAttribArray(0); /* 0 because we defined position as 0 with layout */
	glEnableVertexAttribArray(1); /* Color */

	/* Set texture parameters */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); /* Repeat image when crossing border */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	/* Interpolate between mipmap switches, and also inside a mipmap on minification */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	/* Init these first as they also bind in their creation */
	GLuint containerTexture = createTexture("textures/wall.jpg");
	GLuint smileyTexture = createTexture("textures/honorable.jpg");

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, containerTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, smileyTexture);

	/* Uniforms */
	glUniform1i(glGetUniformLocation(shaderProgram, "texture0"), 0);
	glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 1);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); /* Set color on color frame clear (RGBa) */
	glEnable(GL_DEPTH_TEST);

	float time, lastTime = 0.0f;

	mat4 model, view, perspective; /* In World Space, the camera is at (0, 0, 0) */

	vec3 cameraPos = {0.0f, 0.0f, 0.0f};
	vec3 relativeViewTarget, viewTarget;

	/* Render loop */
	GLenum glError;
	while (!glfwWindowShouldClose(window)) {
		/* Game loop */

		if ((glError = glGetError())) {
			fprintf(stderr, "OpenGL Error %d\n", glError);
		}

		processInput(window, cameraPos, relativeViewTarget); /* Process user inputs for this frame */

		time = glfwGetTime();

		if (time - lastTime < 1.0f/FPS)
			glfwWaitEventsTimeout(1.0/FPS - (time - lastTime));

		lastTime = glfwGetTime(); /* Last frame */

		/* Set camera direction to appropriate pitch/yaw */
		relativeViewTarget[0] = cos(glm_rad(yaw)) * cos(glm_rad(pitch));
		relativeViewTarget[1] = sin(glm_rad(pitch));
		relativeViewTarget[2] = sin(glm_rad(yaw)) * cos(glm_rad(pitch));
		glm_vec3_normalize(relativeViewTarget);

		/* Create view translation matrix from cameraPos to origin */
		glm_vec3_add(cameraPos, relativeViewTarget, viewTarget);
		glm_lookat(cameraPos, viewTarget, upvector, view);

		/* Perspective projection matrix */
		glm_perspective(glm_rad(RSM_FOV), (float) WINDOW_WIDTH / (float) WINDOW_HEIGHT, 0.1f, 100.0f, perspective);

		/* Rendering */
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); /* Clear color buffer */

		/* Set matrices (model set per-block) */
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, (float *) view);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, (float *)perspective);

		for (int i = 0; i < 10; i++) {
			glm_mat4_identity(model);
			glm_translate(model, cubePositions[i]);
			float angle = 20.0f * i;
			if (i % 3 == 0) {
				angle = 25.0f * glfwGetTime();
			}
			glm_rotate(model, glm_rad(angle), (vec3) {1.0f, 0.3f, 0.5f});
			glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, (float *) model);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

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

	/* Resize viewport */
    glViewport(0, 0, width, height);
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
