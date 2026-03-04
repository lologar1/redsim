#include "sim.h"

typedef struct Slice {
	u64 len;
	void *subarray;
} Slice;

usf_hashmap *graphmap_; /* Blockdata * -> Component * */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */

static usf_listptr *primed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (batched) (thread-safe) */
static usf_hashmap *chunkindices_; /* Parallel write (batched) (thread-safe) */

static usf_compatibility_int cleanwires(void *args);
static usf_compatibility_int firstpass(void *args);
static usf_compatibility_int secondpass(void *args);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphmap_ = usf_newhm_ts();
	primed_ = usf_newlistptr_ts();
	next_ = usf_newhm_ts();
	chunkindices_ = usf_newhm_ts();
	graphchanged_ = 1; /* Initial world parsing */
}

usf_compatibility_int sim_run(void *) {
	static f64 lastTime;
	lastTime = glfwGetTime();

	while (usf_atmflagtry(&simstop_, MEMORDER_RELAXED)) {
		if (!RSM_ENABLESIM) { /* Wait for restart if sim is off */
			usf_thrdsleep(RSM_SIMRETRY_TIMESPEC, NULL);
			continue; /* Check again after specified delay */
		}

		/* Periodic delay */
		f64 elapsed;
		elapsed = glfwGetTime() - lastTime;
		if (elapsed < 1.0f/RSM_TICKRATE) { /* Leeway left */
			timespec tosleep = (timespec) {
				.tv_sec = (time_t) (1.0f/RSM_TICKRATE - elapsed),
				.tv_nsec = 1e9/RSM_TICKRATE - floor(elapsed)*1e9
			};
			usf_thrdsleep(&tosleep, NULL); /* Wait until next tick */
		}
		lastTime = glfwGetTime();

		/* Begin critical section */
		usf_mtxlock(graphmap_->lock); /* Thread-safe lock */
		if (RSM_VISUALSIM) usf_mtxlock(chunkmap_->lock); /* Visual modifies world */

		u64 i; /* Hashmap iterators */
		usf_data *entry;

		if (graphchanged_) {
			/* Prime all which exist (update forced in first pass) */
			primed_->size = 0; /* Reset */
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
			Slice *SLICE_ = malloc(sizeof(Slice)); /* Free'd by child thread */ \
			SLICE_->len = SLICELEN_; \
			SLICE_->subarray = SUBARRAY_; \
			\
			usf_thrdcreate(&threadids[NCHUNK_], _FUNCTION, (void *) SLICE_); \
			threadactive[NCHUNK_] = 1; \
		} /* Wait for completion after */ \
		for (NCHUNK_ = 0; NCHUNK_ < NPROCS; NCHUNK_++) { /* Join if started */ \
			if (!threadactive[NCHUNK_]) continue; /* Wasn't distributed */ \
			usf_thrdjoin(threadids[NCHUNK_], NULL); \
			threadactive[NCHUNK_] = 0; \
		} \
	} while(0);

		/* Pass 1 */
		DISTRIBUTE(primed_->array, primed_->size, firstpass);
		graphchanged_ = 0; /* Consume; everything beyond here is as normal */

		/* Reset wirelines for all which emit a change */
		if (RSM_VISUALSIM) DISTRIBUTE(primed_->array, primed_->size, cleanwires);

		/* Pass 2 */
		DISTRIBUTE(primed_->array, primed_->size, secondpass);
#undef DISTRIBUTE

		/* Queue up next primed */
		primed_->size = 0; /* Clear primed_ (leave garbage data) */
		for (i = 0; (entry = usf_inthmnext(next_, &i));)
			usf_listptradd(primed_, entry[0].p);

		if (RSM_VISUALSIM) usf_mtxunlock(chunkmap_->lock);
		usf_mtxunlock(graphmap_->lock);
		/* End critical section */

		/* Remesh appropriate chunks */
		if (RSM_VISUALSIM) {
			for (i = 0; (entry = usf_inthmnext(chunkindices_, &i));)
				cu_asyncRemeshChunk(entry[0].u); /* Key is chunkindex */
			usf_hmclear(chunkindices_);
		}

		usf_hmclear(next_); /* Clear candidates for next time */
	}

	/* Free static structures (graph free'd in client_terminate) */
	usf_freelistptr(primed_);
	usf_freeinthm(next_);
	usf_freeinthm(chunkindices_);

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}

#define UNPACKSLICE(_ARGS, _LENVAR, _ARRVAR) \
	do { \
		Slice *SLICE_; \
		SLICE_ =  (Slice *) _ARGS; \
		_LENVAR = (u64) SLICE_->len; \
		_ARRVAR = (typeof(_ARRVAR)) SLICE_->subarray; \
	} while (0);
static usf_compatibility_int cleanwires(void *args) {
	/* Reset all primed components' wireline's visual state for later overwriting
	 * (Note: only called in visual mode) */

	u64 nprimed;
	Component **primed;
	UNPACKSLICE(args, nprimed, primed);
	
	u64 i, j;
	for (i = 0; i < nprimed; i++) {
		if (primed[i] == NULL) continue; /* Removed early */

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
	u64 nprimed;
	Component **primed;
	UNPACKSLICE(args, nprimed, primed);

	/* Begin critical section */
	u64 i;
	for (i = 0; i < nprimed; i++) {
		Component *component;
		component = primed[i];

		/* Find input status */
		u8 primary, secondary;
#define GETINPUT(_INPUT, _BUFFER) \
	do { \
		u8 *INPUTBUFFER_ = component->buffer[_BUFFER]; \
		u16 NINPUT_; /* Reset to zero, then begin search */ \
		for (_INPUT = NINPUT_ = 0; NINPUT_ < component->inputs[_BUFFER]->size; NINPUT_++) \
			_INPUT = INPUTBUFFER_[NINPUT_] > _INPUT ? INPUTBUFFER_[NINPUT_] : _INPUT; /* Ternary max */ \
	} while (0);
		GETINPUT(primary, 0);
		GETINPUT(secondary, 1);
#undef GETINPUT

		/* Actualize state */
		u8 outstate, statechanged, shiftby;
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
					(component->state[0] = secondary ? component->state[0] : primary)
					!= outstate;
				break;

			case RSM_BLOCK_INVERTER:
				statechanged =
					(component->state[0] = primary ? 0 : RSM_MAX_SS)
					!= outstate;
				break;

			case RSM_BLOCK_BUFFER:
				shiftby = (1 << (component->variant >> 1)) - 1;
				memmove(&component->state[0], &component->state[1], shiftby); /* Shift buffer */
				component->state[shiftby] = primary; /* Push input */
				statechanged = 1; /* Always keep preliminarily primed; selfprime or not in second pass */
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
	}
	/* End critical section */

	free(args);
	return 0;
}

static usf_compatibility_int secondpass(void *args) {
	/* Transmit output from primed components to their targets, and put them in next_ to,
	 * in turn, prime them. Also handle visual wire updates if visual mode is set. */

	/* Unpack arguments */
	u64 nprimed;
	Component **primed;
	UNPACKSLICE(args, nprimed, primed);

	/* Collect per-thread, then merge */
	usf_listptr *candidates;
	candidates = usf_newlistptr();
	usf_listu64 *toremesh;
	toremesh = RSM_VISUALSIM ? usf_newlistu64() : NULL;

	/* Begin critical section */
	u64 i;
	for (i = 0; i < nprimed; i++) {
		Component *component;
		if ((component = primed[i]) == NULL) continue; /* Removed early */

		usf_listptr *connections;
		connections = component->connections;
		Connection **targets; /* Affected components */
		targets = (Connection **) connections->array;

		u64 output; /* Output always in first index */
		output = component->state[0];

		/* Propagate */
		u64 j;
		for (j = 0; j < connections->size; j++) {
			Connection *connection;
			connection = targets[j];

#define WRITETO(_LINKFLAG, _BUFFER) \
	if (connection->linkflags & (_LINKFLAG)) { \
		u8 DECAY_ = connection->decay[_BUFFER]; \
		connection->component->buffer[_BUFFER][connection->index[_BUFFER]] \
			= DECAY_ > output ? 0 : output - DECAY_; /* Cap at 0 */ \
	}
			WRITETO(RSM_LINKFLAG_WRITE_PRIMARY, 0);
			WRITETO(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef WRITETO

			usf_listptradd(candidates, connection->component); /* May have changed */
		}

		if (!RSM_VISUALSIM) continue; /* Only visual handling from now */

		Visualdata *visualdata;
		visualdata = component->visualdata;

		/* Update wireline */
		for (j = 0; j < visualdata->nwires; j++) {
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

		/* Set visual state */
		if (RSM_VISUALSIM && wf_iscomponent(component->id)) {
			if (component->state[0]) component->visualdata->blockdata->variant |= U8(1);
			else component->visualdata->blockdata->variant &= ~U8(1);
		}

		/* Add chunk indices */
		if (RSM_VISUALSIM) for (j = 0; j < visualdata->chunkindices->size; j++)
			usf_listu64add(toremesh, visualdata->chunkindices->array[j]);
	}

	/* Batch elimination of candidate duplicates */
	usf_mtxlock(next_->lock); /* Thread-safe lock */
	for (i = 0; i < candidates->size; i++) /* Add to set */
		usf_inthmput(next_, (u64) candidates->array[i], USFTRUE);
	usf_mtxunlock(next_->lock); /* Thread-safe unlock */

	/* Batch elimination of chunkindex duplicates */
	if (RSM_VISUALSIM) {
		usf_mtxlock(chunkindices_->lock); /* Thread-safe lock */
		for (i = 0; i < toremesh->size; i++)
			usf_inthmput(chunkindices_, toremesh->array[i], USFTRUE);
		usf_mtxunlock(chunkindices_->lock); /* Thread-safe unlock */
	}
	/* End critical section */

	usf_freelistptr(candidates);
	usf_freelistu64(toremesh); /* NULL if not in visual mode */
	free(args);
	return 0;
}
#undef UNPACKSLICE

void sim_freecomponent(void *c) {
	/* Frees a Component */

	if (c == NULL) return;

	Component *component;
	component = c;

	free(component->buffer[0]);
	free(component->buffer[1]);
	usf_freeinthm(component->inputs[0]);
	usf_freeinthm(component->inputs[1]);
	usf_freelistptrfunc(component->connections, free);
	usf_freelistu64(component->visualdata->chunkindices);
	free(component->visualdata->wires);
	free(component->visualdata->wiredecays);
	free(component->visualdata);

	free(component);
}
