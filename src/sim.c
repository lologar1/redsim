#include "sim.h"

/* RSM Component graph simulation
 * Step 0
 * Check if graphchanged_ is set. If so, add every component to primed_.
 *
 * Step 1 (first pass)
 * Distribute primed_ between threads. Each component sets its output signal to its target buffer at the
 * proper index. If visual mode is on, also atomically max its signal with wires' Blockdata * variant.
 * Each target is added to candidates_ (thread_local). When iteration has finished, next_'s lock is
 * acquired and all candidates are shunted to the next_ set.
 * If visual mode is on, targets also get modified in the world and chunk indices are collected in the
 * chunkcandidates_ list, which is then batch dumped to chunkindices_.
 * Return to main process.
 *
 * Step 2 (second pass)
 * Distribute next_ between threads. Each component finds the max of its proper input buffers (depending on
 * set metadata flags) and updates its state accordingly. The component is then added to candidates_ if a
 * state change occurred. When iteration has finished, primed_'s lock is acquired and all candidates are
 * batch-shunted to it (there are no duplicates since these come from a set).
 * Return to main process.
 *
 * Step 3 (optional pass)
 * If visual mode is on, start a remeshing process for every chunk index in chunkindices_.
 *
 * */

usf_mutex *graphlock_;
usf_hashmap *graphmap_; /* Blockdata * -> Component * */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */

static usf_listptr *primed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (batched) (thread-safe) */
static thread_local usf_listptr *candidates_; /* Sequential access (thread_local) */
static usf_hashmap *chunkindices_; /* Parallel write (batched) (thread-safe) */
static thread_local usf_listu64 *chunkcandidates_; /* Sequential access (thread_local) */

static usf_compatibility_int firstpass(void *args);
static usf_compatibility_int secondpass(void *args);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphlock_ = malloc(sizeof(usf_mutex));
	usf_mtxinit(graphlock_, MTXINIT_PLAIN);
	graphmap_ = usf_newhm();
	graphchanged_ = 1;

	primed_ = usf_newlistptr();
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

		/* Critical section */
		usf_mtxlock(graphlock_); /* Thread-safe lock */

		/* Pass 0 */
		if (graphchanged_) {
			graphchanged_ = 0;

			/* Prime all next tick */
			primed_->size = 0; /* Leave garbage data */
			u64 i;
			usf_data *entry;
			for (i = 0; (entry = usf_inthmnext(graphmap_, &i));) {
				Component *primed;
				usf_listptradd(primed_, (primed = entry[1].p)); /* Add all Component * to primed */
#define CLEARBUFFER(_INDEX) \
	usf_listu8set(primed->buffer[_INDEX], primed->buffer[_INDEX]->size - 1, 0); /* Guarantee size */ \
	memset(primed->buffer[_INDEX]->array, 0, primed->buffer[_INDEX]->size); /* Reset */
				CLEARBUFFER(0); CLEARBUFFER(1);
#undef CLEARBUFFER
			}
		}

		usf_hmclear(next_);
		usf_hmclear(chunkindices_);
		u64 nchunk, chunksz;
		usf_thread threadids[RSM_MAX_PROCESSORS];

#define DISTRIBUTE(_ARRAY, _ARRAYSZ, _FUNCTION) \
		for (chunksz = (_ARRAYSZ) / NPROCS, nchunk = 0; nchunk < NPROCS; nchunk++) { \
			typeof(_ARRAY) slice; /* Subarray start */ \
			slice = _ARRAY + (nchunk * chunksz); \
			\
			u64 SLICESZ_; /* If last slice, add truncated extra */ \
			SLICESZ_ = (nchunk == NPROCS - 1) \
				? chunksz + (_ARRAYSZ) % NPROCS \
				: chunksz; \
			\
			u8 *ARGUMENTS_; /* Free'd by child thread */ \
			ARGUMENTS_ = malloc(sizeof(typeof(_ARRAY)) + sizeof(u64)); \
			memcpy(ARGUMENTS_, &slice, sizeof(typeof(_ARRAY))); \
			memcpy(ARGUMENTS_ + sizeof(typeof(_ARRAY)), &SLICESZ_, sizeof(u64)); \
			\
			usf_thrdcreate(&threadids[nchunk], _FUNCTION, ARGUMENTS_); \
		} /* Wait for completion after */ \
		for (nchunk = 0; nchunk < NPROCS; nchunk++) usf_thrdjoin(threadids[nchunk], NULL);

		/* Pass 1 */
		DISTRIBUTE(primed_->array, primed_->size, firstpass);

		/* Pass 2 */
		primed_->size = 0; /* Clear primed_ (leave garbage data) */
		DISTRIBUTE(next_->array, next_->size, secondpass);
#undef DISTRIBUTE

		usf_mtxunlock(graphlock_);
		/* End critical section */

		/* Pass 3 (visual) */
		if (RSM_VISUALSIM) {
			u64 i;
			usf_data *entry;
			for (i = 0; (entry = usf_inthmnext(chunkindices_, &i));)
				cu_asyncRemeshChunk(entry[0].u); /* Key is chunkindex */
		}
	}

	/* Free static structures (graph free'd in client_terminate) */
	usf_freelistptr(primed_);
	usf_freeinthm(next_);
	usf_freeinthm(chunkindices_);

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}

static usf_compatibility_int firstpass(void *args) {
	/* Transmit output from primed components to their targets, and put them in next_ to,
	 * in turn, prime them. Also handle visual stuff if mode is set. */
	return 0;

	/* Unpack arguments */
	Component **primed;
	primed = (Component **) args;

	u64 nprimed;
	nprimed = (u64) ((u8 *) args + sizeof(Component **));

	u64 i;
	for (i = 0; i < nprimed; i++) {
		Component *component;
		component = primed[i]; /* Component to propagate */
	}

	free(args);
	return 0;
}

static usf_compatibility_int secondpass(void *args) {
	/* Update state of candidate components and finally prime them if it has changed. */

	free(args);
	return 0;
}
