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
i32 rsm_isComponent(Blocktype block);

#endif
