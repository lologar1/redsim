#include <stdio.h>
#include <stdlib.h>

#include "renderer.h"
#include "client.h"
#include "rsmlayout.h"

int32_t main() {
	/* Make OpenGL context */
	glfwInit();

	/* Set OpenGL version 4.6 */
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	/* Use OpenGL core-profile */
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	/* Create window */
	GLFWwindow* window = glfwCreateWindow(DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT, WINDOW_NAME, NULL, NULL);

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
	fprintf(stderr, "Renderer initialization success!\n");

	if (atexit(client_terminate)) /* Register termination function for program exit */
		fprintf(stderr, "Warning: atexit client_terminate registration failed!\n");

	renderer_render(window);

	/* Window was closed rather than exit() from program */
	client_terminate(); /* Dealloc everything */
	glfwDestroyWindow(window);
	glfwTerminate(); /* On window close */

	fprintf(stderr, "Process exited normally.\n");
	fflush(stdout); fflush(stderr);
	_Exit(RSM_EXIT_NORMAL); /* Do not run client_terminate through atexit */
}
