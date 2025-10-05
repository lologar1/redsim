#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include "renderutils.h"
#include "client.h"
#include "rsmlayout.h"

#include <stdio.h>
#include <math.h>

#define WINDOW_NAME "Redsim V0.1"
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

extern GLuint textureAtlas;

void framebuffer_size_callback(GLFWwindow* window, int width, int height); /* Function to remap viewport whenever window   is resized */
int render(GLFWwindow* window); /* Render scene with calls to the client */

#endif
