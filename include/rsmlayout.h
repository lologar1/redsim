#ifndef RSMLAYOUT_H
#define RSMLAYOUT_H

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

extern unsigned int RSM_TEXTURE_SIZE_PIXELS;
extern unsigned int RSM_MAX_BLOCKMESH_VERTICES;
extern unsigned int RSM_MAX_BLOCKMESH_INDICES;
extern unsigned int RSM_MAX_MESHDATA_NAME_LENGTH;

extern char textureBasePath[10];
extern char textureBlockPath[8];
extern char textureGuiPath[5];
extern char textureBlockmapPath[13];
extern char meshFormatExtension[6];
extern char textureFormatExtension[5];

typedef enum RSM_EXITCODE {
	RSM_EXIT_NORMAL = 0,
	RSM_EXIT_EXCBUF,
	RSM_EXIT_NOBLOCKMAP,
	RSM_EXIT_NOMESHDATA,
	RSM_EXIT_NOTEXTURE,
	RSM_EXIT_BADTEXTURE,
	RSM_EXIT_BADVERTEXDATA
} RSM_EXITCODE;

#endif
