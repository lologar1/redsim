#ifndef GUI_H
#define GUI_H

#include "renderutils.h"
#include "redsim.h"

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
	MAX_GUI_PRIORITY
} GUIPriority;
#define PICON (pSolidIcon) /* First icon offset */

extern GLuint guiAtlas, guiVAO[MAX_GUI_PRIORITY];
extern usf_hashmap *namemap;
extern unsigned int hotbarIndex, inventoryIndex, nGUIIndices[MAX_GUI_PRIORITY];
extern uint64_t hotbar[RSM_HOTBAR_SLOTS][2];
extern uint64_t submenus[RSM_INVENTORY_ICONS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];

void initGUI(void); /* OpenGL stuff for GUI */
void parseGUIdata(void); /* Create texture atlas and gui elements */

float atlasAdjust(float y, GUIPriority priority);
void meshAppend(unsigned int priority, float *v, unsigned int sizev, unsigned int *i, unsigned int sizei);

void renderGUI(void); /* Render the GUI */
void renderItemSprite(uint64_t id, uint64_t variant, float x, float y, float w, float h);
void renderCrosshair(void);
void renderHotbar(void);
void renderInventory(void);
void initInventory(void);

#endif
