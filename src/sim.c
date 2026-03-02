#include "sim.h"

/* RSM Parallel component simulation
 *
 * primed_ is an array of nprimed_ Component, distributed between multiple threads.
 * primed_ and nprimed_ are only ever read by these threads.
 *
 * Once a component is evaluated, its internal state (buffer) is atomically
 * compared and set according to its execution rules.
 *
 * The component is then added to the thread_local list candidates_ pending
 * synchronization with the global next_ hashmap (used as a hashset) of
 * components to evaluate next tick.
 *
 * After a thread has finished evaluation of its batch, it dumps its candidates
 * into the next_ hashmap (thread-safe) to eliminate duplicates.
 *
 * When parallel evaluation has finished executing, the next_ hashmap is transferred
 * to the primed_ array (and size to nprimed_), cleared, and everything restarts. */

usf_mutex *graphlock_; /* Avoid concurrent read/write to the component graph */
usf_hashmap *graphmap_; /* Map Blockdata * handle to corresponding Component * */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */

static usf_listptr *primed_; /* Parallel read-only */
static u64 nprimed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (thread-safe) */
static thread_local usf_listptr *candidates_; /* Sequential access (thread_local) */

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphlock_ = malloc(sizeof(usf_mutex));
	usf_mtxinit(graphlock_, MTXINIT_PLAIN);
	graphmap_ = usf_newhm();
	graphchanged_ = 1;

	primed_ = usf_newlistptr();
	nprimed_ = 0;
	next_ = usf_newhm_ts(); /* Thread-safe */
}

usf_compatibility_int sim_run(void *) {
	usf_thrdfence(MEMORDER_SEQ_CST); /* Synchronize with client init */

	static f64 lastTime;
	lastTime = glfwGetTime();

	while (usf_atmflagtry(&simstop_, MEMORDER_RELAXED)) {
		if (!RSM_ENABLESIM) { /* Do not busy-wait if sim is off */
			usf_thrdsleep(RSM_SIMSLEEP_TIMESPEC, NULL);
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
		usf_mtxlock(graphlock_);
		if (graphchanged_) {
			graphchanged_ = 0;

			/* Prime all next tick */
			u64 i, j;
			usf_data *entry;

			for (i = j = 0; (entry = usf_inthmnext(graphmap_, &i)); j++)
				usf_listptrset(primed_, j, entry[1].p);
			nprimed_ = graphmap_->size;
		}

		usf_mtxunlock(graphlock_);
	}

	fprintf(stderr, "Simulation stopped!\n");
	return 0;
}
