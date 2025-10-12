#ifndef GUI_H
#define GUI_H

#include "renderer.h"
#include "renderutils.h"

void initGUI(void); /* OpenGL stuff for GUI */
void parseGUIdata(void); /* Create texture atlas and gui elements */

/* Priority list for which GUI elements get drawn in front. This list MUST mimic
 * the order in guimap as indices are also used for texture atlas offsetting */
typedef enum {
	pHotbar = 0,
	pCrosshair,
	MAX_GUI_PRIORITY
} GUIPriority;

float atlasAdjust(float y, GUIPriority priority);
void meshAppend(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei,
		float v[], unsigned int i[], size_t sv, size_t si);

void renderGUI(void); /* Render the GUI */
void renderCrosshair(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei);
void renderHotbar(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei);

#endif
