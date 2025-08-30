#include "usflib2.h"
#include <stdio.h>
#include <time.h>

double test(uint64_t);

int main() {
	uint64_t i, j;
	double avg;

	srand(time(NULL));

	printf("skptest: Time taken per middle element access in function of size\n");
	for (i = 2; i < 100000; i *= 2) {
		avg = 0;

		for (j = 0; j < 8; j++) {
			avg += test(i);
		}

		/* Time in millis */
		printf("(%lu, %f)\n", i, avg/8);
		fflush(stdout);
	}

	printf("skptest: End of test !\n");
	return 0;
}

double test(uint64_t cycles) {
	uint64_t i, nanos;
	struct timespec start, end;

	usf_skiplist *skp;
	skp = usf_skset(NULL, 0, USFNULL);

	for (i = 0; i < cycles; i++) {
		usf_skset(skp, i, USFDATAU(i));
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for (i = 0; i < 10000; i++)
		nanos = usf_skget(skp, cycles/2).u;
	clock_gettime(CLOCK_MONOTONIC, &end);

	nanos = (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);

	/* Cleanup */
	usf_freesk(skp);
	return nanos/10000;
}
