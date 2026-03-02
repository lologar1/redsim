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
	 * block modification at coordinates coords. */

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	/* Put self in affected components */
	if (wf_isregistrable(block->id)) {
		Linkinfo *linkinfo;
		if ((linkinfo = usf_inthmget(afcontext->affected, (u64) block).p) == NULL) {
			/* Only coords & linkflags used for affected, so rest is unset */
			linkinfo = calloc(1, sizeof(Linkinfo));
			glm_vec3_copy(coords, linkinfo->coords);
			usf_inthmput(afcontext->affected, (u64) block, USFDATAP(linkinfo)); /* Put */
		}
		linkinfo->linkflags |= RSM_LINKFLAG_READ;
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
			case RSM_BLOCK_DIODE:
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
	}

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

		return; /* Constant source opaque can thus not be hard-powered */
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
	Blockdata *outblock; /* Used for multiple targets */
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
	if (outblock->id == RSM_BLOCK_WIRE || wf_iscomponent(outblock->id)) /* Only start if not hard-powering */ \
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

	/* Create or reinitialize component representation in graph */
	Component *component;
	if ((component = usf_inthmget(graphmap_, (u64) block).p) == NULL)
		usf_inthmput(graphmap_, (u64) block, USFDATAP(component = calloc(1, sizeof(Component))));

	component->id = block->id;
	component->metadata = (block->id == RSM_BLOCK_LIGHT_DIGITAL ? RSM_FLAG_FORCEVISUAL : 0);
	component->runtime = 0;
	memset(component->buffer, 0, sizeof(component->buffer));
	memset(component->state, 0, sizeof(component->state));

	if (component->connections) usf_freelistptrfunc(component->connections, free);
	if (component->visualdata) {
		usf_freelistu64(component->visualdata->chunkindices);
		free(component->visualdata->components);
		free(component->visualdata->wires);
		free(component->visualdata->wiredecays);
		free(component->visualdata);
	}
	if (component->reassert) usf_freelistptr(component->reassert);

	/* Set (hashmap) iterators */
	u64 i, j;
	usf_data *entry;

	cmd_logf("=== REGISTER %d %d ===\n", block->id, block->variant);
	usf_listptr *reassert; /* To prime if any target is modified */
	reassert = component->reassert = usf_newlistptr();
	for (i = 0; (entry = usf_inthmnext(linkcontext->affected, &i));) { /* Filter affected */
		Linkinfo *link;
		if (!((link = entry[1].p)->linkflags & ~RSM_LINKFLAG_READ)) {
			Blockdata *handle; /* Delete from affected and retrieve Blockdata * handle */
			handle = ((Linkinfo *) usf_inthmdel(linkcontext->affected, entry[0].u).p)->block;

			cmd_logf("rst CMP %d\n", handle->id);
			usf_listptradd(reassert, getcomponent(handle));

			free(link); /* Free Linkinfo * as no longer free'd by freecontext */
		}
	}

	usf_listptr *connections; /* List of Connection */
	connections = component->connections = usf_newlistptr();

	Visualdata *visualdata; /* Graphical information */
	visualdata = component->visualdata = malloc(sizeof(Visualdata));
	visualdata->chunkindices = usf_newlistu64();
	visualdata->components = malloc(linkcontext->affected->size * sizeof(Blockdata *));
	visualdata->wires = malloc(linkcontext->wires->size * sizeof(Blockdata *));
	visualdata->wiredecays = malloc(linkcontext->wires->size * sizeof(u8 *));

	for (i = 0; (entry = usf_inthmnext(linkcontext->chunkindices, &i));) /* Copy chunk indices */
		usf_listu64add(visualdata->chunkindices, entry[0].u); /* Chunk index is key */

	for (i = j = 0; (entry = usf_inthmnext(linkcontext->wires, &i)); j++) { /* Copy wire data */
		visualdata->wires[j] = entry[0].p;
		visualdata->wiredecays[j] = (u8) entry[1].u;
		cmd_logf("wr dcy %lu\n", entry[1].u);
	}

	for (i = j = 0; (entry = usf_inthmnext(linkcontext->affected, &i)); j++) { /* Perform handshakes */
		Linkinfo *link;
		link = (Linkinfo *) entry[1].p;

		Connection *connection;
		connection = malloc(sizeof(Connection));
		connection->component = getcomponent(link->block);
		connection->linkflags = link->linkflags;
		memcpy(connection->decay, link->decay, sizeof(link->decay));

		usf_listptradd(connections, connection); /* Connection established */
		visualdata->components[j] = link->block; /* For graphical update */

		cmd_logf("con w/%d %d dcy %"PRIu8"/%"PRIu8"\n", link->block->id, link->block->variant, link->decay[0], link->decay[1]);
	}

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

	usf_freequeuefunc(context->next, free); /* Free Fillcandidate * */
	usf_freeinthmfunc(context->affected, free); /* Free Linkinfo * */
	usf_freeinthm(context->seen); /* No allocated data associated */
	if (context->wires) usf_freeinthm(context->wires); /* Idem */
	if (context->chunkindices) usf_freeinthm(context->chunkindices); /* Idem */

	free(context);
}

void wf_registercoords(vec3 coords) {
	/* Registers a change to the world (at position coords) in the graph; If called after a change to
	 * the circuitry, the component graph will accurately reflect that change when this function exits. */

	usf_mtxlock(graphlock_); /* Thread-safe lock */

	/* Find all affected components */
	Fillcontext *afcontext;
	afcontext = wf_newcontext(RSM_DISCARD_VISUAL_INFO);
	wf_findaffected(coords, afcontext);

	wf_registercontext(afcontext);

	graphchanged_ = 1; /* Prime all next tick */
	usf_mtxunlock(graphlock_); /* Thread-safe unlock */

	wf_freecontext(afcontext); /* Free parsed structure */
}

void wf_registercontext(Fillcontext *context) {
	/* Registers a precalculated (batched) Fillcontext. This Fillcontext must have been populated
	 * by wf_findaffected so that it only contains components which the wire reads from. */

	u64 i;
	usf_data *entry;
	for (i = 0; (entry = usf_inthmnext(context->affected, &i));)
		wf_registercomponent(((Linkinfo *) entry[1].p)->coords);
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
