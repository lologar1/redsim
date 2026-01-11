#ifndef COMMAND_H
#define COMMAND_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <usfhashmap.h>
#include <usfskiplist.h>
#include "rsmlayout.h"
#include "usfstring.h"
#include "client.h"

extern int64_t ret_selection[6];
extern int64_t ret_positions[6];

extern char cmdbuffer[RSM_MAX_COMMAND_LENGTH];
extern char cmdlog[RSM_MAX_COMMAND_LOG_LINES][RSM_MAX_COMMAND_LENGTH];
extern char (*logptr)[RSM_MAX_COMMAND_LENGTH];
extern char *cmdptr;
extern usf_hashmap *cmdmap, *varmap, *aliasmap;

void cmd_init(void);
void cmd_parseChar(char c);
void cmd_log(char *s);
void cmd_logf(char *format, ...);
void cmd_xeq(char *rawcmd);

#endif
