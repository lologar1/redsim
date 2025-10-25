#ifndef REDSIM_H
#define REDSIM_H

#include <cglm/cglm.h>
#include "chunkutils.h"
#include "usfhashmap.h"
#include "rsmlayout.h"
#include "userio.h"

typedef enum {
	NORMAL,
	MENU,
	INVENTORY,
	COMMAND
} Gamestate;

extern Gamestate gamestate;

void rsm_move(vec3 position);
int AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2);
int64_t chunkOffsetConvertFloat(float absoluteComponent);
uint64_t blockOffsetConvertFloat(float absoluteComponent);

#endif
