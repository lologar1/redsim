#ifndef SIM_H
#define SIM_H

#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <cglm/cglm.h>
#include "usfstd.h"
#include "usfatomic.h"
#include "usflist.h"
#include "usfthread.h"
#include "blocks.h"
#include "chunkutils.h"
#include "usftime.h"

typedef struct Visualdata {
	u64 chunkindex;
	Blockdata *component;
} Visualdata;

typedef struct Wireline {
	u64 nwires;
	u64 *chunkindices;
	Blockdata **wiredata;
	u64 *ssoffsets;
} Wireline;

typedef struct Component {
	u16 id; /* Underlying component ID */
	u16 metadata; /* Flags (e.g. force visual for lamps, etc.) */
	u32 runtime; /* Runtime information, profiling */
	atomic_u8 buffer[8]; /* Temporary state inbetween exec and write stages */
	u8 state[8]; /* Stored SS power; for buffer 8s, max. is 8 bytes */
	usf_listptr *connections; /* List of Connection to components */
	Wireline *wireline;
} Component;
static_assert(sizeof(Component) == 40, "RSM: Component alignment/struct packing is not tight!");

typedef struct Connection {
	Component *component;
	Visualdata *visualdata; /* Information used to visually update blocks */
	u32 flags;
	u8 pdecay;
	u8 sdecay;
} Connection;

extern usf_mutex *graphlock_;
extern usf_hashmap *graphmap_;
extern i32 graphchanged_;
extern atomic_flag simstop_;

void sim_init(void);
void sim_registerCoords(vec3 coords);
void sim_registerPos(i64 x, i64 y, i64 z);
usf_compatibility_int sim_run(void *);

#endif
