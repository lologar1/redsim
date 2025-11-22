#ifndef CHUNKUTILS_H
#define CHUNKUTILS_H

#include <pthread.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <stdarg.h>
#include "usfhashmap.h"
#include "usfqueue.h"
#include "rsmlayout.h"
#include "client.h"

#define UPVECTOR ((vec3) {0.0f, 1.0f, 0.0f})
#define RIGHTVECTOR ((vec3) {1.0f, 0.0f, 0.0f})
#define MESHCENTER ((vec3) {0.5f, 0.5f, 0.5f})

#define CHUNKCOORDMASK (0x1FFFFF)
#define TOCHUNKINDEX(x, y, z) ((uint64_t) ((x) & CHUNKCOORDMASK) << 42 | (uint64_t) ((y) & CHUNKCOORDMASK) << 21 | (uint64_t) ((z) & CHUNKCOORDMASK))
#define COORDSTOCHUNKINDEX(v) \
	TOCHUNKINDEX(chunkOffsetConvertFloat(v[0]), chunkOffsetConvertFloat(v[1]), chunkOffsetConvertFloat(v[2]))
#define COORDSTOBLOCKDATA(v, chunk) \
	 (&(*chunk)[blockOffsetConvertFloat(v[0])][blockOffsetConvertFloat(v[1])][blockOffsetConvertFloat(v[2])])
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
	unsigned int *opaqueIndices; /* Never change */
	unsigned int *transIndices;
	/* For count : nmembers is as follows : opaque vertices, trans vertices, opaque indices, trans indices */
	unsigned int count[4]; /* Never change */
} Blockmesh;

typedef struct {
    unsigned int id : 13;       /* Block type e.g. WIRE */
    unsigned int variant : 8;   /* Block variant e.g. CONCRETE_GREEN */
    Rotation rotation : 3;      /* Block rotation */
    unsigned int metadata : 32; /* Metadata field for general purpose per-block storage */
} Blockdata;

typedef Blockdata Chunkdata[CHUNKSIZE][CHUNKSIZE][CHUNKSIZE];

typedef struct Rawmesh { /* Raw data remeshed asynchronously, passed to main thread to dump in GL buffers */
	uint64_t chunkindex;
	float *opaqueVertexBuffer, *transVertexBuffer;
	unsigned int *opaqueIndexBuffer, *transIndexBuffer;
	unsigned int nOV, nTV, nOI, nTI;
} Rawmesh;

/* Buffers serving as remeshing scratchpad ; alloc'd once in parseBlockdata */
extern float *opaqueVertexBuffer, *transVertexBuffer;
extern unsigned int *opaqueIndexBuffer, *transIndexBuffer;
extern Blockmesh **blockmeshes;

void *pushRawmesh(void *chunkindexptr);
void remeshChunk(uint64_t chunkindex);
void updateMeshlist(void);
void generateMeshlist(void);
void getBlockmesh(Blockmesh *blockmesh, unsigned int id, unsigned int variant, Rotation rotation, int64_t x, int64_t y, int64_t z);
void translocationMatrix(mat4 translocation, vec3 translation, Rotation rotation);
void rotationMatrix(mat4 rotAdjust, Rotation rotation, vec3 meshcenter);
Blockdata *coordsToBlock(vec3 coords, uint64_t *chunkindex);

void pathcat(char *destination, int n, ...);

#endif
