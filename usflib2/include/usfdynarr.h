#ifndef USFDYNARR_H
#define USFDYNARR_H

#include "usfdata.h"
#include <stdlib.h>
#include <string.h>

typedef struct usf_dynarr {
	usf_data *array;
	uint64_t capacity;
	uint64_t size;
} usf_dynarr;

usf_dynarr *usf_newda(uint64_t);
usf_dynarr *usf_arrtodyn(usf_data *, uint64_t);
usf_data usf_daappend(usf_dynarr *, usf_data);
usf_data usf_daget(usf_dynarr *, uint64_t);
usf_data usf_daset(usf_dynarr *, uint64_t, usf_data);
void usf_freeda(usf_dynarr *);


#endif
