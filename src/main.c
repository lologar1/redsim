#include <stdio.h>

#include "renderer.h"
#include "rsmlayout.h"

int main() {
	/* Start redsim */
	printf("Starting RedsimV0.1\n");

	int returnCode;
	returnCode = render();

	printf("Process ended with code %d\n", returnCode);

	return returnCode;
}
