#ifndef USFIO_H
#define USFIO_H

#include <stdio.h>
#include "usfdynarr.h"

char **usf_ftot(char *, char *, uint64_t *);
char *usf_ftos(char *, char *, uint64_t *);
void usf_printtxt(char **, uint64_t, FILE *);

#endif
