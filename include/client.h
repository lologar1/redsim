#ifndef CLIENT_H
#define CLIENT_H

#include <glad/glad.h>
#include <pthread.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "rsmlayout.h"
#include "redsim.h"
#include "userio.h"
#include "renderutils.h"
#include "guiutils.h"
#include "gui.h"
#include "usfhashmap.h"
#include "usfqueue.h"

/* Client procedures */
extern float pitch, yaw;
extern vec3 orientation, position;

extern usf_hashmap *chunkmap, *meshmap, *datamap, *namemap;

extern GLuint **meshes;
extern int32_t nmesh;

extern pthread_mutex_t meshlock;
extern usf_queue *meshqueue;

void client_init(void);

void client_frameEvent(GLFWwindow *window);
void client_getOrientationVector(vec3 ori);
void client_getPosition(vec3 pos);

void client_terminate(void);

#endif
