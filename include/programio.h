#ifndef PROGRAMIO_H
#define PROGRAMIO_H

#include <stdlib.h>
#include <string.h>
#include "ft2build.h"
#include FT_FREETYPE_H
#include "usfstring.h"
#include "rsmlayout.h"
#include "guiutils.h"

/* Forward declarations */
struct Blockmesh;
struct Textchar;

extern u64 MAX_BLOCK_ID;
extern u64 *MAX_BLOCK_VARIANT;
extern u64 NBLOCKTEXTURES;
extern u64 OV_BUFSZ, TV_BUFSZ, OI_BUFSZ, TI_BUFSZ; /* Max buffer sizes (used in remeshing) */
extern struct Blockmesh **BLOCKMESHES;
extern f32 (**BOUNDINGBOXES)[6];
extern u64 **SPRITEIDS;

extern u64 SUBMENUS[RSM_INVENTORY_SUBMENUS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];
extern f32 LINEHEIGHT;
extern struct Textchar *TEXTCHARS;
extern u64 NCHARTEXTURES;

extern GLuint blockTextureAtlas_;
extern GLuint guiTextureAtlas_;

void pio_parseBlockdata(void);
void pio_parseGUIdata(void);
void pio_pathcat(char *destination, u64 n, ...);

#endif
