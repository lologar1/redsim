#ifndef USFHASHMAP_H
#define USFHASHMAP_H

#include "usfdata.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define USF_HASHMAP_DEFAULTSIZE 16
#define USF_HASHMAP_RESIZE_MULTIPLIER 2

uint64_t usf_strhash(const char *);
uint64_t usf_hash(uint64_t);

typedef struct usf_hashmap {
	usf_data **array; //Key : value
	uint64_t size;
	uint64_t capacity;
} usf_hashmap;

usf_hashmap *usf_newhm(void);
usf_hashmap *usf_strhmput(usf_hashmap *, char *, usf_data);
usf_hashmap *usf_inthmput(usf_hashmap *, uint64_t, usf_data);
usf_data usf_strhmget(usf_hashmap *, char *);
usf_data usf_inthmget(usf_hashmap *, uint64_t);
usf_data usf_strhmdel(usf_hashmap *, char *);
usf_data usf_inthmdel(usf_hashmap *, uint64_t);
void usf_resizestrhm(usf_hashmap *, uint64_t);
void usf_resizeinthm(usf_hashmap *, uint64_t);
void usf_freestrhm(usf_hashmap *);
void usf_freehm(usf_hashmap *);

#endif
