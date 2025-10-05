#define STB_IMAGE_IMPLEMENTATION /* Init stb_image library once */
#include "stb_image.h"

#include "renderutils.h"

GLuint textureAtlas;
Blockmesh ***blockmeshes;

/* Shader stuff */

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

unsigned char *atlasdata, *currentimage; /* Temporary buffer */
GLsizei atlassz;

void atlasAppend(char *meshname) {
	/* Creates a texture atlas (in the atlasdata temporary buffer)
	 * and dynamically adjust both the buffer and atlas dimension variables. */

	stbi_set_flip_vertically_on_load(1); /* Right images */

	char *texPath;
	texPath = malloc(sizeof(textureBasePath) + sizeof(textureBlockPath) +
			strlen(meshname) + 1 + sizeof(textureFormatExtension));
	strcpy(texPath, textureBasePath);
	strcat(texPath, textureBlockPath);
	strcat(texPath, meshname);
	strcat(texPath, textureFormatExtension);

	int width, height, ncolorchannels;
	unsigned char *imagedata;

	imagedata = stbi_load(texPath, &width, &height, &ncolorchannels, 4);

	if (imagedata == NULL) {
		fprintf(stderr, "Error loading texture at %s (does it exist?), aborting.\n", texPath);
		exit(RSM_EXIT_NOTEXTURE);
	}

	if (width != (int) RSM_TEXTURE_SIZE_PIXELS || height != (int) RSM_TEXTURE_SIZE_PIXELS) {
		fprintf(stderr, "Texture at %s does not match required size %u, aborting.\n", texPath,
				RSM_TEXTURE_SIZE_PIXELS);
		exit(RSM_EXIT_BADTEXTURE);
	}

	free(texPath);

	/* Grow texture atlas by the size of the image and append it.
	 * We can avoid storing which image maps to where by assuming a linear
	 * strip of RSM_TEXTURE_SIZE_PIXELS by RSM_TEXTURE_SIZE_PIXELS  */
	unsigned int texsz = RSM_TEXTURE_SIZE_PIXELS * RSM_TEXTURE_SIZE_PIXELS * 4;
	atlasdata = realloc(atlasdata, atlassz + texsz);
	currentimage = atlasdata + atlassz;
	atlassz += texsz;

	memcpy(currentimage, imagedata, texsz);
	stbi_image_free(imagedata);
}

void parseBlockmeshes(void) {
	/* Create a texture atlas (and set it to textureAtlas, which is used by the renderer) which
	 * corresponds neatly with adjusted coordinates of meshes, which should then be set as templates
	 * in blockmeshes (indirection is id, then variant, which gives a pointer to the template itself) */

	char *texMeshPath, *blockmapPath;

	/* Allocs one byte extra since there are two \0 chars
	 * Concatenate base path (textures/) with subpaths */
	texMeshPath = malloc(sizeof(textureBasePath) + sizeof(textureBlockPath));
	strcpy(texMeshPath, textureBasePath);
	strcat(texMeshPath, textureBlockPath);

	blockmapPath = malloc(sizeof(textureBasePath) + sizeof(textureBlockmapPath));
	strcpy(blockmapPath, textureBasePath);
	strcat(blockmapPath, textureBlockmapPath);

	fprintf(stderr, "Loading blockmeshes from directory %s\n", texMeshPath);
	fprintf(stderr, "Loading blockmap from file %s\n", blockmapPath);

	char **blockmap;
	uint64_t nblocks;

	blockmap = usf_ftot(blockmapPath, "r", &nblocks);

	if (blockmap == NULL) {
		fprintf(stderr, "Error reading blockmap %s (Does it exist?), aborting.\n", blockmapPath);
		exit(RSM_EXIT_NOBLOCKMAP);
	}

	/* Alloc ID indirection layer (equivalent to number of defined meshes (lines) in blockmap */
	blockmeshes = malloc(nblocks * sizeof(Blockmesh **));

	/* Iterate through ids and variants and create appropriate blockmesh templates */
	uint64_t id, nvariants, nvariant;
	uint64_t texid, ntextures;
	Blockmesh *template;
	char **variants, *variant;

	char meshdatapath[RSM_MAX_MESHDATA_NAME_LENGTH], **meshdata, *vectordata;
	uint64_t meshdatalen, d;
	Vertex vertexdata; /* Scratchpad for loading raw vertex data from file */

	/* Precompute number of textures to get right UV coordinate mappings */
	for (ntextures = id = 0; id < nblocks; id++)
		ntextures += usf_scount(blockmap[id], ' ') + 1;

	/* Now load mesh data */
	for (texid = id = 0; id < nblocks; id++) {
		blockmap[id][strlen(blockmap[id]) - 1] = '\0'; /* Remove trailing \n */
		variants = usf_scsplit(blockmap[id], ' ', &nvariants);
		blockmeshes[id] = malloc(nvariants * sizeof(Blockmesh *));

		for (nvariant = 0; nvariant < nvariants; nvariant++, texid++) {
			blockmeshes[id][nvariant] = template = calloc(1, sizeof(Blockmesh));
			variant = variants[nvariant];

			/* Append texture for this mesh to the atlas */
			atlasAppend(variant);

			if (sizeof(textureBasePath) + sizeof(textureBlockPath) + strlen(variant) +
					sizeof(meshFormatExtension) > RSM_MAX_MESHDATA_NAME_LENGTH) {
				fprintf(stderr, "Mesh format name too long at %s exceeding %u (with extensions), aborting.\n",
						variant, RSM_MAX_MESHDATA_NAME_LENGTH);
				exit(RSM_EXIT_EXCBUF);
			}

			strcpy(meshdatapath, texMeshPath);
			strcat(meshdatapath, variant);
			strcat(meshdatapath, meshFormatExtension);

			/* Now build the template using raw mesh data from the text file */
			meshdata = usf_ftot(meshdatapath, "r", &meshdatalen);
			if (meshdata == NULL) {
				fprintf(stderr, "Error reading raw mesh data at %s (Does it exist?), aborting.\n", meshdatapath);
				exit(RSM_EXIT_NOMESHDATA);
			}

			/* Loop through all mesh data and set appropriate fields */
			uint64_t nindices, n;
			char **indices;
			for (d = 0; d < meshdatalen; d++) {
				vectordata = meshdata[d];

				switch (vectordata[0]) {
					case 'o':
						loadVertexData(vertexdata, vectordata + 1);
						vertexdata[7] = (vertexdata[7] * RSM_TEXTURE_SIZE_PIXELS
							+ RSM_TEXTURE_SIZE_PIXELS * texid)
							/ (ntextures * RSM_TEXTURE_SIZE_PIXELS); /* Adjust to atlas UV coords */

						template->opaqueVertices = realloc(template->opaqueVertices,
								(template->count[0] + 8) * sizeof(float));
						memcpy(template->opaqueVertices + template->count[0],
								vertexdata, sizeof(Vertex));
						template->count[0] += 8;
						break;
					case 't':
						loadVertexData(vertexdata, vectordata + 1);
						vertexdata[7] = (vertexdata[7] * RSM_TEXTURE_SIZE_PIXELS
							+ RSM_TEXTURE_SIZE_PIXELS * texid)
							/ (ntextures * RSM_TEXTURE_SIZE_PIXELS);

						template->transVertices = realloc(template->transVertices,
								(template->count[1] + 8) * sizeof(float));
						memcpy(template->transVertices + template->count[1],
								vertexdata, sizeof(Vertex));
						template->count[1] += 8;
						break;
					case 'i':
						indices = usf_scsplit(vectordata + 1, ' ', &nindices);
						template->opaqueIndices = realloc(template->opaqueIndices,
								(template->count[2] + nindices) * sizeof(unsigned int));

						for (n = 0; n < nindices; n++)
							template->opaqueIndices[template->count[2] + n] = atof(indices[n]);

						template->count[2] += nindices;
						free(indices);
						break;
					case 'e':
						indices = usf_scsplit(vectordata + 1, ' ', &nindices);
						template->transIndices = realloc(template->transIndices,
								(template->count[3] + nindices) * sizeof(unsigned int));

						for (n = 0; n < nindices; n++)
							template->transIndices[template->count[3] + n] = atof(indices[n]);

						template->count[3] += nindices;
						free(indices);
						break;
					case '#': /* To allow for comments. Other chars would work but would trigger error message */
					case '\n':
						continue;
					default:
						fprintf(stderr, "Unknown data format %c at line %lu in mesh data template file %s, skipping.\n", vectordata[0], d, meshdatapath);
						continue;
				}

			}

			usf_freetxt(meshdata, meshdatalen);
		}

		free(variants); /* Alloc'd by usf_scsplit so no new allocations for substrings */
	}

	/* Generate and bind atlas to OpenGL renderer */
	glGenTextures(1, &textureAtlas);
	glBindTexture(GL_TEXTURE_2D, textureAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_TEXTURE_SIZE_PIXELS, atlassz/(4 * RSM_TEXTURE_SIZE_PIXELS), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, atlasdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);

	/* Cleanup */
	free(atlasdata);
	usf_freetxt(blockmap, nblocks);
	free(texMeshPath);
	free(blockmapPath);
}

void loadVertexData(Vertex vertex, char *vector) {
	/* Parse textual vector data into numerical vertex data (first char is skipped by caller) */

	if (sscanf(vector, "%f %f %f %f %f %f %f %f", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4],
			&vertex[5], &vertex[6], &vertex[7]) == EOF) {
		fprintf(stderr, "Error parsing vertex data for line %s, aborting.\n", vector);
		exit(RSM_EXIT_BADVERTEXDATA);
	}
}
