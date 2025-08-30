#include "usfskiplist.h"

/*
A skiplist simulates an infinitely (64 bit addr) large array whose elements can
be set, deleted (set to 0, although this frees memory), or accessed
(returns literal 0 when not present).

All operations take O(log n) time. Index 0 may not be deleted, but it can be set to 0.
*/

usf_skiplist *usf_skset(usf_skiplist *skiplist, uint64_t i, usf_data data) {
	int j;
	usf_skipnode *node, *next, **ptrs;
	usf_skipnode *position[USF_SKIPLIST_HEADSIZE];

	if (skiplist == NULL) { //Make skiplist
		//Allocate skiplist
		skiplist = malloc(sizeof(usf_skiplist));
		skiplist -> size = 1; //Base node

		node = malloc(sizeof(usf_skipnode)); //Head

		//Allocate head pointers
		node -> nextnodes = calloc(sizeof(usf_skipnode **), USF_SKIPLIST_HEADSIZE);

		//Set base data
		node -> index = 0;
		node -> data = USFNULL;

		skiplist -> head = node;
	}

	//Insert data at index i in skiplist
	node = skiplist -> head; //Start

	for (j = USF_SKIPLIST_HEADSIZE - 1; j >= 0; j--) {
		next = node -> nextnodes[j];

		while (next && next -> index <= i) {
			node = next;
			next = node -> nextnodes[j];
		}

		//At max node in this stage
		position[j] = node;
	}

	if (node -> index == i) {
		//Already present
		node -> data = data;
		return skiplist;
	}

	//Add and link
	node = malloc(sizeof(usf_skipnode));
	node -> index = i;
	node -> data = data;

	for (j = 1; j < USF_SKIPLIST_HEADSIZE; j++)
		if (rand() & 1) break; //Probabilistic upkeep

	ptrs = malloc(sizeof(usf_skipnode *) * j);

	for (j--; j >= 0; j--) {
		ptrs[j] = position[j] -> nextnodes[j]; //Link to next
		position[j] -> nextnodes[j] = node; //Link prev to this
	}

	node -> nextnodes = ptrs;
	skiplist -> size++; //Add element
	return skiplist;
}

usf_data usf_skget(usf_skiplist *skiplist, uint64_t i) {
	int j;
	usf_skipnode *node, *next;

	node = skiplist -> head;

	for (j = USF_SKIPLIST_HEADSIZE - 1; j >= 0; j--) {
		next = node -> nextnodes[j];

		while (next && next -> index <= i) {
			/* Loop unrolling: two iterations per while cycle */
			node = next;
			if ((next = node -> nextnodes[j]) == NULL || next -> index > i) break;

			node = next;
			next = node -> nextnodes[j];
		}

		if (node -> index == i) break;
	}

	if (node -> index != i) return USFNULL;

	return node -> data;
}

usf_data usf_skdel(usf_skiplist *skiplist, uint64_t i) {
	int j;
	usf_skipnode *node, *next;
	usf_data v;

	if (i == 0) {
		/* Cannot delete 0, although setting to 0 has
		virtually the same effect */
		v = usf_skget(skiplist, 0);
		usf_skset(skiplist, 0, USFNULL);
		return v;
	}

	node = skiplist -> head;

	for (j = USF_SKIPLIST_HEADSIZE - 1; j >= 0; j--) {
		next = node -> nextnodes[j];

		while (next && next -> index < i) {
			node = next;
			next = node -> nextnodes[j];
		}

		//Unlink
		if (next != NULL && next -> index == i)
			node -> nextnodes[j] = next -> nextnodes[j];
	}

	//Set node to the one we want to delete
	node = next;

	//Did not find
	if (node == NULL || node -> index != i) return USFNULL;

	v = node -> data; //Get data

	free(node -> nextnodes); //Free pointers
	free(node); //Free node itself
	skiplist -> size--; //Decrement count

	return v; //User takes care of data
}

void usf_freesk(usf_skiplist *skiplist) {
	usf_skipnode *node, *r;

	if (skiplist == NULL) return;

	//Free by walking level 0 linked list
	node = skiplist -> head;

	while(node != NULL) {
		r = node;
		node = r -> nextnodes[0]; //Next

		free(r -> nextnodes);
		free(r);
	}

	free(skiplist);
}
