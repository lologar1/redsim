#include "sim.h"

typedef struct Slice {
	u64 len;
	void *subarray;
} Slice;

usf_mutex *graphlock_;
usf_hashmap *graphmap_; /* Blockdata * -> Component * */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */

static usf_listptr *primed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (batched) (thread-safe) */
static usf_hashmap *chunkindices_; /* Parallel write (batched) (thread-safe) */

static usf_compatibility_int zerothpass(void *args);
static usf_compatibility_int firstpass(void *args);
static usf_compatibility_int secondpass(void *args);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphlock_ = malloc(sizeof(usf_mutex));
	usf_mtxinit(graphlock_, MTXINIT_PLAIN);
	graphmap_ = usf_newhm();
	graphchanged_ = 1;

	primed_ = usf_newlistptr_ts();
	next_ = usf_newhm_ts();
	chunkindices_ = usf_newhm_ts();
}

usf_compatibility_int sim_run(void *) {
	static f64 lastTime;
	lastTime = glfwGetTime();

	while (usf_atmflagtry(&simstop_, MEMORDER_RELAXED)) {
		if (!RSM_ENABLESIM) { /* Do not busy-wait if sim is off */
			usf_thrdsleep(RSM_SIMRETRY_TIMESPEC, NULL);
			continue; /* Check again after specified delay */
		}

		f64 elapsed;
		elapsed = glfwGetTime() - lastTime;
		if (elapsed < 1.0f/RSM_TICKRATE) { /* Running at capacity! */
			timespec tosleep = (timespec) {
				.tv_sec = (time_t) (1.0f/RSM_TICKRATE - elapsed),
				.tv_nsec = 1e9/RSM_TICKRATE - (elapsed - (i64) elapsed) * 1e9
			};

			usf_thrdsleep(&tosleep, NULL); /* Wait until next tick */
		}
		lastTime = glfwGetTime();

		/* Begin critical section */
		usf_mtxlock(graphlock_); /* Thread-safe lock */

		if (graphchanged_) {
			/* Prime all which exist; graphchanged_ forces update in the first pass. */
			u64 i;
			usf_data *entry;
			for (i = 0; (entry = usf_inthmnext(graphmap_, &i));) {
				Component *component;
				if ((component = entry[1].p)->id == 0) continue; /* Not participating in graph */
				usf_listptradd(primed_, component);
			}
		}

		usf_thread threadids[RSM_MAX_PROCESSORS];
		static u8 threadactive[RSM_MAX_PROCESSORS]; /* Init zero */
#define DISTRIBUTE(_ARRAY, _ARRAYLEN, _FUNCTION) \
	do { \
		u64 CHUNKLEN_, NCHUNK_; \
		for (CHUNKLEN_ = (_ARRAYLEN) / NPROCS, NCHUNK_ = 0; NCHUNK_ < NPROCS; NCHUNK_++) { \
			typeof(*_ARRAY) *SUBARRAY_; \
			SUBARRAY_ = _ARRAY + (NCHUNK_ * CHUNKLEN_); \
			\
			u64 SLICELEN_; /* If last slice, add truncated extra */ \
			SLICELEN_ = (NCHUNK_ == NPROCS - 1) \
				? CHUNKLEN_ + (_ARRAYLEN) % NPROCS \
				: CHUNKLEN_; \
			\
			if (SLICELEN_ == 0) continue; /* No job for this subchunk */ \
			\
			Slice *SLICE_; /* Free'd by child thread */ \
			SLICE_ = malloc(sizeof(Slice)); \
			SLICE_->len = SLICELEN_; \
			SLICE_->subarray = SUBARRAY_; \
			\
			usf_thrdcreate(&threadids[NCHUNK_], _FUNCTION, (void *) SLICE_); \
			threadactive[NCHUNK_] = 1; \
		} /* Wait for completion after */ \
		for (NCHUNK_ = 0; NCHUNK_ < NPROCS; NCHUNK_++) { /* Join if started */ \
			if (threadactive[NCHUNK_]) usf_thrdjoin(threadids[NCHUNK_], NULL); \
			threadactive[NCHUNK_] = 0; \
		} \
	} while(0);

		/* Pass 0 (visual) */
		if (RSM_VISUALSIM) DISTRIBUTE(primed_->array, primed_->size, zerothpass);

		/* Pass 1 */
		DISTRIBUTE(primed_->array, primed_->size, firstpass);
		graphchanged_ = 0; /* Consume; everything beyond here is as normal */

		/* Pass 2 */
		DISTRIBUTE(primed_->array, primed_->size, secondpass);

		/* Queue up next primed */
		primed_->size = 0; /* Clear primed_ (leave garbage data) */
		u64 i;
		usf_data *entry;
		for (i = 0; (entry = usf_inthmnext(next_, &i));)
			usf_listptradd(primed_, entry[0].p);
		usf_hmclear(next_);
#undef DISTRIBUTE

		usf_mtxunlock(graphlock_);
		/* End critical section */

		/* Pass 3 (visual) */
		if (RSM_VISUALSIM) {
			u64 i;
			usf_data *entry;
			for (i = 0; (entry = usf_inthmnext(chunkindices_, &i));)
				cu_asyncRemeshChunk(entry[0].u); /* Key is chunkindex */
			usf_hmclear(chunkindices_);
		}
	}

	/* Free static structures (graph free'd in client_terminate) */
	usf_freelistptr(primed_);
	usf_freeinthm(next_);
	usf_freeinthm(chunkindices_);

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}

static usf_compatibility_int zerothpass(void *args) {
	/* Reset all wirelines' visual state */

	/* Unpack arguments */
	Slice *slice;
	slice = (Slice *) args;

	u64 nprimed;
	nprimed = slice->len;
	Component **primed;
	primed = (Component **) slice->subarray;
	
	/* Begin resetting */
	u64 i, j;
	for (i = 0; i < nprimed; i++) {
		Visualdata *visualdata;
		visualdata = primed[i]->visualdata;

		for (j = 0; j < visualdata->nwires; j++)
			usf_atmmst(&visualdata->wires[j]->variant, 0, MEMORDER_RELEASE);
	}

	free(args);
	return 0;
}	

static usf_compatibility_int firstpass(void *args) {
	/* Update state of candidate components and keep primed if it has changed */

	/* Unpack arguments */
	Slice *slice = (Slice *) args;
	u64 nprimed;
	nprimed = slice->len;
	Component **primed;
	primed = slice->subarray;

	/* Begin critical section */
	u64 i;
	for (i = 0; i < nprimed; i++) {
		Component *component;
		component = primed[i];

		/* Find input status */
		u8 primary, secondary;
#define GETINPUT(_INPUT, _BUFFER) \
	do { \
		u8 *INPUTBUFFER_; \
		INPUTBUFFER_ = component->buffer[_BUFFER]; \
		\
		u16 NINPUT_; \
		for (_INPUT = NINPUT_ = 0; NINPUT_ < component->ninputs[_BUFFER]; NINPUT_++) \
			_INPUT = INPUTBUFFER_[NINPUT_] > _INPUT ? INPUTBUFFER_[NINPUT_] : _INPUT; /* Ternary max */ \
	} while (0);
		GETINPUT(primary, 0);
		GETINPUT(secondary, 1);
#undef GETINPUT

		/* Actualize state */
		u8 outstate, statechanged;
		outstate = component->state[0]; /* Previous output */
		switch (component->id) {
			case RSM_BLOCK_TRANSISTOR_ANALOG:
				statechanged =
					(component->state[0] = secondary >= primary ? 0 : primary - secondary)
					!= outstate;
				break;

			case RSM_BLOCK_TRANSISTOR_DIGITAL:
				statechanged =
					(component->state[0] = secondary > primary ? 0 : primary)
					!= outstate;
				break;

			case RSM_BLOCK_LATCH:
				statechanged =
					(secondary ? component->state[0] : primary)
					!= outstate;
				break;

			case RSM_BLOCK_INVERTER:
				statechanged =
					(primary ? 0 : RSM_MAX_SS)
					!= outstate;
				break;

			case RSM_BLOCK_BUFFER:
				u8 shiftby;
				shiftby = (1 << (component->variant >> 1)) - 1;

				memmove(&component->state[0], &component->state[1], shiftby); /* Shift buffer */
				component->state[shiftby] = primary; /* Push input */

				static const u8 zeroes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
				statechanged = memcmp(component->state, zeroes, shiftby + 1);

				break;

			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				statechanged =
					(component->state[0] = component->variant)
					!= outstate; /* Could change on init */
				break;

			case RSM_BLOCK_LIGHT_DIGITAL:
				component->state[0] = primary ? 1 : 0;
				statechanged = 0; /* Cannot output to other components; discard early */
				break;

			default:
				unreachable(); /* Optimize */
				fprintf(stderr, "Actualizing unknown component ID %"PRIu8" in simulation!\n", component->id);
				break;
		}

		if (!statechanged && !graphchanged_) /* Change did not affect other components; discard (unless force) */
			primed[i] = NULL;

		if (RSM_VISUALSIM && wf_iscomponent(component->id)) { /* Set visual state */
			if (component->state[0]) component->visualdata->blockdata->variant |= U8(1);
			else component->visualdata->blockdata->variant &= ~U8(1);
		}
	}
	/* End critical section */

	free(args);
	return 0;
}

static usf_compatibility_int secondpass(void *args) {
	/* Transmit output from primed components to their targets, and put them in next_ to,
	 * in turn, prime them. Also handle visual wire updates if visual mode is set. */

	/* Unpack arguments */
	Slice *slice;
	slice = (Slice *) args;

	u64 nprimed;
	nprimed = slice->len;
	Component **primed;
	primed = (Component **) slice->subarray;

	usf_listptr *candidates; /* Affected (to prime) candidates */
	candidates = usf_newlistptr();

	usf_listu64 *chunkcandidates; /* Collect chunk indices to remesh for each component */
	chunkcandidates = RSM_VISUALSIM ? usf_newlistu64() : NULL;

	/* Begin critical section */
	u64 i;
	for (i = 0; i < nprimed; i++) {
		Component *component;
		if ((component = primed[i]) == NULL) continue; /* Removed; doesn't affect others */

		usf_listptr *connections; /* Targets */
		connections = component->connections;

		Connection **targets; /* Faster access */
		targets = (Connection **) connections->array;

		u64 output; /* Output always in first index */
		output = component->state[0];

		/* Propagate */
		u64 j;
		for (j = 0; j < connections->size; j++) {
			Connection *connection;
			connection = targets[j];

			usf_listptradd(candidates, connection->component); /* Affected */

			u8 decay;
#define WRITETO(_LINKFLAG, _BUFFER) \
	if (connection->linkflags & (_LINKFLAG)) { \
		decay = connection->decay[_BUFFER]; \
		connection->component->buffer[_BUFFER][connection->index[_BUFFER]] \
			= decay > output ? 0 : output - decay; /* Cap at 0 */ \
	}
			WRITETO(RSM_LINKFLAG_WRITE_PRIMARY, 0);
			WRITETO(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef WRITETO
		}

		if (!RSM_VISUALSIM) continue; /* Only visual handling from now */

		Visualdata *visualdata;
		visualdata = component->visualdata;

		for (j = 0; j < visualdata->nwires; j++) { /* Update wireline */
			Blockdata *wire;
			wire = visualdata->wires[j];

			u8 wirestate, wiredecay, wirepower;
			wirestate = usf_atmmld(&wire->variant, MEMORDER_ACQUIRE); /* Old */
			wiredecay = visualdata->wiredecays[j];
			wirepower = wiredecay > output ? 0 : output - wiredecay; /* New */

			while (wirestate < wirepower) { /* Overwrites */
				if (usf_atmcmpxch_weak(
							&wire->variant, /* Target */
							&wirestate, /* Wasn't modified */
							wirepower, /* Hence replace */
							MEMORDER_ACQ_REL, MEMORDER_ACQUIRE))
					break;
				/* wirestate set to new value if modified, so recheck */
			}
		}

		/* Add chunk indices */
		if (RSM_VISUALSIM) for (j = 0; j < visualdata->chunkindices->size; j++)
			usf_listu64add(chunkcandidates, visualdata->chunkindices->array[j]);
	}

	/* Batch elimination of candidate duplicates */
	usf_mtxlock(next_->lock); /* Thread-safe lock */
	for (i = 0; i < candidates->size; i++) /* Add to set */
		usf_inthmput(next_, (u64) candidates->array[i], USFTRUE);
	usf_mtxunlock(next_->lock); /* Thread-safe unlock */

	/* Batch elimination of chunkindex duplicates */
	if (RSM_VISUALSIM) {
		usf_mtxlock(chunkindices_->lock); /* Thread-safe lock */
		for (i = 0; i < chunkcandidates->size; i++)
			usf_inthmput(chunkindices_, chunkcandidates->array[i], USFTRUE);
		usf_mtxunlock(chunkindices_->lock); /* Thread-safe unlock */
	}
	/* End critical section */

	usf_freelistptr(candidates);
	usf_freelistu64(chunkcandidates); /* NULL if not in visual mode */
	free(args);
	return 0;
}

void sim_freecomponent(void *c) {
	/* Frees a Component */

	if (c == NULL) return;

	Component *component;
	component = c;

	usf_freelistptrfunc(component->connections, free);
	free(component->buffer[0]);
	free(component->buffer[1]);

	if (component->visualdata) {
		usf_freelistu64(component->visualdata->chunkindices);
		free(component->visualdata->wires);
		free(component->visualdata->wiredecays);
		free(component->visualdata);
	}

	free(component);
}
