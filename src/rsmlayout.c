#include "rsmlayout.h"

/* Default layout */
float RSM_FLY_X_ACCELERATION = 50.0f;
float RSM_FLY_Y_ACCELERATION = 50.0f;
float RSM_FLY_Z_ACCELERATION = 50.0f;
float RSM_FLY_FRICTION = 0.01f;
float RSM_FLY_SPEED_CAP = 23.0f;

//Keybinds defined in header file for fast switch statements

float RSM_MOUSE_SENSITIVITY = 0.1f;

float RSM_REACH = 3.5f;
float RSM_FOV = 120.0f;
unsigned int RENDER_DISTANCE = 8;
float RENDER_DISTANCE_NEAR = 0.1f;

/* Attention! Make sure to set constant sizes in rsmlayout.h otherwise compiler will complain */
char textureBasePath[] = "textures/";
char textureBlockPath[] = "blocks/";
char textureGuiPath[] = "gui/";
char textureBlockmapPath[] = "blockmap.txt";
char textureGuimapPath[] = "guimap.txt";
char meshFormatExtension[] = ".mesh";
char textureFormatExtension[] = ".png";
char boundingboxFormatExtension[] = ".boundingbox";
char layoutFormatExtension[] = ".layout";
