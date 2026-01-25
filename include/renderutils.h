#ifndef RENDERUTILS_H
#define RENDERUTILS_H

#include <glad/glad.h>
#include "chunkutils.h"
#include "usfstring.h"
#include "usfio.h"
#include "redsim.h"

GLuint ru_createShader(GLenum shaderType, char *shaderSource);
GLuint ru_createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
void ru_atlasAppend(char *meshname, u64 sizex, u64 sizey, u8 **atlasptr, GLsizei *atlassz);

#endif
