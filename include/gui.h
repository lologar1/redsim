#ifndef GUI_H
#define GUI_H

#include "guiutils.h"
#include "renderutils.h"
#include "rsmlayout.h"
#include "redsim.h"
#include "command.h"
#include "usfmath.h"

extern uint32_t hotslotIndex, hotbarIndex, submenuIndex;
extern uint64_t hotbar[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2];
extern uint64_t submenus[RSM_INVENTORY_SUBMENUS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];

void gui_updateGUI(void); /* Calls renderGUI at most once per frame */
void gui_renderGUI(void);

#endif
