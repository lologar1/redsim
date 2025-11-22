#ifndef RENDERUTILS_H
#define RENDERUTILS_H

#include <glad/glad.h>
#include "chunkutils.h"
#include "usfstring.h"
#include "usfio.h"
#include "redsim.h"

#define CHUNKVOLUME (CHUNKSIZE * CHUNKSIZE * CHUNKSIZE)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

extern float (**boundingboxes)[6];
extern GLuint textureAtlas;
extern uint64_t **spriteids;

extern size_t ov_bufsiz, tv_bufsiz, oi_bufsiz, ti_bufsiz;

typedef float Vertex[8];

GLuint createShader(GLenum shaderType, char *shaderSource);
GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
void atlasAppend(char *meshname, int texSizeX, int texSizeY, unsigned char **atlasptr, GLsizei *atlassize);
void parseBlockdata(void); /* Create texture atlas, blockmeshes and bounding boxes */
void loadVertexData(Vertex vertex, char *vector);
void parseBoundingBox(char *boxname, uint64_t id, uint64_t variant);

#endif
