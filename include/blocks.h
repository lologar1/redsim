#ifndef BLOCKS_H
#define BLOCKS_H

#include "rsmlayout.h"

#define CHUNKVOLUME (CHUNKSIZE * CHUNKSIZE * CHUNKSIZE)
#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})
#define LOW16MASK 0xFFFF
#define LOW21MASK 0x1FFFFF
#define LOW32MASK 0xFFFFFFFF
#define TOCHUNKINDEX(_X, _Y, _Z) (((u64) (_X) & LOW21MASK) << 42 \
		| ((u64) (_Y) & LOW21MASK) << 21 \
		| ((u64) (_Z) & LOW21MASK))
#define TOBLOCKINDEX(_POS) TOCHUNKINDEX(_POS[0], _POS[1], _POS[2]) /* Same cast */
#define SIGNED21CAST64(_N) ((i64) ((_N) | ((_N) & (1 << 20) ? (u64) ~LOW21MASK : 0)))
#define UNPACK21CAST64(_INDEX) { \
		SIGNED21CAST64((_INDEX) >> 42), \
		SIGNED21CAST64(((_INDEX) >> 21) & LOW21MASK), \
		SIGNED21CAST64((_INDEX) & LOW21MASK)}

typedef enum Blocktype {
	RSM_BLOCK_AIR,
	RSM_BLOCK_SILICON,
	RSM_BLOCK_GLASS,
	RSM_BLOCK_DIODE,
	RSM_BLOCK_TARGET,
	RSM_BLOCK_TRANSISTOR_ANALOG,
	RSM_BLOCK_TRANSISTOR_DIGITAL,
	RSM_BLOCK_LATCH,
	RSM_BLOCK_INVERTER,
	RSM_BLOCK_BUFFER,
	RSM_BLOCK_RESISTOR,
	RSM_BLOCK_CONSTANT_SOURCE_OPAQUE,
	RSM_BLOCK_CONSTANT_SOURCE_TRANS,
	RSM_BLOCK_LIGHT_DIGITAL,
	RSM_BLOCK_WIRE
} Blocktype;

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
