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
usf_hashmap *graphmap_; /* Maps XYZ (21 bits) to corresponding Component */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_i32 simstop_; /* Set on program termination */

static Component *primed_; /* Parallel read-only */
static u64 nprimed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (thread-safe) */
static thread_local usf_listptr *candidates_; /* Sequential access (thread_local) */

static void findAffected(vec3 coords, usf_hashmap *affected);

void sim_init(void) {
	/* Initialize simulation structures and start sim process */

	graphlock_ = malloc(sizeof(usf_mutex));
	usf_mtxinit(graphlock_, MTXINIT_PLAIN);
	graphmap_ = usf_newhm();
	graphchanged_ = 1;
	simstop_ = 0;

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

	usf_mtxlock(graphlock_);

	usf_hashmap *affected; /* Components affected by this action */
	affected = usf_newhm();

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	switch(block->id) {
		case RSM_BLOCK_AIR:
			break; /* Nothing to do */

		case RSM_BLOCK_SILICON: /* Support blocks */
		case RSM_BLOCK_GLASS:
		case RSM_BLOCK_DIODE:
		case RSM_BLOCK_TARGET:
		case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
		case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
		case RSM_BLOCK_LIGHT_DIGITAL:
			f32 a, b, c;
			vec3 adjacent;
			for (a = -1; a < 2; a++) for (b = -1; b < 2; b++) for (c = -1; c < 2; c++) {
				glm_vec3_copy(coords, adjacent);
				adjacent[0] += a; adjacent[1] += b; adjacent[2] += c;
				findAffected(adjacent, affected); /* In case of soft or hard powering, look at all neighbors */
			}

		default: /* Either wire or component */
			findAffected(coords, affected);
			break;
	}

	u64 i;
	usf_data *entry;
	for (i = 0; (entry = usf_inthmnext(affected, &i));) {
	}

	usf_freeinthm(affected);

	graphchanged_ = 1;
	usf_mtxunlock(graphlock_);
}

void sim_registerPos(i64 x, i64 y, i64 z) {
	/* Position (integer) wrapper for sim_registerCoords */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	sim_registerCoords(coords);
}

void sim_removeCoords(vec3 coords) {
	/* Removes the block at these coordinates from the simulation graph.
	 * The block will also be removed from the world (set to 0) as a side-effect. */

	usf_mtxlock(graphlock_);
	graphchanged_ = 1;

	usf_mtxunlock(graphlock_);
}

void sim_removePos(i64 x, i64 y, i64 z) {
	/* Position (integer) wrapper for sim_removeCoords */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	sim_removeCoords(coords);
}

usf_compatibility_int sim_run(void *) {

	return 0;
}

static void findAffected(vec3 coords, usf_hashmap *affected) {

}
