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
extern char cmdbuffer_[RSM_MAX_COMMAND_LENGTH];
extern char cmdlog_[RSM_MAX_COMMAND_LOG_LINES][RSM_MAX_COMMAND_LENGTH];
extern char (*logptr_)[RSM_MAX_COMMAND_LENGTH];
extern char *cmdptr_;
extern usf_hashmap *cmdmap_;
extern usf_hashmap *varmap_;
extern usf_hashmap *aliasmap_;

void cmd_init(void);
void cmd_parseChar(char c);
void cmd_log(char *s);
void cmd_logf(char *format, ...);
void cmd_xeq(char *rawcmd);

#endif
