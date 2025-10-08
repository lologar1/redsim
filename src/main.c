#include <stdio.h>

#include "renderer.h"
#include "client.h"
#include "rsmlayout.h"

int main() {
	/* Make OpenGL context */
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

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "Fatal error initializing GLAD, exiting.\n");
		glfwTerminate();
		return -1;
	}

	/* Start rendering and gameloop */
	fprintf(stderr, "Starting RedsimV0.1\n");
	client_init();
	int returnCode = render(window);

	fprintf(stderr, "Process ended with code %d\n", returnCode);

	return returnCode;
}
