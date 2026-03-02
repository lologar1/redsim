#ifndef WIREFILL_H
#define WIREFILL_H

#include "usfstd.h"
#include "usfhashmap.h"
#include "usfqueue.h"

#include "sim.h"

/* Convenience for wf_newcontext */
#define RSM_KEEP_VISUAL_INFO 0
#define RSM_DISCARD_VISUAL_INFO 1

#define INVROT(_ROT) (((_ROT) + 1) % 4 + 1)
#define PERPROT(_ROT) ((_ROT) % 4 + 1)
#define COLINROT(_ROT, _OTHER) (((_ROT) == (_OTHER)) || ((_ROT) == INVROT(_OTHER)))

typedef struct Linkinfo {
	Blockdata *block;
	vec3 coords;
	u8 linkflags; /* Data about link interaction with wireline (wire, read, etc.) */
	u8 decay[2]; /* Primary and secondary write decay */
} Linkinfo;

typedef struct Fillcontext {
	usf_queue *next; /* Blocks to check (may connect to this wireline) */
	usf_hashmap *affected; /* Component Blockdata * -> Linkinfo * */
	usf_hashmap *seen; /* Wire Blockdata * -> decay + 1 (hashmap inits to 0; used for unfilled) */
	usf_hashmap *wires; /* (Optional, unset if NULL) Wire Blockdata * -> decay */
	usf_hashmap *chunkindices; /* (Optional, unset if NULL) Set of chunkindex */
} Fillcontext;

typedef struct Fillcandidate {
	vec3 coords;
	Rotation from;
	u8 decay;
	u8 status;
} Fillcandidate;

void wf_findaffected(vec3 coords, Fillcontext *afcontext);
void wf_componentconnect(vec3 coords, Rotation from, Fillcontext *fillcontext, u8 status); /* Entry point */
void wf_wirefill(vec3 coords, Rotation from, u8 decay, Fillcontext *fillcontext, u8 status);
void wf_registercomponent(vec3 coords);

Fillcontext *wf_newcontext(i32 discardvisual);
void wf_freecontext(Fillcontext *context);
void wf_registercoords(vec3 coords);
void wf_registercontext(Fillcontext *context);

i32 wf_iscomponent(Blocktype id);
i32 wf_isregistrable(Blocktype id);

#endif
