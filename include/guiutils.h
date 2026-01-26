#ifndef GUIUTILS_H
#define GUIUTILS_H

#include <ctype.h>
#include "client.h"
#include "rsmlayout.h"
#include "usfmath.h"

typedef enum GUITextureID { /* GUI texture IDs must match guimap declaration order! Used for atlas indexing. */
#define TITEMICONSID tItemIcons0
	tItemIcons0,
	tItemIcons1,
	tItemIcons2,
	tItemIcons3,
#define TICONID tSolidIcon
	tSolidIcon,
	tTransIcon,
	tComponentIcon,
	tMiscIcon,
	tInventorySlot,
	tSlotSelection,
	tHotbarSlot,
	tCrosshair,
	MAX_GUI_TEXTUREID
} GUITextureID;

typedef enum GUIMeshID {
	mItemIcons,
	mCommandBackground,
	mCommand,
	mInventory,
	mSlotSelection,
	mHotbarSlot,
	mCrosshair,
	MAX_GUI_MESHID
} GUIMeshID;

typedef struct Textchar {
	f32 uv[4]; /* Coords in guiAtlas */
	i64 size[2]; /* Width/height in pixels */
	i64 bearing[2]; /* x bearing/y bearing in pixels */
	i64 advance; /* glyph advance in pixels */
} Textchar;

extern GLuint guiVAOs_[MAX_GUI_MESHID];
extern u64 nGUIIndices_[MAX_GUI_MESHID];

void gu_initGUI(void);
f32 gu_guiAtlasAdjust(f32 v, GUITextureID textureid);
void gu_meshSet(u64 meshid, f32 *v, u64 nvertices, u32 *i, u64 nindices);
u64 gu_drawText(char *str, f32 *vbuf, u32 *ibuf, u64 ioffset, f32 x, f32 y, f32 scale, GUIMeshID priority);

#endif
