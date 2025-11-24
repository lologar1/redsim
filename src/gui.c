#include "gui.h"

FT_Library ft;
FT_Face typeface;

GLuint guiAtlas;
Textchar textchars[128]; /* Hold rendering info for each ASCII 0-127 */
float lineheight;

GLuint guiVBO[MAX_GUI_PRIORITY], guiEBO[MAX_GUI_PRIORITY], guiVAO[MAX_GUI_PRIORITY];
unsigned int nGUIIndices[MAX_GUI_PRIORITY];

usf_hashmap *namemap; /* Name -> (id:variant) in the form of a uint64_t partitioned in 2 32-bit numbers */

unsigned int hotbarIndex; /* Current hotbar */
unsigned int hotslotIndex; /* Current slot */
unsigned int inventoryIndex; /* Current submenu */

uint64_t hotbar[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2]; /* Hotbar status id:variant */
uint64_t submenus[RSM_INVENTORY_ICONS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];

void initGUI(void) {
	/* Create the appropriate OpenGL structures for the GUI */
	glGenVertexArrays(MAX_GUI_PRIORITY, guiVAO);
	glGenBuffers(MAX_GUI_PRIORITY, guiVBO);
	glGenBuffers(MAX_GUI_PRIORITY, guiEBO);

	/* Attributes */
	for (int i = 0; i < MAX_GUI_PRIORITY; i++) {
		glBindVertexArray(guiVAO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, guiVBO[i]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, guiEBO[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
	}

	glBindVertexArray(0);

	initInventory(); /* Create base inventory mesh once */
}

void initFont(unsigned char **guiatlas, GLsizei *atlassize) {
	/* Add character font to guiAtlas and create char structs for text rendering */
	FT_GlyphSlot glyph;

	if (FT_Init_FreeType(&ft)) {
		fprintf(stderr, "Error initializing FreeType library, aborting.\n");
		exit(RSM_EXIT_TEXTFAIL);
	}

	char typefacePath[RSM_MAX_PATH_NAME_LENGTH];
	pathcat(typefacePath, 2, TYPEFACE_PATH, FONT_PATH);

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
	unsigned int nsubtexture, nchartexture;
	Textchar *tc;

	for (c = 0; c < 128; c++) {
		nsubtexture = c % CHARS_PER_TEXTURE;
		nchartexture = c / CHARS_PER_TEXTURE;

		if (FT_Load_Char(typeface, c, FT_LOAD_RENDER)) {
			fprintf(stderr, "Error rendering character %c (%d), aborting.\n", c, (int) c);
			exit(RSM_EXIT_TEXTFAIL);
		}

		glyph = typeface->glyph;

#define XOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS - USF_MIN(RSM_CHARACTER_TEXTURE_SIZE_PIXELS, glyph->bitmap.width)) / 2)
#define YOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS - USF_MIN(RSM_CHARACTER_TEXTURE_SIZE_PIXELS, glyph->bitmap.rows)) / 2)
		memset(chartexture[nsubtexture], 0, FONT_TEXSZ); /* Padding */
		for (unsigned int row = 0; row < glyph->bitmap.rows; row++) {
			for (unsigned int col = 0; col < glyph->bitmap.width; col++) {
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
		tc->uv[1] = guiAtlasAdjust(soy - VOFFSET, MAX_GUI_PRIORITY + nchartexture);
		tc->uv[2] = sox + CHAR_STRIDE - UOFFSET;
		tc->uv[3] = guiAtlasAdjust(soy + VOFFSET - CHAR_STRIDE, MAX_GUI_PRIORITY + nchartexture);

		tc->size[0] = glyph->bitmap.width; tc->size[1] = glyph->bitmap.rows;
		tc->bearing[0] = glyph->bitmap_left; tc->bearing[1] = glyph->bitmap_top;
		tc->advance = glyph->advance.x / 64;

		if (nsubtexture == CHARS_PER_TEXTURE - 1) {
			/* Dump to atlas */
			unsigned int subtexture, pixelindex;
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

char *iconlayouts[RSM_INVENTORY_ICONS]; /* For use by inventory init ; free'd then */
void parseGUIdata(void) {
	/* Create GUI texture atlas and parse GUI elements from disk */
	char guiPath[sizeof(RESOURCE_BASE_PATH) + sizeof(TEXTURE_GUI_PATH)];
	char guimapPath[sizeof(RESOURCE_BASE_PATH) + sizeof(GUIMAP_PATH)];

	pathcat(guiPath, 2, RESOURCE_BASE_PATH, TEXTURE_GUI_PATH);
	pathcat(guimapPath, 2, RESOURCE_BASE_PATH, GUIMAP_PATH);

	char *guimap, **elements, *element;
	uint64_t nelement, nelements;

	guimap = usf_ftos(guimapPath, NULL);
	elements = usf_scsplit(guimap, '\n', &nelements);
	nelements--; /* One more because terminating \n is included as separator */

	char guitexturepath[RSM_MAX_PATH_NAME_LENGTH];
	unsigned char *guiAtlasData = NULL;
	GLsizei guiAtlasSize = 0;

	for (nelement = 0; nelement < nelements; nelement++) {
		element = elements[nelement];

		/* Append texture for this element to the atlas */
		pathcat(guitexturepath, 3, guiPath, element, TEXTURE_EXTENSION);

		atlasAppend(guitexturepath, RSM_GUI_TEXTURE_SIZE_PIXELS, RSM_GUI_TEXTURE_SIZE_PIXELS,
				&guiAtlasData, &guiAtlasSize);

		/* Check for icons */
		if (!(nelement >= PICON && nelement < PICON + RSM_INVENTORY_ICONS)) continue;
		/* Reuse same buffer for this name. A bit unpretty, but what do you want */
		pathcat(guitexturepath, 3, guiPath, element, LAYOUT_EXTENSION);

		if ((iconlayouts[nelement - PICON] = usf_ftos(guitexturepath, NULL)) == NULL) {
			fprintf(stderr, "Cannot find inventory submenu layout at %s, aborting.\n", guitexturepath);
			exit(RSM_EXIT_NOLAYOUT);
		}
	}

	free(elements);
	free(guimap); /* Will free the char **elements split substrings */

	/* Generate font and add packed textures here */
	initFont(&guiAtlasData, &guiAtlasSize);

	glGenTextures(1, &guiAtlas);
	glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_GUI_TEXTURE_SIZE_PIXELS, guiAtlasSize / (4 * RSM_GUI_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, guiAtlasData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	free(guiAtlasData);
}

float guiAtlasAdjust(float y, GUIPriority priority) {
	return (y * RSM_GUI_TEXTURE_SIZE_PIXELS + RSM_GUI_TEXTURE_SIZE_PIXELS * priority)
		/ ((MAX_GUI_PRIORITY + CHAR_TEXTURES) * RSM_GUI_TEXTURE_SIZE_PIXELS);
}

void meshAppend(unsigned int priority, float *v, unsigned int sizev, unsigned int *i, unsigned int sizei) {
	glBindVertexArray(guiVAO[priority]);
	glBindBuffer(GL_ARRAY_BUFFER, guiVBO[priority]);
	glBufferData(GL_ARRAY_BUFFER, sizev * sizeof(float), v, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizei * sizeof(unsigned int), i, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	nGUIIndices[priority] = sizei;
}

/* Reset each GUI draw */
float *spritevbuf;
unsigned int *spriteibuf;
unsigned int sizesv, sizesi;
void renderGUI(void) {
	/* Redraw the GUI. Every GUI element is currently encoded in this .c file.
	 * As remeshing the GUI is an inefficient process (dynamic arrays) it is better to only call
	 * this function when necessary (e.g. user input which may modify GUI) rather than every frame. */
	spritevbuf = NULL;
	spriteibuf = NULL;
	sizesv = sizesi = 0;

	renderCrosshair();
	renderHotbar();
	renderInventory(); /* Will handle itself */
	renderCommand();

	meshAppend(pItemIcons0, spritevbuf, sizesv, spriteibuf, sizesi);

	free(spritevbuf);
	free(spriteibuf);
}

unsigned int drawText(char *str, float *vbuf, unsigned int *ibuf, unsigned int ioffset, float x, float y, float scale, GUIPriority priority) {
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

		unsigned int i[6] = {0 + ioffset, 1 + ioffset, 2 + ioffset, 0 + ioffset, 2 + ioffset, 3 + ioffset};

		memcpy(vbuf, v, sizeof(v));
		memcpy(ibuf, i, sizeof(i));

		letter += tc->advance * scale;
		ioffset += 4;
		vbuf += sizeof(v)/sizeof(float); ibuf += sizeof(i)/sizeof(unsigned int);
	}

	return nchars;
}

void renderCommand(void) {
	/* Draw command logs and command buffer if gamestate is COMMAND */

	/* 5 floats per vertex * 4 vertices per char, times log lines plus buffer. 6 indices/char for i */
	static float v[RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) * 20];
	static unsigned int i[RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) * 6];

	float *vptr;
	unsigned int *iptr, ioffset;

	/* Hijacking the pItemIcons1 layer (mesh), as it is never used (all icons go in pItemIcons0) */
	unsigned int nlog, nchars;

	char *s;
	for (vptr = v, iptr = i, ioffset = nlog = 0; nlog < RSM_MAX_COMMAND_LOG_LINES; nlog++) {
		s = cmdlog[((uint64_t) (logptr - cmdlog) + nlog) % RSM_MAX_COMMAND_LOG_LINES];

		nchars = drawText(s, vptr, iptr, ioffset, RSM_COMMAND_POS_X_PIXELS,
				RSM_COMMAND_POS_Y_PIXELS + (RSM_MAX_COMMAND_LOG_LINES - nlog) * lineheight,
				RSM_COMMAND_TEXT_SCALING, pItemIcons1);

		vptr += nchars * 20; iptr += nchars * 6; ioffset += nchars * 4;
	}

	if (gamestate == COMMAND) {
		nchars = drawText(cmdbuffer, vptr, iptr, ioffset, RSM_COMMAND_POS_X_PIXELS, RSM_COMMAND_POS_Y_PIXELS,
				RSM_COMMAND_TEXT_SCALING, pItemIcons1);
		vptr += nchars * 20; iptr += nchars * 6;
	}

	meshAppend(pItemIcons1, v, vptr - v, i, iptr - i);
}

void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h) {
	/* Draw an item sprite by id and variant in specified square (pixels) */
	uint64_t spriteid, spritetex, spriteoffset;
	float sox, soy;

	if (id == 0 && variant == 0) return; /* Holding air */

	spriteid = spriteids[id][variant];

#define SPRITES_PER_TEXTURE ((RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS) / (RSM_SPRITE_TEXTURE_SIZE_PIXELS * RSM_SPRITE_TEXTURE_SIZE_PIXELS))
	spritetex = spriteid / SPRITES_PER_TEXTURE;
	spriteoffset = spriteid % SPRITES_PER_TEXTURE;

	/* In-texture sprite offset x and y (sox and soy) */
#define SPRITES_PER_LINE (RSM_GUI_TEXTURE_SIZE_PIXELS / RSM_SPRITE_TEXTURE_SIZE_PIXELS)
#define SPRITE_STRIDE ((float) RSM_SPRITE_TEXTURE_SIZE_PIXELS / RSM_GUI_TEXTURE_SIZE_PIXELS)
	sox = (spriteoffset % SPRITES_PER_LINE) * SPRITE_STRIDE;
	soy = 1.0f - (spriteoffset / SPRITES_PER_LINE) * SPRITE_STRIDE - SPRITE_STRIDE;

	float v[] = {
		x, y, pItemIcons0, sox, guiAtlasAdjust(soy, pItemIcons0 + spritetex),
		x, y+h, pItemIcons0, sox, guiAtlasAdjust(soy + SPRITE_STRIDE, pItemIcons0 + spritetex),
		x+w, y, pItemIcons0, sox + SPRITE_STRIDE, guiAtlasAdjust(soy, pItemIcons0 + spritetex),
		x+w, y+h, pItemIcons0, sox + SPRITE_STRIDE, guiAtlasAdjust(soy + SPRITE_STRIDE, pItemIcons0 + spritetex),
	};

	spritevbuf = realloc(spritevbuf, (sizesv + 4*5) * sizeof(float));
	spriteibuf = realloc(spriteibuf, (sizesi + 6) * sizeof(unsigned int));

	memcpy(spritevbuf + sizesv, v, sizeof(v));

	/* To get the highest unused index, divide by the number of quads (2 triangles = 6 indices) and
	 * multiply by the number of unique indices created by quad (4, one for each vertex) */
	unsigned int ind = (sizesi / 6) * 4;
	spriteibuf[sizesi + 0] = ind + 0; spriteibuf[sizesi + 1] = ind + 1; spriteibuf[sizesi + 2] = ind + 2;
	spriteibuf[sizesi + 3] = ind + 1; spriteibuf[sizesi + 4] = ind + 2; spriteibuf[sizesi + 5] = ind + 3;

	sizesv += 4*5;
	sizesi += 6;
}

void renderCrosshair(void) {
	/* Draw crosshair */
	float v[5 * 4] = {
		WINDOW_WIDTH/2 - RSM_CROSSHAIR_SIZE_PIXELS, WINDOW_HEIGHT/2 - RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 0.0f, guiAtlasAdjust(0.0f, pCrosshair),
		WINDOW_WIDTH/2 - RSM_CROSSHAIR_SIZE_PIXELS, WINDOW_HEIGHT/2 + RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 0.0f, guiAtlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + RSM_CROSSHAIR_SIZE_PIXELS, WINDOW_HEIGHT/2 + RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 1.0f, guiAtlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + RSM_CROSSHAIR_SIZE_PIXELS, WINDOW_HEIGHT/2 - RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 1.0f, guiAtlasAdjust(0.0f, pCrosshair),
	};

	meshAppend(pCrosshair, v, sizeof(v)/sizeof(float), QUADI, QUADISIZE);
}

void renderHotbar(void) {
	/* Draw hotbar */
#define HSLOT_SIZE (RSM_HOTBAR_SLOT_SIZE_PIXELS)
#define HOTBAR_BASE (WINDOW_WIDTH/2 - HSLOT_SIZE * ((float) RSM_HOTBAR_SLOTS/2))
	float v[(2 * RSM_HOTBAR_SLOTS + 2) * 5]; /* Space for all vertices plus slot selection */

	for (int i = 0; i < RSM_HOTBAR_SLOTS + 1; i++) {
		int offset = i * 10;
		v[offset + 0] = HOTBAR_BASE + HSLOT_SIZE * i;
		v[offset + 1] = 0.0f;
		v[offset + 2] = pHotbarSlot;
		v[offset + 3] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 4] = guiAtlasAdjust(0.0f, pHotbarSlot);

		v[offset + 5] = HOTBAR_BASE + HSLOT_SIZE * i;
		v[offset + 6] = HSLOT_SIZE;
		v[offset + 7] = pHotbarSlot;
		v[offset + 8] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 9] = guiAtlasAdjust(1.0f, pHotbarSlot);
	}

	unsigned int i[6 * RSM_HOTBAR_SLOTS];
	for (int j = 0; j < RSM_HOTBAR_SLOTS * 2; j++) {
		int offset = j * 3;
		i[offset + 0] = 0 + j; i[offset + 1] = 1 + j; i[offset + 2] = 2 + j;
	}

	meshAppend(pHotbarSlot, v, sizeof(v)/sizeof(float), i, sizeof(i)/sizeof(unsigned int));

	/* Slot selection */
	float selection[] = {
		HOTBAR_BASE + HSLOT_SIZE * hotslotIndex, 0.0f, pSlotSelection, 0.0f, guiAtlasAdjust(0.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * hotslotIndex, HSLOT_SIZE, pSlotSelection, 0.0f, guiAtlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * (hotslotIndex + 1), HSLOT_SIZE, pSlotSelection, 1.0f, guiAtlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * (hotslotIndex + 1), 0.0f, pSlotSelection, 1.0f, guiAtlasAdjust(0.0f, pSlotSelection),
	};

	meshAppend(pSlotSelection, selection, sizeof(selection)/sizeof(float), QUADI, QUADISIZE);

	for (int j = 0; j < RSM_HOTBAR_SLOTS; j++) {
		renderItemSprite(hotbar[hotbarIndex][j][0], hotbar[hotbarIndex][j][1],
				HOTBAR_BASE + HSLOT_SIZE * j + RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOT_SIZE - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOT_SIZE - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS);
	}
}

unsigned int submenuSpriteIndices[RSM_INVENTORY_ICONS];
void renderInventory(void) {
#define ISLOT_SIZE (RSM_INVENTORY_SLOT_SIZE_PIXELS)
#define INV_BASE_X (WINDOW_WIDTH/2 - ISLOT_SIZE * ((float) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_BASE_Y (WINDOW_HEIGHT/2 - ISLOT_SIZE * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_ICONS_Y (WINDOW_HEIGHT/2 + ISLOT_SIZE * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
	/* Display the created base inventory mesh, and populate it with the sprites corresponding to the
	 * currently chosen submenu (icon). Also highlight which submenu is chosen. */
#define NINV_INDICES (RSM_INVENTORY_SLOTS_HORIZONTAL * RSM_INVENTORY_SLOTS_VERTICAL*6 + RSM_INVENTORY_ICONS*6)

	/* Do not display all submenus at the same time (and none when not in inventory) */
	memset(nGUIIndices + PICON, 0, sizeof(unsigned int) * RSM_INVENTORY_ICONS);

	if (gamestate != INVENTORY) {
		/* Disable all inventory GUI rendering as renderer skips meshes with 0 indices */
		nGUIIndices[pInventorySlot] = 0;
		return;
	}

	nGUIIndices[pInventorySlot] = NINV_INDICES;
	nGUIIndices[PICON + inventoryIndex] = submenuSpriteIndices[inventoryIndex];

	/* ID 0 1 is one of the sprite placeholders. In this case submenu selection; refer to blockmap */
	renderItemSprite(0, 1, INV_BASE_X + RSM_INVENTORY_ICON_SIZE_PIXELS * inventoryIndex, INV_ICONS_Y,
			RSM_INVENTORY_ICON_SIZE_PIXELS, RSM_INVENTORY_ICON_SIZE_PIXELS);
}

void initInventory(void) {
	/* Initialize inventory mesh, which is made up of RSM_INVENTORY_SLOTS_HORIZONTAL by
	 * RSM_INVENTORY_SLOTS_VERTICAL slots, centered on the screen, with clickable icons
	 * on top which act as subfolders on top (starting at the leftmost horizontal slot) */
#define NINV_VERTICES ((RSM_INVENTORY_SLOTS_HORIZONTAL + 1) * 2 * RSM_INVENTORY_SLOTS_VERTICAL * 5 + RSM_INVENTORY_ICONS * 4 * 5)
	float v[NINV_VERTICES];
	unsigned int i[NINV_INDICES]; /* Defined earlier for use in renderInventory */

	unsigned int j, k, ioffset, voffset;

	/* Generate inv background once */
	ioffset = voffset = 0;

	/* Slots */
	for (j = 0; j < RSM_INVENTORY_SLOTS_VERTICAL; j++) {
		for (k = 0; k < RSM_INVENTORY_SLOTS_HORIZONTAL * 2; k++, ioffset += 3) {
			/* Quite serpentine formula to get the right indices */
			i[ioffset + 0] = 0 + k + j * RSM_INVENTORY_SLOTS_HORIZONTAL * 2 + 2 * j;
			i[ioffset + 1] = 1 + k + j * RSM_INVENTORY_SLOTS_HORIZONTAL * 2 + 2 * j;
			i[ioffset + 2] = 2 + k + j * RSM_INVENTORY_SLOTS_HORIZONTAL * 2 + 2 * j;
		}
	}

	for (j = 0; j < RSM_INVENTORY_SLOTS_VERTICAL; j++) {
		for (k = 0; k < RSM_INVENTORY_SLOTS_HORIZONTAL + 1; k++, voffset += 10) {
			v[voffset + 0] = INV_BASE_X + ISLOT_SIZE * k;
			v[voffset + 1] = INV_BASE_Y + ISLOT_SIZE * j;
			v[voffset + 2] = pInventorySlot;
			v[voffset + 3] = k % 2 == 0 ? 0.0f : 1.0f; v[voffset + 4] = guiAtlasAdjust(0.0f, pInventorySlot);

			v[voffset + 5] = INV_BASE_X + ISLOT_SIZE * k;
			v[voffset + 6] = INV_BASE_Y + ISLOT_SIZE * (j + 1);
			v[voffset + 7] = pInventorySlot;
			v[voffset + 8] = k % 2 == 0 ? 0.0f : 1.0f; v[voffset + 9] = guiAtlasAdjust(1.0f, pInventorySlot);
		}
	}

	/* Icons */
#define ICON_SIZE (RSM_INVENTORY_ICON_SIZE_PIXELS)
	for (j = 0; j < RSM_INVENTORY_ICONS; j++, ioffset += 6) {
#define NINV_SLOT_INDICES ((RSM_INVENTORY_SLOTS_HORIZONTAL * 2 + 2) * RSM_INVENTORY_SLOTS_VERTICAL)
		i[ioffset + 0] = 0 + j * 4 + NINV_SLOT_INDICES;
		i[ioffset + 1] = 1 + j * 4 + NINV_SLOT_INDICES;
		i[ioffset + 2] = 2 + j * 4 + NINV_SLOT_INDICES;

		i[ioffset + 3] = 1 + j * 4 + NINV_SLOT_INDICES;
		i[ioffset + 4] = 2 + j * 4 + NINV_SLOT_INDICES;
		i[ioffset + 5] = 3 + j * 4 + NINV_SLOT_INDICES;
	}

	for (j = 0; j < RSM_INVENTORY_ICONS; j++, voffset += 20) {
		v[voffset+0] = INV_BASE_X + ICON_SIZE * j; v[voffset+1] = INV_ICONS_Y;
		v[voffset+2] = pInventorySlot; v[voffset+3] = 0.0f; v[voffset+4] = guiAtlasAdjust(0.0f, PICON + j);
		v[voffset+5] = INV_BASE_X + ICON_SIZE * j; v[voffset+6] = INV_ICONS_Y + ICON_SIZE;
		v[voffset+7] = pInventorySlot; v[voffset+8] = 0.0f; v[voffset+9] = guiAtlasAdjust(1.0f, PICON + j);
		v[voffset+10] = INV_BASE_X + ICON_SIZE * (j + 1); v[voffset+11] = INV_ICONS_Y;
		v[voffset+12] = pInventorySlot; v[voffset+13] = 1.0f; v[voffset+14] = guiAtlasAdjust(0.0f, PICON + j);
		v[voffset+15] = INV_BASE_X + ICON_SIZE * (j + 1); v[voffset+16] = INV_ICONS_Y + ICON_SIZE;
		v[voffset+17] = pInventorySlot; v[voffset+18] = 1.0f; v[voffset+19] = guiAtlasAdjust(1.0f, PICON + j);
	}

	meshAppend(pInventorySlot, v, sizeof(v)/sizeof(float), i, sizeof(i)/sizeof(unsigned int));

	/* Sprites for each submenu */
	char **iconlayout;
	uint64_t nsprites, itemuid, itemid, itemvariant;
	unsigned int xslotoffset, yslotoffset;
	for (j = 0; j < RSM_INVENTORY_ICONS; j++) {
		/* Populate icon sprites for this submenu and set it in the gui layer corresponding to its icon.
		 * This way, the base inventory (slots and icons) are in the pInventorySlot layer while the
		 * icon sprits live in the icon layers, saving them from being re-drawn each GUI render.
		 * We can also use the renderSprite function for this since initInventory is called before
		 * everything, and this renderSprite can be repurposed and assumed to be 0 here */
		iconlayout = usf_scsplit(iconlayouts[j], '\n', &nsprites);
		nsprites--; /* Account for terminating \n */

		for (k = 0; k < nsprites; k++) {
			itemuid = usf_strhmget(namemap, iconlayout[k]).u;
			itemid = GETID(itemuid); itemvariant = GETVARIANT(itemuid);

			xslotoffset = k % RSM_INVENTORY_SLOTS_HORIZONTAL;
			yslotoffset = k / RSM_INVENTORY_SLOTS_HORIZONTAL;

			/* Set in submenus for access by user input (quick copy to hotbar) */
			submenus[j][xslotoffset][RSM_INVENTORY_SLOTS_VERTICAL - yslotoffset - 1] = itemuid;

			/* Adjust to pixel size for sprite offsets */
			xslotoffset *= ISLOT_SIZE;
			yslotoffset *= ISLOT_SIZE;

			renderItemSprite(itemid, itemvariant,
					INV_BASE_X + xslotoffset + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					INV_ICONS_Y - ISLOT_SIZE - yslotoffset + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					ISLOT_SIZE - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2,
					ISLOT_SIZE - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2);
		}

		submenuSpriteIndices[j] = sizesi;
		meshAppend(PICON + j, spritevbuf, sizesv, spriteibuf, sizesi);

		/* Reset sprite buffers */
		sizesv = sizesi = 0;
		free(spritevbuf);
		free(spriteibuf);
		spritevbuf = NULL;
		spriteibuf = NULL;

		free(iconlayout);
		free(iconlayouts[j]);
	}
}
