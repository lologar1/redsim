#include "sim.h"

typedef struct Slice {
	u64 len;
	Component **subarray;
} Slice;

usf_hashmap *graphmap_; /* Blockdata * -> Component * */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */
usf_mutex ticklock_;
usf_cond tickstep_;

static usf_listptr *primed_;
static usf_listptr *primedwires_;
static usf_hashmap *candidates_;
static usf_hashmap *candidatewires_;
static usf_hashmap *candidatechunks_;

static usf_compatibility_int update(void *args);
static usf_compatibility_int updatewires(void *args);
static void cleartoprimed(void *c);
static void cleartoprimedwires(void *c);
static void cleartoremesh(void *c);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphmap_ = usf_newhm_ts();
	usf_mtxinit(&ticklock_, MTXINIT_PLAIN);
	usf_cndinit(&tickstep_);

	primed_ = usf_newlistptr();
	primedwires_ = usf_newlistptr();
	candidates_ = usf_newhm_ts();
	candidatewires_ = usf_newhm_ts();
	candidatechunks_ = usf_newhm_ts();

	graphchanged_ = 1; /* Initial world parsing */
}

usf_compatibility_int sim_run(void *) {
	static f64 lastTime;
	lastTime = glfwGetTime();

	while (usf_atmflagtry(&simstop_, MEMORDER_RELAXED)) {
		if (!RSM_ENABLESIM) {
			usf_mtxlock(&ticklock_);
			while (!RSM_TICKSTEP && !RSM_ENABLESIM) usf_cndwait(&tickstep_, &ticklock_);
			RSM_TICKSTEP = USF_MAX(RSM_TICKSTEP - 1, 0); /* Consume */
			usf_mtxunlock(&ticklock_);
		}

		/* Periodic delay */
		f64 elapsed, diff;
		elapsed = glfwGetTime() - lastTime;
		if (elapsed < 1.0f/RSM_TICKRATE) { /* Leeway left */
			diff = 1.0f/RSM_TICKRATE - elapsed; /* To sleep */
			timespec tosleep = (timespec) {
				.tv_sec = (time_t) diff,
				.tv_nsec = 1000000000 * (diff - trunc(diff))
			};
			usf_thrdsleep(&tosleep, NULL); /* Wait until next tick */
		}
		lastTime = glfwGetTime();

		/* Begin critical section */
		usf_mtxlock(graphmap_->lock); /* Thread-safe lock */

		usf_hashiter iter;
		if (graphchanged_) { /* Reset and prime all */
			primedwires_->size = 0;
			primed_->size = 0;
			for (usf_hmiterskim(graphmap_, &iter); usf_hmiternext(&iter);) { /* Skim safe: lock acquired */
				Component *component;
				if ((component = iter.entry->value.p)->id == 0) continue; /* Not participating in graph */
				usf_listptradd(primed_, component);
			}
		}

		/* Update */
		usf_thread threadids[RSM_MAX_PROCESSORS];
		u8 threadactive[RSM_MAX_PROCESSORS] = {0};
#define DISTRIBUTE(_ARRAY, _ARRAYLEN, _FUNCTION) \
	do { \
		u64 CHUNKLEN_, NCHUNK_; \
		for (CHUNKLEN_ = (_ARRAYLEN) / NPROCS, NCHUNK_ = 0; NCHUNK_ < NPROCS; NCHUNK_++) { \
			Component **SUBARRAY_; \
			SUBARRAY_ = (Component **) _ARRAY + (NCHUNK_ * CHUNKLEN_); \
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
		DISTRIBUTE(primed_->array, primed_->size, update);
		graphchanged_ = 0; /* Consume */

		if (RSM_VISUALSIM) DISTRIBUTE(primedwires_->array, primedwires_->size, updatewires);
#undef DISTRIBUTE
		usf_mtxunlock(graphmap_->lock);
		/* End critical section (potential garbage data below is reset by graphchanged_ flag) */

		/* Consolidate */
		primed_->size = 0;
		usf_hmclearfunc(candidates_, cleartoprimed);

		if (!RSM_VISUALSIM) continue; /* Only visual mode updates beyond this point */
		primedwires_->size = 0;
		usf_hmclearfunc(candidatewires_, cleartoprimedwires);
		usf_hmclearfunc(candidatechunks_, cleartoremesh);
	}

	/* Free static structures (graph free'd in client_terminate) */
	usf_freelistptr(primed_);
	usf_freelistptr(primedwires_);
	usf_freehm(candidates_);
	usf_freehm(candidatewires_);
	usf_freehm(candidatechunks_);

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}

#define UNPACKSLICE(_ARGS, _LENVAR, _ARRVAR) \
	do { \
		Slice *SLICE_; \
		SLICE_ =  (Slice *) _ARGS; \
		_LENVAR = (u64) SLICE_->len; \
		_ARRVAR = (Component **) SLICE_->subarray; \
	} while (0);
static usf_compatibility_int update(void *args) {
	/* Update state of primed components and, on change, transmit it to its connections and mark them
	 * as candidates for next tick.
	 *
	 * Visual mode: synchronize components' visual state.
	 * Visual mode: add connection wires to candidatewires.
	 * Visual mode: add chunks to toremesh. */

	usf_hashmap *threadcandidates = usf_newhm();
	usf_hashmap *threadcandidatewires = RSM_VISUALSIM ? usf_newhm() : NULL;
	usf_hashmap *threadcandidatechunks = RSM_VISUALSIM ? usf_newhm() : NULL;

	u64 nprimed, i;
	Component **primed;
	UNPACKSLICE(args, nprimed, primed);
	for (i = 0; i < nprimed; i++) {
		Component *component;
		component = primed[i];

		/* Get inputs */
		u8 primary, secondary, n, *buffer, ninputs;
#define GETINPUT(_INPUT, _BUFFER) \
	buffer = component->buffer[_BUFFER]; \
	ninputs = component->inputs[_BUFFER]->size; \
	for (_INPUT = n = 0; n < ninputs; n++, buffer++) \
		_INPUT = *buffer > _INPUT ? *buffer : _INPUT; /* Ternary max */
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
				statechanged = component->state[0] != outstate;

				static u8 zeroes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
				if (memcmp(component->state, zeroes, shiftby + 1))
					usf_inthmput(threadcandidates, (u64) component, USFTRUE); /* Prime self if not empty */
				break;

			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				statechanged =
					(component->state[0] = component->variant)
					!= outstate; /* Could change on init */
				break;

			case RSM_BLOCK_LIGHT_DIGITAL:
				statechanged =
					(component->state[0] = primary ? 1 : 0)
					!= outstate;
				break;

			default:
				unreachable();
				fprintf(stderr, "Actualizing unknown component ID %"PRIu8" in simulation!\n", component->id);
				break;
		}

		if (!statechanged && !graphchanged_) continue; /* No effect from this point */
		outstate = component->state[0]; /* Now outstate is actualized output */

		/* Propagate and find candidates */
		usf_listptr *connections = component->connections;
		Connection **targets = (Connection **) connections->array;
		Visualdata *visualdata = component->visualdata;

		u64 j;
		for (j = 0; j < connections->size; j++) {
			Connection *connection = targets[j];
			Component *target = connection->component;

#define WRITE(_INBUF, _INDEX, _DECAY) \
	_INBUF[_INDEX] = _DECAY > outstate ? 0 : outstate - _DECAY;
#define WRITECOMP(_FLAG, _BUFFER) \
	if (connection->linkflags & _FLAG) \
		WRITE(target->buffer[_BUFFER], connection->index[_BUFFER], connection->decay[_BUFFER]);
			WRITECOMP(RSM_LINKFLAG_WRITE_PRIMARY, 0);
			WRITECOMP(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef WRITECOMP
			usf_inthmput(threadcandidates, (u64) target, USFTRUE);
		}

		if (!RSM_VISUALSIM) { /* If light, directly insert in global chunkindices */
			if (component->id == RSM_BLOCK_LIGHT_DIGITAL) {
				u64 selfchunkindex = visualdata->chunkindices->array[0];
				usf_inthmput(candidatechunks_, selfchunkindex, USFDATAU(selfchunkindex));
			}
			continue;
		}

		Component **wires = visualdata->wires;
		u16 *wireindices = visualdata->wireindices;
		u8 *wiredecays = visualdata->wiredecays;
		for (j = 0; j < visualdata->nwires; j++) {
			WRITE(wires[j]->buffer[0], wireindices[j], wiredecays[j]);
			usf_inthmput(threadcandidatewires, (u64) wires[j], USFTRUE);
#undef WRITE
		}

		/* Set visual state */
		if (wf_ispowerable(component->id)) {
			if (outstate) visualdata->blockdata->variant |= 0x01;
			else visualdata->blockdata->variant &= 0xFE;
		}

		/* To remesh */
		usf_listu64 *chunkindices = visualdata->chunkindices;
		for (j = 0; j < chunkindices->size; j++) /* Values copied to on shunt */
			usf_inthmput(threadcandidatechunks, chunkindices->array[j], USFTRUE);
	}

	/* Shunt to main structures */
	usf_hashiter iter;
#define SHUNT(_MAINMAP, _THREADMAP) \
	usf_mtxlock(_MAINMAP->lock); \
	for (usf_hmiterskim(_THREADMAP, &iter); usf_hmiternext(&iter);) \
		usf_inthmput(_MAINMAP, iter.entry->key.u, iter.entry->key); \
	usf_mtxunlock(_MAINMAP->lock);

	SHUNT(candidates_, threadcandidates);
	if (RSM_VISUALSIM) {
		SHUNT(candidatewires_, threadcandidatewires);
		SHUNT(candidatechunks_, threadcandidatechunks);
	}
#undef SHUNT
	/* End critical section */

	usf_freehm(threadcandidates);
	usf_freehm(threadcandidatewires);
	usf_freehm(threadcandidatechunks);
	free(args);
	return 0;
}

static usf_compatibility_int updatewires(void *args) {
	/* Visual mode: update state of wires.
	 * Remeshing already queued by update if in visual mode. */

	u64 nwires, i;
	Component **wires;
	UNPACKSLICE(args, nwires, wires);
	for (i = 0; i < nwires; i++) {
		Component *wire = wires[i];
		u8 *buffer = wire->buffer[0];

		u8 wirepower, n;
		for (wirepower = n = 0; n < wire->inputs[0]->size; n++, buffer++) \
			wirepower = *buffer > wirepower ? *buffer : wirepower; /* Ternary max */
		wire->visualdata->blockdata->variant = wirepower; /* Set visual state */
	}

	free(args);
	return 0;
}
#undef UNPACKSLICE

void cleartoprimed(void *c) {
	/* Registers a candidate to be primed */

	usf_listptradd(primed_, (Component *) c);
}

void cleartoprimedwires(void *c) {
	/* Registers a candidate wire to be primed */

	usf_listptradd(primedwires_, (Component *) c);
}

void cleartoremesh(void *c) {
	/* Remeshes a queued chunk index */

	printf("ASYNC %lu\n", (u64) c);
	cu_asyncRemeshChunk((u64) c);
}

void sim_freecomponent(void *c) {
	/* Frees a Component */

	if (c == NULL) return;

	Component *component;
	component = c;

	free(component->buffer[0]);
	free(component->buffer[1]);
	usf_freehm(component->inputs[0]);
	usf_freehm(component->inputs[1]);
	usf_freelistptrfunc(component->connections, free);
	usf_freelistu64(component->visualdata->chunkindices);
	free(component->visualdata->wires);
	free(component->visualdata->wiredecays);
	free(component->visualdata->wireindices);
	free(component->visualdata);

	free(component);
}
