#ifndef SIM_H
#define SIM_H

#include <cglm/cglm.h>
#include "usfstd.h"
#include "usfatomic.h"
#include "usflist.h"
#include "usfthread.h"
#include "usftime.h"

#include "blocks.h"
#include "chunkutils.h"
#include "wirefill.h"

typedef struct Visualdata {
	usf_listu64 *chunkindices; /* Unique chunk indices to remesh */
	Blockdata **components; /* Match with connections */
	Blockdata **wires; /* Match with wiredecay */
	u8 *wiredecays;
} Visualdata;

typedef struct Component {
	u16 id; /* Underlying block ID */
	u16 metadata; /* Flags (e.g. force visual for lamps, etc.) */
	u32 runtime; /* Runtime information, profiling */
	atomic_u8 buffer[8]; /* Temporary state inbetween exec and write stages */
	u8 state[8]; /* Stored SS power; for buffer 8s, max. is 8 bytes */
	usf_listptr *connections; /* List of Connection */
	usf_listptr *reassert; /* To prime if any target is modified */
	Visualdata *visualdata; /* Visual updating */
} Component;
static_assert(sizeof(Component) == 48, "RSM: Component alignment/struct packing is not tight!");

typedef struct Connection {
	Component *component;
	u8 decay[2];
	u8 linkflags;
} Connection;

extern usf_mutex *graphlock_;
extern usf_hashmap *graphmap_;
extern i32 graphchanged_;
extern atomic_flag simstop_;

void sim_init(void);
usf_compatibility_int sim_run(void *);

#endif
