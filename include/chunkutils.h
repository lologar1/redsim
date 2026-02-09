#ifndef CHUNKUTILS_H
#define CHUNKUTILS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "usfhashmap.h"
#include "usfqueue.h"
#include "usfskiplist.h"
#include "programio.h"
#include "rsmlayout.h"
#include "blocks.h"
#include "client.h"

void cu_asyncRemeshChunk(u64 chunkindex);
void cu_updateMeshlist(void);
void cu_generateMeshlist(void);

void cu_translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation);
void cu_rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter);
i64 cu_chunkOffsetConvertFloat(f32 absoluteComponent);
u64 cu_blockOffsetConvertFloat(f32 absoluteComponent);
Blockdata *cu_coordsToBlock(vec3 coords, u64 *chunkindex);
Blockdata *cu_posToBlock(i64 x, i64 y, i64 z, u64 *chunkindex);
i32 cu_AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2);

void cu_deferRemesh(usf_skiplist *toremesh, u64 chunkindex);
void cu_deferArea(usf_skiplist *toremesh, i64 x, i64 y, i64 z);
void cu_doRemesh(usf_skiplist *toremesh);

#endif
