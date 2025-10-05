#ifndef RENDERUTILS_H
#define RENDERUTILS_H

#include <glad/glad.h>
#include "chunkutils.h"
#include "usfstring.h"
#include "usfio.h"

#define INFOLOG_LENGTH 512

typedef float Vertex[8];

GLuint createShader(GLenum shaderType, char *shaderSource);
GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
void atlasAppend(char *meshname);
void parseBlockmeshes(void); /* Create texture atlas and blockmeshes */
void loadVertexData(Vertex vertex, char *vector);

#endif
