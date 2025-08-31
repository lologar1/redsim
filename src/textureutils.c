#define STB_IMAGE_IMPLEMENTATION /* Initialize stb_image library once */
#include "stb_image.h"

#include "textureutils.h"
#include "usfstring.h"

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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	else
		fprintf(stderr, "Unsupported texture format (must be jpg or png) at %s\n", name);

	glGenerateMipmap(GL_TEXTURE_2D);

	/* Free image data */
	stbi_image_free(image);

	return texture;
}
