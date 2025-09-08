#define STB_IMAGE_IMPLEMENTATION /* Init stb_image library once */
#include "stb_image.h"

#include "renderutils.h"

GLuint createShader(GLenum shaderType, char *shaderSource) {
	/* Creates a shader from source shaderSource of type shaderType and compiles
	 * it, logging errors to stderr and returning the shader ID */

	int success;
	char infoLog[INFOLOG_LENGTH];
	char *src;
	GLuint shaderID;

	shaderID = glCreateShader(shaderType); /* Create gl shader of proper type */
	src = usf_ftos(shaderSource, "r", NULL); /* Read shader source file as a single string */
	glShaderSource(shaderID, 1, (const char **) &src, NULL); /* Set source to file */
	glCompileShader(shaderID); /* Compile */
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success); /* Check compilation status */

	if (!success) {
		glGetShaderInfoLog(shaderID, INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error compiling shader at %s, see log:\n%s", shaderSource, infoLog);
	}

	free(src); /* Free source file memory */

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

GLuint createTexture(char *name) {
	/* Load an image and create an OpenGL texture object with it
	 * including mipmaps */

	stbi_set_flip_vertically_on_load(1); /* Right images */

	/* Load image */
	int width, height, nColorChannels;
	unsigned char *image = stbi_load(name, &width, &height, &nColorChannels, 0);

	if (!image) {
		fprintf(stderr, "Error loading texture at %s\n", name);
	}

	/* Generate texture */
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	if (usf_sendswith(name, ".jpg") || usf_sendswith(name, ".jpeg"))
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	else if (usf_sendswith(name, ".png"))
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else
		fprintf(stderr, "Unsupported texture format (must be jpg or png) at %s\n", name);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);

	/* Free image data */
	stbi_image_free(image);

	return texture;
}
