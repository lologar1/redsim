#ifndef RENDERUTILS_H
#define RENDERUTILS_H

#include <glad/glad.h>
#include "chunkutils.h"
#include "usfstring.h"
#include "usfio.h"
#include "redsim.h"

#define CHUNKVOLUME (CHUNKSIZE * CHUNKSIZE * CHUNKSIZE)

extern float (**boundingboxes)[6];
extern GLuint textureAtlas;
extern uint64_t **spriteids;

extern uint64_t MAX_BLOCK_ID, *MAX_BLOCK_VARIANT;
extern size_t ov_bufsiz, tv_bufsiz, oi_bufsiz, ti_bufsiz;

typedef float Vertex[8];

GLuint ru_createShader(GLenum shaderType, char *shaderSource);
GLuint ru_createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
void ru_atlasAppend(char *meshname, int32_t texSizeX, int32_t texSizeY, unsigned char **atlasptr, GLsizei *atlassize);
void ru_parseBlockdata(void); /* Create texture atlas, blockmeshes and bounding boxes */
void ru_deallocateVAO(GLuint VAO); /* Frees a VAO and its associated VBO and EBO */

#endif
