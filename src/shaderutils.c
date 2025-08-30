#include "shaderutils.h"

GLuint createShader(GLenum shaderType, char *shaderSource) {
	/* Creates a shader from source shaderSource of type shaderType and compiles
	 * it, logging errors to stderr and returning the shader ID */

	int success;
	char infoLog[INFOLOG_LENGTH];
	const char *src;
	GLuint shaderID;

	shaderID = glCreateShader(shaderType); /* Create gl shader of proper type */
	src = (const char *) usf_ftos(shaderSource, "r", NULL); /* Read shader source file as a single string */
	glShaderSource(shaderID, 1, &src, NULL); /* Set source to file */
	glCompileShader(shaderID); /* Compile */
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success); /* Check compilation status */

	if (!success) {
		glGetShaderInfoLog(shaderID, INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error compiling shader at %s, see log:\n%s", shaderSource, infoLog);
	}

	return shaderID;
}

GLuint createShaderProgram(GLuint vertexShader, GLuint fragmentShader) {
	/* Creates and links a shader program using a vertex and a fragment shader,
	 * logging errors to stderr and returning the program object ID. The shader
	 * objects are then deleted as they are no longer needed ! */

	int success;
	char infoLog[INFOLOG_LENGTH];
	GLuint programID;

	programID = glCreateProgram(); /* Create program */
	glAttachShader(programID, vertexShader); /* Attach vertex shader */
	glAttachShader(programID, fragmentShader); /* Attach fragment shader */
	glLinkProgram(programID); /* Link */
	glGetProgramiv(programID, GL_LINK_STATUS, &success); /* Check linking success */

	if (!success) {
		glGetProgramInfoLog(programID, INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error linking shader program, see log:\n%s", infoLog);
	} else { /* Delete shaders as they are no longer needed if the program exists */
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	}

	return programID;
}
