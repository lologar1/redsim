#include "command.h"

char cmdbuffer[RSM_MAX_COMMAND_LENGTH];
char cmdlog[RSM_MAX_COMMAND_LOG_LINES][RSM_MAX_COMMAND_LENGTH];
char (*logptr)[RSM_MAX_COMMAND_LENGTH] = cmdlog;
char *cmdchar = cmdbuffer;

void cmd_parseChar(char c) {
	/* Process a typed character into cmdbuffer */
	if ((unsigned char) c > 127) return;

	if (c == '\b') {
		if (cmdchar - cmdbuffer) cmdchar--; /* Decrement if not at beginning */
		*cmdchar = '\0';
		return;
	}

	if (c == '\n') {
		cmd_log(cmdbuffer);
		*(cmdchar = cmdbuffer) = '\0';
		return;
	}

	if (cmdchar - cmdbuffer + 1 >= RSM_MAX_COMMAND_LENGTH) return;

	*cmdchar++ = c;
	*cmdchar = '\0';
}

void cmd_log(char *s) {
	/* Logs a string and rolls the log array */
	strcpy(*logptr, s);
	logptr = cmdlog + (uint64_t) (logptr - cmdlog + 1) % RSM_MAX_COMMAND_LOG_LINES;
}
