#include "rsmlayout.h"
#include <GLFW/glfw3.h>

/* Default layout */
int RSM_KEY_ESCAPE = GLFW_KEY_ESCAPE;
int RSM_KEY_FORWARD = GLFW_KEY_W;
int RSM_KEY_BACKWARD = GLFW_KEY_S;
int RSM_KEY_RIGHT = GLFW_KEY_D;
int RSM_KEY_LEFT = GLFW_KEY_A;
int RSM_KEY_DOWN = GLFW_KEY_LEFT_SHIFT;
int RSM_KEY_UP = GLFW_KEY_SPACE;

float RSM_FLY_X_ACCELERATION = 125.0f;
float RSM_FLY_Y_ACCELERATION = 130.0f;
float RSM_FLY_Z_ACCELERATION = 125.0f;
float RSM_FLY_X_DECELERATION = 100.0f;
float RSM_FLY_Y_DECELERATION = 100.0f;
float RSM_FLY_Z_DECELERATION = 100.0f;
float RSM_FLY_X_CAP = 13.0f;
float RSM_FLY_Y_CAP = 10.0f;
float RSM_FLY_Z_CAP = 13.0f;

float RSM_MOUSE_SENSITIVITY = 0.1f;

float RSM_FPS = 240.0f;
float RSM_FOV = 120.0f;
unsigned int RENDER_DISTANCE = 8;
float RENDER_DISTANCE_NEAR = 0.1f;

unsigned int RSM_TEXTURE_SIZE_PIXELS = 32;
unsigned int RSM_MAX_BLOCKMESH_VERTICES = 10000;
unsigned int RSM_MAX_BLOCKMESH_INDICES = 30000;
unsigned int RSM_MAX_MESHDATA_NAME_LENGTH = 512;

/* Attention! Make sure to set constant sizes in rsmlayout.h otherwise compiler will complain */
char textureBasePath[] = "textures/";
char textureBlockPath[] = "blocks/";
char textureGuiPath[] = "gui/";
char textureBlockmapPath[] = "blockmap.txt";
char meshFormatExtension[] = ".mesh";
char textureFormatExtension[] = ".png";
