#include "wirefill.h"

#define FORSIDES(_EFFECTOR, _COORDSCOPY, _COORDSORIGINAL) \
	_COORDSCOPY[2] = _COORDSORIGINAL[2] + 1; _EFFECTOR(NORTH); \
	_COORDSCOPY[2] = _COORDSORIGINAL[2] - 1; _EFFECTOR(SOUTH); \
	_COORDSCOPY[2] = _COORDSORIGINAL[2]; \
	_COORDSCOPY[0] = _COORDSORIGINAL[0] + 1; _EFFECTOR(WEST); \
	_COORDSCOPY[0] = _COORDSORIGINAL[0] - 1; _EFFECTOR(EAST); \
	_COORDSCOPY[0] = _COORDSORIGINAL[0];

#define FORORTHO(_EFFECTOR, _COORDSCOPY, _COORDSORIGINAL) \
	_COORDSCOPY[1] = _COORDSORIGINAL[1] + 1; _EFFECTOR(UP); \
	_COORDSCOPY[1] = _COORDSORIGINAL[1] - 1; _EFFECTOR(DOWN); \
	_COORDSCOPY[1] = _COORDSORIGINAL[1]; \
	FORSIDES(_EFFECTOR, _COORDSCOPY, _COORDSORIGINAL);

static Component *getcomponent(Blockdata *handle);
static void clearbufferslots(Connection *connection);
static void freeconnection(void *c);

void wf_findaffected(vec3 coords, Fillcontext *afcontext) {
	/* Prepare afcontext with all the affected (need to be re-registered) components following the
	 * block modification at coordinates coords.
	 *
	 * A component marked RSM_LINKFLAG_READ will be registered as it presumably affects components
	 * which sit on the changed wireline.
	 * A component marked RSM_LINKFLAG_WRITE_* will have its input indices reset prior to registering by
	 * registercontext, as all components which affect it will be registered in the same batch.
	 *
	 * Note: ticklock_ must be acquired during batch registering ! */

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	Component *component;
	component = getcomponent(block);

	if (wf_isregistrable(block->id)) {
		/* Put self in affected components */
		Linkinfo *linkinfo;
		if ((linkinfo = usf_inthmget(afcontext->affected, (u64) block).p) == NULL) {
			/* Only coords & linkflags used for affected, so rest is unset */
			linkinfo = calloc(1, sizeof(Linkinfo));
			glm_vec3_copy(coords, linkinfo->coords);
			usf_inthmput(afcontext->affected, (u64) block, USFDATAP(linkinfo)); /* Put */
		}
		linkinfo->linkflags |= RSM_LINKFLAG_READ; /* Set to be registered */
	}
	/* Reset input indices (all affected will re-register)
	 * Recipients' slots for this component are cleared below and retaken in registering */
	usf_hmclear(component->inputs[0]);
	usf_hmclear(component->inputs[1]);

	usf_listptr *connections;
	connections = component->connections;
	u64 i; /* Reset all outgoing slots to 0 in case reregistering disconnects some of them */
	for (i = 0; i < connections->size; i++) clearbufferslots(connections->array[i]);

	Rotation rotation;
	rotation = block->rotation;

	vec3 adjacent;
	glm_vec3_copy(coords, adjacent);
	if (wf_isregistrable(block->id)) { /* Only check where components may write to */
		/* Only find affected which the wire reads from (will trigger this component) */

		switch(block->id) {
			/* Only written from back */
			case RSM_BLOCK_INVERTER:
			case RSM_BLOCK_BUFFER:
				switch (rotation) {
					case NORTH: adjacent[2]--; break;
					case WEST: adjacent[0]--; break;
					case SOUTH: adjacent[2]++; break;
					case EAST: adjacent[0]++; break;
					case UP: adjacent[1]--; break;
					case DOWN: adjacent[1]++; break;
					default:
						fprintf(stderr, "Illegal rotation %d for inverter/buffer while registering at "
								"%f %f %f, skipping.\n", rotation, coords[0], coords[1], coords[2]);
						return;
				}

				wf_componentconnect(adjacent, INVROT(rotation), afcontext, RSM_WIREFILL_READONLY);
				break;

			/* Written from back and side */
			case RSM_BLOCK_TRANSISTOR_ANALOG:
			case RSM_BLOCK_TRANSISTOR_DIGITAL:
			case RSM_BLOCK_LATCH:
#define CONNECTIFWIRE(_DIM, _OFFSET, _ROT) \
	adjacent[_DIM] = coords[_DIM] + _OFFSET; \
	if (!(cu_coordsToBlock(adjacent, NULL)->metadata & RSM_BIT_CONDUCTOR)) /* Component, wire, or irrelevant */ \
		wf_componentconnect(adjacent, _ROT, afcontext, RSM_WIREFILL_READONLY);
#define TRIAFFECT(_PDIM, _POSROT, _SDIM) \
	adjacent[_PDIM] += (rotation == _POSROT ? -1 : 1); /* Primary input */ \
	wf_componentconnect(adjacent, INVROT(rotation), afcontext, RSM_WIREFILL_READONLY); \
	adjacent[_PDIM] = coords[_PDIM]; /* Reset */ \
	CONNECTIFWIRE(_SDIM, 1, _POSROT); \
	CONNECTIFWIRE(_SDIM, -1, INVROT(_POSROT));
				if (rotation & 1) { TRIAFFECT(2, NORTH, 0); } /* North & South */
				else { TRIAFFECT(0, WEST, 2); } /* East & West */
#undef CONNECTIFWIRE
#undef TRIAFFECT
				break;

			/* Written from every direction */
			case RSM_BLOCK_LIGHT_DIGITAL: /* Every direction */
#define AFFECTDIR(_ROT) \
	wf_componentconnect(adjacent, _ROT, afcontext, RSM_WIREFILL_READONLY);
				FORORTHO(AFFECTDIR, adjacent, coords);
				break;

			/* No others are registered (apart from self) for constant sources */
			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				break;
		}
	} else if (block->metadata & RSM_BIT_CONDUCTOR || block->id == RSM_BLOCK_WIRE)
		wf_componentconnect(coords, NONE, afcontext, RSM_WIREFILL_READONLY); /* Wire or solid block */
	else { /* Special rules */
		switch(block->id) {
			case RSM_BLOCK_DIODE: /* Resistor included in solid blocks above */
				adjacent[1]++; /* Only wire on top will be affected */
				wf_componentconnect(adjacent, UP, afcontext, RSM_WIREFILL_READONLY);
				break;

			case RSM_BLOCK_AIR:
				/* Update orthogonal to reregister components which write to the destroyed
				 * block, and update up/down diagonals only for wire connections */
				FORORTHO(AFFECTDIR, adjacent, coords); /* Normal connections */
#undef AFFECTDIR
#define AFFECTWIRE(_ROT) \
	if (cu_coordsToBlock(adjacent, NULL)->id == RSM_BLOCK_WIRE) \
		wf_componentconnect(adjacent, _ROT, afcontext, RSM_WIREFILL_READONLY);
				adjacent[1] = coords[1] + 1; FORSIDES(AFFECTWIRE, adjacent, coords); /* Up connections */
				adjacent[1] = coords[1] - 1; FORSIDES(AFFECTWIRE, adjacent, coords); /* Down connections */
#undef AFFECTWIRE

				component->id = 0; /* Prevent destroyed component being primed */
				break;

			default: break; /* Will not affect circuitry */
		}
	}
}

void wf_componentconnect(vec3 coords, Rotation from, Fillcontext *fillcontext, u8 status) {
	/* Entry point for a wirefill (target presumed to be hard-powered) */

	Blockdata *block;
	if ((block = cu_coordsToBlock(coords, NULL))->id == RSM_BLOCK_AIR) return; /* Early exit */

	if (block->id == RSM_BLOCK_WIRE || wf_isregistrable(block->id))
		wf_wirefill(coords, from, 0, fillcontext, status); /* Components and registrables passed on */
	else if (block->metadata & RSM_BIT_CONDUCTOR) { /* Hard-powered checks surroundings */
		vec3 adjacent;
		glm_vec3_copy(coords, adjacent);

		Blockdata *neighbor;
#define FILLDIR(_ROT) \
	do { \
		neighbor = cu_coordsToBlock(adjacent, NULL); \
		if (neighbor->id == RSM_BLOCK_WIRE \
				|| (wf_iscomponent(neighbor->id) && !SIDEROT(_ROT, neighbor->rotation))) /* No side inputs */ \
			wf_wirefill(adjacent, _ROT, 0, fillcontext, status); \
	} while (0);
		FORORTHO(FILLDIR, adjacent, coords);
#undef FILLDIR
	} else return; /* Doesn't affect world */

	while (fillcontext->next->size) { /* Breadth-first search */
		Fillcandidate *candidate;
		candidate = usf_dequeue(fillcontext->next).p;

		wf_wirefill(candidate->coords, candidate->from, candidate->decay, fillcontext, candidate->status);

		free(candidate);
	} /* Exhausted candidates: fillcontext is ready */
}

void wf_wirefill(vec3 coords, Rotation from, u8 decay, Fillcontext *fillcontext, u8 status) {
	/* Find all affected components and wires from this point and update fillcontext accordingly. */

	if (decay == 255) return; /* Maximum decay */

	u64 chunkindex;
	Blockdata *block;
	block = cu_coordsToBlock(coords, &chunkindex);

	/* Log chunkindex for remeshing if acquiring visual data */
	if (fillcontext->chunkindices) usf_inthmput(fillcontext->chunkindices, chunkindex, USFTRUE);

	if (wf_isregistrable(block->id)) { /* Register if affected */
		u64 linkflags;
		linkflags = 0;

		switch(block->id) {
			/* Back input, front output */
			case RSM_BLOCK_BUFFER:
				if (block->rotation == from) linkflags |= RSM_LINKFLAG_WRITE_PRIMARY;
				else if (block->rotation == INVROT(from)) linkflags |= RSM_LINKFLAG_READ;
				else return; /* Not parallel: no connection */
				break;

			/* Back and side inputs, front output */
			case RSM_BLOCK_TRANSISTOR_ANALOG:
			case RSM_BLOCK_TRANSISTOR_DIGITAL:
			case RSM_BLOCK_LATCH:
				if (COLINROT(block->rotation, from))
					linkflags |= (block->rotation == from)
						? RSM_LINKFLAG_WRITE_PRIMARY /* Primary input */
						: RSM_LINKFLAG_READ; /* Output */
				else linkflags |= RSM_LINKFLAG_WRITE_SECONDARY; /* Secondary input */
				break;

			/* Back input, multidirectional output */
			case RSM_BLOCK_INVERTER:
				if (block->rotation == from) linkflags |= RSM_LINKFLAG_WRITE_PRIMARY;
				else linkflags |= RSM_LINKFLAG_READ;
				break;

			/* Multidirectional input */
			case RSM_BLOCK_LIGHT_DIGITAL: /* Every direction */
				linkflags |= RSM_LINKFLAG_WRITE_PRIMARY;
				break;

			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				linkflags |= RSM_LINKFLAG_READ; /* Wireline always reads from constant sources */
				break;

			default:
				fprintf(stderr, "Unknown component while linking in wirefill (ID %"PRIu64", variant "
						"%"PRIu64", at %f %f %f. Skipping.\n", block->id, block->variant,
						coords[0], coords[1], coords[2]);
				return;
		}

		if (status == RSM_WIREFILL_READONLY) linkflags &= RSM_LINKFLAG_READ; /* Remove writes on READONLY */
		if (!linkflags) return; /* Component does not interact */

		Linkinfo *linkinfo;
		if ((linkinfo = usf_inthmget(fillcontext->affected, (u64) block).p) == NULL) /* Retrieve or init */
			usf_inthmput(fillcontext->affected, (u64) block, USFDATAP(linkinfo = calloc(1, sizeof(Linkinfo))));

		linkinfo->linkflags |= linkflags;
		if (linkflags & RSM_LINKFLAG_WRITE_PRIMARY) linkinfo->decay[0] = decay;
		if (linkflags & RSM_LINKFLAG_WRITE_SECONDARY) linkinfo->decay[1] = decay;
		linkinfo->block = block;
		glm_vec3_copy(coords, linkinfo->coords);

		return; /* Constant source opaque cannot be hard-powered */
	}

	/* If not wire, component or conductor: no impact on circuitry */
	if (!(block->id == RSM_BLOCK_WIRE || block->metadata & RSM_BIT_CONDUCTOR)) return;

	u64 lastdecay, wbit; /* Guarantee connection is higher priority (lower decay) than last */
	lastdecay = usf_inthmget(fillcontext->seen, (u64) block).u;
	wbit = U64(1) << 63; /* Higher priority to write signals */
	if (status == RSM_WIREFILL_READONLY) wbit = ~wbit;

	/* If lastdecay is 0: untouched. Always put decay + 1 to differentiate */
	if (lastdecay && lastdecay - 1 <= ((u64) decay & wbit)) return; /* Shorter path already found */
	usf_inthmput(fillcontext->seen, (u64) block, USFDATAU(((u64) decay & wbit) + 1)); /* Visit */

	/* Retrieve adjacent blocks (costly) */
	Blockdata *neighbors[7]; /* (Unused), NORTH, WEST, SOUTH, EAST, UP, DOWN */
	vec3 adj, adjacent[7]; /* Same ordering */

	glm_vec3_copy(coords, adj); /* For FORORTHO macro */
#define GETADJACENT(_ROT) \
	glm_vec3_copy(adj, adjacent[_ROT]); \
	neighbors[_ROT] = cu_coordsToBlock(adj, NULL);
	FORORTHO(GETADJACENT, adj, coords);
#undef GETADJACENT

	/* End condition: soft-powered block (only propagate to adjacent components) */
	if (block->metadata & RSM_BIT_CONDUCTOR) {
		Rotation direction;
		for (direction = NORTH; direction < COMPLEX; direction++) { /* Iterate 6 rotations */
			if (!wf_iscomponent(neighbors[direction]->id) || SIDEROT(neighbors[direction]->rotation, direction))
				continue; /* Disallow secondary inputs */

			/* Won't recurse since components terminate; okay to immediately wirefill without creating
			 * a Fillcandidate for later */
			wf_wirefill(adjacent[direction], direction, decay, fillcontext, status);
		}
		return;
	}

	/* Wire handling */

	/* Reset state to avoid garbage */
	Component *wire = getcomponent(block);
	wire->id = 0; /* Does not participate */
	wire->visualdata->blockdata = block;
	if (wire->buffer[0]) memset(wire->buffer[0], 0, wire->inputs[0]->size);
	usf_atmmst(&block->variant, 0, MEMORDER_RELAXED);

	u8 resistance;
	resistance = neighbors[DOWN]->id == RSM_BLOCK_RESISTOR ? neighbors[DOWN]->variant : 0;

	if ((u32) decay + (u32) resistance >= 255) return; /* Resistance kills any signal */
	decay += resistance; /* New decay */

	/* Log wire Blockdata * for visuals */
	if (fillcontext->wires && status == RSM_WIREFILL_READWRITE)
		usf_inthmput(fillcontext->wires, (u64) wire, USFDATAU(decay));

	/* Downwards soft-powering */
	if (neighbors[DOWN]->metadata & RSM_BIT_CONDUCTOR) /* Again OK to recurse since will end locally */
		wf_wirefill(adjacent[DOWN], DOWN, decay, fillcontext, status);

	/* Upwards read-only from hardpower or inverter */
	if (neighbors[UP]->metadata & RSM_BIT_CONDUCTOR || neighbors[UP]->id == RSM_BLOCK_INVERTER)
		wf_wirefill(adjacent[UP], UP, decay, fillcontext, RSM_WIREFILL_READONLY);

	/* Get wire diagonal connections (up/down climb) */
#define GETDIAG(_ROTCHECK, _YOFFSET, _DIM, _OFFSET, _ROT, _CLIMB) \
	if (!(neighbors[_ROTCHECK]->metadata & RSM_BIT_CONDUCTOR)) { \
		glm_vec3_copy(coords, _CLIMB##adjacent[_ROT]); \
		_CLIMB##adjacent[_ROT][1] += _YOFFSET; \
		_CLIMB##adjacent[_ROT][_DIM] += _OFFSET; \
		_CLIMB##neighbors[_ROT] = cu_coordsToBlock(_CLIMB##adjacent[_ROT], NULL); \
		if (_CLIMB##neighbors[_ROT]->id != RSM_BLOCK_WIRE) \
			_CLIMB##neighbors[_ROT] = NULL; /* Disconnect if not wire */ \
	} else _CLIMB##neighbors[_ROT] = NULL;

	/* (Unused), NORTH, WEST, SOUTH, EAST */
	Blockdata *upneighbors[5];
	vec3 upadjacent[5];
	GETDIAG(UP, 1, 2, 1, NORTH, up);
	GETDIAG(UP, 1, 0, 1, WEST, up);
	GETDIAG(UP, 1, 2, -1, SOUTH, up);
	GETDIAG(UP, 1, 0, -1, EAST, up);

	Blockdata *downneighbors[5];
	vec3 downadjacent[5];
	GETDIAG(NORTH, -1, 2, 1, NORTH, down);
	GETDIAG(WEST, -1, 0, 1, WEST, down);
	GETDIAG(SOUTH, -1, 2, -1, SOUTH, down);
	GETDIAG(EAST, -1, 0, -1, EAST, down);
#undef GETDIAG

#define NEWCANDIDATE(_ADJACENT, _ROT, _STATUS) /* Create and enqueue a new Fillcandidate */ \
	do { \
		Fillcandidate *CANDIDATE_; \
		CANDIDATE_ = malloc(sizeof(Fillcandidate)); \
		glm_vec3_copy(_ADJACENT[_ROT], CANDIDATE_->coords); \
		CANDIDATE_->from = _ROT; \
		CANDIDATE_->decay = decay + RSM_NATURAL_DECAY; /* Wire-to-wire */ \
		CANDIDATE_->status = _STATUS; \
		usf_enqueue(fillcontext->next, USFDATAP(CANDIDATE_)); \
	} while (0)

	/* Side connections (component, soft-power or wire) */
#define CONNECTSAT(_ROT) /* Check if neighbor connects to wire */ \
	((neighbors[_ROT]->metadata & RSM_BIT_WIRECONNECT_ALL) /* Anyconnect */ \
	 || (neighbors[_ROT]->metadata & RSM_BIT_WIRECONNECT_LINE /* Parallel connect */ \
		 && COLINROT(neighbors[_ROT]->rotation, _ROT)))
#define POINTSAT(_ROT) /* Check if wire connects to neighbor */ \
	((CONNECTSAT(_ROT)) \
	|| (!CONNECTSAT(PERPROT(_ROT)) /* No side connections */ \
		&& !CONNECTSAT(INVROT(PERPROT(_ROT))) \
		&& !upneighbors[PERPROT(_ROT)] && !downneighbors[PERPROT(_ROT)] /* No side climbs */ \
		&& !upneighbors[INVROT(PERPROT(_ROT))] && !downneighbors[INVROT(PERPROT(_ROT))]))
#define CONNECTSIDE(_ROT) \
	if (POINTSAT(_ROT)) { \
		if (neighbors[_ROT]->id == RSM_BLOCK_WIRE) /* Wire connection (register BFS candidate) */ \
			NEWCANDIDATE(adjacent, _ROT, status); \
		else if (neighbors[_ROT]->metadata & RSM_BIT_CONDUCTOR /* Soft-power */ \
				|| wf_iscomponent(neighbors[_ROT]->id)) /* Component */ \
			wf_wirefill(adjacent[_ROT], _ROT, decay, fillcontext, status); /* Recurse locally */ \
	} else wf_wirefill(adjacent[_ROT], _ROT, decay, fillcontext, RSM_WIREFILL_READONLY); /* Hard-power read */
	CONNECTSIDE(NORTH);
	CONNECTSIDE(WEST);
	CONNECTSIDE(SOUTH);
	CONNECTSIDE(EAST);
#undef CONNECTSAT
#undef POINTSAT
#undef CONNECTSIDE

	/* Diagonal connections (up and down); wire only */
	Rotation direction;
	for (direction = NORTH; direction < UP; direction++) { /* Iterate cardinal directions */
#define DIAGPROPAGATE(_KIND, _OTHERDIODEVARIANT) \
	if (_KIND##neighbors[direction]) \
		NEWCANDIDATE(_KIND##adjacent, direction, \
				(neighbors[DOWN]->id == RSM_BLOCK_DIODE && neighbors[DOWN]->variant == _OTHERDIODEVARIANT) \
				? RSM_WIREFILL_READONLY \
				: status); \
		/* Diode variants: 0 is UP, 1 is DOWN */
		DIAGPROPAGATE(up, 1);
		DIAGPROPAGATE(down, 0);
#undef DIAGPROPAGATE
	}
#undef NEWCANDIDATE
}

void wf_registercomponent(vec3 coords) {
	/* Register component at given coordinates and perform proper handshake procedure in graphmap_.
	 * If targets do not exist, a placeholder is created assuming they will be, in turn, registered. */

	u64 chunkindex;
	Blockdata *block;
	block = cu_coordsToBlock(coords, &chunkindex);

	Fillcontext *linkcontext;
	linkcontext = wf_newcontext(RSM_KEEP_VISUAL_INFO);

	vec3 outcoords;
	Blockdata *outblock;
	glm_vec3_copy(coords, outcoords);
	switch (block->id) {
		/* Single hard-powered output */
		case RSM_BLOCK_TRANSISTOR_ANALOG:
		case RSM_BLOCK_TRANSISTOR_DIGITAL:
		case RSM_BLOCK_LATCH:
		case RSM_BLOCK_BUFFER:
			switch (block->rotation) {
				case NORTH: outcoords[2]++; break;
				case WEST: outcoords[0]++; break;
				case SOUTH: outcoords[2]--; break;
				case EAST: outcoords[0]--; break;
				default:
					fprintf(stderr, "Illegal component rotation while registering component (ID "
							"%"PRIu16" variant %"PRIu8") at %f %f %f, skipping.\n", block->id,
							block->variant, coords[0], coords[1], coords[2]);
					wf_freecontext(linkcontext);
					return;
			}

			wf_componentconnect(outcoords, block->rotation, linkcontext, RSM_WIREFILL_READWRITE);
			break;

		/* Single hard-powered + surrounding soft-powered outputs */
		case RSM_BLOCK_INVERTER:
			outcoords[1]++; /* Hard power up and reset outcoords for soft-powered outputs */
			wf_componentconnect(outcoords, UP, linkcontext, RSM_WIREFILL_READWRITE); outcoords[1] = coords[1];

#define SOFTOUTPUT(_ROT) \
	outblock = cu_coordsToBlock(outcoords, NULL); \
	/* Don't electrify, and skip components' secondary inputs */ \
	if (outblock->id == RSM_BLOCK_WIRE || (wf_iscomponent(outblock->id) && !SIDEROT(_ROT, outblock->rotation))) \
		wf_componentconnect(outcoords, _ROT, linkcontext, RSM_WIREFILL_READWRITE);
			FORORTHO(SOFTOUTPUT, outcoords, coords);
#undef SOFTOUTPUT
			break;

		/* Surrounding soft-powered outputs */
		case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
		case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
#define COMPOUTPUT(_ROT) \
	outblock = cu_coordsToBlock(outcoords, NULL); \
	if (wf_iscomponent(outblock->id)) /* Only start if component (constant source simulates soft-power) */ \
		wf_componentconnect(outcoords, _ROT, linkcontext, RSM_WIREFILL_READWRITE);
			FORORTHO(COMPOUTPUT, outcoords, coords);
#undef COMPOUTPUT
			break;

		case RSM_BLOCK_LIGHT_DIGITAL: break; /* Not output */

		default:
			fprintf(stderr, "Unknown component while registering block (ID %"PRIu16" variant %"PRIu8
				") at %f %f %f, skipping.\n", block->id, block->variant, coords[0], coords[1], coords[2]);
			wf_freecontext(linkcontext);
			return;
	}

	Component *component;
	component = getcomponent(block);

	component->id = block->id;
	component->variant = block->variant;
	memset(component->state, 0, sizeof(component->state));
	/* inputs reset by findaffected if being is being replaced. Buffers are realloc'd on handshake according
	 * to the number of inputs registered. */

	usf_listptr *connections; /* Connections to be built */
	usf_freelistptrfunc(component->connections, freeconnection); /* Free old Connections, and realloc new list */
	connections = component->connections = usf_newlistptr();
	connections->size = 0;

	Visualdata *visualdata; /* Graphical information */
	visualdata = component->visualdata;
	visualdata->blockdata = block;
	visualdata->chunkindices->size = 0;
	visualdata->nwires = linkcontext->wires->size; /* + 1 in realloc to avoid realloc(0) */
	visualdata->wires = realloc(visualdata->wires, visualdata->nwires * sizeof(Blockdata *) + 1);
	visualdata->wiredecays = realloc(visualdata->wiredecays, visualdata->nwires * sizeof(u8 *) + 1);
	visualdata->wireindices = realloc(visualdata->wireindices, visualdata->nwires * sizeof(u16 *) + 1);

	/* Building */
	usf_hashiter iter;

	/* Copy chunk indices */
	usf_listu64add(visualdata->chunkindices, chunkindex); /* At least register self, but not twice! */
	for (usf_hmiterbegin(linkcontext->chunkindices, &iter); usf_hmiternext(&iter);)
		if (iter.entry->key.u != chunkindex) usf_listu64add(visualdata->chunkindices, iter.entry->key.u);
	usf_hmiterend(&iter);

	/* Copy wire data */
	u64 i;
	for (i = 0, usf_hmiterbegin(linkcontext->wires, &iter); usf_hmiternext(&iter); i++) {
		Component *wire;
		visualdata->wires[i] = (wire = iter.entry->key.p);
		visualdata->wiredecays[i] = (u8) iter.entry->value.u;

		/* Only care about visualdata->blockdata & input buffer. Former is autoset in wirefill. */
#define LINK(_COMPONENT, _BUFFER, _STORAGE) \
	do { \
		u16 BUFINDEX_; \
		u16 NINPUTS_ = _COMPONENT->inputs[_BUFFER]->size; \
		if ((BUFINDEX_ = usf_inthmget(_COMPONENT->inputs[_BUFFER], (u64) block).u) == 0) \
			usf_inthmput(_COMPONENT->inputs[_BUFFER], (u64) block, USFDATAU((BUFINDEX_ = NINPUTS_++) + 1)); \
		else BUFINDEX_--; /* Remove presence indicator */ \
		_STORAGE = BUFINDEX_; \
		/* Ensure buffer size */ \
		u8 **INBUFPTR_ = &_COMPONENT->buffer[_BUFFER]; \
		*INBUFPTR_ = realloc(*INBUFPTR_, NINPUTS_); \
	} while (0);
		LINK(wire, 0, visualdata->wireindices[i]);
	} usf_hmiterend(&iter);

	/* Setup component connections (handshakes) */
	for (usf_hmiterbegin(linkcontext->affected, &iter); usf_hmiternext(&iter);) {
		Linkinfo *link = (Linkinfo *) iter.entry->value.p;

		if (!(link->linkflags & ~RSM_LINKFLAG_READ)) continue; /* Does not write to */

		Connection *connection;
		connection = calloc(1, sizeof(Connection));

		connection->component = getcomponent(link->block); /* May forward-alloc (if not yet registered) */
		connection->linkflags = link->linkflags;
		memcpy(connection->decay, link->decay, sizeof(link->decay)); /* Copy link decays */

		if (link->linkflags & RSM_LINKFLAG_WRITE_PRIMARY) LINK(connection->component, 0, connection->index[0]);
		if (link->linkflags & RSM_LINKFLAG_WRITE_SECONDARY) LINK(connection->component, 1, connection->index[1]);
#undef LINK
		clearbufferslots(connection);

		usf_listptradd(connections, connection); /* Connection established */
	} usf_hmiterend(&iter);

	/* Setup wire connections */

	wf_freecontext(linkcontext);
}

Fillcontext *wf_newcontext(i32 discardvisual) {
	/* Inits a new Fillcontext, with or without visual data tracking */

	Fillcontext *context;
	context = malloc(sizeof(Fillcontext));
	context->next = usf_newqueue();
	context->affected = usf_newhm();
	context->seen = usf_newhm();
	
	if (discardvisual) context->wires = context->chunkindices = NULL;
	else {
		context->wires = usf_newhm();
		context->chunkindices = usf_newhm();
	}

	return context;
}

void wf_freecontext(Fillcontext *context) {
	/* Frees a Fillcontext */

	if (context == NULL) return;

	usf_freequeuefunc(context->next, free); /* Free Fillcandidate * */
	usf_freehmfunc(context->affected, free); /* Free Linkinfo * */
	usf_freehm(context->seen); /* No allocated data associated */
	usf_freehm(context->wires); /* Idem */
	usf_freehm(context->chunkindices); /* Idem */

	free(context);
}

void wf_registercontext(Fillcontext *context) {
	/* Registers a precalculated (batched) Fillcontext. This Fillcontext must have been populated
	 * by wf_findaffected and contain only components which the wireline reads from.
	 *
	 * Note: ticklock_ must be acquired during batch registering! */

	usf_hashiter iter;
	for (usf_hmiterbegin(context->affected, &iter); usf_hmiternext(&iter);)
		wf_registercomponent(((Linkinfo *) iter.entry->value.p)->coords);
	usf_hmiterend(&iter);

	graphchanged_ = 1; /* Prime all next tick */
}

void wf_registercoords(vec3 coords) {
	/* Registers a change to the world (at position coords) in the graph; If called after a change to
	 * the circuitry, the component graph will accurately reflect that change when this function exits. */

	usf_mtxlock(&ticklock_); /* Thread-safe lock */
	Fillcontext *afcontext;
	afcontext = wf_newcontext(RSM_DISCARD_VISUAL_INFO);
	wf_findaffected(coords, afcontext); /* Find */
	wf_registercontext(afcontext); /* Register */
	usf_mtxunlock(&ticklock_); /* Thread-safe unlock */

	wf_freecontext(afcontext); /* Free transient data */
}

i32 wf_iscomponent(Blocktype id) {
	/* Check if a given block ID is a component. A component has the following properties:
	 * Is registrable in the component graph.
	 * Is not conductor.
	 * Can read soft-powered signals. */

	return (id >= RSM_BLOCK_TRANSISTOR_ANALOG && id <= RSM_BLOCK_BUFFER) || id == RSM_BLOCK_LIGHT_DIGITAL;
}

i32 wf_isregistrable(Blocktype id) {
	/* Check if a given block ID is registrable in the component graph */

	return id != RSM_BLOCK_RESISTOR && id >= RSM_BLOCK_TRANSISTOR_ANALOG && id <= RSM_BLOCK_LIGHT_DIGITAL;
}

i32 wf_ispowerable(Blocktype id) {
	/* Check if a given block ID is able to be powered */

	return id >= RSM_BLOCK_TRANSISTOR_ANALOG && id <= RSM_BLOCK_LIGHT_DIGITAL
		&& !(id >= RSM_BLOCK_RESISTOR && id <= RSM_BLOCK_CONSTANT_SOURCE_TRANS);
}

static Component *getcomponent(Blockdata *handle) {
	/* Retrieve (or init placeholder pending registering) a Component * from its
	 * Blockdata * handle and return it */

	Component *component;
	if ((component = usf_inthmget(graphmap_, (u64) handle).p) == NULL) {
		usf_inthmput(graphmap_, (u64) handle, USFDATAP(component = calloc(1, sizeof(Component))));
		component->inputs[0] = usf_newhm(); /* Non thread-safe since parallel access is read-only */
		component->inputs[1] = usf_newhm();
		component->connections = usf_newlistptr();
		component->visualdata = calloc(1, sizeof(Visualdata));
		component->visualdata->chunkindices = usf_newlistu64();
	}

	return component;
}

static void clearbufferslots(Connection *connection) {
	/* Clears the corresponding buffer slot through the connection */

	if (connection->linkflags & RSM_LINKFLAG_WRITE_PRIMARY)
		connection->component->buffer[0][connection->index[0]] = 0;
	if (connection->linkflags & RSM_LINKFLAG_WRITE_SECONDARY)
		connection->component->buffer[1][connection->index[1]] = 0;
}

static void freeconnection(void *c) {
	/* Frees a connection while resetting its output buffer slot to 0 */

	clearbufferslots(c);
	free(c);
}
