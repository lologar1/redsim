#ifndef GUIUTILS_H
#define GUIUTILS_H

#include <ctype.h>
#include "client.h"
#include "rsmlayout.h"
#include "ft2build.h"
#include FT_FREETYPE_H

#include "usfmath.h"

/* Priority list for which GUI elements get drawn in front. This list MUST mimic
 * the order in guimap as indices are also used for texture atlas offsetting
 * TOP = shown with most priority (smallest Z) */
typedef enum {
	pItemIcons0, /* This prio is used for all icons ; the others are simply for texture loading (contiguous) */
	pItemIcons1,
	pItemIcons2,
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
#define GUI_PICON pSolidIcon /* First submenu icon */

typedef struct Textchar {
	float uv[4]; /* Coords in guiAtlas */
	int32_t size[2]; /* In pixels */
	int32_t bearing[2];
	int32_t advance;
} Textchar;

extern GLuint guiAtlas, guiVAO[MAX_GUI_PRIORITY];
extern uint32_t nGUIIndices[MAX_GUI_PRIORITY];
extern float lineheight;
extern int32_t GUI_SCHEDULEDUPDATE; /* Flag set by updateGUI, consumed each frame if need be */

extern Textchar textchars[128];

void gu_initGUI(void);
void gu_parseGUIdata(void);
void gu_loadFont(unsigned char **guiatlas, GLsizei *atlassize);
float gu_guiAtlasAdjust(float y, GUIPriority priority);
void gu_meshSet(uint32_t priority, float *v, uint32_t sizev, uint32_t *i, uint32_t sizei);
uint32_t drawText(char *str, float *vbuf, uint32_t *ibuf, uint32_t ioffset, float x, float y, float scale, GUIPriority priority);

#endif
