#ifndef SIM_H
#define SIM_H

#include "usfstd.h"
#include "usflist.h"
#include "usfthread.h"
#include "chunkutils.h"

typedef struct Component {
	Blockdata *block; /* Pointer to the corresponding block */
	atomic_u8 buffer[8]; /* Temporary state inbetween exec and write stages */
	u8 state[8]; /* Stored SS power; for buffer 8s, max. is 8 bytes */
	usf_listptr *connections; /* List of Connection to components */
	usf_listptr *wireline; /* List of Connection to wires */
} Component;

typedef struct Connection {
	Component *component;
	u8 ssoffset; /* Signal strength decay */
} Connection;

extern usf_mutex graphlock_;
extern i32 graphchanged_;

#endif
