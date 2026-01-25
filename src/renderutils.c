#include "stb_image.h" /* Special .c file which includes stb_image implementation */
#include "renderutils.h"

GLuint ru_createShader(GLenum shaderType, char *shaderSource) {
	/* Creates and compiles an OpenGL shader from source code with error logging.
	 * Returns the created shader OpenGL ID */

	char infoLog[RSM_MAX_SHADER_INFOLOG_LENGTH], *src;
	GLuint shaderID;
	shaderID = glCreateShader(shaderType); /* Create gl shader of proper type */
	src = usf_ftos(shaderSource, NULL); /* Read shader source file as a single string */
	glShaderSource(shaderID, 1, (const char **) &src, NULL); /* Set source to file */
	glCompileShader(shaderID); /* Compile */

	i32 success;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success); /* Check compilation status */
	if (!success) {
		glGetShaderInfoLog(shaderID, RSM_MAX_SHADER_INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error compiling shader at %s, see log:\n%s", shaderSource, infoLog);
		exit(RSM_EXIT_GLERROR);
	}
	free(src); /* Free source file memory */

	return shaderID;
}

GLuint ru_createShaderProgram(GLuint vertexShader, GLuint fragmentShader) {
	/* Creates and links a shader program using a vertex and a fragment shader,
	 * logging errors to stderr and returning the program object ID. The shader
	 * objects are then deleted as they are no longer needed ! */

	char infoLog[RSM_MAX_SHADER_INFOLOG_LENGTH];
	GLuint programID;
	programID = glCreateProgram(); /* Create program */
	glAttachShader(programID, vertexShader); /* Attach vertex shader */
	glAttachShader(programID, fragmentShader); /* Attach fragment shader */
	glLinkProgram(programID); /* Link */

	i32 success;
	glGetProgramiv(programID, GL_LINK_STATUS, &success); /* Check linking success */
	if (!success) {
		glGetProgramInfoLog(programID, RSM_MAX_SHADER_INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error linking shader program, see log:\n%s", infoLog);
		exit(RSM_EXIT_GLERROR);
	} else { /* Delete shaders as they are no longer needed if the program exists */
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	}

	return programID;
}

void ru_atlasAppend(char *meshname, u64 sizex, u64 sizey, u8 **atlasptr, GLsizei *atlassz) {
	/* Loads and appends an RGBA image into the given texture atlas.
	 * The atlas is assumed to be of uniform width (sizex).
	 * If either of the size parameters are 0, this function has no effect. */

	if (sizex == 0 || sizey == 0) return; /* Loading zero-sized image */

	stbi_set_flip_vertically_on_load(1); /* Right images so Y=0 is on bottom */

	i32 width, height, ncolorchannels;
	u8 *imagedata;
	imagedata = stbi_load(meshname, &width, &height, &ncolorchannels, 4);

	if (imagedata == NULL) {
		fprintf(stderr, "Error loading texture at %s (does it exist?), aborting.\n", meshname);
		exit(RSM_EXIT_NOTEXTURE);
	}

	if ((u64) width != sizex || (u64) height != sizey) {
		fprintf(stderr, "Texture at %s does not match required size %"PRIu64" by %"PRIu64", aborting.\n",
				meshname, sizex, sizey);
		exit(RSM_EXIT_BADTEXTURE);
	}

	u64 imagesz = sizex * sizey * 4;
	*atlasptr = realloc(*atlasptr, *atlassz + imagesz);
	memcpy(*atlasptr + *atlassz, imagedata, imagesz);
	*atlassz += imagesz;

	stbi_image_free(imagedata);
}
