#include "gui.h"

uint32_t hotslotIndex, hotbarIndex, submenuIndex;
uint64_t hotbar[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2]; /* Hotbar status id:variant */
uint64_t submenus[RSM_INVENTORY_SUBMENUS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL]; /*UIDs*/

void renderCrosshair(void);
void renderHotbar(void);
void renderInventory(void);
void renderCommand(void);
void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h);

void gui_updateGUI(void) {
	/* Calls renderGUI at most once per frame */
	GUI_SCHEDULEDUPDATE = 1;
}

/* Reset each GUI draw */
float *spritevbuf;
uint32_t *spriteibuf;
uint32_t sizesv, sizesi;
void gui_renderGUI(void) {
	/* Redraw the GUI. All elements are completely remeshed and sent to the GPU for rendering. */
	spritevbuf = NULL;
	spriteibuf = NULL;
	sizesv = sizesi = 0;

	renderCrosshair();
	renderHotbar();
	renderInventory();
	renderCommand();

	/* Any item sprites are rendered to the screen at the same priority */
	gu_meshSet(pItemIcons0, spritevbuf, sizesv, spriteibuf, sizesi);

	free(spritevbuf);
	free(spriteibuf);
}

void renderCrosshair(void) {
	/* Draw crosshair */
	float v[4 * 5] = {
		screenWidth/2 - RSM_CROSSHAIR_SIZE_PIXELS, screenHeight/2 - RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 0.0f, gu_guiAtlasAdjust(0.0f, pCrosshair),
		screenWidth/2 - RSM_CROSSHAIR_SIZE_PIXELS, screenHeight/2 + RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 0.0f, gu_guiAtlasAdjust(1.0f, pCrosshair),
		screenWidth/2 + RSM_CROSSHAIR_SIZE_PIXELS, screenHeight/2 + RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 1.0f, gu_guiAtlasAdjust(1.0f, pCrosshair),
		screenWidth/2 + RSM_CROSSHAIR_SIZE_PIXELS, screenHeight/2 - RSM_CROSSHAIR_SIZE_PIXELS, pCrosshair, 1.0f, gu_guiAtlasAdjust(0.0f, pCrosshair),
	};
	uint32_t i[6] = {0, 1, 2, 0, 2, 3};

	gu_meshSet(pCrosshair, v, sizeof(v)/sizeof(float), i, sizeof(i)/sizeof(uint32_t));
}

void renderHotbar(void) {
	/* Draw hotbar, hotbar selection and hotbar sprites */
	uint32_t i;
#define HSLOTSZ RSM_HOTBAR_SLOT_SIZE_PIXELS /* Purely for smaller lines */
#define HOTBAR_BASE (screenWidth/2 - HSLOTSZ * ((float) RSM_HOTBAR_SLOTS/2))

	/* Hotbar & sprites */
	float hotbarv[RSM_HOTBAR_SLOTS * 4 * 5];
	uint32_t hotbari[RSM_HOTBAR_SLOTS * 6];
	for (i = 0; i < RSM_HOTBAR_SLOTS; i++) {
#define GUI_HOTBAR_SLOT_VERTICES (float []) { \
		HOTBAR_BASE + HSLOTSZ * i, 0.0f, pHotbarSlot, 0.0f, gu_guiAtlasAdjust(0.0f, pHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * i, HSLOTSZ, pHotbarSlot, 0.0f, gu_guiAtlasAdjust(1.0f, pHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * (i + 1), HSLOTSZ, pHotbarSlot, 1.0f, gu_guiAtlasAdjust(1.0f, pHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * (i + 1), 0.0f, pHotbarSlot, 1.0f, gu_guiAtlasAdjust(0.0f, pHotbarSlot) }

#define GUI_HOTBAR_SLOT_INDICES (uint32_t []) {0 + i*4, 1 + i*4, 2 + i*4, 0 + i*4, 2 + i*4, 3 + i*4}

		memcpy(hotbarv + i * 4 * 5, GUI_HOTBAR_SLOT_VERTICES, 4 * 5 * sizeof(float));
		memcpy(hotbari + i * 6, GUI_HOTBAR_SLOT_INDICES, 6 * sizeof(uint32_t));

		/* Air is not rendered (UID 0:0) */
		renderItemSprite(hotbar[hotbarIndex][i][0], hotbar[hotbarIndex][i][1],
				HOTBAR_BASE + HSLOTSZ * i + RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOTSZ - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOTSZ - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS);
	}
	gu_meshSet(pHotbarSlot, hotbarv, sizeof(hotbarv)/sizeof(float), hotbari, sizeof(hotbari)/sizeof(uint32_t));

	/* Slot selection */
#define GUI_SELECTION_VERTICES (float []) { \
	HOTBAR_BASE+HSLOTSZ*hotslotIndex, 0.0f, pSlotSelection,0.0f,gu_guiAtlasAdjust(0.0f, pSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*hotslotIndex, HSLOTSZ, pSlotSelection,0.0f,gu_guiAtlasAdjust(1.0f, pSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*(hotslotIndex+1), HSLOTSZ, pSlotSelection,1.0f,gu_guiAtlasAdjust(1.0f, pSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*(hotslotIndex+1), 0.0f, pSlotSelection,1.0f,gu_guiAtlasAdjust(0.0f, pSlotSelection) }

#define GUI_SELECTION_INDICES (uint32_t []) {0, 1, 2, 0, 2, 3}

	gu_meshSet(pSlotSelection, GUI_SELECTION_VERTICES, sizeof(GUI_SELECTION_VERTICES)/sizeof(float),
			GUI_SELECTION_INDICES, sizeof(GUI_SELECTION_INDICES)/sizeof(uint32_t));
}

void renderInventory(void) {
	/* Draw inventory, submenu icons, submenu selection and submenu sprites */
	int32_t x, y, i;
#define ISLOTSZ RSM_INVENTORY_SLOT_SIZE_PIXELS
#define ICONSZ RSM_INVENTORY_ICON_SIZE_PIXELS
#define INV_BASEX (screenWidth/2 - ISLOTSZ * ((float) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_BASEY (screenHeight/2 - ISLOTSZ * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SUBMENUY (screenHeight/2 + ISLOTSZ * ((float) RSM_INVENTORY_SLOTS_VERTICAL/2))

	if (gamestate != INVENTORY) {
		nGUIIndices[pInventorySlot] = 0; /* Obsolete data not displayed */
		return;
	}

	float invv[RSM_INVENTORY_SLOTS_HORIZONTAL*RSM_INVENTORY_SLOTS_VERTICAL * 20 + RSM_INVENTORY_SUBMENUS * 20];
	uint32_t invi[RSM_INVENTORY_SLOTS_HORIZONTAL*RSM_INVENTORY_SLOTS_VERTICAL * 6 + RSM_INVENTORY_SUBMENUS * 6];

	uint64_t uid;
	for (i = x = 0; x < RSM_INVENTORY_SLOTS_HORIZONTAL; x++) {
		for (y = 0; y < RSM_INVENTORY_SLOTS_VERTICAL; y++, i++) {
#define GUI_INVENTORY_SLOT_VERTICES (float []) { \
	INV_BASEX+ISLOTSZ*x,INV_BASEY+ISLOTSZ*y,pInventorySlot,0.0f,gu_guiAtlasAdjust(0.0f,pInventorySlot), \
	INV_BASEX+ISLOTSZ*x,INV_BASEY+ISLOTSZ*(y+1),pInventorySlot,0.0f,gu_guiAtlasAdjust(1.0f,pInventorySlot), \
	INV_BASEX+ISLOTSZ*(x+1),INV_BASEY+ISLOTSZ*(y+1),pInventorySlot,1.0f,gu_guiAtlasAdjust(1.0f,pInventorySlot), \
	INV_BASEX+ISLOTSZ*(x+1),INV_BASEY+ISLOTSZ*y,pInventorySlot,1.0f,gu_guiAtlasAdjust(0.0f,pInventorySlot) }

#define GUI_INVENTORY_SLOT_INDICES (uint32_t []) {0 + i*4, 1 + i*4, 2 + i*4, 0 + i*4, 2 + i*4, 3 + i*4}

			memcpy(invv + i * 20, GUI_INVENTORY_SLOT_VERTICES, 20 * sizeof(float));
			memcpy(invi + i * 6, GUI_INVENTORY_SLOT_INDICES, 6 * sizeof(uint32_t));

			/* Render sprite at this slot if it exists */
			uid = submenus[submenuIndex][x][y];
			renderItemSprite(GETID(uid), GETVARIANT(uid),
					INV_BASEX + ISLOTSZ * x + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					INV_BASEY + ISLOTSZ * y + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					ISLOTSZ - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2,
					ISLOTSZ - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2);
		}
	}

	for (x = 0; x < RSM_INVENTORY_SUBMENUS; x++, i++) {
#define GUI_INVENTORY_SUBMENU_VERTICES (float []) { \
	INV_BASEX+ICONSZ*x, INV_SUBMENUY, pInventorySlot, 0.0f, gu_guiAtlasAdjust(0.0f, GUI_PICON+x), \
	INV_BASEX+ICONSZ*x, INV_SUBMENUY + ICONSZ, pInventorySlot, 0.0f, gu_guiAtlasAdjust(1.0f, GUI_PICON+x), \
	INV_BASEX+ICONSZ*(x+1), INV_SUBMENUY + ICONSZ, pInventorySlot, 1.0f, gu_guiAtlasAdjust(1.0f, GUI_PICON+x), \
	INV_BASEX+ICONSZ*(x+1), INV_SUBMENUY, pInventorySlot, 1.0f, gu_guiAtlasAdjust(0.0f, GUI_PICON+x) }

		memcpy(invv + i * 20, GUI_INVENTORY_SUBMENU_VERTICES, 20 * sizeof(float));
		memcpy(invi + i * 6, GUI_INVENTORY_SLOT_INDICES, 6 * sizeof(uint32_t)); /* Submenus are squares too */
	}

	/* ID 0 1 is the placeholder for submenu selection */
	renderItemSprite(0, 1, INV_BASEX + ICONSZ * submenuIndex, INV_SUBMENUY, ICONSZ, ICONSZ);

	gu_meshSet(pInventorySlot, invv, sizeof(invv)/sizeof(float), invi, sizeof(invi)/sizeof(uint32_t));
}

void renderCommand(void) {
	/* Draw command logs and command buffer if gamestate is COMMAND */

	/* 5 floats per vertex * 4 vertices per char, times log lines plus buffer. 6 indices/char for i */
	static float v[RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) * 20];
	static uint32_t i[RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) * 6];

	float *vptr;
	uint32_t *iptr, ioffset;

	/* Hijacking the pItemIcons1 layer (mesh), as it is never used (all sprites go in pItemIcons0) */
	uint32_t nlog, nchars;
	char *s;
	for (vptr = v, iptr = i, ioffset = nlog = 0; nlog < RSM_MAX_COMMAND_LOG_LINES; nlog++) {
		s = cmdlog[((uint64_t) (logptr - cmdlog) + nlog) % RSM_MAX_COMMAND_LOG_LINES];

		nchars = drawText(s, vptr, iptr, ioffset, RSM_COMMAND_POS_X_PIXELS * RSM_GUI_SCALING_FACTOR,
				RSM_COMMAND_POS_Y_PIXELS * RSM_GUI_SCALING_FACTOR
				+ (RSM_MAX_COMMAND_LOG_LINES - nlog) * lineheight * RSM_COMMAND_TEXT_SCALING,
				RSM_COMMAND_TEXT_SCALING, pItemIcons1);

		vptr += nchars * 20; iptr += nchars * 6; ioffset += nchars * 4;
	}

	if (gamestate == COMMAND) {
		nchars = drawText(cmdbuffer, vptr, iptr, ioffset, RSM_COMMAND_POS_X_PIXELS * RSM_GUI_SCALING_FACTOR,
				RSM_COMMAND_POS_Y_PIXELS * RSM_GUI_SCALING_FACTOR,
				RSM_COMMAND_TEXT_SCALING, pItemIcons1);
		vptr += nchars * 20; iptr += nchars * 6;
	}

	gu_meshSet(pItemIcons1, v, vptr - v, i, iptr - i);
}

void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h) {
	/* Draw an item sprite by id and variant in specified square (pixels) */
	uint64_t spriteid, spritetex, spriteoffset;
	float sox, soy;

	if (id >= MAX_BLOCK_ID) return; /* Invalid sprite */
	if (variant >= MAX_BLOCK_VARIANT[id]) variant = 0; /* Default to 0 */
	if (id == 0 && variant == 0) return; /* Holding air */

	spriteid = spriteids[id][variant]; /* Get sprite addition order to the atlas from spriteids */

#define SPRITES_PER_TEXTURE ((RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS) / (RSM_SPRITE_TEXTURE_SIZE_PIXELS * RSM_SPRITE_TEXTURE_SIZE_PIXELS))
	spritetex = spriteid / SPRITES_PER_TEXTURE;
	spriteoffset = spriteid % SPRITES_PER_TEXTURE;

	/* In-texture sprite offset x and y (sox and soy) */
#define SPRITES_PER_LINE (RSM_GUI_TEXTURE_SIZE_PIXELS / RSM_SPRITE_TEXTURE_SIZE_PIXELS)
#define SPRITE_STRIDE ((float) RSM_SPRITE_TEXTURE_SIZE_PIXELS / RSM_GUI_TEXTURE_SIZE_PIXELS)
	sox = (spriteoffset % SPRITES_PER_LINE) * SPRITE_STRIDE;
	soy = 1.0f - (spriteoffset / SPRITES_PER_LINE) * SPRITE_STRIDE - SPRITE_STRIDE;

#define v (float []) { \
		x, y, pItemIcons0, sox, gu_guiAtlasAdjust(soy, pItemIcons0 + spritetex), \
		x, y+h, pItemIcons0, sox, gu_guiAtlasAdjust(soy + SPRITE_STRIDE, pItemIcons0 + spritetex), \
		x+w, y, pItemIcons0, sox+SPRITE_STRIDE, gu_guiAtlasAdjust(soy, pItemIcons0 + spritetex), \
		x+w, y+h, pItemIcons0, sox+SPRITE_STRIDE, gu_guiAtlasAdjust(soy+SPRITE_STRIDE, pItemIcons0 + spritetex) }

	spritevbuf = realloc(spritevbuf, (sizesv + 4*5) * sizeof(float));
	spriteibuf = realloc(spriteibuf, (sizesi + 6) * sizeof(uint32_t));

	memcpy(spritevbuf + sizesv, v, 20 * sizeof(float));

	/* To get the highest unused index, divide by the number of quads (2 triangles = 6 indices) and
	 * multiply by the number of unique indices created by quad (4, one for each vertex) */
	uint32_t ind = (sizesi / 6) * 4;
	spriteibuf[sizesi + 0] = ind + 0; spriteibuf[sizesi + 1] = ind + 1; spriteibuf[sizesi + 2] = ind + 2;
	spriteibuf[sizesi + 3] = ind + 1; spriteibuf[sizesi + 4] = ind + 2; spriteibuf[sizesi + 5] = ind + 3;

	sizesv += 4*5;
	sizesi += 6;
}
