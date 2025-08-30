#include "usfdynarr.h"

usf_dynarr *usf_newda(uint64_t size) {
	usf_dynarr *da = malloc(sizeof(usf_dynarr));
	usf_data *array = malloc(sizeof(usf_data) * size);

	if (da == NULL || array == NULL) return NULL;

	da -> array = array;
	da -> size = da -> capacity = size;

	return da;
}

usf_dynarr *usf_arrtodyn(usf_data *arr, uint64_t size) {
	usf_dynarr *da;
	usf_data *array;

	da = usf_newda(size);
	if (da == NULL) return NULL;

	array = da -> array;

	if (arr) {
		memcpy(array, arr, size); //Copy to array
		free(arr); //Free if non null (can be used as constructor)
	}

	return da;
}

usf_data usf_daappend(usf_dynarr *da, usf_data data) {
	return usf_daset(da, da -> size, data);
}

usf_data usf_daget(usf_dynarr *da, uint64_t index) {
	if (da == NULL || index >= da -> size) return USFNULL;
	return da -> array[index];
}

usf_data usf_daset(usf_dynarr *da, uint64_t index, usf_data data) {
	uint64_t newsize;

	if (da == NULL) return USFNULL;

	if (da -> capacity <= index) {
		//Resize
		newsize = index * 2 + 1; //Prevent 0 error

		da -> array = realloc(da -> array, sizeof(usf_data) * newsize);
		da -> capacity = newsize;
	}

	if (da -> size <= index)
		da -> size = index + 1;

	da -> array[index] = data;

	return data;
}

void usf_freeda(usf_dynarr *da) {
	/* Frees a dynamic array as if it were malloc'ed and free() was called */
	free(da -> array);
	free(da);
}
