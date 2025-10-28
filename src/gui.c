#include "gui.h"

GLuint guiAtlas;

GLuint guiVBO[MAX_GUI_PRIORITY], guiEBO[MAX_GUI_PRIORITY], guiVAO[MAX_GUI_PRIORITY];
unsigned int nGUIIndices[MAX_GUI_PRIORITY];

usf_hashmap *namemap; /* Name -> (id:variant) in the form of a uint64_t partitioned in 2 32-bit numbers */

unsigned int hotbarIndex; /* Current slot */
unsigned int inventoryIndex; /* Current submenu */

uint64_t hotbar[RSM_HOTBAR_SLOTS][2]; /* Hotbar status id:variant */
uint64_t submenus[RSM_INVENTORY_ICONS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];

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

	initInventory(); /* Create base inventory mesh once */
}

char *iconlayouts[RSM_INVENTORY_ICONS]; /* For use by inventory init ; free'd then */
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
		pathcat(guitexturepath, 3, guiPath, element, textureFormatExtension);

		atlasAppend(guitexturepath, RSM_GUI_TEXTURE_SIZE_PIXELS, &guiAtlasData, &guiAtlasSize);

		/* Check for icons */
		if (!(nelement >= PICON && nelement < PICON + RSM_INVENTORY_ICONS)) continue;
		/* Reuse same buffer for this name. A bit unpretty, but what do you want */
		pathcat(guitexturepath, 3, guiPath, element, layoutFormatExtension);

		if ((iconlayouts[nelement - PICON] = usf_ftos(guitexturepath, "r", NULL)) == NULL) {
			fprintf(stderr, "Cannot find inventory submenu layout at %s, aborting.\n", guitexturepath);
			exit(RSM_EXIT_NOLAYOUT);
		}
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

float guiAtlasAdjust(float y, GUIPriority priority) {
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
	renderInventory(); /* Will handle itself */

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
	float v[] = {
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
		HOTBAR_BASE + HSLOT_SIZE * hotbarIndex, 0.0f, pSlotSelection, 0.0f, guiAtlasAdjust(0.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * hotbarIndex, HSLOT_SIZE, pSlotSelection, 0.0f, guiAtlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * (hotbarIndex + 1), HSLOT_SIZE, pSlotSelection, 1.0f, guiAtlasAdjust(1.0f, pSlotSelection),
		HOTBAR_BASE + HSLOT_SIZE * (hotbarIndex + 1), 0.0f, pSlotSelection, 1.0f, guiAtlasAdjust(0.0f, pSlotSelection),
	};

	meshAppend(pSlotSelection, selection, sizeof(selection)/sizeof(float), QUADI, QUADISIZE);

	for (int j = 0; j < RSM_HOTBAR_SLOTS; j++) {
		renderItemSprite(hotbar[j][0], hotbar[j][1],
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

	/* ID 0 1 is one of the sprite placeholders ; refer to blockmap */
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
			itemid = itemuid >> 32; itemvariant = itemuid & 0xFFFFFFFF;

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
