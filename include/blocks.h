#ifndef BLOCKS_H
#define BLOCKS_H

#include "rsmlayout.h"

#define CHUNKVOLUME (CHUNKSIZE * CHUNKSIZE * CHUNKSIZE)
#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})
#define CHUNKCOORDMASK (0x1FFFFF)
#define TOCHUNKINDEX(_X, _Y, _Z) (((u64) (_X) & CHUNKCOORDMASK) << 42 \
		| ((u64) (_Y) & CHUNKCOORDMASK) << 21 \
		| ((u64) (_Z) & CHUNKCOORDMASK))

typedef enum Rotation : u8 {
	NONE,
	NORTH,
	WEST,
	SOUTH,
	EAST,
	UP,
	DOWN,
	COMPLEX
} Rotation;

typedef struct Blockmesh {
	f32 *opaqueVertices, *transVertices;
	u32 *opaqueIndices, *transIndices;
	u64 count[4]; /* OV, TV, OI, TI */
} Blockmesh;

typedef struct Blockdata {
	u16 id;
	u8 variant;
	Rotation rotation;
	u32 metadata;
} Blockdata;

typedef Blockdata Chunkdata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE];

typedef struct Rawmesh {
	u64 chunkindex;
	f32 *opaqueVertexBuffer, *transVertexBuffer;
	u32 *opaqueIndexBuffer, *transIndexBuffer;
	u64 nOV, nTV, nOI, nTI;
} Rawmesh;

#endif
