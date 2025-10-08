#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* This file serves to init stb_image, as this should only be done once.
 * Also, avoid recompiling the library each time the .c file which implements it gets recompiled */
