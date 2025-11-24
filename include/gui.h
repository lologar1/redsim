#ifndef GUI_H
#define GUI_H

#include "ft2build.h"
#include FT_FREETYPE_H /* Freetype include macro */
#include <ctype.h>
#include "renderutils.h"
#include "rsmlayout.h"
#include "redsim.h"
#include "command.h"
#include "usfmath.h"

#define QUADI ((unsigned int[])  {0, 1, 2, 0, 2, 3})
#define QUADISIZE 6LU

/* Priority list for which GUI elements get drawn in front. This list MUST mimic
 * the order in guimap as indices are also used for texture atlas offsetting
 * TOP = shown with most priority (smallest Z) */
typedef enum {
	pItemIcons0, /* This prio is used for all icons ; the others are simply for texture loading (contiguous) */
	pItemIcons1,
	pSolidIcon,
	pTransIcon,
	pComponentIcon,
	pMiscIcon,
	pInventorySlot,
	pSlotSelection,
	pHotbarSlot,
	pCrosshair,
	MAX_GUI_PRIORITY /* Character textures come after this */
} GUIPriority;
#define PICON (pSolidIcon) /* First icon offset */

typedef struct Textchar {
	float uv[4]; /* Coords in guiAtlas */
	int size[2]; /* In pixels */
	int bearing[2];
	int advance;
} Textchar;

extern FT_Library ft; /* FreeType library */
extern FT_Face typeface;
extern GLuint guiAtlas, guiVAO[MAX_GUI_PRIORITY];
extern usf_hashmap *namemap;
extern unsigned int hotslotIndex, hotbarIndex, inventoryIndex, nGUIIndices[MAX_GUI_PRIORITY];
extern uint64_t hotbar[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2];
extern uint64_t submenus[RSM_INVENTORY_ICONS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];

void initGUI(void);/* OpenGL stuff for GUI */
void initFont(unsigned char **guiatlas, GLsizei *atlassize); /* Init textures for character rendering */
void parseGUIdata(void); /* Create texture atlas and gui elements */

float guiAtlasAdjust(float y, GUIPriority priority);
void meshAppend(unsigned int priority, float *v, unsigned int sizev, unsigned int *i, unsigned int sizei);

void renderGUI(void); /* Render the GUI */
unsigned int drawText(char *str, float *vbuf, unsigned int *ibuf, unsigned int ioffset, float x, float y, float scale, GUIPriority priority);
void renderCommand(void);
void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h);
void renderCrosshair(void);
void renderHotbar(void);
void renderInventory(void);
void initInventory(void);

#endif
