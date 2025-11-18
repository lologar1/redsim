#ifndef REDSIM_H
#define REDSIM_H

#include <math.h>
#include <cglm/cglm.h>
#include "chunkutils.h"
#include "usfhashmap.h"
#include "rsmlayout.h"
#include "userio.h"
#include "ret.h"

#define ASUID(id, variant) ((uint64_t) (((uint64_t) id << 32) | ((uint64_t) variant)))
#define GETID(uid) ((uint64_t) uid >> 32)
#define GETVARIANT(uid) ((uint64_t) uid & 0xFFFFFFFF)
#define VECAPPLY(v, f) \
	v[0] = f(v[0]); v[1] = f(v[1]); v[2] = f(v[2]);

typedef enum {
	NORMAL,
	MENU,
	INVENTORY,
	COMMAND
} Gamestate;

extern usf_hashmap *datamap;
extern Gamestate gamestate;
extern GLuint wiremesh[2];
extern vec3 *playerBBOffsets;
extern unsigned int nPlayerBBOffsets;

void rsm_move(vec3 position);
int AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2);
int64_t chunkOffsetConvertFloat(float absoluteComponent);
uint64_t blockOffsetConvertFloat(float absoluteComponent);
void initWiremesh(void);
void rsm_updateWiremesh(void);
void rsm_interact(void);

#endif
