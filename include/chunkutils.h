#ifndef CHUNKUTILS_H
#define CHUNKUTILS_H

#include <pthread.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "usfhashmap.h"
#include "usfqueue.h"
#include "programio.h"
#include "rsmlayout.h"
#include "client.h"

#define CHUNKVOLUME (CHUNKSIZE * CHUNKSIZE * CHUNKSIZE)
#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})
#define CHUNKCOORDMASK (0x1FFFFF) /* Lower 21 bits */
#define TOCHUNKINDEX(X, Y, Z) (((u64) (X) & CHUNKCOORDMASK) << 42 \
		| ((u64) (Y) & CHUNKCOORDMASK) << 21 \
		| ((u64) (Z) & CHUNKCOORDMASK))

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
	f32 *opaqueVertices;
	f32 *transVertices;
	u32 *opaqueIndices;
	u32 *transIndices;
	u64 count[4]; /* OV, TV, OI, TI */
} Blockmesh;

typedef struct Blockdata {
    u16 id;
	u8 variant;
	Rotation rotation;
    u32 metadata;
} Blockdata;

typedef Blockdata Chunkdata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE];

typedef struct Rawmesh { /* Remeshed data passed back to main thread */
	u64 chunkindex;
	f32 *opaqueVertexBuffer, *transVertexBuffer;
	u32 *opaqueIndexBuffer, *transIndexBuffer;
	u64 nOV, nTV, nOI, nTI;
} Rawmesh;

void cu_asyncRemeshChunk(u64 chunkindex);
void cu_updateMeshlist(void);
void cu_generateMeshlist(void);
void cu_translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation);
void cu_rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter);
i64 cu_chunkOffsetConvertFloat(f32 absoluteComponent);
u64 cu_blockOffsetConvertFloat(f32 absoluteComponent);
Blockdata *cu_coordsToBlock(vec3 coords, u64 *chunkindex);
Blockdata *cu_posToBlock(i64 x, i64 y, i64 z, u64 *chunkindex);
i32 cu_AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2);

#endif
