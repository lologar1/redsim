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

#define FORORTHO(_BODY) \
	i32 _DX, _DY, _DZ; \
	Rotation _ROT; \
	_ROT = WEST; _DX = 1; _DY = 0; _DZ = 0; _BODY; \
	_ROT = EAST; _DX = -1; _DY = 0; _DZ = 0; _BODY; \
	_ROT = UP; _DX = 0; _DY = 1; _DZ = 0; _BODY; \
	_ROT = DOWN; _DX = 0; _DY = -1; _DZ = 0; _BODY; \
	_ROT = NORTH; _DX = 0; _DY = 0; _DZ = 1; _BODY; \
	_ROT = SOUTH; _DX = 0; _DY = 0; _DZ = -1; _BODY; \

#define FORCUBE(_BODY) \
	i32 _DX, _DY, _DZ; \
	for (_DX = -1; _DX < 3; _DX++) \
	for (_DY = -1; _DY < 3; _DY++) \
	for (_DZ = -1; _DZ < 3; _DZ++) { \
		_BODY; \
	}

usf_mutex *graphlock_; /* Avoid concurrent read/write to the component graph */
usf_hashmap *graphmap_; /* Maps XYZ (21 bits) to corresponding Component */
i32 graphchanged_; /* Set when the component graph is modified (will re-prime everything) */
atomic_flag simstop_; /* Set on program termination */

static usf_listptr *primed_; /* Parallel read-only */
static u64 nprimed_; /* Parallel read-only */
static usf_hashmap *next_; /* Parallel write (thread-safe) */
static thread_local usf_listptr *candidates_; /* Sequential access (thread_local) */

static void registercomponent(u64 blockindex);
static void componentconnect(vec3 coords, usf_hashmap *restrict affected, Rotation from,
		usf_hashmap *restrict wires);
static void wirefill(vec3 coords, usf_hashmap *restrict affected, usf_hashmap *restrict seen,
		Rotation from, u64 decay, usf_hashmap *restrict wires);
static void freeconnection(void *c);

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

void sim_registerCoords(vec3 coords) {
	/* This function must be called after each world modification to ensure graph consistency.
	 *
	 * If the block is a component, only it will be registered (or re-registered)
	 * Else, all orthogonally adjacent blocks are checked: if they are components, they will be registered.
	 * If they are hard-powered or wires, all components affected by them will be registered.
	 * This process ensures no desync between the graph and the world, as a block only has incidence on
	 * its direct neighbors, never more. */

	usf_mtxlock(graphlock_); /* Thread-safe lock */

	usf_hashmap *affected; /* Components affected by this action */
	affected = usf_newhm();

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	if (block->id == RSM_BLOCK_AIR) { /* Block is destroyed; remove corresponding Component */
		Component *component;
		if ((component = usf_inthmdel(graphmap_, TOBLOCKINDEX(coords)).p)) {
			usf_freelistptrfunc(component->connections, freeconnection);
			free(component->wireline); /* Is base of all allocations within */
			free(component);
		}
	}

	vec3 adjacent;
#define _COMPONENTCONNECT \
	glm_vec3_copy(coords, adjacent); \
	adjacent[0] += _DX; adjacent[1] += _DY; adjacent[2] += _DZ; \
	componentconnect(adjacent, affected, NONE, NULL); /* In case of powering, look at all neighbors. */
	FORCUBE(_COMPONENTCONNECT);
#undef _COMPONENTCONNECT

	/* Iterate over affected and re-register them to account for any possible changes */
	u64 i;
	usf_data *entry;
	for (i = 0; (entry = usf_inthmnext(affected, &i));)
		registercomponent(entry[0].u);

	graphchanged_ = 1; /* Prime all next tick */
	usf_freeinthm(affected); /* Delete temporary hashmap */

	usf_mtxunlock(graphlock_); /* Thread-safe unlock */
}

void sim_registerPos(i64 x, i64 y, i64 z) {
	/* Position (integer) wrapper for sim_registerCoords */

	vec3 coords = {(f32) x, (f32) y, (f32) z};
	sim_registerCoords(coords);
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

/* Parsing the 3D world into the component graph goes like this:
 * First iterate in a FORCUBE loop and feed that into componentconnect to find all components affected
 * by the change. We only care about the resulting hashmaps' keys (blockindices), not floodflags.
 *
 * Then, for each component which may have its connections changed, begin a componentconnect for its hard-
 * powered output, and in the case of the inverter, on its sides only of it is a wire or another component.
 * The resulting hashmap contains all components which may interact with it. We then filter it by only
 * processing the components which the wire wires to (RSM_FLOODFILL_COMPONENT_WRITE), upon which we build a
 * Connection object for that component.
 *
 * After all is said and done, the Component's connections are completely rebuilt. */

static void registercomponent(u64 blockindex) {
	/* Update Component entry in graph for these coordinates and perform
	 * a connection handshake with all other affected components */

	vec3 coords = UNPACK21CAST64(blockindex);

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	usf_hashmap *wires; /* Hold wire visual data */
	wires = usf_newhm();

	/* All components except inverter output from their front (hardpower).
	 * The inverter hardpowers upwards and softpowers all around */
	if (!rsm_isComponent(block->id)) {
		fprintf(stderr, "Registering non-component at %f %f %f, skipping!\n", coords[0], coords[1], coords[2]);
		return;
	}

	/* TODO REMOVE DEBUG */
	cmd_logf("RegID %lu\n", block->id);

	usf_hashmap *affected; /* Potential handshakes */
	affected = usf_newhm();

	vec3 entrypoint; /* Hard-powering direction */

	glm_vec3_copy(coords, entrypoint);
	if (block->id == RSM_BLOCK_INVERTER) {
		entrypoint[1]++; /* Hard powers up */
		printf("Inverter\n");
		componentconnect(entrypoint, affected, UP, wires);
		printf("Done visit hardpower \n");

		Blockdata *neighbor; /* Soft powering */
#define _CONNECT \
	glm_vec3_copy(coords, entrypoint); \
	entrypoint[0] += _DX; entrypoint[1] += _DY; entrypoint[2] += _DZ; \
	neighbor = cu_coordsToBlock(coords, NULL); \
	if (rsm_isComponent(neighbor->id) || neighbor->id == RSM_BLOCK_WIRE) \
		componentconnect(entrypoint, affected, _ROT, wires);
		FORORTHO(_CONNECT);
#undef _CONNECT
	} else if (block->rotation) { /* No rotation: no output (digital lamp) */
		switch(block->rotation) {
			case NORTH: entrypoint[2]++; break;
			case WEST: entrypoint[0]++; break;
			case SOUTH: entrypoint[2]--; break;
			case EAST: entrypoint[0]--; break;
			default:
				fprintf(stderr, "Illegal component rotation while creating graph at %"PRIu8
						"\n", block->rotation);
				break;
		}
		componentconnect(entrypoint, affected, block->rotation, wires);
	}

	/* TODO REMOVE DEBUG */
	cmd_logf("Wsz/Afsz %lu %lu\n", wires->size, affected->size);

	Component *component; /* Retrieve or init Component representation */
	if ((component = usf_inthmget(graphmap_, blockindex).p) == NULL)
		usf_inthmput(graphmap_, blockindex, USFDATAP(component = calloc(1, sizeof(Component))));

	component->id = block->id; /* Component ID */
	switch(block->id) { /* Component flags */
		case RSM_BLOCK_LIGHT_DIGITAL: component->metadata |= RSM_FLAG_FORCEVISUAL; break;
	}

	usf_listptr *connections; /* Destroy old connections if they exist */
	if ((connections = component->connections)) usf_freelistptrfunc(connections, freeconnection);
	connections = component->connections = usf_newlistptr();

	/* Connections */
	u64 i;
	usf_data *entry;
	for (i = 0; (entry = usf_inthmnext(affected, &i));) {
		u64 floodflags, pdecay, sdecay;
		floodflags = entry[1].u & LOW32MASK;
		pdecay = entry[1].u >> 48;
		sdecay = (entry[1].u >> 32) & LOW16MASK;

		if (!(floodflags & ~RSM_FLOODFILL_COMPONENT_READ)) continue; /* If wire only reads; not affected */

		vec3 blockcoords = UNPACK21CAST64(entry[0].u);

		Connection *connection;
		connection = malloc(sizeof(Connection));

		if ((connection->component = usf_inthmget(graphmap_, entry[0].u).p) == NULL) {
			fprintf(stderr, "Illegal connection to nonexistent component for ID %"PRIu16" at block positions"
					"%f, %f, %f.\n", block->id, blockcoords[0], blockcoords[1], blockcoords[2]);
			free(connection);
			break;
		}

		connection->flags = floodflags;
		connection->pdecay = pdecay;
		connection->sdecay = sdecay;

		Visualdata *visualdata;
		visualdata = malloc(sizeof(Visualdata));

		u64 chunkindex;
		visualdata->component = cu_coordsToBlock(blockcoords, &chunkindex);
		visualdata->chunkindex = chunkindex;

		connection->visualdata = visualdata;
		usf_listptradd(connections, connection);

		cmd_logf("CMP! ID/DECAY %lu %lu+%lu\n", connection->component->id, connection->pdecay, connection->sdecay);
	}

	/* Wireline */
	u64 nwires, *chunkindices, *ssoffsets;
	Blockdata **wiredata;
	Wireline *wireline;

	nwires = wires->size; /* Number of wire blocks */

	free(component->wireline); /* Calloc'd; safe */
	u8 *buffer; /* Allocate wireline as a single heap block */
	buffer = malloc(sizeof(Wireline)
			+ nwires * sizeof(u64) /* chunkindices */
			+ nwires * sizeof(u64) /* ssoffsets */
			+ nwires * sizeof(Blockdata *)); /* wiredata */

	/* Allocate */
	component->wireline = wireline = (Wireline *) buffer; buffer += sizeof(Wireline);
	chunkindices = (u64 *) buffer; buffer += nwires * sizeof(u64);
	ssoffsets = (u64 *) buffer; buffer += nwires * sizeof(u64);
	wiredata = (Blockdata **) buffer;

	/* Init */
	wireline->nwires = nwires; wireline->chunkindices = chunkindices;
	wireline->wiredata = wiredata; wireline->ssoffsets = ssoffsets;

	/* Populate */
	u64 j;
	for (i = j = 0; (entry = usf_inthmnext(wires, &i)); j++) {
		vec3 wirecoords = UNPACK21CAST64(entry[0].u);

		u64 chunkindex;
		wiredata[j] = cu_coordsToBlock(wirecoords, &chunkindex);
		chunkindices[j] = chunkindex;
		ssoffsets[j] = entry[1].u; /* decay */

		cmd_logf("WIRE! Decay %lu @ %f %f %f\n", entry[1].u, wirecoords[0], wirecoords[1], wirecoords[2]);
	}

	usf_freeinthm(affected);
	usf_freeinthm(wires);
}

static void componentconnect(vec3 coords, usf_hashmap *restrict affected,
		Rotation from, usf_hashmap *restrict wires) {
	/* Entry point for a wirefill. If the block in question is a wire or component, wirefill is directly called.
	 * If it is a conductor, all nearby wires are called, as the block is hard-powered. */

	Blockdata *block;
	if ((block = cu_coordsToBlock(coords, NULL))->id == RSM_BLOCK_AIR)
		return; /* Early exit to avoid creating hashmap */

	usf_hashmap *seen; /* Visited coords for floodfill */
	seen = usf_newhm();

	if (rsm_isComponent(block->id) || block->id == RSM_BLOCK_WIRE)
		wirefill(coords, affected, seen, from, 1, wires); /* Component or wires passed directly */
	else if (block->variant & RSM_BIT_CONDUCTOR) {
		vec3 adjacent;
#define _FILLCOORDS \
	glm_vec3_copy(coords, adjacent); \
	adjacent[0] += _DX; adjacent[1] += _DY; adjacent[2] += _DZ; \
	wirefill(adjacent, affected, seen, _ROT, 1, wires);
		FORORTHO(_FILLCOORDS);
#undef _FILLCOORDS
	}

	usf_freeinthm(seen); /* Delete temporary hashmap */
}

static void wirefill(vec3 coords, usf_hashmap *restrict affected, usf_hashmap *restrict seen,
		Rotation from, u64 decay, usf_hashmap *restrict wires) {
	/* Finds all components this wire interacts with and adds them to affected.
	 * If the wire writes to a component, the RSM_FLOODFILL_COMPONENT_WRITE_* flags are set.
	 * If the wire instead reads from the component, the RSM_FLOODFILL_COMPONENT_READ flag is set.
	 * Note that all flags may be set at once. */

	if (decay > 255) return; /* 0 guaranteed */

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	if (rsm_isComponent(block->id)) { /* End condition: found a component */
		u64 floodflags;
		floodflags = 0; /* Init empty */

		switch (block->id) { /* Differentiate component types */
			/* Side-inputtable, front output */
			case RSM_BLOCK_TRANSISTOR_ANALOG:
			case RSM_BLOCK_TRANSISTOR_DIGITAL:
			case RSM_BLOCK_LATCH:
				if ((block->rotation & 1) == (from & 1))
					floodflags |= (block->rotation == from)
						? RSM_FLOODFILL_COMPONENT_WRITE_PRIMARY /* Front input */
						: RSM_FLOODFILL_COMPONENT_READ; /* Output */
				else floodflags |= RSM_FLOODFILL_COMPONENT_WRITE_SECONDARY; /* Side input */
				break;

			/* Same-direction, all output */
			case RSM_BLOCK_INVERTER:
				if (block->rotation == from) floodflags |= RSM_FLOODFILL_COMPONENT_WRITE_PRIMARY;
				else floodflags |= RSM_FLOODFILL_COMPONENT_READ;
				break;

			/* Same-direction, front output */
			case RSM_BLOCK_BUFFER:
				if ((block->rotation & 1) != (from & 1)) break; /* Need to be parallel */
				if (block->rotation == from) floodflags |= RSM_FLOODFILL_COMPONENT_WRITE_PRIMARY;
				else floodflags |= RSM_FLOODFILL_COMPONENT_READ;
				break;

			/* All-direction, no output */
			case RSM_BLOCK_LIGHT_DIGITAL:
				floodflags |= RSM_FLOODFILL_COMPONENT_WRITE_PRIMARY;
				break;

			default:
				fprintf(stderr, "Unrecognized component while parsing graph (%d)\n", block->id);
				return;
		}

		u64 connectiondata; /* Old connection data */
		connectiondata = usf_inthmget(affected, TOBLOCKINDEX(coords)).u;

		u64 pdecay, sdecay; /* Two decay fields for possible inputs; on output, keep as-is */
		pdecay = connectiondata >> 48; sdecay = (connectiondata >> 32) & LOW16MASK;
		if (floodflags & RSM_FLOODFILL_COMPONENT_WRITE_PRIMARY) pdecay = decay - 1;
		if (floodflags & RSM_FLOODFILL_COMPONENT_WRITE_SECONDARY) sdecay = decay - 1;

		floodflags |= connectiondata & LOW32MASK; /* Add to other floodflags */
		usf_inthmput(affected, TOBLOCKINDEX(coords), USFDATAU((pdecay << 16 | sdecay) << 32 | floodflags));
		return;
	}

	/* Skip blocks which will not affect circuitry */
	if (!((block->metadata & RSM_BIT_CONDUCTOR) || (block->id == RSM_BLOCK_WIRE))) return;

	/* Skip if already floodfilled with lower decay */
	u64 visited;
	visited = usf_inthmget(seen, TOBLOCKINDEX(coords)).u; /* Possibly already wirefilled */
	if (visited && visited <= decay) return; /* Shorter path found (decay starts at 1 as hashmap default is 0) */
	usf_inthmput(seen, TOBLOCKINDEX(coords), USFDATAU(decay));

	/* Build neighborhood */
	Blockdata *neighbors[7]; /* (Unused), North, West, South, East, Up, Down */
	vec3 adjacent[7]; /* Same ordering */
#define _VISITADJACENT \
	glm_vec3_copy(coords, adjacent[_ROT]); \
	adjacent[_ROT][0] += _DX; adjacent[_ROT][1] += _DY; adjacent[_ROT][2] += _DZ; \
	neighbors[_ROT] = cu_coordsToBlock(adjacent[_ROT], NULL);
	FORORTHO(_VISITADJACENT);
#undef _VISITADJACENT

	if (block->metadata & RSM_BIT_CONDUCTOR) {
		/* End condition: found a soft-powered block (ortho to write to adjacent components) */
		Rotation direction;
		for (direction = NORTH; direction < COMPLEX; direction++) { /* Iterate 6 faces */
			if (!rsm_isComponent(neighbors[direction]->id)) continue; /* Only transfer softpower to components */
			wirefill(adjacent[direction], affected, seen, direction, decay, wires);
		}
		return;
	}

	/* If not conductor, must be a wire */

	/* Check for resistance */
	u64 resistance;
	resistance = neighbors[DOWN]->id == RSM_BLOCK_RESISTOR ? neighbors[DOWN]->variant : 0;
	
	/* Add wire to visual wireline with real decay */
	if (wires) usf_inthmput(wires, TOBLOCKINDEX(coords), USFDATAU(decay + resistance - 1));

	/* Downwards soft-powering (no ss loss) */
	if (neighbors[DOWN]->metadata & RSM_BIT_CONDUCTOR)
		wirefill(adjacent[DOWN], affected, seen, DOWN, decay + resistance, wires);

	/* Wire-adjacent connections (up and down) */
#define _ISTRANS(_ROT) (neighbors[_ROT]->metadata & RSM_BIT_CONDUCTOR)
	vec3 tempadj; /* Adjacent blocks only checked once */
	Blockdata *tempneigh;
#define _CONNECTWIRE(_ROT, _DY) \
	glm_vec3_copy(adjacent[_ROT], tempadj); tempadj[1] += _DY; /* Get coord */ \
	tempneigh = cu_coordsToBlock(tempadj, NULL); \
	if (tempneigh->id == RSM_BLOCK_WIRE) \
		wirefill(tempadj, affected, seen, _ROT, decay + resistance + RSM_NATURAL_DECAY, wires);

	if (_ISTRANS(UP)) { /* May connect upwards */
		_CONNECTWIRE(NORTH, 1);
		_CONNECTWIRE(WEST, 1);
		_CONNECTWIRE(SOUTH, 1);
		_CONNECTWIRE(EAST, 1);
	}

	/* May connect downwards */
	if (_ISTRANS(NORTH)) { _CONNECTWIRE(NORTH, -1); }
	if (_ISTRANS(WEST)) { _CONNECTWIRE(WEST, -1); }
	if (_ISTRANS(SOUTH)) { _CONNECTWIRE(SOUTH, -1); }
	if (_ISTRANS(EAST)) { _CONNECTWIRE(EAST, -1); }
#undef _CONNECTWIRE
#undef _ISTRANS

	/* Soft powering and component connections from wire */
#define _TRYCONNECT(_ROT) \
	if ((neighbors[_ROT]->metadata & RSM_BIT_WIRECONNECT_ALL) /* Check if anyconnect */ \
			|| ((neighbors[_ROT]->metadata & RSM_BIT_WIRECONNECT_LINE) \
				&& ((neighbors[_ROT]->rotation & 1) == (_ROT & 1)))) /* Check if parallel connect */ \
		wirefill(adjacent[_ROT], affected, seen, _ROT, decay + resistance + /* Only decay on wire-to-wire */ \
				(neighbors[_ROT]->id == RSM_BLOCK_WIRE ? RSM_NATURAL_DECAY : 0), wires);
	_TRYCONNECT(NORTH);
	_TRYCONNECT(WEST);
	_TRYCONNECT(SOUTH);
	_TRYCONNECT(EAST);
#undef _TRYCONNECT

	/* TODO DEBUG */ gui_updateGUI();
}

static void freeconnection(void *c) {
	/* Free a Connection structure to another component */

	Connection *connection = (Connection *) c;
	free(connection->visualdata); /* Only pointers */
	free(connection);
}
