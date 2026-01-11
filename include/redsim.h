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

typedef enum {
	RSM_BLOCK_AIR,
	RSM_BLOCK_SILICON,
	RSM_BLOCK_GLASS,
	RSM_BLOCK_DIODE,
	RSM_BLOCK_TARGET,
	RSM_BLOCK_TRANSISTOR_ANALOG,
	RSM_BLOCK_TRANSISTOR_DIGITAL,
	RSM_BLOCK_LATCH,
	RSM_BLOCK_INVERTER,
	RSM_BLOCK_BUFFER,
	RSM_BLOCK_RESISTOR,
	RSM_BLOCK_CONSTANT_SOURCE_OPAQUE,
	RSM_BLOCK_CONSTANT_SOURCE_TRANS
} Blocktype;

typedef enum {
	RSM_SPECIAL_ID,
	RSM_SPECIAL_SUBMENUSELECT,
	RSM_SPECIAL_SELECTIONTOOL,
	RSM_SPECIAL_INFORMATIONTOOL
} SpecialVariant;

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
