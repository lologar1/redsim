#ifndef REDSIM_H
#define REDSIM_H

#include <math.h>
#include <pthread.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "chunkutils.h"
#include "usfhashmap.h"
#include "usfqueue.h"
#include "rsmlayout.h"
#include "userio.h"

#define ASUID(id, variant) ((u64) (((u64) id << 32) | ((u64) variant)))
#define GETID(uid) ((u64) uid >> 32)
#define GETVARIANT(uid) ((u64) uid & 0xFFFFFFFF)
#define VECAPPLY(v, f) \
	v[0] = f(v[0]); v[1] = f(v[1]); v[2] = f(v[2]);

typedef enum Gamestate {
	NORMAL,
	MENU,
	INVENTORY,
	COMMAND
} Gamestate;

typedef enum Blocktype {
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
	RSM_BLOCK_CONSTANT_SOURCE_TRANS,
	RSM_BLOCK_LIGHT_DIGITAL,
	RSM_BLOCK_WIRE
} Blocktype;

typedef enum SpecialVariant {
	RSM_SPECIAL_ID,
	RSM_SPECIAL_SUBMENUSELECT,
	RSM_SPECIAL_SELECTIONTOOL,
	RSM_SPECIAL_INFORMATIONTOOL
} SpecialVariant;

extern Gamestate gamestate_;
extern i64 ret_selection_[6];
extern i64 ret_positions_[6];

void rsm_move(vec3 position);
void rsm_updateWiremesh(void);
void rsm_interact(void);
void rsm_checkMeshes(void);

#endif
