#ifndef COMMAND_H
#define COMMAND_H

#include <stdio.h>
#include <string.h>
#include "rsmlayout.h"

extern char cmdbuffer[RSM_MAX_COMMAND_LENGTH];
extern char cmdlog[RSM_MAX_COMMAND_LOG_LINES][RSM_MAX_COMMAND_LENGTH];
extern char (*logptr)[RSM_MAX_COMMAND_LENGTH];
extern char *cmdchar;

void cmd_parseChar(char c);
void cmd_log(char *s);

#endif
