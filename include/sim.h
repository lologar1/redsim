#ifndef SIM_H
#define SIM_H

#include <cglm/cglm.h>
#include "usfstd.h"
#include "usflist.h"
#include "usfthread.h"
#include "blocks.h"
#include "chunkutils.h"

typedef struct Visualdata {
	u64 chunkindex; /* Used for remeshing */
	Blockdata *block; /* Pointer to the corresponding block */
	usf_listptr *wireline; /* List of Connection to wires */
} Visualdata;

typedef struct Component {
	u16 id; /* Underlying component ID */
	u16 metadata; /* Flags (e.g. force visual for lamps, etc.) */
	u32 runtime; /* Runtime information, profiling */
	atomic_u8 buffer[8]; /* Temporary state inbetween exec and write stages */
	u8 state[8]; /* Stored SS power; for buffer 8s, max. is 8 bytes */
	usf_listptr *connections; /* List of Connection to components */
	Visualdata *visualdata; /* Information used to visually update blocks */
} Component;
static_assert(sizeof(Component) == 40, "RSM: Component alignment/struct packing is not tight!");

typedef struct Connection {
	Component *component;
	u8 ssoffset; /* Signal strength decay */
} Connection;

extern usf_mutex *graphlock_;
extern usf_hashmap *graphmap_;
extern i32 graphchanged_;
extern atomic_i32 simstop_;

void sim_init(void);
void sim_registerCoords(vec3 coords);
void sim_registerPos(i64 x, i64 y, i64 z);
void sim_removeCoords(vec3 coords);
void sim_removePos(i64 x, i64 y, i64 z);
usf_compatibility_int sim_run(void *);

#endif
