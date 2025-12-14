#ifndef CHUNKUTILS_H
#define CHUNKUTILS_H

#include <pthread.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdarg.h>
#include <inttypes.h>
#include "usfhashmap.h"
#include "usfqueue.h"
#include "rsmlayout.h"
#include "client.h"

#define UPVECTOR ((vec3) {0.0f, 1.0f, 0.0f})
#define RIGHTVECTOR ((vec3) {1.0f, 0.0f, 0.0f})
#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})

#define CHUNKCOORDMASK (0x1FFFFF)
#define TOCHUNKINDEX(x, y, z) ((uint64_t) ((x) & CHUNKCOORDMASK) << 42 \
		| (uint64_t) ((y) & CHUNKCOORDMASK) << 21 \
		| (uint64_t) ((z) & CHUNKCOORDMASK))

#define COORDSTOCHUNKINDEX(v) \
	TOCHUNKINDEX(cu_chunkOffsetConvertFloat(v[0]), \
			cu_chunkOffsetConvertFloat(v[1]), \
			cu_chunkOffsetConvertFloat(v[2]))

#define COORDSTOBLOCKDATA(v, chunk) \
	 (&(*chunk)[cu_blockOffsetConvertFloat(v[0])] \
	  [cu_blockOffsetConvertFloat(v[1])] \
	  [cu_blockOffsetConvertFloat(v[2])])

#define SIGNED21CAST64(n) ((n) | (n & (1 << 20) ? (uint64_t) ~CHUNKCOORDMASK : 0))

typedef enum {
    NONE,
    NORTH,
    WEST,
    SOUTH,
    EAST,
    UP,
    DOWN,
    COMPLEX
} Rotation;

typedef struct {
	float *opaqueVertices; /* Recalculated for rotation */
	float *transVertices;
	uint32_t *opaqueIndices; /* Never change */
	uint32_t *transIndices;
	/* For count : nmembers is as follows : opaque vertices, trans vertices, opaque indices, trans indices */
	uint32_t count[4]; /* Never change */
} Blockmesh;

typedef struct {
    uint32_t id : 13;       /* Block type e.g. WIRE */
    uint32_t variant : 8;   /* Block variant e.g. CONCRETE_GREEN */
    Rotation rotation : 3;      /* Block rotation */
    uint32_t metadata : 32; /* Metadata field for general purpose per-block storage */
} Blockdata;

typedef Blockdata Chunkdata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE];

typedef struct Rawmesh { /* Raw data remeshed asynchronously, passed to main thread to dump in GL buffers */
	uint64_t chunkindex;
	float *opaqueVertexBuffer, *transVertexBuffer;
	uint32_t *opaqueIndexBuffer, *transIndexBuffer;
	uint32_t nOV, nTV, nOI, nTI;
} Rawmesh;

extern Blockmesh **blockmeshes;

void cu_asyncRemeshChunk(uint64_t chunkindex);
void cu_updateMeshlist(void);
void cu_generateMeshlist(void);
void cu_translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation);
void cu_rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter);
Blockdata *cu_coordsToBlock(vec3 coords, uint64_t *chunkindex);
int64_t cu_chunkOffsetConvertFloat(float absoluteComponent);
uint64_t cu_blockOffsetConvertFloat(float absoluteComponent);
int32_t cu_AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2);

/*TODO*/
void pathcat(char *destination, int32_t n, ...);

#endif
