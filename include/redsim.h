#ifndef REDSIM_H
#define REDSIM_H

#include <cglm/cglm.h>
#include "chunkutils.h"
#include "usfhashmap.h"
#include "rsmlayout.h"

extern usf_hashmap *chunkmap;
extern float (**boundingboxes)[6];

void rsm_move(vec3 position, vec3 momentum);
void AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2, vec3 mask);
int64_t chunkOffsetConvertFloat(float absoluteComponent);
uint64_t blockOffsetConvertFloat(float absoluteComponent);

#endif
