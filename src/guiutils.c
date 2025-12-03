#include "guiutils.h"

GLuint guiAtlas;
GLuint guiVBO[MAX_GUI_PRIORITY], guiEBO[MAX_GUI_PRIORITY], guiVAO[MAX_GUI_PRIORITY];
uint32_t nGUIIndices[MAX_GUI_PRIORITY];
float lineheight;
int32_t GUI_SCHEDULEDUPDATE = 0;

FT_Library ft;
FT_Face typeface;
Textchar textchars[128]; /* Hold rendering info for each ASCII 0-127 */

void gu_initGUI(void) {
	/* Create the appropriate OpenGL structures for the GUI */
	uint32_t i;
	glGenVertexArrays(MAX_GUI_PRIORITY, guiVAO);
	glGenBuffers(MAX_GUI_PRIORITY, guiVBO);
	glGenBuffers(MAX_GUI_PRIORITY, guiEBO);

	/* Attributes */
	for (i = 0; i < MAX_GUI_PRIORITY; i++) {
		glBindVertexArray(guiVAO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, guiVBO[i]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, guiEBO[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
	}

	glBindVertexArray(0);
}

void gu_parseGUIdata(void) {
	/* Create GUI texture atlas and parse submenu layouts & font from disk */
#define guiPath RESOURCE_BASE_PATH TEXTURE_GUI_PATH
#define guimapPath RESOURCE_BASE_PATH GUIMAP_PATH
	char *guimap, **elements, *element;
	uint64_t nelement, nelements;

	guimap = usf_ftos(guimapPath, NULL);
	elements = usf_scsplit(guimap, '\n', &nelements);
	nelements--; /* One more because terminating \n is included as separator */

	char guitexturepath[RSM_MAX_PATH_NAME_LENGTH];
	unsigned char *guiAtlasData = NULL;
	GLsizei guiAtlasSize = 0;

	char **submenulayout;
	uint64_t nsprites, i, itemuid, xslotoffset, yslotoffset;

	for (nelement = 0; nelement < nelements; nelement++) {
		element = elements[nelement];

		/* Append texture for this element to the atlas */
		pathcat(guitexturepath, 3, guiPath, element, TEXTURE_EXTENSION);

		ru_atlasAppend(guitexturepath, RSM_GUI_TEXTURE_SIZE_PIXELS, RSM_GUI_TEXTURE_SIZE_PIXELS,
				&guiAtlasData, &guiAtlasSize);

		/* Submenu handling */
		if (!(nelement >= GUI_PICON && nelement < GUI_PICON + RSM_INVENTORY_SUBMENUS)) continue;
		/* Reuse same buffer for this name. A bit unpretty, but what do you want */
		pathcat(guitexturepath, 3, guiPath, element, LAYOUT_EXTENSION);

		if ((submenulayout = usf_ftot(guitexturepath, &nsprites)) == NULL) {
			fprintf(stderr, "Cannot find inventory submenu layout at %s, aborting.\n", guitexturepath);
			exit(RSM_EXIT_NOLAYOUT);
		}

		for (i = 0; i < nsprites; i++) {
			submenulayout[i][strlen(submenulayout[i]) - 1] = '\0'; /* Remove trailing newline */
			itemuid = usf_strhmget(namemap, submenulayout[i]).u;

			xslotoffset = i % RSM_INVENTORY_SLOTS_HORIZONTAL;
			yslotoffset = i / RSM_INVENTORY_SLOTS_HORIZONTAL;

			submenus[nelement - GUI_PICON][xslotoffset][RSM_INVENTORY_SLOTS_VERTICAL - yslotoffset-1] = itemuid;
		}

		usf_freetxt(submenulayout, nsprites);
	}

	free(elements);
	free(guimap); /* Will free the char **elements split substrings */

	/* Generate font and add packed textures here */
	gu_loadFont(&guiAtlasData, &guiAtlasSize);

	glGenTextures(1, &guiAtlas);
	glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_GUI_TEXTURE_SIZE_PIXELS, guiAtlasSize / (4 * RSM_GUI_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, guiAtlasData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(guiAtlasData);
}

void gu_loadFont(unsigned char **guiatlas, GLsizei *atlassize) {
	/* Add character font to guiAtlas and create char structs for text rendering */
	FT_GlyphSlot glyph;

	if (FT_Init_FreeType(&ft)) {
		fprintf(stderr, "Error initializing FreeType library, aborting.\n");
		exit(RSM_EXIT_TEXTFAIL);
	}

#define typefacePath TYPEFACE_PATH FONT_PATH
	if (FT_New_Face(ft, typefacePath, 0, &typeface)) {
		fprintf(stderr, "Error retrieving typeface at %s, aborting.\n", typefacePath);
		exit(RSM_EXIT_TEXTFAIL);
	}

	FT_Set_Pixel_Sizes(typeface, RSM_CHARACTER_TEXTURE_SIZE_PIXELS, RSM_CHARACTER_TEXTURE_SIZE_PIXELS);
	lineheight = typeface->size->metrics.height / 64;

	/* Load first 128 ASCII characters (8 64x64 textures) into the guiAtlas texture */
#define FONT_TEXSZ (RSM_CHARACTER_TEXTURE_SIZE_PIXELS * RSM_CHARACTER_TEXTURE_SIZE_PIXELS)
#define CHARS_PER_TEXTURE ((RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS) / (RSM_CHARACTER_TEXTURE_SIZE_PIXELS * RSM_CHARACTER_TEXTURE_SIZE_PIXELS))
#define CHARS_PER_LINE (RSM_GUI_TEXTURE_SIZE_PIXELS / RSM_CHARACTER_TEXTURE_SIZE_PIXELS)
	unsigned char chartexture[CHARS_PER_TEXTURE][FONT_TEXSZ]; /* Grayscale */
#define CHAR_TEXTURES (128 / CHARS_PER_TEXTURE)
	unsigned char charatlas[sizeof(chartexture) * 4 * CHAR_TEXTURES]; /* Same format as guiAtlas */

	unsigned char c;
	uint32_t nsubtexture, nchartexture;
	Textchar *tc;

	for (c = 0; c < 128; c++) {
		nsubtexture = c % CHARS_PER_TEXTURE;
		nchartexture = c / CHARS_PER_TEXTURE;

		if (FT_Load_Char(typeface, c, FT_LOAD_RENDER)) {
			fprintf(stderr, "Error rendering character %c (%d), aborting.\n", c, (int32_t) c);
			exit(RSM_EXIT_TEXTFAIL);
		}

		glyph = typeface->glyph;

#define XOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS - USF_MIN(RSM_CHARACTER_TEXTURE_SIZE_PIXELS, glyph->bitmap.width)) / 2)
#define YOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS - USF_MIN(RSM_CHARACTER_TEXTURE_SIZE_PIXELS, glyph->bitmap.rows)) / 2)
		memset(chartexture[nsubtexture], 0, FONT_TEXSZ); /* Padding */
		for (uint32_t row = 0; row < glyph->bitmap.rows; row++) {
			for (uint32_t col = 0; col < glyph->bitmap.width; col++) {
				chartexture[nsubtexture][(row + YOFFSET) * RSM_CHARACTER_TEXTURE_SIZE_PIXELS + col + XOFFSET] =
					glyph->bitmap.buffer[row * glyph->bitmap.pitch + col];
			}
		}
#define CHAR_STRIDE ((float) RSM_CHARACTER_TEXTURE_SIZE_PIXELS / RSM_GUI_TEXTURE_SIZE_PIXELS)
		float sox, soy;
		sox = (nsubtexture % CHARS_PER_LINE) * CHAR_STRIDE;
		soy = (nsubtexture / CHARS_PER_LINE) * CHAR_STRIDE + CHAR_STRIDE;

		tc = textchars + c;

#define UOFFSET ((float) XOFFSET / RSM_GUI_TEXTURE_SIZE_PIXELS)
#define VOFFSET ((float) YOFFSET / RSM_GUI_TEXTURE_SIZE_PIXELS)
		tc->uv[0] = sox + UOFFSET;
		tc->uv[1] = gu_guiAtlasAdjust(soy - VOFFSET, MAX_GUI_PRIORITY + nchartexture);
		tc->uv[2] = sox + CHAR_STRIDE - UOFFSET;
		tc->uv[3] = gu_guiAtlasAdjust(soy + VOFFSET - CHAR_STRIDE, MAX_GUI_PRIORITY + nchartexture);

		tc->size[0] = glyph->bitmap.width; tc->size[1] = glyph->bitmap.rows;
		tc->bearing[0] = glyph->bitmap_left; tc->bearing[1] = glyph->bitmap_top;
		tc->advance = glyph->advance.x / 64;

		if (nsubtexture == CHARS_PER_TEXTURE - 1) {
			/* Dump to atlas */
			uint32_t subtexture, pixelindex;
			for (subtexture = 0; subtexture < CHARS_PER_TEXTURE; subtexture++) {
				for (pixelindex = 0; pixelindex < FONT_TEXSZ; pixelindex++) {
					/* Macro hell... each padded glyph is packed into atlases of GUI textures, which are
					 * themselves packed into a bigger gui atlas. Hard to understand. */
#define ATLASOFFSET (nchartexture * RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS * 4)
#define CHAROFFSET (((subtexture / CHARS_PER_LINE) * CHARS_PER_LINE * FONT_TEXSZ + \
			(subtexture % CHARS_PER_LINE) * RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * 4)
#define PIXELOFFSET (((pixelindex / RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * RSM_GUI_TEXTURE_SIZE_PIXELS + \
		pixelindex % RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * 4)
					/* Copy color to RGB channels (what guiAtlas wants) */
					memset(&charatlas[ATLASOFFSET + CHAROFFSET + PIXELOFFSET],
							chartexture[subtexture][pixelindex], 3);
					charatlas[ATLASOFFSET + CHAROFFSET + PIXELOFFSET + 3] = chartexture[subtexture][pixelindex]
						? 255 : 0; /* Transparent on background */
				}
			}
		}
	}

	/* Copy to actual guiAtlas */
	*guiatlas = realloc(*guiatlas, *atlassize + sizeof(charatlas));
	memcpy(*guiatlas + *atlassize, charatlas, sizeof(charatlas));
	*atlassize += sizeof(charatlas);

	FT_Done_Face(typeface);
	FT_Done_FreeType(ft);
}

float gu_guiAtlasAdjust(float y, GUIPriority priority) {
	/* Adjusts y UV from local (GUI texture) to global (GUI atlas) */
	return (y * RSM_GUI_TEXTURE_SIZE_PIXELS + RSM_GUI_TEXTURE_SIZE_PIXELS * priority)
		/ ((MAX_GUI_PRIORITY + CHAR_TEXTURES) * RSM_GUI_TEXTURE_SIZE_PIXELS);
}

void gu_meshSet(uint32_t priority, float *v, uint32_t sizev, uint32_t *i, uint32_t sizei) {
	glBindVertexArray(guiVAO[priority]);
	glBindBuffer(GL_ARRAY_BUFFER, guiVBO[priority]);
	glBufferData(GL_ARRAY_BUFFER, sizev * sizeof(float), v, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizei * sizeof(uint32_t), i, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	nGUIIndices[priority] = sizei;
}

uint32_t drawText(char *str, float *vbuf, uint32_t *ibuf, uint32_t ioffset, float x, float y, float scale, GUIPriority priority) {
	/* Add the adjusted quads to a vbuf and ibuf for text rendering of string str. Note that both of these
	 * buffers should be big enough to handle the string (4 vertices + 6 indices per char.
	 * Indices start at offset ioffset.
	 * Important: vbuf and ibuf must point to the place where the quads will be rendered! */
	unsigned char *c, nchars;
	float letter, line, xpos, ypos;
	float width, height;
	Textchar *tc;

	for (letter = x, nchars = line = 0, c = (unsigned char *) str; *c; c++) {
		if (*c > 127) continue;
		tc = textchars + *c;

		if (*c == '\n') {
			line += lineheight * scale;
			letter = x;
			continue;
		}

		if (isspace(*c)) {
			letter += tc->advance * scale;
			continue;
		}

		nchars++; /* Rendered this char */

		xpos = letter + tc->bearing[0] * scale; ypos = (y - line) - (tc->size[1] - tc->bearing[1]) * scale;
		width = tc->size[0] * scale; height = tc->size[1] * scale;

		float v[5 * 4] = {
			xpos, ypos, priority, tc->uv[0], tc->uv[1],
			xpos, ypos + height, priority, tc->uv[0], tc->uv[3],
			xpos + width, ypos + height, priority, tc->uv[2], tc->uv[3],
			xpos + width, ypos, priority, tc->uv[2], tc->uv[1]
		};

		uint32_t i[6] = {0 + ioffset, 1 + ioffset, 2 + ioffset, 0 + ioffset, 2 + ioffset, 3 + ioffset};

		memcpy(vbuf, v, sizeof(v));
		memcpy(ibuf, i, sizeof(i));

		letter += tc->advance * scale;
		ioffset += 4;
		vbuf += sizeof(v)/sizeof(float); ibuf += sizeof(i)/sizeof(uint32_t);
	}

	return nchars;
}
