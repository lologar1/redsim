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

usf_mutex graphlock_; /* Avoid concurrent read/write to the component graph */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */

static Component *primed_; /* Parallel read-only */
static u64 nprimed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (thread-safe) */
static thread_local usf_listptr *candidates_; /* Sequential access (thread_local) */

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	usf_mtxinit(&graphlock_, MTXINIT_PLAIN);
	graphchanged_ = 1;

	primed_ = malloc(8192 * sizeof(Component)); /* Default size */
	nprimed_ = 0;
	next_ = usf_newhm_ts(); /* Thread-safe */
}

void sim_registerCoords(vec3 coords) {
	/* Integrates the block at these coordinates into the simulation graph.
	 * The simulation will update all components next tick as a side-effect.
	 * If the block at these coordinates is not a component, this function has no effect.
	 *
	 * Component inputs and outputs are currently hard-coded in this function, as
	 * there is no need of making a modular system as RSM only features a handful of
	 * primitives. */

}

void sim_registerPos(i64 x, i64 y, i64 z) {
	/* Position (integer) wrapper for sim_registerCoords */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	sim_registerCoords(coords);
}

void sim_removeCoords(vec3 coords) {
	/* Removes the block at these coordinates from the simulation graph.
	 * The block will also be removed from the world (set to 0) as a side-effect. */
}

void sim_removePos(i64 x, i64 y, i64 z) {
	/* Position (integer) wrapper for sim_removeCoords */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	sim_removeCoords(coords);
}
