#ifndef CLIENT_H
#define CLIENT_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "cglm/cglm.h"
#include "rsmlayout.h"
#define upvector (vec3) {0.0f, 1.0f, 0.0f}

/* Renderer communication */
void client_getChunks(GLuint (**chunks)[4], int *nchunks);

void client_frameEvent(GLFWwindow *window);
void client_mouseEvent(GLFWwindow *window, double xpos, double ypos);
void client_keyboardEvent(GLFWwindow *window, int key, int scancode, int action, int mods);
void client_getOrientationVector(vec3 ori);
void client_getPosition(vec3 pos);

#endif
