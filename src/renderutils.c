#include "stb_image.h"

#include "renderutils.h"

float (**boundingboxes)[6];
GLuint textureAtlas;
uint64_t **spriteids; /* Note that getting a sprite from an item which doesn't have one will yield the first. */

size_t ov_bufsiz, tv_bufsiz, oi_bufsiz, ti_bufsiz; /* Remeshing buffer sizes */

/* Shader stuff */
GLuint createShader(GLenum shaderType, char *shaderSource) {
	/* Creates a shader from source shaderSource of type shaderType and compiles
	 * it, logging errors to stderr and returning the shader ID */

	int success;
	char infoLog[RSM_MAX_SHADER_INFOLOG_LENGTH];
	char *src;
	GLuint shaderID;

	shaderID = glCreateShader(shaderType); /* Create gl shader of proper type */
	src = usf_ftos(shaderSource, NULL); /* Read shader source file as a single string */
	glShaderSource(shaderID, 1, (const char **) &src, NULL); /* Set source to file */
	glCompileShader(shaderID); /* Compile */
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success); /* Check compilation status */

	if (!success) {
		glGetShaderInfoLog(shaderID, RSM_MAX_SHADER_INFOLOG_LENGTH, NULL, infoLog); /* Get log */
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
	char infoLog[RSM_MAX_SHADER_INFOLOG_LENGTH];
	GLuint programID;

	programID = glCreateProgram(); /* Create program */
	glAttachShader(programID, vertexShader); /* Attach vertex shader */
	glAttachShader(programID, fragmentShader); /* Attach fragment shader */
	glLinkProgram(programID); /* Link */
	glGetProgramiv(programID, GL_LINK_STATUS, &success); /* Check linking success */

	if (!success) {
		glGetProgramInfoLog(programID, RSM_MAX_SHADER_INFOLOG_LENGTH, NULL, infoLog); /* Get log */
		fprintf(stderr, "Error linking shader program, see log:\n%s", infoLog);
	} else { /* Delete shaders as they are no longer needed if the program exists */
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
	}

	return programID;
}

void atlasAppend(char *meshname, int texSizeX, int texSizeY, unsigned char **atlasptr, GLsizei *atlassize) {
	/* Creates a texture atlas (in the texAtlasData temporary buffer)
	 * and dynamically adjust both the buffer and atlas dimension variables. */
	stbi_set_flip_vertically_on_load(1); /* Right images so Y=0 is on bottom */

	int width, height, ncolorchannels;
	unsigned char *imagedata;

	imagedata = stbi_load(meshname, &width, &height, &ncolorchannels, 4);

	if (imagedata == NULL) {
		fprintf(stderr, "Error loading texture at %s (does it exist?), aborting.\n", meshname);
		exit(RSM_EXIT_NOTEXTURE);
	}

	if (width != texSizeX || height != texSizeY) {
		fprintf(stderr, "Texture at %s does not match required size %u by %u, aborting.\n",
				meshname, texSizeX, texSizeY);
		exit(RSM_EXIT_BADTEXTURE);
	}

	/* Grow texture atlas by the size of the image (+ padding) and append it. We can avoid storing which
	 * image maps to where by assuming a linear strip (vertical) of texSize by texSize  */
	size_t texsz = texSizeX * texSizeY * 4;

	*atlasptr = realloc(*atlasptr, *atlassize + texsz);
	memcpy(*atlasptr + *atlassize, imagedata, texsz);
	*atlassize += texsz;

	stbi_image_free(imagedata);
}

void parseBlockdata(void) {
	/* Create a texture atlas (and set it to textureAtlas, which is used by the renderer) which
	 * corresponds neatly with adjusted coordinates of meshes, which should then be set as templates
	 * in blockmeshes (indirection is id, then variant, which gives a pointer to the template itself).
	 * Also handle collision bounding boxes */

	/* Allocs one byte extra since there are two \0 chars, but that's fine.
	 * Concatenate base path (textures/) with subpaths */
	char texMeshPath[sizeof(textureBasePath) + sizeof(textureBlockPath)];
	char blockmapPath[sizeof(textureBasePath) + sizeof(textureBlockmapPath)];

	strcpy(texMeshPath, textureBasePath); strcat(texMeshPath, textureBlockPath);
	strcpy(blockmapPath, textureBasePath); strcat(blockmapPath, textureBlockmapPath);

	fprintf(stderr, "Loading blockmeshes from directory %s\n", texMeshPath);
	fprintf(stderr, "Loading blockmap from file %s\n", blockmapPath);

	char **blockmap;
	uint64_t nblocks;

	blockmap = usf_ftot(blockmapPath, &nblocks);

	if (blockmap == NULL) {
		fprintf(stderr, "Error reading blockmap at %s (Does it exist?), aborting.\n", blockmapPath);
		exit(RSM_EXIT_NOBLOCKMAP);
	}

	/* Alloc ID indirection layer (equivalent to number of defined meshes (lines) in blockmap */
	blockmeshes = malloc(nblocks * sizeof(Blockmesh *));
	boundingboxes = malloc(nblocks * sizeof(float (*)[6]));
	spriteids = malloc(nblocks * sizeof(uint64_t *));

	/* Iterate through ids and variants and create appropriate blockmesh templates */
	uint64_t id, nvariants, nvariant, uid;
	uint64_t texid, spriteid, ntextiles;
	Blockmesh template;
	char **variants, *variant, *metadata;

	char meshdatapath[RSM_MAX_PATH_NAME_LENGTH], **meshdata, *vectordata;
	char meshtexturepath[RSM_MAX_PATH_NAME_LENGTH];
	uint64_t meshdatalen, d;
	Vertex vertexdata; /* Scratchpad for loading raw vertex data from file */

	/* Precompute number of textures to get right UV coordinate mappings */
	char *override;
	uint64_t ntextures, overrides;
	for (ntextures = id = 1; id < nblocks; id++) { /* Skip id 0 (only one texture for wiremesh */
		/* For each $ override, add its texture tile count, then add 1 for each default handle */
		for (overrides = 0, override = strchr(blockmap[id], '$'); override; override = strchr(override, '$')) {
			ntextures += strtoul(++override, NULL, 10); /* Increment override here for next iteration */
			overrides++;
		}

		ntextures += usf_scount(blockmap[id], ' ') + 1 - overrides; /* Take default declarations into account */
	}

	unsigned char *texAtlasData = NULL; /* Temporary buffer */
	GLsizei texAtlasSize = 0; /* Buffer size in bytes */

	/* Now load mesh data */
	for (spriteid = texid = id = 0; id < nblocks; id++) {
		/* Read specifications from file */
		blockmap[id][strlen(blockmap[id]) - 1] = '\0'; /* Remove trailing \n */
		variants = usf_scsplit(blockmap[id], ' ', &nvariants);

		blockmeshes[id] = malloc(nvariants * sizeof(Blockmesh));
		spriteids[id] = calloc(nvariants, sizeof(uint64_t));

		/* calloc to avoid having uninitialized data in no-collision blocks */
		boundingboxes[id] = calloc(nvariants, sizeof(float [6]));

		for (nvariant = 0; nvariant < nvariants; nvariant++, texid += ntextiles) {
			uid = ASUID(id, nvariant); /* Unique id:variant identifier */

			memset(&template, 0, sizeof(Blockmesh)); /* Reset template for this UID */
			variant = variants[nvariant];

			/* Find all block declaration parameters (*, :, $) */
			if (variant[0] == '*') {
				spriteids[id][nvariant] = spriteid++; /* To get sprite from specific identifier (id+variant) */
				variant++;
			}

			if ((metadata = strchr(variant, ':'))) { /* Block has default metadata */
				*metadata++ = '\0'; /* Cut name here */
				usf_inthmput(datamap, uid, USFDATAU(strtoul(metadata, NULL, 10)));
			}

			if ((metadata = strchr(variant, '$'))) { /* Texture is bigger than one image */
				*metadata++ = '\0';
				if ((ntextiles = strtoul(metadata, NULL, 10)) > RSM_MAX_BLOCKMESH_TEXTURETILES) {
					fprintf(stderr, "Block %lu variant %lu exceeds maximum texture tile count at %lu > %u, "
							"aborting.\n", id, nvariant, ntextiles, RSM_MAX_BLOCKMESH_TEXTURETILES);
					exit(RSM_EXIT_EXCBUF);
				}
			} else ntextiles = 1; /* One texture tile used */

			/* Log only the handle into namemap for future reference */
			usf_strhmput(namemap, variant, USFDATAU(uid));

			/* Allow for SPRITE_PLACEHOLDERs in blockmap not taking space in the atlas */
			if (id == 0 && nvariant) {
				ntextiles = 0; /* Not allocating a texture tile for this placeholder */
				continue;
			}

			/* If it exists, will parse this block's bounding box to boundingboxes */
			parseBoundingBox(variant, id, nvariant);

			/* Append texture for this mesh to the atlas */
			pathcat(meshtexturepath, 3, texMeshPath, variant, textureFormatExtension);
			atlasAppend(meshtexturepath, RSM_BLOCK_TEXTURE_SIZE_PIXELS,
					RSM_BLOCK_TEXTURE_SIZE_PIXELS * ntextiles, &texAtlasData, &texAtlasSize);

			/* Now build the template using raw mesh data from the text file */
			pathcat(meshdatapath, 3, texMeshPath, variant, meshFormatExtension);
			meshdata = usf_ftot(meshdatapath, &meshdatalen);
			if (meshdata == NULL) {
				fprintf(stderr, "Error reading raw mesh data at %s (Does it exist?), aborting.\n", meshdatapath);
				exit(RSM_EXIT_NOMESHDATA);
			}

			/* Loop through mesh data specification lines and adjust tex coords before appending to template */
			uint64_t nindices, n;
			char **indices;
			for (d = 0; d < meshdatalen; d++) {
				vectordata = meshdata[d];

				switch (vectordata[0]) {
#define ATLASADJUST(y) ((y * RSM_BLOCK_TEXTURE_SIZE_PIXELS + RSM_BLOCK_TEXTURE_SIZE_PIXELS * texid) \
		/ (ntextures * RSM_BLOCK_TEXTURE_SIZE_PIXELS))

#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#define VERTEXADJUST(COUNTSECTION, VERTEXSECTION) \
		loadVertexData(vertexdata, vectordata + 1); \
		vertexdata[7] = ATLASADJUST(CLAMP(vertexdata[7], RSM_TEXTURE_PADDING, 1 - RSM_TEXTURE_PADDING)); \
		template.VERTEXSECTION = realloc(template.VERTEXSECTION, \
				(template.count[COUNTSECTION] + (sizeof(Vertex)/sizeof(float))) * sizeof(float)); \
		memcpy(template.VERTEXSECTION + template.count[COUNTSECTION], vertexdata, sizeof(Vertex)); \
		template.count[COUNTSECTION] += sizeof(Vertex)/sizeof(float);
					case 'o':
						VERTEXADJUST(0, opaqueVertices);
						break;
					case 't':
						VERTEXADJUST(1, transVertices);
						break;
#define INDEXADJUST(COUNTSECTION, INDEXSECTION) \
		indices = usf_scsplit(vectordata + 1, ' ', &nindices); \
		template.INDEXSECTION = realloc(template.INDEXSECTION, \
				(template.count[COUNTSECTION] + nindices) * sizeof(unsigned int)); \
		for (n = 0; n < nindices; n++) \
			template.INDEXSECTION[template.count[COUNTSECTION] + n] = strtof(indices[n], NULL); \
		template.count[COUNTSECTION] += nindices; \
		free(indices);
					case 'i':
						INDEXADJUST(2, opaqueIndices);
						break;
					case 'e':
						INDEXADJUST(3, transIndices);
						break;
					case '#': /* To allow for comments. Other chars would work but would trigger error message */
					case '\n':
						continue;
					default:
						fprintf(stderr, "Unknown data format %c at line %lu in mesh data template file %s, skipping.\n", vectordata[0], d, meshdatapath);
						continue;
				}

			}

			/* Guard against buffer overflow when copying to scratchpad on chunk remeshing */
			if (template.count[0] > RSM_MAX_BLOCKMESH_VERTICES
					|| template.count[1] > RSM_MAX_BLOCKMESH_VERTICES) {
				fprintf(stderr, "Blockmesh for ID %lu variant %lu exceeds maximum blockmesh vertex count (%u or %u) > %u, aborting.\n", id, nvariant, template.count[0], template.count[1], RSM_MAX_BLOCKMESH_VERTICES);
				exit(RSM_EXIT_EXCBUF);
			}

			if (template.count[2] > RSM_MAX_BLOCKMESH_INDICES
					|| template.count[3] > RSM_MAX_BLOCKMESH_INDICES) {
				fprintf(stderr, "Blockmesh for ID %lu variant %lu exceeds maximum blockmesh index count (%u or %u) > %u, aborting.\n", id, nvariant, template.count[2], template.count[3], RSM_MAX_BLOCKMESH_INDICES);
				exit(RSM_EXIT_EXCBUF);
			}

			/* Adjust maximum sizes for buffer allocation */
			ov_bufsiz = MAX(ov_bufsiz, template.count[0]); tv_bufsiz = MAX(tv_bufsiz, template.count[1]);
			oi_bufsiz = MAX(oi_bufsiz, template.count[2]); ti_bufsiz = MAX(ti_bufsiz, template.count[3]);

			usf_freetxt(meshdata, meshdatalen);

			/* Set to blockmeshes */
			blockmeshes[id][nvariant] = template;
		}

		free(variants); /* Alloc'd by usf_scsplit so no new allocations for substrings */
	}

	/* Size in bytes of each remeshing scratchpad buffer (to alloc buffers in rawmesh) */
	ov_bufsiz *= sizeof(float) * CHUNKVOLUME; tv_bufsiz *= sizeof(float) * CHUNKVOLUME;
	oi_bufsiz *= sizeof(unsigned int) * CHUNKVOLUME; ti_bufsiz *= sizeof(unsigned int) * CHUNKVOLUME;

	fprintf(stderr, "Scratchpad buffer sizes for remeshing (vertex opaque/trans, index opaque/trans): "
			"%.2fMB, %.2fMB, %.2fMB, %.2fMB\n",
			ov_bufsiz/1e6, tv_bufsiz/1e6, oi_bufsiz/1e6, ti_bufsiz/1e6);

	/* Generate and bind atlas to OpenGL renderer */
	glGenTextures(1, &textureAtlas);
	glBindTexture(GL_TEXTURE_2D, textureAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_BLOCK_TEXTURE_SIZE_PIXELS, texAtlasSize	/ (4 * RSM_BLOCK_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, texAtlasData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	/* Cleanup */
	free(texAtlasData);
	usf_freetxt(blockmap, nblocks);
}

void loadVertexData(Vertex vertex, char *vector) {
	/* Parse textual vector data into numerical vertex data (first char is skipped by caller) */

	if (sscanf(vector, "%f %f %f %f %f %f %f %f", &vertex[0], &vertex[1], &vertex[2], &vertex[3], &vertex[4],
			&vertex[5], &vertex[6], &vertex[7]) == EOF) {
		fprintf(stderr, "Error parsing vertex data for line %s, aborting.\n", vector);
		exit(RSM_EXIT_BADVERTEXDATA);
	}
}

void parseBoundingBox(char *boxname, uint64_t id, uint64_t variant) {
	/* If it exists, parse the bounding box for this block and load it to
	 * boundingboxes */

	char boundingboxpath[RSM_MAX_PATH_NAME_LENGTH];

	pathcat(boundingboxpath, 4, textureBasePath, textureBlockPath, boxname, boundingboxFormatExtension);

	char *boundingbox;
	boundingbox = usf_ftos(boundingboxpath, NULL);

	if (boundingbox == NULL) return; /* Block must be passthrough, no bounding box */

	float *bb = boundingboxes[id][variant];

	if (sscanf(boundingbox, "%f %f %f %f %f %f\n",
				&bb[0], &bb[1], &bb[2], &bb[3], &bb[4], &bb[5]) == EOF) {
		fprintf(stderr, "Error parsing bounding box data for line %s, aborting.\n", boundingbox);
		exit(RSM_EXIT_BADBOUNDINGBOXDATA);
	}

	free(boundingbox); /* Cleanup disk file */
}
