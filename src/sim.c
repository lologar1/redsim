#include "sim.h"

typedef enum Threadflag : atomic_u32 {
	WAIT,
	UPDATE,
	PROPAGATE,
	WIRE,
	TERMINATE
} Threadflag;

typedef struct Slice {
	alignas(64) u64 len;
	usf_hashentry *array;
	Threadflag flag;
	atomic_flag wait;
} Slice;
static_assert(sizeof(Slice) == 64, "redsim: thread slice size is not 64");

usf_mutex ticklock_;
usf_cond tickstep_;
usf_hashmap *graphmap_; /* Protected by ticklock_ */
i32 graphchanged_; /* Protected by ticklock_ */
atomic_flag simstop_; /* Set on program termination */

static usf_hashmap *primed_;
static usf_hashmap *candidates_;
static usf_hashmap *candidatewires_;
static usf_hashmap *candidatechunks_;

static atomic_u64 running_;
static usf_thread threads[RSM_MAX_PROCESSORS];
static Slice jobs[RSM_MAX_PROCESSORS];

static usf_compatibility_int startworker(void *n);
static void distribute(usf_hashentry *array, u64 len, Threadflag threadflag);
static void tickwait(timespec *start, timespec *end);

static void update(u64 nproc);
static void propagate(u64 nproc);
static void wire(u64 nproc);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphmap_ = usf_newhm(); /* Protected by ticklock_ */
	usf_mtxinit(&ticklock_, MTXINIT_RECURSIVE);
	usf_cndinit(&tickstep_);

	primed_ = usf_newhm();
	candidates_ = usf_newhm();
	candidatewires_ = usf_newhm();
	candidatechunks_ = usf_newhm();

	u64 i;
	for (i = 1; i < NPROCS; i++) {
		usf_atmflagtry(&jobs[i].wait, MEMORDER_RELEASE); /* Set at first */
		usf_thrdcreate(&threads[i], startworker, (void *) i);
	}

	graphchanged_ = 1; /* Initial world parsing */
}

usf_compatibility_int sim_run(void *) {
	/* Starts the simulation */

	/* Profiling */
	u64 nsimticks;
	timespec simstart, simend, simelapsed;
	clock_gettime(CLOCK_MONOTONIC, &simstart);

	usf_hashiter iter;
	timespec start, end;
	for (nsimticks = 0; usf_atmflagtry(&simstop_, MEMORDER_RELAXED); nsimticks++) {
		usf_mtxlock(&ticklock_); /* Thread-safe lock */
		if (RSM_TICKRATE) clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

		if (graphchanged_) {
			usf_hmclear(primed_); /* Reset */
			for (usf_hmiterskim(graphmap_, &iter); usf_hmiternext(&iter);) {
				Component *component;
				if ((component = iter.entry->value.p)->id == 0) continue; /* Not participating in graph */
				usf_inthmput(primed_, (u64) component, USFTRUE);
			}
		}

		/* Begin critical section */
		/* Pass 1: candidatewires_ & candidatechunks_ */
		distribute(primed_->array, primed_->capacity, UPDATE); graphchanged_ = 0; /* Consume */

		/* Pass 1.5: visual */
		if (RSM_VISUALSIM) {
			distribute(candidatewires_->array, candidatewires_->capacity, WIRE);
			candidatewires_->size = 0; /* Cleared */
		}

		/* Pass 2: candidates_ */
		distribute(primed_->array, primed_->capacity, PROPAGATE);
		primed_->size = 0; USF_SWAP(primed_, candidates_); /* Cleared; recycle */
		/* End critical section */

		for (usf_hmiterskim(candidatechunks_, &iter); usf_hmiternext(&iter);) {
			cu_asyncRemeshChunk(iter.entry->key.u);
			memset(iter.entry, 0, sizeof(usf_hashentry));
		}
		candidatechunks_->size = 0; /* Cleared */

		if (!RSM_ENABLESIM) {
			while (!RSM_TICKSTEP && !RSM_ENABLESIM) usf_cndwait(&tickstep_, &ticklock_);
			RSM_TICKSTEP = USF_MAX(RSM_TICKSTEP - 1, 0); /* Consume */
		} else if (RSM_TICKRATE) {
			usf_mtxunlock(&ticklock_);
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
			tickwait(&start, &end);
			usf_mtxlock(&ticklock_);
		}

		usf_mtxunlock(&ticklock_); /* Thread-safe unlock */
	}

	/* Profiling */
	clock_gettime(CLOCK_MONOTONIC, &simend);
	usf_tsdiff(&simend, &simstart, &simelapsed);
	printf("Avg. nanoseconds per tick: %f\n", (1e9 * simelapsed.tv_sec + simelapsed.tv_nsec) / (f64) nsimticks);

	/* Simulation thread termination */
	u64 i;
	for (i = 1; i < NPROCS; i++) {
		jobs[i].flag = TERMINATE;
		usf_atmflagclr(&jobs[i].wait, MEMORDER_RELEASE);
		usf_thrdjoin(threads[i], NULL);
	}
	usf_freehm(primed_);
	usf_freehm(candidates_);
	usf_freehm(candidatewires_);
	usf_freehm(candidatechunks_);

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}

static void distribute(usf_hashentry *array, u64 len, Threadflag threadflag) {
	/* Distributes a graph evaluation job to threads */

	u64 chunklen = len / NPROCS;
	u64 chunkremainder = len % NPROCS;

	u64 nproc;
	for (nproc = 0; nproc < NPROCS; nproc++) {
		u64 slicelen = chunklen + (nproc == NPROCS - 1 ? chunkremainder : 0);
		if (slicelen == 0) break; /* No more */

		usf_hashentry *subarray = array + nproc * chunklen;
		jobs[nproc].len = slicelen;
		jobs[nproc].array = subarray;
		jobs[nproc].flag = threadflag;

		if (nproc) { /* Dispatch to other workers */
			usf_atmaddi(&running_, 1, MEMORDER_ACQ_REL);
			usf_atmflagclr(&jobs[nproc].wait, MEMORDER_RELEASE);
		}
	}

	/* Block 0 work */
	if (jobs[0].len) {
		switch (threadflag) {
			case WAIT: break; /* Nothing to do this tick */
			case UPDATE: update(0); break;
			case PROPAGATE: propagate(0); break;
			case WIRE: wire(0); break;
			case TERMINATE: return;
		}
	}

	while (usf_atmmld(&running_, MEMORDER_ACQUIRE));
}

static void tickwait(timespec *start, timespec *end) {
	/* Waits until end of current tick */
	timespec diff;
	usf_tsdiff(end, start, &diff);

	i64 pertick = 1000000000 / RSM_TICKRATE;
	i64 elapsed = diff.tv_sec * 1000000000 + diff.tv_nsec;
	if (elapsed >= pertick) return; /* At capacity */

	i64 towait = pertick - elapsed;
	diff.tv_sec = towait / 1000000000;
	diff.tv_nsec = towait % 1000000000;
	usf_thrdsleep(&diff, NULL);
}

static usf_compatibility_int startworker(void *n) {
	/* Starts a thread to evaluate graph slices */

	u64 nproc = (u64) n;

	for (;;) {
		while (usf_atmflagtry(&jobs[nproc].wait, MEMORDER_ACQ_REL));

		switch (jobs[nproc].flag) {
			case WAIT: unreachable(); break;
			case UPDATE: update(nproc); break;
			case PROPAGATE: propagate(nproc); break;
			case WIRE: wire(nproc); break;
			case TERMINATE: return 0;
		}

		jobs[nproc].flag = WAIT;
		usf_atmsubi(&running_, 1, MEMORDER_ACQ_REL);
	}
}

static void update(u64 nproc) {
	/* Update state of primed components.
	 * Visual mode: synchronize components' visual state.
	 * Visual mode: add connection wires to candidatewires.
	 * Visual mode: add chunks to toremesh. */

	u64 nprimed = jobs[nproc].len;
	usf_hashentry *primed = (usf_hashentry *) jobs[nproc].array;

	u64 i;
	for (i = 0; i < nprimed; i++) {
		if (primed[i].flag != USF_HASHMAP_KEY_INTEGER) continue;
		Component *component = primed[i].key.p;

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
					component->metadata |= RSM_SIMFLAG_REPRIME;
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

		if (!statechanged && !graphchanged_) {
			component->metadata |= RSM_SIMFLAG_DISCARD;
			continue; /* No effect later on */
		}
		outstate = component->state[0]; /* Now outstate is actualized output */

		Visualdata *visualdata = component->visualdata;
		if (!RSM_VISUALSIM) {
			/* Special case for lights; always update visually */
			if (component->id == RSM_BLOCK_LIGHT_DIGITAL) {
				if (outstate) visualdata->blockdata->variant |= 0x01;
				else visualdata->blockdata->variant &= 0xFE;
				u64 selfchunkindex = visualdata->chunkindices->array[0];
				usf_inthmput(candidatechunks_, selfchunkindex, USFTRUE);
			}
			continue;
		}

		/* Visual updates */
		Component **wires = visualdata->wires;
		u16 *wireindices = visualdata->wireindices;
		u8 *wiredecays = visualdata->wiredecays;

		/* Affect wireline */
		u64 j;
		for (j = 0; j < visualdata->nwires; j++) {
#define WRITE(_INBUF, _INDEX, _DECAY) \
	_INBUF[_INDEX] = _DECAY > outstate ? 0 : outstate - _DECAY;
			WRITE(wires[j]->buffer[0], wireindices[j], wiredecays[j]);
			usf_inthmput(candidatewires_, (u64) wires[j], USFTRUE);
		}

		/* Set visual state */
		if (wf_ispowerable(component->id)) {
			if (outstate) visualdata->blockdata->variant |= 0x01;
			else visualdata->blockdata->variant &= 0xFE;
		}

		/* To remesh */
		usf_listu64 *chunkindices = visualdata->chunkindices;
		for (j = 0; j < chunkindices->size; j++) /* Values copied to on shunt */
			usf_inthmput(candidatechunks_, chunkindices->array[j], USFTRUE);
	}
}

static void propagate(u64 nproc) {
	/* Propagate primed components' changes to next tick */

	u64 nprimed = jobs[nproc].len;
	usf_hashentry *primed = (usf_hashentry *) jobs[nproc].array;

	u64 i;
	for (i = 0; i < nprimed; i++) {
		if (primed[i].flag != USF_HASHMAP_KEY_INTEGER) continue;
		Component *component = primed[i].key.p;

		u32 metadata = component->metadata; component->metadata = 0; /* Consume */
		if (metadata & RSM_SIMFLAG_REPRIME) usf_inthmput(candidates_, (u64) component, USFTRUE);
		if (metadata & RSM_SIMFLAG_DISCARD) continue;

		usf_listptr *connections = component->connections;
		Connection **targets = (Connection **) connections->array;
		u8 outstate = component->state[0];

		u64 j;
		for (j = 0; j < connections->size; j++) {
			Connection *connection = targets[j];
			Component *target = connection->component;

#define WRITECOMP(_FLAG, _BUFFER) \
	if (connection->linkflags & _FLAG) \
		WRITE(target->buffer[_BUFFER], connection->index[_BUFFER], connection->decay[_BUFFER]);
			WRITECOMP(RSM_LINKFLAG_WRITE_PRIMARY, 0);
			WRITECOMP(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef WRITE
#undef WRITECOMP
			usf_inthmput(candidates_, (u64) target, USFTRUE);
		}

		memset(primed + i, 0, sizeof(usf_hashentry)); /* Clear */
	}
}

static void wire(u64 nproc) {
	/* Visual mode: update state of wires.
	 * Remeshing already queued by update if in visual mode. */

	u64 nwires = jobs[nproc].len;
	usf_hashentry *subarray = (usf_hashentry *) jobs[nproc].array;

	u64 i;
	for (i = 0; i < nwires; i++) {
		if (subarray[i].flag != USF_HASHMAP_KEY_INTEGER) continue;
		Component *wire = subarray[i].key.p;

		u8 *buffer, wirepower, n;
		for (buffer = wire->buffer[0], wirepower = n = 0; n < wire->inputs[0]->size; n++, buffer++) \
			wirepower = *buffer > wirepower ? *buffer : wirepower; /* Ternary max */
		wire->visualdata->blockdata->variant = wirepower; /* Set visual state */

		memset(subarray + i, 0, sizeof(usf_hashentry)); /* Clear */
	}
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
