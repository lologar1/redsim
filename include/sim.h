#ifndef SIM_H
#define SIM_H

#define _GNU_SOURCE
#include <cglm/cglm.h>
#include <pthread.h>
#include <sched.h> /* NON-PORTABLE */

#include "usfstd.h"
#include "usfatomic.h"
#include "usflist.h"
#include "usfthread.h"
#include "usftime.h"

#include "blocks.h"
#include "chunkutils.h"
#include "wirefill.h"

typedef struct Visualdata {
	Blockdata *blockdata; /* Self */
	usf_listu64 *chunkindices; /* Unique chunk indices to remesh */
	u64 nwires;
	struct Component **wires; /* Match with wiredecay */
	u16 *wireindices;
	u8 *wiredecays;
} Visualdata;

typedef struct Component {
	u8 id; /* Underlying block ID */
	u8 variant; /* Underlying block variant */
	u8 state[8]; /* Internal state; for buffer 8, max. is 8 bytes */
	u32 metadata; /* Internal simulation metadata */
	u8 *buffer[2]; /* Primary and secondary inputs */
	usf_hashmap *inputs[2]; /* Blockdata * -> input offset */
	usf_listptr *connections; /* List of Connection */
	Visualdata *visualdata; /* Visual updating */
} Component;
static_assert(sizeof(Component) == 64, "redsim: Component struct size is not 64 bytes");

typedef struct Connection {
	Component *component;
	u16 index[2]; /* Buffer index (primary, secondary) */
	u8 decay[2]; /* Primary, secondary */
	u8 linkflags;
} Connection;

extern usf_hashmap *graphmap_; /* Blockdata * -> Component * */
extern i32 graphchanged_;
extern atomic_flag simstop_;
extern usf_mutex ticklock_;
extern usf_cond tickstep_;

void sim_init(void);
usf_compatibility_int sim_run(void *);

void sim_freecomponent(void *c);

#endif
