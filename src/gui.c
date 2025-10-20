#include "gui.h"

GLuint guiAtlas;

GLuint guiVBO[MAX_GUI_PRIORITY], guiEBO[MAX_GUI_PRIORITY], guiVAO[MAX_GUI_PRIORITY];
unsigned int nGUIIndices[MAX_GUI_PRIORITY];

unsigned int hotbarIndex;
uint64_t hotbar[RSM_HOTBAR_SLOTS][2] = {{1, 3}, {1, 6}, {1, 7}}; /* Hotbar status id:variant */

extern uint64_t **spriteids;

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
}

void parseGUIdata(void) {
	/* Create GUI texture atlas and parse GUI elements from disk */
	char guiPath[sizeof(textureBasePath) + sizeof(textureGuiPath)];
	char guimapPath[sizeof(textureBasePath) + sizeof(textureGuimapPath)];

	strcpy(guiPath, textureBasePath);
	strcat(guiPath, textureGuiPath);

	strcpy(guimapPath, textureBasePath);
	strcat(guimapPath, textureGuimapPath);

	char *guimap, **elements, *element;
	uint64_t nelement, nelements;

	guimap = usf_ftos(guimapPath, "r", NULL);
	elements = usf_scsplit(guimap, '\n', &nelements);
	nelements--; /* One more because terminating \n is included as separator */

	char guitexturepath[RSM_MAX_PATH_NAME_LENGTH];
	unsigned char *guiAtlasData = NULL;
	GLsizei guiAtlasSize = 0;

	for (nelement = 0; nelement < nelements; nelement++) {
		element = elements[nelement];

		/* Append texture for this element to the atlas */
		if (sizeof(guiPath) + strlen(element) + sizeof(textureFormatExtension) > RSM_MAX_PATH_NAME_LENGTH) {
			fprintf(stderr, "GUI texture name too long at %s exceeding %u (with extensions), aborting.\n",
					element, RSM_MAX_PATH_NAME_LENGTH);
			exit(RSM_EXIT_EXCBUF);
		}

		strcpy(guitexturepath, guiPath);
		strcat(guitexturepath, element);
		strcat(guitexturepath, textureFormatExtension);

		atlasAppend(guitexturepath, RSM_GUI_TEXTURE_SIZE_PIXELS, &guiAtlasData, &guiAtlasSize);
	}

	free(elements);
	free(guimap); /* Will free the char **elements split substrings */

	glGenTextures(1, &guiAtlas);
	glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_GUI_TEXTURE_SIZE_PIXELS, guiAtlasSize / (4 * RSM_GUI_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, guiAtlasData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);

	free(guiAtlasData);
}

float atlasAdjust(float y, GUIPriority priority) {
	return (y * RSM_GUI_TEXTURE_SIZE_PIXELS + RSM_GUI_TEXTURE_SIZE_PIXELS * priority)
		/ (MAX_GUI_PRIORITY * RSM_GUI_TEXTURE_SIZE_PIXELS);
}

void meshAppend(unsigned int priority, float *v, unsigned int sizev, unsigned int *i, unsigned int sizei) {
	glBindVertexArray(guiVAO[priority]);
	glBindBuffer(GL_ARRAY_BUFFER, guiVBO[priority]);
	glBufferData(GL_ARRAY_BUFFER, sizev * sizeof(float), v, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizei * sizeof(unsigned int), i, GL_DYNAMIC_DRAW);
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

	meshAppend(pItemIcons0, spritevbuf, sizesv, spriteibuf, sizesi);

	free(spritevbuf);
	free(spriteibuf);
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
		x, y, pItemIcons0, sox, atlasAdjust(soy, pItemIcons0 + spritetex),
		x, y+h, pItemIcons0, sox, atlasAdjust(soy + SPRITE_STRIDE, pItemIcons0 + spritetex),
		x+w, y, pItemIcons0, sox + SPRITE_STRIDE, atlasAdjust(soy, pItemIcons0 + spritetex),
		x+w, y+h, pItemIcons0, sox + SPRITE_STRIDE, atlasAdjust(soy + SPRITE_STRIDE, pItemIcons0 + spritetex),
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
#define CROSSHAIR_SIZE (32.0f * RSM_GUI_SCALING_FACTOR)
	float v[] = {
		WINDOW_WIDTH/2 - CROSSHAIR_SIZE, WINDOW_HEIGHT/2 - CROSSHAIR_SIZE, pCrosshair, 0.0f, atlasAdjust(0.0f, pCrosshair),
		WINDOW_WIDTH/2 - CROSSHAIR_SIZE, WINDOW_HEIGHT/2 + CROSSHAIR_SIZE, pCrosshair, 0.0f, atlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + CROSSHAIR_SIZE, WINDOW_HEIGHT/2 + CROSSHAIR_SIZE, pCrosshair, 1.0f, atlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + CROSSHAIR_SIZE, WINDOW_HEIGHT/2 - CROSSHAIR_SIZE, pCrosshair, 1.0f, atlasAdjust(0.0f, pCrosshair),
	};

	meshAppend(pCrosshair, v, sizeof(v)/sizeof(float), QUADI, QUADISIZE);
}

void renderHotbar(void) {
	/* Draw hotbar */
#define SLOT_SIZE (RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_SCALING_FACTOR)
#define HOTBAR_BASE (WINDOW_WIDTH/2 - SLOT_SIZE * ((float) RSM_HOTBAR_SLOTS/2))
	float v[(2 * RSM_HOTBAR_SLOTS + 2) * 5]; /* Space for all vertices plus slot selection */

	for (int i = 0; i < RSM_HOTBAR_SLOTS + 1; i++) {
		int offset = i * 10;
		v[offset + 0] = HOTBAR_BASE + SLOT_SIZE * i;
		v[offset + 1] = 0.0f;
		v[offset + 2] = pHotbar;
		v[offset + 3] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 4] = atlasAdjust(0.0f, pHotbar);

		v[offset + 5] = HOTBAR_BASE + SLOT_SIZE * i;
		v[offset + 6] = SLOT_SIZE;
		v[offset + 7] = pHotbar;
		v[offset + 8] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 9] = atlasAdjust(1.0f, pHotbar);
	}

	unsigned int i[6 * RSM_HOTBAR_SLOTS];
	for (int j = 0; j < RSM_HOTBAR_SLOTS * 2; j++) {
		int offset = j * 3;
		i[offset + 0] = 0 + j; i[offset + 1] = 1 + j; i[offset + 2] = 2 + j;
	}

	meshAppend(pHotbar, v, sizeof(v)/sizeof(float), i, sizeof(i)/sizeof(unsigned int));

	/* Slot selection */
	float selection[] = {
		HOTBAR_BASE + SLOT_SIZE * hotbarIndex, 0.0f, pSlotSelection, 0.0f, atlasAdjust(0.0f, pSlotSelection),
		HOTBAR_BASE + SLOT_SIZE * hotbarIndex, SLOT_SIZE, pSlotSelection, 0.0f, atlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + SLOT_SIZE * (hotbarIndex + 1), SLOT_SIZE, pSlotSelection, 1.0f, atlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + SLOT_SIZE * (hotbarIndex + 1), 0.0f, pSlotSelection, 1.0f, atlasAdjust(0.0f, pSlotSelection),
	};

	meshAppend(pSlotSelection, selection, sizeof(selection)/sizeof(float), QUADI, QUADISIZE);

	for (int j = 0; j < RSM_HOTBAR_SLOTS; j++) {
		renderItemSprite(hotbar[j][0], hotbar[j][1],
				HOTBAR_BASE + SLOT_SIZE * j + (RSM_SPRITE_OFFSET_PIXELS * RSM_GUI_SCALING_FACTOR),
				RSM_SPRITE_OFFSET_PIXELS * RSM_GUI_SCALING_FACTOR,
				SLOT_SIZE - 2 * (RSM_SPRITE_OFFSET_PIXELS * RSM_GUI_SCALING_FACTOR),
				SLOT_SIZE - 2 * (RSM_SPRITE_OFFSET_PIXELS * RSM_GUI_SCALING_FACTOR));
	}
}
