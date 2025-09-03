#ifndef RENDERUTILS_H
#define RENDERUTILS_H

#include <glad/glad.h>
#include "usfstring.h"
#include "usfio.h"

#define INFOLOG_LENGTH 512

GLuint createShader(GLenum shaderType, char *shaderSource);
GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader);
GLuint createTexture(char *name);

#endif
