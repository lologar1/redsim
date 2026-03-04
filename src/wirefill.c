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

void wf_findaffected(vec3 coords, Fillcontext *afcontext) {
	/* Prepare afcontext with all the affected (need to be re-registered) components following the
	 * block modification at coordinates coords.
	 *
	 * A component marked RSM_LINKFLAG_READ will be registered as it presumably affects components
	 * which sit on the changed wireline.
	 * A component marked RSM_LINKFLAG_WRITE_* will have its input indices reset prior to registering by
	 * registercontext, as all components which affect it will be registered in the same batch.
	 *
	 * Note: graph lock must be acquired during batch registering ! */

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

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

		/* Reset input indices (all affected will re-register)
		 * Write-to components' indices are reset in registercontext */
		Component *component;
		component = getcomponent(block);
		memset(component->ninputs, 0, sizeof(component->ninputs));
	}

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

				/* Mark possibly destroyed component so as to not prime it */
				getcomponent(block)->id = 0;
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
	neighbor = cu_coordsToBlock(adjacent, NULL); \
	if (neighbor->id == RSM_BLOCK_WIRE /* Hard-power cannot soft-power blocks, nor connect to side inputs */ \
			|| (wf_iscomponent(neighbor->id) && COLINROT(_ROT, neighbor->rotation))) \
		wf_wirefill(adjacent, _ROT, 0, fillcontext, status);
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

		/* Reset buffers of written-to components to avoid ghost signals persisting. Do this before the
		 * READONLY check so every component on the wire is affected. This creates a false-positive for
		 * diode connections, but is needed to avoid findaffected needing to be switch to READWRITE.
		 *
		 * When modifying the component graph, any components the wire writes to are momentarily reset
		 * (power surge), meaning they lose their write-buffer contents (so in the next tick, they will
		 * act as if they were not connected), but not their state.*/
#define RESETBUF(_FLAG, _BUFFER) \
	do { \
		if (!(linkflags & _FLAG)) break; \
		Component *COMPONENT_ = getcomponent(block); \
		memset(COMPONENT_->buffer[_BUFFER], 0, COMPONENT_->ninputs[_BUFFER]); \
	} while (0);
		RESETBUF(RSM_LINKFLAG_WRITE_PRIMARY, 0);
		RESETBUF(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef RESETBUF

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

	u64 lastdecay; /* Guarantee connection is higher priority (lower decay) than last */
	lastdecay = usf_inthmget(fillcontext->seen, (u64) block).u;

	/* If lastdecay is 0: untouched. Always put decay + 1 to differentiate */
	if (lastdecay && lastdecay - 1 <= decay) return; /* Shorter path already found */
	usf_inthmput(fillcontext->seen, (u64) block, USFDATAU((u64) decay + 1)); /* Visit */

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
			if (neighbors[direction]->rotation == PERPROT(direction) /* Disallow soft-power side inputs */
					|| neighbors[direction]->rotation == INVROT(PERPROT(direction))
					|| !wf_iscomponent(neighbors[direction]->id))
				continue; /* Component sides do not interact through blocks (no soft/hard-powering) */

			/* Won't recurse since components terminate; okay to immediately wirefill without creating
			 * a Fillcandidate for later */
			wf_wirefill(adjacent[direction], direction, decay, fillcontext, status);
		}
		return;
	}

	/* Wire handling */
	usf_atmmst(&block->variant, 0, MEMORDER_RELAXED); /* Reset state to avoid disconnected garbage */

	u8 resistance;
	resistance = neighbors[DOWN]->id == RSM_BLOCK_RESISTOR ? neighbors[DOWN]->variant : 0;

	if ((u32) decay + (u32) resistance >= 255) return; /* Resistance kills any signal */
	decay += resistance; /* New decay */

	/* Log wire Blockdata * for visuals */
	if (fillcontext->wires) usf_inthmput(fillcontext->wires, (u64) block, USFDATAU(decay));

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

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

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

			wf_componentconnect(outcoords, block->rotation, linkcontext, 0);
			break;

		/* Single hard-powered + surrounding soft-powered outputs */
		case RSM_BLOCK_INVERTER:
			outcoords[1]++; /* Hard power up and reset outcoords for soft-powered outputs */
			wf_componentconnect(outcoords, UP, linkcontext, 0); outcoords[1] = coords[1];

#define SOFTOUTPUT(_ROT) \
	outblock = cu_coordsToBlock(outcoords, NULL); \
	/* Don't electrify, and skip components' secondary inputs */ \
	if (outblock->id == RSM_BLOCK_WIRE || (wf_iscomponent(outblock->id) && COLINROT(_ROT, outblock->rotation))) \
		wf_componentconnect(outcoords, _ROT, linkcontext, 0);
			FORORTHO(SOFTOUTPUT, outcoords, coords);
#undef SOFTOUTPUT
			break;

		/* Surrounding soft-powered outputs */
		case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
		case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
#define COMPOUTPUT(_ROT) \
	outblock = cu_coordsToBlock(outcoords, NULL); \
	if (wf_iscomponent(outblock->id)) /* Only start if component (constant source simulates soft-power) */ \
		wf_componentconnect(outcoords, _ROT, linkcontext, 0);
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
	/* ninputs reset in findaffected if block is being replaced. Otherwise, don't break others' connections
	 * to this component.
	 * Buffer (unknown length) is likewise managed by connections _to_ this component, where it is automatically
	 * resized to accomodate for all writers. */

	/* Old */
	usf_listptr *oldconnections; /* Retrieved to retake old connection indices */
	oldconnections = component->connections;
	if (component->visualdata) { /* Skip visualdata deallocation if component wasn't initialized before */
		usf_freelistu64(component->visualdata->chunkindices);
		free(component->visualdata->wires);
		free(component->visualdata->wiredecays);
		free(component->visualdata);
	}

	/* New */
	usf_listptr *connections; /* Connections to be built */
	connections = component->connections = usf_newlistptr();
	Visualdata *visualdata; /* Graphical information */
	visualdata = component->visualdata = malloc(sizeof(Visualdata));
	visualdata->chunkindices = usf_newlistu64();
	visualdata->wires = malloc(linkcontext->wires->size * sizeof(Blockdata *));
	visualdata->wiredecays = malloc(linkcontext->wires->size * sizeof(u8 *));

	/* Building */
	u64 i, j;
	usf_data *entry;

	/* Copy graphical data */
	for (i = 0; (entry = usf_inthmnext(linkcontext->chunkindices, &i));)
		usf_listu64add(visualdata->chunkindices, entry[0].u);
	for (i = j = 0; (entry = usf_inthmnext(linkcontext->wires, &i)); j++) {
		visualdata->wires[j] = entry[0].p;
		visualdata->wiredecays[j] = (u8) entry[1].u;
	}
	visualdata->nwires = linkcontext->wires->size;
	visualdata->blockdata = block;

	/* Setup connections (handshakes) */
	for (i = j = 0; (entry = usf_inthmnext(linkcontext->affected, &i)); j++) {
		Linkinfo *link;
		link = (Linkinfo *) entry[1].p;

		if (!(link->linkflags & ~RSM_LINKFLAG_READ)) continue; /* Does not write to */

		Connection *connection;
		connection = malloc(sizeof(Connection));

		connection->component = getcomponent(link->block); /* May forward-alloc (if not yet registered) */
		connection->linkflags = link->linkflags;
		memcpy(connection->decay, link->decay, sizeof(link->decay)); /* Copy link decays */
#define LINKWITH(_FLAG, _BUFFER) \
	do { \
		u16 CINDEX_; \
		if (link->linkflags & (_FLAG)) { \
			u8 I_, RETOOK_; /* Found old slot? */ \
			RETOOK_ = 0; /* Default not */ \
			if (oldconnections) for (I_ = 0; I_ < oldconnections->size; I_++) { \
				Connection *oldconnection; \
				oldconnection = oldconnections->array[I_]; \
				if (oldconnection->component != connection->component) continue; /* Not to same target */ \
				if (!(oldconnection->linkflags & _FLAG)) break; /* Wasn't registered */ \
				\
				CINDEX_ = oldconnection->index[_BUFFER]; /* Retake! */ \
				RETOOK_ = 1; /* Mark */ \
				break; \
			} \
			/* Either 0 (on calloc init), or valid from other registrations */ \
			if (!RETOOK_) CINDEX_ = connection->component->ninputs[_BUFFER]++; \
		} else break; /* Leave as-is; not used */ \
		connection->index[_BUFFER] = CINDEX_; /* Set target index */ \
		\
		u8 **INBUFPTR_ = &connection->component->buffer[_BUFFER]; \
		u16 NINPUTS_ = connection->component->ninputs[_BUFFER]; \
		*INBUFPTR_ = realloc(*INBUFPTR_, NINPUTS_); /* Ensure minimal size */ \
		memset(*INBUFPTR_, 0, NINPUTS_); /* Ensure no leftover data */ \
	} while (0);
		LINKWITH(RSM_LINKFLAG_WRITE_PRIMARY, 0);
		LINKWITH(RSM_LINKFLAG_WRITE_SECONDARY, 1);
#undef LINKWITH

		usf_listptradd(connections, connection); /* Connection established */
	}

	usf_freelistptrfunc(oldconnections, free); /* Free the last of the old data */
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
	usf_freeinthmfunc(context->affected, free); /* Free Linkinfo * */
	usf_freeinthm(context->seen); /* No allocated data associated */
	usf_freeinthm(context->wires); /* Idem */
	usf_freeinthm(context->chunkindices); /* Idem */

	free(context);
}

void wf_registercontext(Fillcontext *context) {
	/* Registers a precalculated (batched) Fillcontext. This Fillcontext must have been populated
	 * by wf_findaffected and contain only components which the wireline reads from.
	 *
	 * Note: graph lock must be acquired during batch registering! */

	u64 i;
	usf_data *entry;
	for (i = 0; (entry = usf_inthmnext(context->affected, &i));)
		wf_registercomponent(((Linkinfo *) entry[1].p)->coords);
	graphchanged_ = 1; /* Prime all next tick */
}

void wf_registercoords(vec3 coords) {
	/* Registers a change to the world (at position coords) in the graph; If called after a change to
	 * the circuitry, the component graph will accurately reflect that change when this function exits. */

	usf_mtxlock(graphmap_->lock); /* Thread-safe lock */
	Fillcontext *afcontext;
	afcontext = wf_newcontext(RSM_DISCARD_VISUAL_INFO);
	wf_findaffected(coords, afcontext); /* Find */
	wf_registercontext(afcontext); /* Register */
	usf_mtxunlock(graphmap_->lock); /* Thread-safe unlock */

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

static Component *getcomponent(Blockdata *handle) {
	/* Retrieve (or init placeholder pending registering) a Component * from its
	 * Blockdata * handle and return it */

	Component *component;
	if ((component = usf_inthmget(graphmap_, (u64) handle).p) == NULL)
		usf_inthmput(graphmap_, (u64) handle, USFDATAP(component = calloc(1, sizeof(Component))));

	return component;
}
