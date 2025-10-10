#include "rsmlayout.h"

/* Default layout */
float RSM_FLY_X_ACCELERATION = 30.0f;
float RSM_FLY_Y_ACCELERATION = 30.0f;
float RSM_FLY_Z_ACCELERATION = 30.0f;
float RSM_FLY_FRICTION = 0.01f;
float RSM_FLY_SPEED_CAP = 10.0f;

//Keybinds defined in header file for fast switch statements

float RSM_MOUSE_SENSITIVITY = 0.1f;

float RSM_FPS = 240.0f;
float RSM_FOV = 120.0f;
unsigned int RENDER_DISTANCE = 8;
float RENDER_DISTANCE_NEAR = 0.1f;
//define CHUNKSIZE
//define PLAYER_BOUNDINGBOX_RELATIVE_CORNER
//define PLAYER_BOUNDINGBOX_DIMENSIONS

//Bitmasks for block metadata defined in the header file

/* Attention! Make sure to set constant sizes in rsmlayout.h otherwise compiler will complain */
char textureBasePath[] = "textures/";
char textureBlockPath[] = "blocks/";
char textureGuiPath[] = "gui/";
char textureBlockmapPath[] = "blockmap.txt";
char meshFormatExtension[] = ".mesh";
char textureFormatExtension[] = ".png";
char boundingboxFormatExtension[] = ".boundingbox";
