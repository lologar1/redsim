#include <stdio.h>
#include "usfio.h"

char *textNoNewline[] =
	{ 	"First line\n",
		"Second line\n",
		"Third line no newline",
		"And followup\n",
		"We're done !\n", /* MANDATORY newline termination */
	};

char *textWellFormatted[] =
	{ 	"First line\n",
		"Second line\n",
		"Third line no newlineAnd followup\n",
		"We're done !\n",
	};

char **readout, *str;
uint64_t lines, i;

int main() {
	/* First write a file */
	FILE *toWrite = fopen("iotest_write.txt", "w");
	usf_printtxt(textNoNewline, 5, toWrite);
	fclose(toWrite);

	readout = usf_ftot("iotest_write.txt", "r", &lines);
	for (i = 0; i < lines; i++) {
		if (strcmp(readout[i], textWellFormatted[i]) == 0)
			printf("iotest: Success comparison for line %lu: %s\n", i, readout[i]);
		else {
			printf("%s", readout[i]);
			printf("SEP\n");
			printf("%s", textWellFormatted[i]);
			printf("SEP\n");
		}
	}

	if ((str = usf_ftos("iotest_write.txt", "r", &lines)) == NULL)
		printf("iotest: Error in usf_ftos while reading file\n");
	else
		printf("iotest: printing file in single string:\n%s", str);

	printf("iotest: End of test !\n");
}
