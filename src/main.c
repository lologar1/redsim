/* GL Loader Generator for cross-platform function loading */
#include <glad/glad.h>

/* OpenGL Framework Library */
#include <GLFW/glfw3.h>

/* utils and usflib2 includes */
#include "shaderutils.h"

/* C Standard Library include */
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define WINDOW_NAME "Redsim V0.1"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

void framebuffer_size_callback(GLFWwindow* window, int width, int height); /* Function to remap viewport whenever window is resized */
void processInput(GLFWwindow *window); /* Check for user input each frame */

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

	/* Set this function to be called whenever the window resizes */
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

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

	/* Triangle test stuff */
	float vertices[] = {
		0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, // Bottom right
		-0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, // Bottom left
		0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, // Top middle
	};

	unsigned int indices[] = {
		0, 1, 2,   // first triangle
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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) 0);

	/* Color */
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));

	/* Enable attributes */
	glEnableVertexAttribArray(0); /* 0 because we defined position as 0 with layout */
	glEnableVertexAttribArray(1); /* Color */

	glUseProgram(shaderProgram); /* Use shader program */

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); /* Wireframe mode */

	/* Render loop */
	while (!glfwWindowShouldClose(window)) {
		processInput(window); /* Process user inputs for this frame */

		/* Rendering */
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f); /* Set color on color frame clear (RGBa) */
		glClear(GL_COLOR_BUFFER_BIT); /* Clear color buffer */

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0); /* Draw triangles from the 6 indices in the EBO */

	    glfwSwapBuffers(window); /* Render what's been drawn */
	    glfwPollEvents(); /* Update state and call appropriate callback functions for user input */
	}

	/* Window has been closed */
	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow *window) {
	/* Check for user input each frame */

	if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) /* Check if ESCAPE is currently pressed */
		glfwSetWindowShouldClose(window, true); /* Close window next frame */
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	/* Resize viewport when this function (window resize) is called.
	 * This function is called when the window is first displayed too. */

	(void) window; /* Needed to fit prototype */

	/* Resize viewport */
    glViewport(0, 0, width, height);
}
