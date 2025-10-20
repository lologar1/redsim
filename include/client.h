#ifndef CLIENT_H
#define CLIENT_H

#include <glad/glad.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "rsmlayout.h"
#include "redsim.h"
#include "userio.h"
#include "renderutils.h"
#include "gui.h"

/* Client procedures */
extern float pitch, yaw;
extern vec3 orientation, position;
extern usf_hashmap *chunkmap, *meshmap;
extern GLuint **meshes;
extern int nmesh;

void client_init(void);

void client_frameEvent(GLFWwindow *window);
void client_getOrientationVector(vec3 ori);
void client_getPosition(vec3 pos);

#endif
