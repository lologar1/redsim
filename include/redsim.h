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
extern vec3 ret_selection_[2];
extern vec3 ret_positions_[2];

void rsm_move(vec3 position);
void rsm_updateWiremesh(void);
void rsm_interact(void);
void rsm_placecoords(vec3 coords, vec3 adjacent);
void rsm_breakcoords(vec3 coords);
void rsm_checkMeshes(void);

#endif
