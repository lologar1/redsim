#include "usfio.h"

char **usf_ftot(char *file, char *options, uint64_t *l) {
	/* Reads a file file with options options and returns number of lines in l.
	 * Returns NULL if an error occured, otherwise an array of pointers to each line. */

	FILE *f = fopen(file, options);

	usf_dynarr *text;
	usf_dynarr *line;

	unsigned char fbuffer[BUFSIZ]; /* File buffer */
	char *s, **txt; /* Final text arrays */
	uint64_t i, lines = 0;
	int64_t j;
	usf_data chr;
	size_t loaded; /* Number of bytes loaded into buffer this batch */

	if (f == NULL) return NULL; /* Failed to open file */

	text = usf_newda(16); /* All lines */
	line = usf_newda(16); /* Current line */

	j = 0; /* Character offset */

	for (;;) {
		loaded = fread(fbuffer, 1, BUFSIZ, f);

		if (ferror(f)) {
			/* Error while reading file, free and close */
			usf_freeda(line); /* Free line buffer */

			for (i = 0; i < lines; i++)
				free(usf_daget(text, i).p); /* Free already parsed lines */

			usf_freeda(text); /* Free text array */
			fclose(f);
			return NULL; /* NULL returned when error occurs */
		}

		for (i = 0; i < loaded; i++, j++) {
			/* i: index inside file buffer
			 * j: index inside line buffer */

			chr = USFDATAU(fbuffer[i]); /* Cast current char to USFDATA */
			usf_daset(line, j, chr); /* Set it in line buffer at index j */

			if (chr.u == '\n') { /* A line ends with \n */
				usf_daset(line, j + 1, USFDATAU('\0')); /* 0-terminator */

				/* Allocate space for this line */
				s = malloc(j + 2); /* Include 0-terminator and adjust for size */

				/* Copy to permanent array */
				for (j++; j >= 0; j--)
					s[j] = (char) line->array[j].u;

				usf_daset(text, lines++, USFDATAP(s)); //Add line
			}
		}

		if (feof(f)) break; /* No more bytes to load */
	}

	if (lines == 0) return NULL; /* No lines processed */

	if (l != NULL) *l = lines; /* Set number of lines */

	txt = malloc(sizeof(char *) * lines); /* Prepare string array */

	usf_freeda(line); /* Free line buffer as everything was allocated */

	for (i = 0; i < lines; i++)
		txt[i] = usf_daget(text, i).p; /* Transfer parsed lines */

	usf_freeda(text); /* Free text array */
	fclose(f); /* Close file */

	return txt;
}

void usf_printtxt(char **text, uint64_t len, FILE *stream) {
	/* Prints an array of strings of length len to stream stream */
	uint64_t i;

	for (i = 0; i < len; i++)
		fprintf(stream, "%s", text[i]);
}

char *usf_ftos(char *file, char *options, uint64_t *l) {
	/* Reads a file file with options options and returns contents
	 * as a single 0-terminated string, with length in l, NULL if an error occured */

	FILE *f = fopen(file, options);
	if (f == NULL) return NULL;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL; /* Error while seeking end of file */
	}

	uint64_t length = ftell(f);
	if (length <= 0) {
		fclose(f);
		return NULL;
	}

	rewind(f); /* Go back to start */

	char *str = malloc(length + 1); /* Adjust for terminator */

	if (fread(str, 1, length, f) == 0) {
		fclose(f);
		free(str);
		return NULL;
	}

	str[length] = '\0'; /* Terminate */

	fclose(f);

	if (l != NULL) *l = length;

	return str;
}
