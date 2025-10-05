#ifndef CLIENT_H
#define CLIENT_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <math.h>
#include "rsmlayout.h"
#include "chunkutils.h"
#include "renderutils.h"

/* Client procedures */
void client_init(void);

/* Renderer communication */
void client_getChunks(GLuint ***chunks, int *nchunks);

void client_frameEvent(GLFWwindow *window);
void client_mouseEvent(GLFWwindow *window, double xpos, double ypos);
void client_keyboardEvent(GLFWwindow *window, int key, int scancode, int action, int mods);
void client_getOrientationVector(vec3 ori);
void client_getPosition(vec3 pos);

#endif
