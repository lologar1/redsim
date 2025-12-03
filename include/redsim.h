#ifndef REDSIM_H
#define REDSIM_H

#include <math.h>
#include <pthread.h>
#include <cglm/cglm.h>
#include "chunkutils.h"
#include "usfhashmap.h"
#include "usfqueue.h"
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

extern Gamestate gamestate;
extern GLuint wiremesh[2];
extern vec3 *playerBBOffsets;
extern uint32_t nPlayerBBOffsets;

void rsm_move(vec3 position);
void rsm_initWiremesh(void);
void rsm_updateWiremesh(void);
void rsm_interact(void);
void rsm_checkMeshes(void);

#endif
