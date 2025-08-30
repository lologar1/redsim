#include "usfstring.h"

//Indirect string compare (wrapper)
int usf_indstrcmp(const void *a, const void *b) {
	char **x = (char **) a;
	char **y = (char **) b;

	if (*x == NULL) {
		if (*y == NULL)
			return 0;
		return -1;
	} else if (*y == NULL)
		return 1;

	return strcmp(* (char **) a, * (char **) b);
}

//Wrapper for length
int usf_indstrlen(const void *a, const void *b) {
	char **x = (char **) a;
	char **y = (char **) b;

	if (*x == NULL) {
		if (*y == NULL)
			return 0;
		return -1;
	} else if (*y == NULL)
		return 1;

	return strlen(* (char **) a) - strlen(* (char **) b);
}

//Test if a string startswith another; return substring
char *usf_sstartswith(char *base, char *prefix) {
	if (base == NULL || prefix == NULL) return NULL;

	while (*prefix) //Up to either \0 char
		if (*prefix++ != *base++) //Compare
			return NULL;
	return base;
}

//Test if a string ends with another
int usf_sendswith(char *base, char *suffix) {
	int offset;

	offset = strlen(base) - strlen(suffix);

	//Is bigger; doesn't endwith
	if (offset < 0) return 0;

	return !strcmp(base + offset, suffix);
}

//Count occurences of char in string
unsigned int usf_scount(char *str, char c) {
	unsigned int count = 0;

	while (*str) {
		if (*str == c)
			count++;
		str++;
	}

	return count;
}

int usf_sarrcontains(char **array, uint64_t len, char *string) {
	uint64_t i;

	if (string == NULL)
		return 0;

	for (i = 0; i < len; i++) {
		if (array[i] == NULL)
			continue;

		if (!strcmp(array[i], string))
			return 1;
	}

	return 0;
}

void usf_reversetxt(char **array, uint64_t len) {
	uint64_t i = 0, j = len - 1;
	char *temp;

	while (i < j) {
		//Swap
		temp = array[j];
		array[j] = array[i];
		array[i] = temp;

		//Offset pointers
		j--;
		i++;
	}
}

int usf_sreplace(char *s, char template, char replacement) {
    int n = 0;

    while((s = strchr(s, template)) != NULL) {
        *s++ = replacement;
        n++;
    }

    return n;
}
