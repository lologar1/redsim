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
	GLFWwindow* window = glfwCreateWindow(1920, 1080, WINDOW_NAME, NULL, NULL);

	if (window == NULL) {
		fprintf(stderr, "Fatal error creating GLFW window, exiting.\n");
		glfwTerminate();
		exit(RSM_EXIT_INITFAIL);
	}

	glfwMakeContextCurrent(window);

#if VSYNC == true
	glfwSwapInterval(1);
#else
	glfwSwapInterval(0);
#endif

	/* Initialize GLAD */
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "Fatal error initializing GLAD, exiting.\n");
		glfwTerminate();
		exit(RSM_EXIT_INITFAIL);
	}

	/* Start rendering and gameloop */
	fprintf(stderr, "Starting %s\n", WINDOW_NAME);
	client_init();
	renderer_initShaders();
	renderer_initBuffers();

	renderer_render(window);

	client_terminate(); /* Dealloc everything */
	glfwDestroyWindow(window);
	glfwTerminate(); /* On window close */

	fprintf(stderr, "Process exited normally.\n");
	exit(RSM_EXIT_NORMAL);
}
