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
