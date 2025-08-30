#include "usfhashmap.h"

uint64_t usf_strhash(const char *str) {
    uint64_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

uint64_t usf_hash(uint64_t val) {
	val += 137;
	val ^= val >> 33;
	val *= 0xFF51AFD7ED558CCD; //Prime
	val ^= val >> 31;
	val *= 0xA635194A4D16E3CB; //Prime
	val ^= val >> 27;
	return val;
}

usf_hashmap *usf_newhm() {
	usf_hashmap *hashmap = malloc(sizeof(usf_hashmap));
	hashmap -> size = 0; //Empty at start
	hashmap -> capacity = USF_HASHMAP_DEFAULTSIZE;
    hashmap -> array = calloc(sizeof(usf_data **), USF_HASHMAP_DEFAULTSIZE);

	return hashmap;
}

usf_hashmap *usf_strhmput(usf_hashmap *hashmap, char *key, usf_data value) {
	uint64_t i, hash, cap;
	usf_data *entry;

	if (hashmap == NULL) //Initialize if non-existent
		hashmap = usf_newhm();

	if (key == NULL) return hashmap; //Cannot put at null

	if (hashmap -> size + 1 > hashmap -> capacity / USF_HASHMAP_RESIZE_MULTIPLIER) //Keep twice the capacity
		usf_resizestrhm(hashmap, hashmap -> capacity * USF_HASHMAP_RESIZE_MULTIPLIER);

	cap = hashmap -> capacity;
	i = usf_strhash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash]; //Element already present
		if (entry == NULL || entry[0].p == NULL || !strcmp((char *) entry[0].p, key)) {
			//Empty node, can put here (key : value)
			if (entry == NULL) {
				entry = hashmap -> array[hash] = calloc(sizeof(usf_data *), 2);
			}

			if (entry[0].p == NULL) {
				//Key (+1 for terminating \0)
				entry[0] = (usf_data) { .p = malloc(sizeof(char) * (strlen(key) + 1)) };
				strcpy(entry[0].p, key);
				hashmap -> size++;
			}

			entry[1] = value;
			break; //Successfully put
		}
	}

	return hashmap;
}

usf_hashmap *usf_inthmput(usf_hashmap *hashmap, uint64_t key, usf_data value) {
	uint64_t i, hash, cap;
	usf_data *entry;

	if (hashmap == NULL) //Init
		hashmap = usf_newhm();

	if (hashmap -> size + 1 > hashmap -> capacity / USF_HASHMAP_RESIZE_MULTIPLIER)
		usf_resizeinthm(hashmap, hashmap -> capacity * USF_HASHMAP_RESIZE_MULTIPLIER);

	cap = hashmap -> capacity;
	i = usf_hash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash];

		/* hashmap itself is used as a DEADBEEF pointer */
		if (entry == NULL || entry == (usf_data *) hashmap || entry[0].u == key) {
			//Empty, or overwriting
			if (entry == NULL || entry == (usf_data *) hashmap) { //Tuple (Key : Value)
				entry = hashmap -> array[hash] = calloc(sizeof(usf_data *), 2);
				entry[0] = USFDATAU(key);
				hashmap -> size++;
			}

			entry[1] = value;
			break;
		}
	}

	return hashmap;
}

usf_data usf_strhmget(usf_hashmap *hashmap, char *key) {
	uint64_t i, hash, cap;
	usf_data *entry;

	if (hashmap == NULL) return USFNULL;
	cap = hashmap -> capacity;

	i = usf_strhash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash];

		if (entry == NULL) //Not present
			return USFNULL;

		if (entry[0].p == NULL || strcmp(key, (char *) entry[0].p))
			continue; //Collision case

		return entry[1];
	}
}

usf_data usf_inthmget(usf_hashmap *hashmap, uint64_t key) {
	uint64_t i, hash, cap;
	usf_data *entry;

	if (hashmap == NULL) return USFNULL;
	cap = hashmap -> capacity;

	i = usf_hash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash];

		if (entry == NULL) //Nonexistent
			return USFNULL;

		if (entry == (usf_data *) hashmap || entry[0].u != key)
			continue; //Collision

		return entry[1];
	}
}

usf_data usf_strhmdel(usf_hashmap *hashmap, char *key) {
	uint64_t i, hash, cap;
	usf_data *entry, v;

	if (hashmap == NULL) return USFNULL;
	cap = hashmap -> capacity;

	i = usf_strhash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash];

		if (entry == NULL)
			return USFNULL;

		if (entry[0].p == NULL || strcmp(key, (char *) entry[0].p))
			continue;

		hashmap -> size--; //Decrement count
		v = entry[1]; //Get data
		free(entry[0].p);
		entry[0] = USFNULL; //Destroy key
		return v;
	}
}

usf_data usf_inthmdel(usf_hashmap *hashmap, uint64_t key) {
	uint64_t i, hash, cap;
	usf_data *entry, v;

	if (hashmap == NULL) return USFNULL;
	cap = hashmap -> capacity;

	i = usf_hash(key);

	for (;; i = usf_hash(i)) {
		hash = i % cap;

		entry = hashmap -> array[hash];

		if (entry == NULL)
			return USFNULL;

		if (entry == (usf_data *) hashmap || entry[0].u != key)
			continue; //Collision

		hashmap -> size--;
		v = entry[1]; //Data
		free(entry); //Destroy tuple
		hashmap -> array[hash] = (usf_data *) hashmap; //Dead node
		return v;
	}
}

void usf_resizeinthm(usf_hashmap *hashmap, uint64_t size) {
	uint64_t i, j, hash;
	usf_data **oldarray, **newarray;

	if (hashmap == NULL || hashmap -> capacity >= size) return;

	oldarray = hashmap -> array;
	newarray = calloc(sizeof(usf_data **), size);

	for (j = 0; j < hashmap -> capacity; j++) {
		if (oldarray[j] == NULL || oldarray[j] == (usf_data *) hashmap)
			continue; //Dead node or empty

		i = usf_hash(oldarray[j][0].u); //New key hash

		for (;; i = usf_hash(i)) {
			hash = i % size;

			if (newarray[hash] == NULL) {
				//Found new spot
				newarray[hash] = oldarray[j];
				break;
			}
		}
	}

	hashmap -> array = newarray;
	hashmap -> capacity = size;
	free(oldarray);
}

void usf_resizestrhm(usf_hashmap *hashmap, uint64_t size) {
	uint64_t i, j, hash;
	usf_data **oldarray, **newarray;

	if (hashmap == NULL || hashmap -> capacity >= size) return;

	oldarray = hashmap -> array;
	newarray = calloc(sizeof(usf_data **), size);

	for (j = 0; j < hashmap -> capacity; j++) {
		if (oldarray[j] == NULL || oldarray[j][0].p == NULL)
			continue; //Dead or uninitialized

		i = usf_strhash(oldarray[j][0].p); //Key

		for (;; i = usf_hash(i)) {
			hash = i % size;

			if (newarray[hash] == NULL) {
				newarray[hash] = oldarray[j];
				break;
			}
		}
	}

	hashmap -> array = newarray;
	hashmap -> capacity = size;
	free(oldarray); //Contents copied
}

void usf_freestrhm(usf_hashmap *hashmap) {
	uint64_t i;
	usf_data **array = hashmap -> array;
	usf_data *entry;

	for (i = 0; i < hashmap -> capacity; i++) {
		entry = array[i];
		if (entry == NULL) continue; //Uninitialized

		if (entry[0].p)
			free(entry[0].p); //Free key pointer

		free(entry); //Free key:val pair
	}

	free(array); //Final free of entire array
	free(hashmap);
}

void usf_freehm(usf_hashmap *hashmap) {
	uint64_t i;
	usf_data **array = hashmap -> array;
	usf_data *entry;

	for (i = 0; i < hashmap -> capacity; i++) {
		entry = array[i];
		if (entry == NULL || entry == (usf_data *) hashmap) continue; //No tuple

		free(entry); //Free key:val pair
	}

	free(array); //Final free of entire array
	free(hashmap);
}
