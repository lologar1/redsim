#ifndef CHUNKUTILS_H
#define CHUNKUTILS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include "rsmlayout.h"
#include "usfhashmap.h"

#define CHUNKSIZE 8

extern vec3 position;

extern usf_hashmap *chunkmap;
extern usf_hashmap *meshmap;
extern GLuint **meshes;
extern int nmesh;

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
	unsigned int count[4]; /* Never change */
} Blockmesh;

typedef struct {
    unsigned int id : 13;       /* Block type e.g. WIRE */
    unsigned int variant : 8;   /* Block variant e.g. CONCRETE_GREEN */
    Rotation rotation : 3;      /* Block rotation */
    unsigned int metadata : 32; /* Metadata field for general purpose per-block storage */
} Blockdata;

typedef Blockdata ***Chunkdata;

void remeshChunk(uint64_t x, uint64_t y, uint64_t z);
void updateMeshlist(vec3 lastPosition);
void generateMeshlist(void);
Blockmesh *getBlockmesh(unsigned int id, unsigned int variant, Rotation rotation, uint64_t x, uint64_t y, uint64_t z);

#endif
