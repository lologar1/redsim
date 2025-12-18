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

#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})

#define CHUNKCOORDMASK (0x1FFFFF) /* Lower 21 bits */
#define TOCHUNKINDEX(X, Y, Z) ((uint64_t) ((X) & CHUNKCOORDMASK) << 42 \
		| (uint64_t) ((Y) & CHUNKCOORDMASK) << 21 \
		| (uint64_t) ((Z) & CHUNKCOORDMASK))

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
    uint16_t id;       /* Block type e.g. WIRE */
    uint8_t variant;   /* Block variant e.g. CONCRETE_GREEN */
    Rotation rotation;      /* Block rotation */
    uint32_t metadata; /* Metadata field for general purpose per-block storage */
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
