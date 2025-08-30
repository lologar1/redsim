#ifndef USFSTRING_H
#define USFSTRING_H

#include <string.h>
#include <stdint.h>

int usf_indstrcmp(const void *, const void *);
int usf_indstrlen(const void *, const void *);
char *usf_sstartswith(char *, char *);
int usf_sendswith(char *, char*);
unsigned int usf_scount(char *, char);
int usf_sarrcontains(char **, uint64_t, char *);
void usf_reversetxt(char **, uint64_t);
int usf_sreplace(char *, char, char);

#endif
