#ifndef SHADERUTILS_H
#define SHADERUTILS_H

#include <glad/glad.h>
#include "usfio.h"

#define INFOLOG_LENGTH 512

GLuint createShader(GLenum shaderType, char *shaderSource);
GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader);

#endif
