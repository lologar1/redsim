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
	u16 id; /* Underlying block ID (cast from u16 as all components < 255) */
	u16 metadata; /* Simulation flags */
	u32 runtime; /* Runtime information, profiling */
	u8 state[8]; /* Internal state; for buffer 8, max. is 8 bytes */
	u32 ninputs[2]; /* Number of buffer inputs (primary, secondary) */
	usf_listu8 *buffer[2]; /* Primary and secondary inputs */
	usf_listptr *connections; /* List of Connection */
	Visualdata *visualdata; /* Visual updating */
} Component;

typedef struct Connection {
	Component *component;
	u32 index[2]; /* Buffer index (primary, secondary) */
	u8 decay[2]; /* Primary, secondary */
	u8 linkflags;
} Connection;

extern usf_mutex *graphlock_; /* graphmap_ mutex */
extern usf_hashmap *graphmap_; /* Blockdata * -> Component * */
extern i32 graphchanged_;
extern atomic_flag simstop_;

void sim_init(void);
usf_compatibility_int sim_run(void *);

#endif
