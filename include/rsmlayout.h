#ifndef RSMLAYOUT_H
#define RSMLAYOUT_H

#include <GLFW/glfw3.h>
#include <stdint.h>

extern int RSM_KEY_ESCAPE;
extern int RSM_KEY_FORWARD;
extern int RSM_KEY_BACKWARD;
extern int RSM_KEY_RIGHT;
extern int RSM_KEY_LEFT;
extern int RSM_KEY_DOWN;
extern int RSM_KEY_UP;

extern float RSM_FLY_X_ACCELERATION;
extern float RSM_FLY_Y_ACCELERATION;
extern float RSM_FLY_Z_ACCELERATION;
extern float RSM_FLY_X_DECELERATION;
extern float RSM_FLY_Y_DECELERATION;
extern float RSM_FLY_Z_DECELERATION;
extern float RSM_FLY_X_CAP;
extern float RSM_FLY_Y_CAP;
extern float RSM_FLY_Z_CAP;

extern float RSM_MOUSE_SENSITIVITY;

extern float RSM_FPS;
extern float RSM_FOV;
extern unsigned int RENDER_DISTANCE;
extern float RENDER_DISTANCE_NEAR;
#define CHUNKSIZE 32
#define PLAYER_BOUNDINGBOX_RELATIVE_CORNER ((vec3) {-0.3f, -1.6f, -0.3f})
#define PLAYER_BOUNDINGBOX_DIMENSIONS ((vec3) {0.6f, 1.8f, 0.6f})

extern unsigned int RSM_TEXTURE_SIZE_PIXELS;
extern unsigned int RSM_MAX_BLOCKMESH_VERTICES;
extern unsigned int RSM_MAX_BLOCKMESH_INDICES;
extern unsigned int RSM_MAX_MESHDATA_NAME_LENGTH;
extern unsigned int RSM_MAX_SHADER_INFOLOG_LENGTH;

extern uint64_t RSM_BIT_COLLISION;
extern uint64_t RSM_BIT_CONDUCTIVE;

extern char textureBasePath[10];
extern char textureBlockPath[8];
extern char textureGuiPath[5];
extern char textureBlockmapPath[13];
extern char meshFormatExtension[6];
extern char textureFormatExtension[5];
extern char boundingboxFormatExtension[13];

typedef enum RSM_EXITCODE {
	RSM_EXIT_NORMAL = 0,
	RSM_EXIT_EXCBUF,
	RSM_EXIT_NOBLOCKMAP,
	RSM_EXIT_NOMESHDATA,
	RSM_EXIT_NOBOUNDINGBOXDATA,
	RSM_EXIT_NOTEXTURE,
	RSM_EXIT_BADTEXTURE,
	RSM_EXIT_BADVERTEXDATA,
	RSM_EXIT_BADBOUNDINGBOXDATA
} RSM_EXITCODE;

#endif
