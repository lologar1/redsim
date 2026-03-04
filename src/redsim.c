#include "redsim.h"

Gamestate gamestate_ = NORMAL;
vec3 ret_selection_[2] = {VEC3(0, 0, 0), VEC3(1, 1, 1)};
vec3 ret_positions_[2] = {VEC3(0, 0, 0), VEC3(0, 0, 0)};

static vec3 lookingBlockCoords_, adjacentBlockCoords_;
static i64 lookingAxis_, stepAxis_; /* stepAxis_ is direction of lookingAxis_ */

void rsm_move(vec3 position) {
	/* Persistent momentum across multiple frames */
	static vec3 speed = {0.0f, 0.0f, 0.0f};

	/* Keep track of delta time passed since last frame */
	static f32 lastTime = 0.0f; /* OK since glfwGetTime() starts from 0 */
	f32 deltaTime;
	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	/* Now update speed for next frame */
	vec3 movement = {0.0f, 0.0f, 0.0f};
	if (rsm_right_) movement[0] += (RSM_FLY_ACCELERATION + RSM_FLY_X_ACCELERATION) * deltaTime;
	if (rsm_left_) movement[0] -= (RSM_FLY_ACCELERATION + RSM_FLY_X_ACCELERATION) * deltaTime;
	if (rsm_up_) movement[1] += (RSM_FLY_ACCELERATION + RSM_FLY_Y_ACCELERATION) * deltaTime;
	if (rsm_down_) movement[1] -= (RSM_FLY_ACCELERATION + RSM_FLY_Y_ACCELERATION) * deltaTime;
	if (rsm_backward_) movement[2] += (RSM_FLY_ACCELERATION + RSM_FLY_Z_ACCELERATION) * deltaTime;
	if (rsm_forward_) movement[2] -= (RSM_FLY_ACCELERATION + RSM_FLY_Z_ACCELERATION) * deltaTime;
	glm_vec3_scale(movement, deltaTime, movement);
	glm_vec3_rotate(movement, -glm_rad(yaw_), GLM_YUP);
	glm_vec3_add(speed, movement, speed);

	/* Cap speed */
	if (glm_vec3_norm(speed) > RSM_FLY_SPEED_CAP) glm_vec3_scale_as(speed, RSM_FLY_SPEED_CAP, speed);

	/* Air deceleration */
	glm_vec3_scale_as(speed, glm_vec3_norm(speed) * powf(RSM_FLY_FRICTION, deltaTime), speed);

	/* Collision handling */
	vec3 playerCornerOld;
	glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, position, playerCornerOld);

	/* Move in X, Y and Z to prevent edge collision bugs. This does mean that the player needs a certain
	 * clearance to move which depends on the framerate, but this value should be small at normal FPS and
	 * the normal player BB size isn't a multiple of 1 so it's fine */
	i32 axis;
	if (!RSM_NOCLIP) for (axis = 0; axis < 3; axis++) {
		vec3 newPosition, playerCornerNew; /* Future position of the player */
		glm_vec3_copy(position, newPosition);
		newPosition[axis] += speed[axis];
		glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, newPosition, playerCornerNew);

		/* Check collision */
		Blockdata *blockdata;
		vec3 *offset, blockposition;
		for (offset = PLAYERBBOFFSETS; (u64) (offset - PLAYERBBOFFSETS) < NPLAYERBBOFFSETS; offset++) {
			glm_vec3_add(playerCornerNew, *offset, blockposition);
			glm_vec3_floor(blockposition, blockposition);
			blockdata = cu_coordsToBlock(blockposition, NULL);

			if (!(blockdata->metadata & RSM_BIT_COLLISION)) continue; /* Block has no collision */

			/* Get bounding box data and adjust player position for precise float computations */
			f32 boundingbox[6];
			memcpy(boundingbox, BOUNDINGBOXES[blockdata->id]
					[blockdata->variant >= MAX_BLOCK_VARIANT[blockdata->id] ? 0 : blockdata->variant],
					sizeof(f32 [6])); /* Clamp variant at 0 for software-defined blocks */

			vec3 relativePCNew, relativePCOld; /* Avoid float imprecision at very large distances */
			glm_vec3_sub(playerCornerNew, blockposition, relativePCNew);
			glm_vec3_sub(playerCornerOld, blockposition, relativePCOld);

			if (blockdata->rotation > NORTH) { /* NONE is 0, and both NORTH and NONE do not modify base model */
				glm_vec3_add(boundingbox, boundingbox+3, boundingbox+3); /* Diametrically opposite corner */

				vec3 boxcenter;
				mat4 rotation;
				glm_vec3_adds(boundingbox, 0.5f, boxcenter);
				cu_rotationMatrix(rotation, blockdata->rotation, boxcenter);
				glm_mat4_mulv3(rotation, boundingbox, 1.0f, boundingbox);
				glm_mat4_mulv3(rotation, boundingbox+3, 1.0f, boundingbox+3);

				/* Find new dimensions and renormalize */
				glm_vec3_sub(boundingbox+3, boundingbox, boundingbox+3);
			}

			/* Landing position is not inside a block; no collision occurs.
			 * AABBIntersect also normalizes BBs. */
			if (!cu_AABBIntersect(boundingbox, boundingbox+3, relativePCNew, PLAYER_BOUNDINGBOX_DIMENSIONS))
				continue;

			if (cu_AABBIntersect(boundingbox, boundingbox+3, relativePCOld, PLAYER_BOUNDINGBOX_DIMENSIONS))
				continue; /* Player is moving inside block; do not block movement */

#define SAFEDISTANCE (nextafterf(fabsf(position[axis]), INFINITY) - fabsf(position[axis])) /* Smallest delta */
			if (boundingbox[axis] + boundingbox[axis+3] < relativePCOld[axis])
				/* Player is "above" */
				speed[axis] = -(relativePCOld[axis] - (boundingbox[axis]+boundingbox[axis+3]))
					+ USF_MAX(SAFEDISTANCE, 0.00048828125f); /* Close to 0, SAFEDISTANCE < imprecision */
			else /* Player is "under */
				speed[axis] = boundingbox[axis] - (relativePCOld[axis] + PLAYER_BOUNDINGBOX_DIMENSIONS[axis])
					- USF_MAX(SAFEDISTANCE, 0.00048828125f);
#undef SAFEDISTANCE

			/* Match speed to orientation, kill component and replace it */
			speed[axis] = 0.0f; /* Kill speed for that component */
		}
	}

	glm_vec3_add(position, speed, position); /* This affects player position */
}

void rsm_updateWiremesh(void) {
	/* Update wiremesh to reflect block highlighting and selection. Colors are taken from the air texture.
	 * The left side of the texture is the selection color while the right side is block highlighting. */

	f32 sx, sy, sz, sa, sb, sc; /* Selection coords and dimensions */
	sa = ret_selection_[0][0]; sb = ret_selection_[0][1]; sc = ret_selection_[0][2];
	sx = ret_selection_[1][0]; sy = ret_selection_[1][1]; sz = ret_selection_[1][2];

	/* Block highlighting coords and dimensions */
	vec3 looking, pos;
	glm_vec3_copy(position_, looking); glm_vec3_copy(position_, pos);
	glm_vec3_muladds(orientation_, RSM_REACH, looking);

	/* 3D Bresenham's */
	vec3 gridpos, direction;
	glm_vec3_floor(pos, gridpos);
	glm_vec3_sub(looking, pos, direction);

#define STEP(_I) direction[_I] > 0 ? 1.0f : -1.0f
	vec3 step = { STEP(0), STEP(1), STEP(2) };
#undef STEP

	/* tDelta is the step size (in tspace) per unit moved in realspace */
#define TDELTA(_I) direction[_I] == 0 ? 0.0f : fabsf(1.0f / direction[_I]) /* 0.0f changed to max tMax below */
	vec3 tDelta = { TDELTA(0), TDELTA(1), TDELTA(2) };
#undef TDELTA

	/* tMax is the current progress (from 0.0f to 1.0f) until we reach looking in tspace */
#define TMAX(_I) tDelta[_I] ? \
	(tDelta[_I] * ((step[_I] > 0 ? (gridpos[_I]+1) - pos[_I] : pos[_I] - gridpos[_I]))) : 1.0f
	vec3 tMax = { TMAX(0), TMAX(1), TMAX(2) };
#undef TMAX

	Blockdata *lookingAt;
	static i64 axis = 0; /* Use old axis if inside block */
	do {
		if ((lookingAt = cu_coordsToBlock(gridpos, NULL))->id) break; /* Block isn't air */

		/* Update face position with the one we just entered */
		glm_vec3_copy(gridpos, adjacentBlockCoords_);

		/* Determine which axis is crossed next, update t. */
		axis = tMax[0] < tMax[1] ? 0 : 1;
		axis = tMax[axis] < tMax[2] ? axis : 2;

		gridpos[axis] += step[axis];
		tMax[axis] += tDelta[axis];
		if (glm_vec3_isnan(tMax)) exit(1);
	} while (glm_vec3_min(tMax) < 1.0f);
	lookingAxis_ = axis; /* Axis of crossing */
	stepAxis_ = step[axis]; /* Direction of axis */
	glm_vec3_copy(gridpos, lookingBlockCoords_);

	/* Normals set to point upwards as to not be illegal (still processed by opaque fragment shader) */
	f32 vertices[8 * 8 * 2] = {
		/* Selection: 0.25f for left side, 0.0f to grab the first texture */
		sa, sb, sc, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa + sx, sb, sc, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa + sx, sb, sc + sz, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa, sb, sc + sz, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,

		sa, sb + sy, sc, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa + sx, sb + sy, sc, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa + sx, sb + sy, sc + sz, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,
		sa, sb + sy, sc + sz, 0.0f, 1.0f, 0.0f, 0.25f, 0.0f,

		/* Block highlighting. If no block is highlighted, the farthest air block is taken since gridpos will
		 * have incremented all the way to looking. */
		gridpos[0], gridpos[1], gridpos[2], 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0] + 1.0f , gridpos[1], gridpos[2], 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0] + 1.0f, gridpos[1], gridpos[2] + 1.0f, 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0], gridpos[1], gridpos[2] + 1.0f, 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0], gridpos[1] + 1.0f, gridpos[2], 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0] + 1.0f, gridpos[1] + 1.0f, gridpos[2], 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0] + 1.0f, gridpos[1] + 1.0f, gridpos[2] + 1.0f, 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
		gridpos[0], gridpos[1] + 1.0f, gridpos[2] + 1.0f, 0.0f, 1.0f, 0.0f, 0.75f, 0.0f,
	};

	u32 indices[3 * 8 * 2] = {
		0, 1, 1, 2, 2, 3, 3, 0, /* Bottom square */
		4, 5, 5, 6, 6, 7, 7, 4, /* Top square */
		0, 4, 1, 5, 2, 6, 3, 7, /* Side squares */

		8, 9, 9, 10, 10, 11, 11, 8,
		12, 13, 13, 14, 14, 15, 15, 12,
		8, 12, 9, 13, 10, 14, 11, 15
	};

	/* Dump to GL buffers */
	GLint buf;
	glBindVertexArray(wiremesh_[0]);
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint) buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
	wiremesh_[1] = sizeof(indices)/sizeof(indices[0]);
}

void rsm_interact(void) {
	/* Place, destroy or interact with the world */

	Blockdata *lookingAt;
	u64 lookingAtChunkindex;
	lookingAt = cu_coordsToBlock(lookingBlockCoords_, &lookingAtChunkindex);

	if (rsm_middleclick_) { /* Pipette tool; using only lookingAt */
		rsm_middleclick_ = 0;

		hotbar_[hotbarIndex_][hotslotIndex_][0] = lookingAt->id;
		hotbar_[hotbarIndex_][hotslotIndex_][1] = /* Disallow illegal sprites */
			(lookingAt->variant >= MAX_BLOCK_VARIANT[lookingAt->id]
			|| SPRITEIDS[lookingAt->id][lookingAt->variant] == 0) ? 0 : lookingAt->variant;

		gui_updateGUI();
		return;
	}

	/* Held item (tool checking) */
	u64 helduid;
	helduid = ASUID(hotbar_[hotbarIndex_][hotslotIndex_][0], hotbar_[hotbarIndex_][hotslotIndex_][1]);
	switch (helduid) {
		case ASUID(RSM_SPECIAL_ID, RSM_SPECIAL_SELECTIONTOOL): /* Selection tool */
#define SELECTCORNER(_INDICATOR, _POSINDEX) \
	if (_INDICATOR) { \
		_INDICATOR = 0; /* Reset indicator */ \
		memcpy(ret_positions_[_POSINDEX], ASVEC3(lookingBlockCoords_), sizeof(vec3)); \
	}
			SELECTCORNER(rsm_leftclick_, 0);
			SELECTCORNER(rsm_rightclick_, 1);
#undef SELECTCORNER

			u64 i;
			for (i = 0; i < 3; i++) { /* Create selection */
				ret_selection_[0][i] = USF_MIN(ret_positions_[0][i], ret_positions_[1][i]);
				ret_selection_[1][i] = USF_MAX(ret_positions_[0][i], ret_positions_[1][i]);
				ret_selection_[1][i] = ret_selection_[1][i] - ret_selection_[0][i] + 1; /* Convert to size */
			}
			return;

		case ASUID(RSM_SPECIAL_ID, RSM_SPECIAL_INFORMATIONTOOL): /* Information tool */
			if (rsm_leftclick_ || rsm_rightclick_ || rsm_middleclick_) {
				rsm_leftclick_ = rsm_rightclick_ = rsm_middleclick_ = 0;
				cmd_logf("ID: %"PRIu16", VAR: %"PRIu8", ROT: %"PRIu8", MTD: %"PRIu32"\n",
						lookingAt->id, lookingAt->variant, lookingAt->rotation, lookingAt->metadata);
				gui_updateGUI();
			}
			return;
	}

	/* World-affecting */
	if (rsm_leftclick_) { /* Block breaking */
		rsm_leftclick_ = 0;
		rsm_breakcoords(lookingBlockCoords_);
		return;
	}

	if (rsm_rightclick_) {
		rsm_rightclick_ = 0;

		if (rsm_forceplace_) goto place; /* Skip interactions */

		switch (lookingAt->id) { /* Interaction or placement */
			case RSM_BLOCK_BUFFER:
				lookingAt->variant = (lookingAt->variant + 2) % 8; /* Change delay, keep state */
				break;

			case RSM_BLOCK_AIR: /* Air placement */
				if (RSM_AIRPLACE) {
					/* Block placement works on adjacent */
					glm_vec3_copy(lookingBlockCoords_, adjacentBlockCoords_);
				} else return;
				goto place;

			default: goto place;
		}

		/* Interacted */
		cu_asyncRemeshChunk(lookingAtChunkindex); /* Remesh only this chunk (interaction spans 1 block) */
		wf_registercoords(lookingBlockCoords_); /* Register change with graph */
		return;

		/* Place */
place:
		rsm_placecoords(adjacentBlockCoords_, lookingBlockCoords_);
		return;
	}
}

void rsm_placecoords(vec3 coords, vec3 adjacent) {
	/* Places the currently held block at the specified position in the world */

	Blockdata *block, *neighbor, template;
	block = cu_coordsToBlock(coords, NULL);
	neighbor = cu_coordsToBlock(adjacent, NULL);

	u8 heldid, heldvariant; /* Held item */
	heldid = hotbar_[hotbarIndex_][hotslotIndex_][0];
	heldvariant = hotbar_[hotbarIndex_][hotslotIndex_][1];

	/* Create template */
	template.id = heldid;
	template.rotation = NONE; /* Default */
	switch (heldid) {
		case RSM_BLOCK_RESISTOR:
		case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
		case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
			/* Use metadata of base type for software-defined variants */
			template.metadata = usf_inthmget(datamap_, ASUID(heldid, 0)).u;
			template.variant = sspower_;
			break;

		case RSM_BLOCK_INVERTER:
			template.variant = lookingAxis_ == 1 ? 0 : 2; /* Kind depends on placement axis */
			template.metadata = usf_inthmget(datamap_, ASUID(heldid, template.variant)).u;
			/* Override rotation */
			if (lookingAxis_ == 0) template.rotation = stepAxis_ > 0 ? EAST : WEST; /* X-aligned */
			if (lookingAxis_ == 1) template.rotation = UP; /* Y-aligned */
			if (lookingAxis_ == 2) template.rotation = stepAxis_ > 0 ? SOUTH : NORTH; /* Z-aligned */
			break;

		default:
			template.metadata = usf_inthmget(datamap_, ASUID(heldid, heldvariant)).u;
			template.variant = heldvariant;
			break;
	}

	if (template.metadata & RSM_BIT_ROTATION) {
		/* Block rotates on placement so rotate it according to angle.
		 * 45 degree offset to align quadrants with orientation FOVs */
		f32 rot = fmodf((yaw_ - 45.0f), 360.0f);
		if (rot < 0.0f) rot = 360.0f - fabsf(rot);

		if (rot < 90.0f) template.rotation = WEST;
		else if (rot < 180.0f) template.rotation = NORTH;
		else if (rot < 270.0f) template.rotation = EAST;
		else template.rotation = SOUTH;
	}

	/* Prevent illegal placements (sidesupported already checks for axis in inverter template init */
	if (template.metadata & RSM_BIT_SIDESUPPORTED) /* Air placement or placement on non-full block */
		if (block == neighbor || !(neighbor->metadata & RSM_BIT_CULLFACES)) return;
	if (template.metadata & RSM_BIT_TOPSUPPORTED) /* Same restrictions, but must place down */
		if (lookingAxis_ != 1 || block == neighbor || !(neighbor->metadata & RSM_BIT_CULLFACES)) return;

	memcpy(block, &template, sizeof(Blockdata)); /* Actualize */
	wf_registercoords(adjacentBlockCoords_); /* Sim sync */

	usf_skiplist *toremesh;
	cu_deferArea(toremesh = usf_newsk(), coords[0], coords[1], coords[2]);
	cu_doRemesh(toremesh); usf_freesk(toremesh);
}

void rsm_breakcoords(vec3 coords) {
	/* Breaks the specified block in the world (replace with air) */

	Blockdata *block;
	block = cu_coordsToBlock(coords, NULL);

	memset(block, 0, sizeof(Blockdata)); /* Reset to air */
	wf_registercoords(coords); /* Sim sync */

	/* Dependent blocks */
#define TESTBLOCK(_DX, _DY, _DZ, _FLAG, _ROT) \
	do { \
		vec3 DCOORDS_; DCOORDS_[0] = (_DX); DCOORDS_[1] = (_DY); DCOORDS_[2] = (_DZ); \
		glm_vec3_add(DCOORDS_, coords, DCOORDS_); /* Offset */ \
		Blockdata *DBLOCK_ = cu_coordsToBlock(DCOORDS_, NULL); \
		\
		if ((DBLOCK_->metadata & _FLAG) && (DBLOCK_->rotation == _ROT)) { \
			memset(DBLOCK_, 0, sizeof(Blockdata)); /* Reset to air */ \
			wf_registercoords(DCOORDS_); /* Sim sync */ \
		} \
	} while (0);
	TESTBLOCK(0, 1, 0, RSM_BIT_TOPSUPPORTED, UP);
	TESTBLOCK(0, 1, 0, RSM_BIT_TOPSUPPORTED, NONE);
	TESTBLOCK(0, 0, -1, RSM_BIT_SIDESUPPORTED, SOUTH);
	TESTBLOCK(0, 0, 1, RSM_BIT_SIDESUPPORTED, NORTH);
	TESTBLOCK(-1, 0, 0, RSM_BIT_SIDESUPPORTED, EAST);
	TESTBLOCK(1, 0, 0, RSM_BIT_SIDESUPPORTED, WEST);
#undef TESTBLOCK

	/* Remesh (dependent also remeshed as they are adjacent) */
	usf_skiplist *toremesh;
	cu_deferArea(toremesh = usf_newsk(), coords[0], coords[1], coords[2]);
	cu_doRemesh(toremesh); usf_freesk(toremesh);
}

void rsm_checkMeshes(void) {
	/* Check if there are new meshes to be sent to the GPU */

	Rawmesh *rawmesh;
	while ((rawmesh = usf_dequeue(meshqueue_).p)) { /* For every new mesh this frame */
		Mesh *mesh;
		mesh = usf_inthmget(meshmap_, rawmesh->chunkindex).p;

		GLint VBO;
		glBindVertexArray(mesh->opaqueVAO); /* Opaque */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO); /* Query VBO */
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint) VBO);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (rawmesh->nOV * sizeof(f32)),
				rawmesh->opaqueVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (rawmesh->nOI * sizeof(u32)),
				rawmesh->opaqueIndexBuffer, GL_DYNAMIC_DRAW);

		glBindVertexArray(mesh->transVAO); /* Trans */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint) VBO);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (rawmesh->nTV * sizeof(f32)),
				rawmesh->transVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (rawmesh->nTI * sizeof(u32)),
				rawmesh->transIndexBuffer, GL_DYNAMIC_DRAW);
		glBindVertexArray(0); /* Unbind */

		/* Set mesh element counts */
		mesh->nOpaqueIndices = rawmesh->nOI;
		mesh->nTransIndices = rawmesh->nTI;

		/* Free scratchpad buffers */
		free(rawmesh->opaqueVertexBuffer); /* All scratchpads stem from this one allocation */
		free(rawmesh);

		cu_updateMeshlist(); /* Include just created mesh in view */
	}
}
