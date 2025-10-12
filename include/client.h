#ifndef CLIENT_H
#define CLIENT_H

#include <glad/glad.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "rsmlayout.h"
#include "chunkutils.h"
#include "renderutils.h"
#include "redsim.h"
#include "userio.h"
#include "gui.h"

extern GLuint guiVAO;
extern unsigned int nGUIIndices;

/* Client procedures */
void client_init(void);

/* Renderer communication */
void client_getGUI(GLuint *gui, unsigned int *ngui);
void client_getChunks(GLuint ***chunks, int *nchunks);

void client_frameEvent(GLFWwindow *window);
void client_getOrientationVector(vec3 ori);
void client_getPosition(vec3 pos);

#endif
