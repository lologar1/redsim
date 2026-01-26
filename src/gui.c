#include "gui.h"

i32 gui_scheduleUpdate_ = 0; /* Consumed each frame if set, and refreshes the GUI */
u64 hotslotIndex_, hotbarIndex_, submenuIndex_;
u64 hotbar_[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2]; /* Hotbar status id:variant */

/* Reset each GUI draw */
static f32 *spritevbuf_;
static u32 *spriteibuf_;
static u64 nspritev_;
static u64 nspritei_;

static void renderCrosshair(void);
static void renderHotbar(void);
static void renderInventory(void);
static void renderCommand(void);
static void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h);

void gui_updateGUI(void) { gui_scheduleUpdate_ = 1; }

void gui_renderGUI(void) {
	/* Redraw the GUI. All elements are completely remeshed and sent to the GPU for rendering. */

	spritevbuf_ = NULL;
	spriteibuf_ = NULL;
	nspritev_ = nspritei_ = 0;

	renderCrosshair();
	renderHotbar();
	renderInventory();
	renderCommand();

	/* Any item sprites are rendered to the screen at the same priority */
	gu_meshSet(mItemIcons, spritevbuf_, nspritev_, spriteibuf_, nspritei_);

	free(spritevbuf_);
	free(spriteibuf_);
}

static void renderCrosshair(void) {
	/* Draw crosshair */

#define CSIZE RSM_CROSSHAIR_SIZE_PIXELS
	f32 v[4 * 5] = {
		screenWidth_/2 - CSIZE, screenHeight_/2 - CSIZE, mCrosshair, 0.0f, gu_guiAtlasAdjust(0.0f, tCrosshair),
		screenWidth_/2 - CSIZE, screenHeight_/2 + CSIZE, mCrosshair, 0.0f, gu_guiAtlasAdjust(1.0f, tCrosshair),
		screenWidth_/2 + CSIZE, screenHeight_/2 + CSIZE, mCrosshair, 1.0f, gu_guiAtlasAdjust(1.0f, tCrosshair),
		screenWidth_/2 + CSIZE, screenHeight_/2 - CSIZE, mCrosshair, 1.0f, gu_guiAtlasAdjust(0.0f, tCrosshair),
	};
	u32 i[6] = {0, 1, 2, 0, 2, 3};
#undef CSIZE

	gu_meshSet(mCrosshair, v, sizeof(v)/sizeof(f32), i, sizeof(i)/sizeof(u32));
}

static void renderHotbar(void) {
	/* Draw hotbar, hotbar selection and hotbar sprites */

#define HSLOTSZ RSM_HOTBAR_SLOT_SIZE_PIXELS /* Purely for smaller lines */
#define HOTBAR_BASE (screenWidth_/2 - HSLOTSZ * ((float) RSM_HOTBAR_SLOTS/2))

	/* Hotbar & sprites */
	f32 hotbarv[RSM_HOTBAR_SLOTS * 4 * 5];
	u32 hotbari[RSM_HOTBAR_SLOTS * 6], i;
	for (i = 0; i < RSM_HOTBAR_SLOTS; i++) {
#define _HOTBAR_SLOT_VERTICES (f32 []) { \
		HOTBAR_BASE + HSLOTSZ * i, 0.0f, mHotbarSlot, 0.0f, gu_guiAtlasAdjust(0.0f, tHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * i, HSLOTSZ, mHotbarSlot, 0.0f, gu_guiAtlasAdjust(1.0f, tHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * (i + 1), HSLOTSZ, mHotbarSlot, 1.0f, gu_guiAtlasAdjust(1.0f, tHotbarSlot), \
		HOTBAR_BASE + HSLOTSZ * (i + 1), 0.0f, mHotbarSlot, 1.0f, gu_guiAtlasAdjust(0.0f, tHotbarSlot) }
#define _HOTBAR_SLOT_INDICES (u32 []) {0 + i*4, 1 + i*4, 2 + i*4, 0 + i*4, 2 + i*4, 3 + i*4}
		memcpy(hotbarv + i * 4 * 5, _HOTBAR_SLOT_VERTICES, 4 * 5 * sizeof(f32));
		memcpy(hotbari + i * 6, _HOTBAR_SLOT_INDICES, 6 * sizeof(u32));

		/* renderItemSprite skips air (UID 0), so no need to test here */
		renderItemSprite(hotbar_[hotbarIndex_][i][0], hotbar_[hotbarIndex_][i][1],
				HOTBAR_BASE + HSLOTSZ * i + RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOTSZ - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS,
				HSLOTSZ - 2 * RSM_HOTBAR_SLOT_SPRITE_OFFSET_PIXELS);
	}
#undef _HOTBAR_SLOT_VERTICES
#undef _HOTBAR_SLOT_INDICES
	gu_meshSet(mHotbarSlot,
			hotbarv, sizeof(hotbarv)/sizeof(f32),
			hotbari, sizeof(hotbari)/sizeof(u32));

	/* Slot selection */
#define _SELECTION_VERTICES (f32 []) { \
	HOTBAR_BASE+HSLOTSZ*hotslotIndex_, 0.0f, mSlotSelection,0.0f,gu_guiAtlasAdjust(0.0f,tSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*hotslotIndex_, HSLOTSZ, mSlotSelection,0.0f,gu_guiAtlasAdjust(1.0f,tSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*(hotslotIndex_+1), HSLOTSZ, mSlotSelection,1.0f,gu_guiAtlasAdjust(1.0f,tSlotSelection), \
	HOTBAR_BASE+HSLOTSZ*(hotslotIndex_+1), 0.0f, mSlotSelection,1.0f,gu_guiAtlasAdjust(0.0f,tSlotSelection) }
#define _SELECTION_INDICES (u32 []) {0, 1, 2, 0, 2, 3}
	gu_meshSet(mSlotSelection,
			_SELECTION_VERTICES, sizeof(_SELECTION_VERTICES)/sizeof(f32),
			_SELECTION_INDICES, sizeof(_SELECTION_INDICES)/sizeof(u32));
#undef _SELECTION_VERTICES
#undef _SELECTION_INDICES
}

static void renderInventory(void) {
	/* Draw inventory, submenu icons, submenu selection (as sprite) and submenu sprites */

	if (gamestate_ != INVENTORY) {
		nGUIIndices_[mInventory] = 0; /* If not open, don't display */
		return;
	}

	static f32 invv[RSM_INVENTORY_SLOTS_HORIZONTAL*RSM_INVENTORY_SLOTS_VERTICAL*20 + RSM_INVENTORY_SUBMENUS*20];
	static u32 invi[RSM_INVENTORY_SLOTS_HORIZONTAL*RSM_INVENTORY_SLOTS_VERTICAL*6 + RSM_INVENTORY_SUBMENUS*6];

	u64 uid, i, x, y;
	for (i = x = 0; x < RSM_INVENTORY_SLOTS_HORIZONTAL; x++) {
		for (y = 0; y < RSM_INVENTORY_SLOTS_VERTICAL; y++, i++) {

/* Shorthands and utility defines */
#define ISLOTSZ RSM_INVENTORY_SLOT_SIZE_PIXELS
#define ICONSZ RSM_INVENTORY_ICON_SIZE_PIXELS
#define INV_BASEX (screenWidth_/2 - ISLOTSZ * ((f32) RSM_INVENTORY_SLOTS_HORIZONTAL/2))
#define INV_BASEY (screenHeight_/2 - ISLOTSZ * ((f32) RSM_INVENTORY_SLOTS_VERTICAL/2))
#define INV_SUBMENUY (screenHeight_/2 + ISLOTSZ * ((f32) RSM_INVENTORY_SLOTS_VERTICAL/2))

#define _INVENTORY_SLOT_VERTICES (f32 []) { \
	INV_BASEX+ISLOTSZ*x,INV_BASEY+ISLOTSZ*y, mInventory, 0.0f, gu_guiAtlasAdjust(0.0f, tInventorySlot), \
	INV_BASEX+ISLOTSZ*x,INV_BASEY+ISLOTSZ*(y+1),mInventory,0.0f,gu_guiAtlasAdjust(1.0f,tInventorySlot), \
	INV_BASEX+ISLOTSZ*(x+1),INV_BASEY+ISLOTSZ*(y+1),mInventory,1.0f,gu_guiAtlasAdjust(1.0f,tInventorySlot), \
	INV_BASEX+ISLOTSZ*(x+1),INV_BASEY+ISLOTSZ*y,mInventory,1.0f,gu_guiAtlasAdjust(0.0f,tInventorySlot) }
#define _INVENTORY_SLOT_INDICES (u32 []) {0 + i*4, 1 + i*4, 2 + i*4, 0 + i*4, 2 + i*4, 3 + i*4}
			memcpy(invv + i * 20, _INVENTORY_SLOT_VERTICES, 20 * sizeof(f32));
			memcpy(invi + i * 6, _INVENTORY_SLOT_INDICES, 6 * sizeof(u32));

			/* Render sprite at this slot (renderItemSprite skips UID 0) */
			uid = SUBMENUS[submenuIndex_][x][y];
			renderItemSprite(GETID(uid), GETVARIANT(uid),
					INV_BASEX + ISLOTSZ * x + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					INV_BASEY + ISLOTSZ * y + RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS,
					ISLOTSZ - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2,
					ISLOTSZ - RSM_INVENTORY_SLOT_SPRITE_OFFSET_PIXELS * 2);
		}
#undef _INVENTORY_SLOT_VERTICES
	}

	for (x = 0; x < RSM_INVENTORY_SUBMENUS; x++, i++) {
#define _INVENTORY_SUBMENU_VERTICES (f32 []) { \
	INV_BASEX+ICONSZ*x, INV_SUBMENUY, mInventory, 0.0f, gu_guiAtlasAdjust(0.0f, TICONID+x), \
	INV_BASEX+ICONSZ*x, INV_SUBMENUY + ICONSZ, mInventory, 0.0f, gu_guiAtlasAdjust(1.0f, TICONID+x), \
	INV_BASEX+ICONSZ*(x+1), INV_SUBMENUY + ICONSZ, mInventory, 1.0f, gu_guiAtlasAdjust(1.0f, TICONID+x), \
	INV_BASEX+ICONSZ*(x+1), INV_SUBMENUY, mInventory, 1.0f, gu_guiAtlasAdjust(0.0f, TICONID+x) }
		memcpy(invv + i * 20, _INVENTORY_SUBMENU_VERTICES, 20 * sizeof(f32));
		memcpy(invi + i * 6, _INVENTORY_SLOT_INDICES, 6 * sizeof(u32)); /* Submenus are squares too */
	}
#undef _INVENTORY_SLOT_INDICES /* Reused as index order is the same for submenus */
#undef _INVENTORY_SUBMENU_INDICES

	/* Slots and submenus sent to GL buffer */
	gu_meshSet(mInventory, invv, sizeof(invv)/sizeof(f32), invi, sizeof(invi)/sizeof(u32));

	/* Render submenu selection as a sprite (on top of everything). ID is (0, 1) */
	renderItemSprite(0, 1, INV_BASEX + ICONSZ * submenuIndex_, INV_SUBMENUY, ICONSZ, ICONSZ);
#undef ISLOTSZ
#undef ICONSZ
#undef INV_BASEX
#undef INV_BASEY
#undef INV_SUBMENUY
}

static void renderCommand(void) {
	/* Draw command buffers and background */

	/* Background is TODO */

	static f32 v[(RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) + 1) * 20];
	static u32 i[(RSM_MAX_COMMAND_LENGTH * (RSM_MAX_COMMAND_LOG_LINES + 1) + 1) * 6];

	/* Draw command buffers */
	char *s;
	f32 *vptr;
	u32 *iptr;
	u64 log, nchars, ioffset;
	for (vptr = v, iptr = i, ioffset = log = 0; log < RSM_MAX_COMMAND_LOG_LINES; log++) {
		s = cmdlog_[((u64) (logptr_ - cmdlog_) + log) % RSM_MAX_COMMAND_LOG_LINES]; /* Get log */

		nchars = gu_drawText(s, vptr, iptr, ioffset,
				RSM_COMMAND_POS_X_PIXELS,
				RSM_COMMAND_POS_Y_PIXELS + (RSM_MAX_COMMAND_LOG_LINES-log) * LINEHEIGHT*RSM_COMMAND_TEXT_SCALING,
				RSM_COMMAND_TEXT_SCALING, mCommand);

		vptr += nchars * 20;
		iptr += nchars * 6;
		ioffset += nchars * 4;
	}

	if (gamestate_ == COMMAND) {
		nchars = gu_drawText(cmdbuffer_, vptr, iptr, ioffset,
				RSM_COMMAND_POS_X_PIXELS * RSM_GUI_SCALING_FACTOR,
				RSM_COMMAND_POS_Y_PIXELS * RSM_GUI_SCALING_FACTOR,
				RSM_COMMAND_TEXT_SCALING, mCommand);

		vptr += nchars * 20;
		iptr += nchars * 6;
	}

	gu_meshSet(mCommand, v, (u64) (vptr - v), i, (u64) (iptr - i));
}

static void renderItemSprite(u64 id, u64 variant, f32 x, f32 y, f32 w, f32 h) {
	/* Draw an item sprite by id and variant at the specified position */

	if (id >= MAX_BLOCK_ID) return; /* Invalid sprite */
	if (variant >= MAX_BLOCK_VARIANT[id]) variant = 0; /* Default to variant 0 */
	if (id == 0 && variant == 0) return; /* Don't render air (bad tex ID placeholder) */

#define SPRITES_PER_TEXTURE ((RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS) \
		/ (RSM_SPRITE_TEXTURE_SIZE_PIXELS * RSM_SPRITE_TEXTURE_SIZE_PIXELS))
	u64 spriteid, spritetex, spriteoffset;
	spriteid = SPRITEIDS[id][variant]; /* Get sprite ID */
	spritetex = spriteid / SPRITES_PER_TEXTURE; /* Sprite GUI texture */
	spriteoffset = spriteid % SPRITES_PER_TEXTURE; /* Sprite offset within GUI texture */
#undef SPRITES_PER_TEXTURE

#define SPRITES_PER_LINE (RSM_GUI_TEXTURE_SIZE_PIXELS / RSM_SPRITE_TEXTURE_SIZE_PIXELS)
#define SPRITE_STRIDE ((f32) RSM_SPRITE_TEXTURE_SIZE_PIXELS / (f32) RSM_GUI_TEXTURE_SIZE_PIXELS)
	f32 sox, soy;
	sox = (spriteoffset % SPRITES_PER_LINE) * SPRITE_STRIDE;
	soy = 1.0f - (spriteoffset / SPRITES_PER_LINE) * SPRITE_STRIDE - SPRITE_STRIDE;

#define _SPRITE_VERTICES (f32 []) { \
		x, y, mItemIcons, sox, gu_guiAtlasAdjust(soy, TITEMICONSID + spritetex), \
		x, y+h, mItemIcons, sox, gu_guiAtlasAdjust(soy + SPRITE_STRIDE, TITEMICONSID + spritetex), \
		x+w, y, mItemIcons, sox+SPRITE_STRIDE, gu_guiAtlasAdjust(soy, TITEMICONSID + spritetex), \
		x+w, y+h, mItemIcons, sox+SPRITE_STRIDE, gu_guiAtlasAdjust(soy+SPRITE_STRIDE, TITEMICONSID + spritetex) }
	spritevbuf_ = realloc(spritevbuf_, (nspritev_ + 20) * sizeof(f32));
	spriteibuf_ = realloc(spriteibuf_, (nspritei_ + 6) * sizeof(u32));
	memcpy(spritevbuf_ + nspritev_, _SPRITE_VERTICES, 20 * sizeof(f32));
#undef SPRITES_PER_LINE
#undef SPRITE_STRIDE
#undef _SPRITE_VERTICES

	u64 ioffset = (nspritei_ / 6) * 4; /* Highest index: divide by ntriangles, multiply by 6 */
	spriteibuf_[nspritei_+0]=ioffset+0; spriteibuf_[nspritei_+1]=ioffset+1; spriteibuf_[nspritei_+2]=ioffset+2;
	spriteibuf_[nspritei_+3]=ioffset+1; spriteibuf_[nspritei_+4]=ioffset+2; spriteibuf_[nspritei_+5]=ioffset+3;

	nspritev_ += 20;
	nspritei_ += 6;
}
