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

extern vec3 *PLAYERBBOFFSETS;
extern u64 NPLAYERBBOFFSETS;

extern f32 pitch_;
extern f32 yaw_;
extern vec3 orientation_;
extern vec3 position_;

extern u8 sspower_;

extern usf_hashmap *chunkmap_;
extern usf_hashmap *meshmap_;
extern usf_hashmap *datamap_;
extern usf_hashmap *namemap_;
extern usf_queue *meshqueue_;

extern GLuint **meshes_;
extern u64 nmeshes_;
extern GLuint wiremesh_[2];

void client_init(void);
void client_savedata(void);
void client_frameEvent(GLFWwindow *window);
void client_terminate(void);

#endif
