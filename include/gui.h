#ifndef GUI_H
#define GUI_H

#include "guiutils.h"
#include "renderutils.h"
#include "rsmlayout.h"
#include "redsim.h"
#include "command.h"
#include "usfmath.h"

extern i32 gui_scheduleUpdate_;
extern u64 hotslotIndex_, hotbarIndex_, submenuIndex_;
extern u64 hotbar_[RSM_HOTBAR_COUNT][RSM_HOTBAR_SLOTS][2];

void gui_updateGUI(void);
void gui_renderGUI(void);

#endif
