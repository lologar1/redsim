#include "usfqueue.h"

//Enqueue to FIFO queue
usf_queue *usf_enqueue(usf_queue *queue, usf_data d) {
	if (queue == NULL) {
		//Generate new queue
		queue = (usf_queue *) malloc(sizeof(usf_queue));

		if (queue == NULL) return NULL; //Memory allocation failure

		//Empty at first
		queue -> first = queue -> last = NULL;
	}

	usf_queuenode *append = (usf_queuenode *) malloc(sizeof(usf_queuenode));

	if (append == NULL) return NULL; //Memory allocation failure

	append -> data = d;
	append -> next = NULL; //Last in line

	if (queue -> last == NULL) //Was empty
		queue -> first = queue -> last = append;
	else { //Normal adding
		queue -> last -> next = append;
		queue -> last = append;
	}

	return queue;
}

//Dequeue from FIFO queue
usf_data usf_dequeue(usf_queue *queue) {
	usf_data d;
	usf_queuenode *old;

	if (queue == NULL || queue -> first == NULL)
		return (usf_data) { .p = NULL }; //No queue, or queue is empty

	old = queue -> first;
	d = old -> data; //Get data

	if ((queue -> first = old -> next) == NULL)
		queue -> last = NULL;

	free(old); //Destroy dequeued node
	return d;
}
