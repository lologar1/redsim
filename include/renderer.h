#ifndef RENDERER_H
#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include "renderutils.h"
#include "client.h"
#include "rsmlayout.h"
#include "gui.h"

#include <stdio.h>
#include <math.h>

extern unsigned int screenWidth, screenHeight;

void framebuffer_size_callback(GLFWwindow *window, int width, int height); /* Function to remap viewport whenever window   is resized */
void renderer_initShaders(void);
void renderer_render(GLFWwindow *window); /* Render scene with calls to the client */
void renderer_initBuffers(void);
void renderer_freeBuffers(void);

#endif
