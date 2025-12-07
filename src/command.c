#include "command.h"

char cmdbuffer[RSM_MAX_COMMAND_LENGTH];
char cmdlog[RSM_MAX_COMMAND_LOG_LINES][RSM_MAX_COMMAND_LENGTH];
char (*logptr)[RSM_MAX_COMMAND_LENGTH] = cmdlog;
char *cmdptr = cmdbuffer;
usf_hashmap *cmdmap;
usf_hashmap *varmap;
usf_hashmap *aliasmap;

void command_help(uint32_t args, char *argv[]);
void command_config(uint32_t args, char *argv[]);
void command_set(uint32_t args, char *argv[]);
void command_setraw(uint32_t args, char *argv[]);
void command_teleport(uint32_t args, char *argv[]);

void cmd_init(void) {
	/* Initialize cmdmap and varmap for use in cmd execution */
#define ALIAS(CMDNAME, CMDPTR) usf_strhmput(cmdmap, CMDNAME, USFDATAP(CMDPTR))
	cmdmap = usf_newhm();
	ALIAS("config", command_config);
	ALIAS("help", command_help);
	ALIAS("set", command_set);
	ALIAS("setraw", command_setraw);
	ALIAS("teleport", command_teleport);
#undef ALIAS

#define ALIAS(VARNAME, VAR) usf_strhmput(varmap, VARNAME, USFDATAP(&VAR))
	varmap = usf_newhm();
	ALIAS("RSM_FLY_ACCELERATION", RSM_FLY_ACCELERATION);
	ALIAS("RSM_FLY_X_ACCELERATION", RSM_FLY_X_ACCELERATION);
	ALIAS("RSM_FLY_Y_ACCELERATION", RSM_FLY_Y_ACCELERATION);
	ALIAS("RSM_FLY_Z_ACCELERATION", RSM_FLY_Z_ACCELERATION);
	ALIAS("RSM_FLY_FRICTION", RSM_FLY_FRICTION);
	ALIAS("RSM_FLY_SPEED_CAP", RSM_FLY_SPEED_CAP);
	ALIAS("RSM_MOUSE_SENSITIVITY", RSM_MOUSE_SENSITIVITY);
	ALIAS("RSM_REACH", RSM_REACH);
	ALIAS("RSM_FOV", RSM_FOV);
	ALIAS("RSM_LOADING_DISTANCE", RSM_LOADING_DISTANCE);
	ALIAS("RSM_RENDER_DISTANCE", RSM_RENDER_DISTANCE);
	ALIAS("RSM_NEARPLANE", RSM_NEARPLANE);
	ALIAS("RSM_DEFAULT_GUI_SCALING_FACTOR", RSM_DEFAULT_GUI_SCALING_FACTOR);
	ALIAS("RSM_COMMAND_POS_X_PIXELS", RSM_COMMAND_POS_X_PIXELS);
	ALIAS("RSM_COMMAND_POS_Y_PIXELS", RSM_COMMAND_POS_Y_PIXELS);
	ALIAS("RSM_COMMAND_TEXT_SIZE", RSM_COMMAND_TEXT_SIZE);
#undef ALIAS

#define ALIAS(ALIASNAME, TRUENAME) usf_strhmput(aliasmap, ALIASNAME, USFDATAP(TRUENAME));
	aliasmap = usf_newhm(); /* Important: always include true name in aliases for absolute referencing */
	/* Commands */
	ALIAS("help", "help");
	ALIAS("config", "config");
	ALIAS("conf", "config");
	ALIAS("set", "set");
	ALIAS("setraw", "setraw");
	ALIAS("teleport", "teleport")
	ALIAS("tp", "teleport");

	/* Variables */
	ALIAS("RSM_FLY_ACCELERATION", "RSM_FLY_ACCELERATION");
	ALIAS("fly_accel", "RSM_FLY_ACCELERATION");
	ALIAS("flyspeed", "RSM_FLY_ACCELERATION");
	ALIAS("fs", "RSM_FLY_ACCELERATION");

	ALIAS("RSM_FLY_X_ACCELERATION", "RSM_FLY_X_ACCELERATION");
	ALIAS("fly_accel_x", "RSM_FLY_X_ACCELERATION");
	ALIAS("flyspeedx", "RSM_FLY_X_ACCELERATION");
	ALIAS("fsx", "RSM_FLY_X_ACCELERATION");

	ALIAS("RSM_FLY_Y_ACCELERATION", "RSM_FLY_Y_ACCELERATION");
	ALIAS("fly_accel_y", "RSM_FLY_Y_ACCELERATION");
	ALIAS("flyspeedy", "RSM_FLY_Y_ACCELERATION");
	ALIAS("fsy", "RSM_FLY_Y_ACCELERATION");

	ALIAS("RSM_FLY_Y_ACCELERATION", "RSM_FLY_Y_ACCELERATION");
	ALIAS("fly_accel_z", "RSM_FLY_Z_ACCELERATION");
	ALIAS("flyspeedz", "RSM_FLY_Z_ACCELERATION");
	ALIAS("fsz", "RSM_FLY_Z_ACCELERATION");

	ALIAS("RSM_FLY_FRICTION", "RSM_FLY_FRICTION");
	ALIAS("fly_friction", "RSM_FLY_FRICTION");
	ALIAS("flyfriction", "RSM_FLY_FRICTION");

	ALIAS("RSM_FLY_SPEED_CAP", "RSM_FLY_SPEED_CAP");
	ALIAS("fly_speedcap", "RSM_FLY_SPEED_CAP");
	ALIAS("flycap", "RSM_FLY_SPEED_CAP");

	ALIAS("RSM_MOUSE_SENSITIVITY", "RSM_MOUSE_SENSITIVITY");
	ALIAS("mouse_sensitivity", "RSM_MOUSE_SENSITIVITY");
	ALIAS("sensitivity", "RSM_MOUSE_SENSITIVITY");

	ALIAS("RSM_REACH", "RSM_REACH");
	ALIAS("reach", "RSM_REACH");

	ALIAS("RSM_FOV", "RSM_FOV");
	ALIAS("fov", "RSM_FOV");

	ALIAS("RSM_LOADING_DISTANCE", "RSM_LOADING_DISTANCE");
	ALIAS("loading_distance", "RSM_LOADING_DISTANCE");
	ALIAS("loadingdistance", "RSM_LOADING_DISTANCE");

	ALIAS("RSM_RENDER_DISTANCE", "RSM_RENDER_DISTANCE");
	ALIAS("render_distance", "RSM_RENDER_DISTANCE");
	ALIAS("renderdistance", "RSM_RENDER_DISTANCE");

	ALIAS("RSM_NEARPLANE", "RSM_NEARPLANE");
	ALIAS("nearplane", "RSM_NEARPLANE");

	ALIAS("RSM_DEFAULT_GUI_SCALING_FACTOR", "RSM_DEFAULT_GUI_SCALING_FACTOR");
	ALIAS("guisize", "RSM_DEFAULT_GUI_SCALING_FACTOR");
	ALIAS("guiscale", "RSM_DEFAULT_GUI_SCALING_FACTOR");

	ALIAS("RSM_COMMAND_POS_X_PIXELS", "RSM_COMMAND_POS_X_PIXELS");
	ALIAS("cmdposx", "RSM_COMMAND_POS_X_PIXELS");

	ALIAS("RSM_COMMAND_POS_Y_PIXELS", "RSM_COMMAND_POS_Y_PIXELS");
	ALIAS("cmdposy", "RSM_COMMAND_POS_Y_PIXELS");

	ALIAS("RSM_COMMAND_TEXT_SIZE", "RSM_COMMAND_TEXT_SIZE");
	ALIAS("textsize", "RSM_COMMAND_TEXT_SIZE");
	ALIAS("textscale", "RSM_COMMAND_TEXT_SIZE");
#undef ALIAS
}

void cmd_parseChar(char c) {
	/* Process a typed character into cmdbuffer */
	if ((unsigned char) c > 127) return;

	if (c == '\b') {
		if (cmdptr - cmdbuffer) cmdptr--; /* Decrement if not at beginning */
		*cmdptr = '\0';
		return;
	}

	if (c == '\n') {
		/* Execute if user entered a command */
		if (usf_sstartswith(cmdbuffer, RSM_COMMAND_PREFIX)) cmd_xeq(cmdbuffer);
		else cmd_log(cmdbuffer);

		*(cmdptr = cmdbuffer) = '\0';
		return;
	}

	if (cmdptr - cmdbuffer + 1 >= RSM_MAX_COMMAND_LENGTH) return;

	*cmdptr++ = c;
	*cmdptr = '\0';
}

void cmd_log(char *s) {
	/* Logs a string and rolls the log array */
	strcpy(*logptr, s);
	logptr = cmdlog + (uint64_t) (logptr - cmdlog + 1) % RSM_MAX_COMMAND_LOG_LINES;
}

void cmd_logf(char *format, ...) {
	/* Logs a formatted string to cmdlog */
	va_list args;
	char cmdout[RSM_MAX_COMMAND_LENGTH];

	va_start(args, format);
	vsnprintf(cmdout, sizeof(cmdout), format, args);
	va_end(args);

	cmd_log(cmdout);
}

void cmd_xeq(char *rawcmd) {
	/* Executes a redsim command from string */
	rawcmd += sizeof(RSM_COMMAND_PREFIX) - 1; /* Skip command prefix (don't include \0) */
	char **argv;
	uint64_t args;
	argv = usf_scsplit(rawcmd, ' ', &args);

	char *cmdname;
	if ((cmdname = usf_strhmget(aliasmap, argv[0]).p) == NULL) {
		cmd_logf("Unknown command %s. Type \"%shelp\" for help.\n", argv[0], RSM_COMMAND_PREFIX);
		goto end;
	}

	void (*cmdfuncptr)(uint32_t args, char *argv[]);
	if ((cmdfuncptr = usf_strhmget(cmdmap, cmdname).p) == NULL) {
		/* Implicit config command */
		if (args < 2) {
			cmd_logf("(Implicit) Syntax: %sconfig [var] [(float) value]\n", RSM_COMMAND_PREFIX);
			goto end;
		}

		float *rsmvar, value;
		rsmvar = usf_strhmget(varmap, cmdname).p;
		value = strtof(argv[1], NULL);
		*rsmvar = value;

		cmd_logf("(Implicit) Set %s to %f.\n", cmdname, value);
		goto end;
	}

	(*cmdfuncptr)(args, argv);

end:
	free(argv);
}

/* Command implementations */
void command_help(uint32_t args, char *argv[]) {
	/* Display helpful info */
	if (args < 2) {
		/* General help */
		cmd_logf("===== RSM HELP =====\n");
		cmd_logf("help: Display this menu. Provide argument for more information.\n");
		cmd_logf("config: Change runtime variables.\n");
		cmd_logf("set: Set current selection to block (default state).\n");
		cmd_logf("setraw: Set current selection to exact blockdata (64 bits).\n");
		cmd_logf("teleport: Sets current position in the world.\n");
		return;
	}

	/* Find pointer to command or variable and display help */
	char *unaliasedname;
	if ((unaliasedname = usf_strhmget(aliasmap, argv[1]).p) == NULL) {
		cmd_logf("No help entry for %s.\n", argv[1]);
		return;
	}

	void *unaliasedptr; /* If it is in the alias registry, it must exist */
	if ((unaliasedptr = usf_strhmget(cmdmap, unaliasedname).p) == NULL)
		unaliasedptr = usf_strhmget(varmap, unaliasedname).p;

	/* Commands */
	if (unaliasedptr == command_help) {
		cmd_logf("Syntax: help ?[command | variable]\n");
		cmd_logf("Displays the general help menu, or help on the argument.\n");
	} else if (unaliasedptr == command_config) {
		cmd_logf("Syntax: (config, conf) [variable] [(float) value]\n");
		cmd_logf("Set RSM runtime variables. It is possible to omit the command\n");
		cmd_logf("using (%s[variable [(float) value]) to implicitly call config.\n", RSM_COMMAND_PREFIX);
	} else if (unaliasedptr == command_set) {
		cmd_logf("Syntax: set [blockname]\n");
		cmd_logf("Set selection to default blockdata matching blockname.\n");
	} else if (unaliasedptr == command_setraw) {
		cmd_logf("Syntax: setraw [blockdata]\n");
		cmd_logf("Set selection to literal blockdata (64 bits) from decimal value.\n");
	} else if (unaliasedptr == command_teleport) {
		cmd_logf("Syntax: teleport [x] [y] [z]\n");
		cmd_logf("Teleports to the specified absolute coordinates.\n");
	}
	/* Variables */
	else if (unaliasedptr == &RSM_FLY_ACCELERATION) {
		cmd_logf("Fly acceleration in blocks/second in all directions.\n");
	} else if (unaliasedptr == &RSM_FLY_X_ACCELERATION) {
		cmd_logf("Fly acceleration bonus in the X direction in blocks/second.\n");
	} else if (unaliasedptr == &RSM_FLY_Y_ACCELERATION) {
		cmd_logf("Fly acceleration bonus in the Y direction in blocks/second.\n");
	} else if (unaliasedptr == &RSM_FLY_Z_ACCELERATION) {
		cmd_logf("Fly acceleration bonus in the Z direction in blocks/second.\n");
	} else if (unaliasedptr == &RSM_FLY_FRICTION) {
		cmd_logf("Natural fly deceleration expressed as %% of speed left/second.\n");
	} else if (unaliasedptr == &RSM_FLY_SPEED_CAP) {
		cmd_logf("Maximum fly speed in blocks/second.\n");
	} else if (unaliasedptr == &RSM_MOUSE_SENSITIVITY) {
		cmd_logf("Mouse sensitivity multiplier expressed as degrees/pixels.\n");
	} else if (unaliasedptr == &RSM_REACH) {
		cmd_logf("Reach in blocks as a ray cast from camera position.\n");
	} else if (unaliasedptr == &RSM_FOV) {
		cmd_logf("Field of view in degrees.\n");
	} else if (unaliasedptr == &RSM_LOADING_DISTANCE) {
		cmd_logf("Chunk loading distance in chunks.\n");
	} else if (unaliasedptr == &RSM_RENDER_DISTANCE) {
		cmd_logf("Render distance in chunks.\n");
	} else if (unaliasedptr == &RSM_NEARPLANE) {
		cmd_logf("Rendering near plane in blocks.\n");
	} else if (unaliasedptr == &RSM_DEFAULT_GUI_SCALING_FACTOR) {
		cmd_logf("GUI scaling factor when screen dimensions match default dimensions.\n");
	} else if (unaliasedptr == &RSM_COMMAND_POS_X_PIXELS) {
		cmd_logf("Command prompt x offset in pixels.\n");
	} else if (unaliasedptr == &RSM_COMMAND_POS_Y_PIXELS) {
		cmd_logf("Command prompt y offset in pixels.\n");
	} else if (unaliasedptr == &RSM_COMMAND_TEXT_SIZE) {
		cmd_logf("Command prompt text scaling factor.\n");
	}
}

void command_config(uint32_t args, char *argv[]) {
	/* Configure some RSM layout values */
	if (args < 3) {
		cmd_logf("Syntax: %sconfig [var] [(float) value]\n", RSM_COMMAND_PREFIX);
		return;
	}

	char *varname;
	if ((varname = usf_strhmget(aliasmap, argv[1]).p) == NULL) {
		cmd_logf("Unknown configuration variable %s.\n", argv[1]);
		return;
	}

	float *rsmvar, value;
	rsmvar = usf_strhmget(varmap, varname).p;
	value = strtof(argv[2], NULL);
	*rsmvar = value;

	cmd_logf("Set %s to %f.\n", varname, value);
}

void command_set(uint32_t args, char *argv[]) {

}

void command_setraw(uint32_t args, char *argv[]) {

}

void command_teleport(uint32_t args, char *argv[]) {
	/* Teleports to position */
	if (args < 4) {
		cmd_logf("Syntax: %steleport [x] [y] [z]\n", RSM_COMMAND_PREFIX);
		return;
	}

	position[0] = strtof(argv[1], NULL);
	position[1] = strtof(argv[2], NULL);
	position[2] = strtof(argv[3], NULL);
}
