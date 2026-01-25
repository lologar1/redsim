#include "redsim.h"

Gamestate gamestate_ = NORMAL;
i64 ret_selection_[6] = {0, 0, 0, 1, 1, 1};
i64 ret_positions_[6] = {0, 0, 0, 0, 0, 0};

static vec3 lookingBlockCoords_, adjacentBlockCoords_;
static i64 lookingAxis_, stepAxis_;

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
	for (axis = 0; axis < 3; axis++) {
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
	sa = ret_selection_[0]; sb = ret_selection_[1]; sc = ret_selection_[2];
	sx = ret_selection_[3]; sy = ret_selection_[4]; sz = ret_selection_[5];

	/* Block highlighting coords and dimensions */
	vec3 looking, pos;
	glm_vec3_copy(position_, looking); glm_vec3_copy(position_, pos);
	glm_vec3_muladds(orientation_, RSM_REACH, looking);

	/* 3D Bresenham's */
	vec3 gridpos, direction;
	glm_vec3_floor(pos, gridpos);
	glm_vec3_sub(looking, pos, direction);

#define _STEP(_I) direction[_I] > 0 ? 1.0f : -1.0f
	vec3 step = { _STEP(0), _STEP(1), _STEP(2) };
#undef _STEP

	/* tDelta is the step size (in tspace) per unit moved in realspace */
#define _TDELTA(_I) direction[_I] == 0 ? 0.0f : fabsf(1.0f / direction[_I]) /* 0.0f changed to max tMax below */
	vec3 tDelta = { _TDELTA(0), _TDELTA(1), _TDELTA(2) };
#undef _TDELTA

	/* tMax is the current progress (from 0.0f to 1.0f) until we reach looking in tspace */
#define _TMAX(_I) tDelta[_I] ? \
	(tDelta[_I] * ((step[_I] > 0 ? (gridpos[_I]+1) - pos[_I] : pos[_I] - gridpos[_I]))) : 1.0f
	vec3 tMax = { _TMAX(0), _TMAX(1), _TMAX(2) };
#undef _TMAX

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
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
	wiremesh_[1] = sizeof(indices)/sizeof(indices[0]);
}

void rsm_interact(void) {
	/* Place, destroy or interact with the block at lookingAt */

	Blockdata *lookingAt, *lookingAdjacent;
	lookingAt = cu_coordsToBlock(lookingBlockCoords_, NULL);
	lookingAdjacent = cu_coordsToBlock(adjacentBlockCoords_, NULL);

	if (rsm_middleclick_) { /* Pipette tool */
		rsm_middleclick_ = 0;

		hotbar_[hotbarIndex_][hotslotIndex_][0] = lookingAt->id;
		hotbar_[hotbarIndex_][hotslotIndex_][1] = /* Disallow illegal sprites */
			(lookingAt->variant >= MAX_BLOCK_VARIANT[lookingAt->id]
			|| SPRITEIDS[lookingAt->id][lookingAt->variant] == 0) ? 0 : lookingAt->variant;

		gui_updateGUI();
		return;
	}

	u64 id, variant, uid;
	id = hotbar_[hotbarIndex_][hotslotIndex_][0];
	variant = hotbar_[hotbarIndex_][hotslotIndex_][1];
	uid = ASUID(id, variant);
	switch (uid) { /* Tools */
		case ASUID(RSM_SPECIAL_ID, RSM_SPECIAL_SELECTIONTOOL): /* Selection tool */
#define _VTOI(_TO, _FROM) (_TO)[0] = (i64) (_FROM)[0]; (_TO)[1] = (i64) (_FROM)[1]; (_TO)[2] = (i64) (_FROM)[2];
			if (rsm_leftclick_) {
				rsm_leftclick_ = 0;
				_VTOI(ret_positions_, lookingBlockCoords_);
			} else if (rsm_rightclick_) {
				rsm_rightclick_ = 0;
				_VTOI(ret_positions_ + 3, lookingBlockCoords_);
			}
#undef _VTOI

			i64 i, minpos, maxpos;
			for (i = 0; i < 3; i++) { /* Create selection */
				maxpos = USF_MAX(ret_positions_[i], ret_positions_[i + 3]);
				minpos = USF_MIN(ret_positions_[i], ret_positions_[i + 3]);

				ret_selection_[i] = minpos;
				ret_selection_[i + 3] = maxpos - minpos + 1;
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

	if (rsm_leftclick_) { /* Normal block breaking */
		rsm_leftclick_ = 0; /* Consume */

		memset(lookingAt, 0, sizeof(Blockdata)); /* Reset to air */

		Blockdata *dblock;
		u64 dindex;
#define TESTBLOCK(X, Y, Z, FLAG, ROT) \
	dblock = cu_posToBlock(lookingBlockCoords_[0]+X,lookingBlockCoords_[1]+Y,lookingBlockCoords_[2]+Z,&dindex); \
	if ((dblock->metadata & FLAG) && dblock->rotation == ROT) memset(dblock, 0, sizeof(Blockdata)); /* Destroy */
		TESTBLOCK(0, 1, 0, RSM_BIT_TOPSUPPORTED, NONE);
		TESTBLOCK(0, 0, 1, RSM_BIT_SIDESUPPORTED, SOUTH);
		TESTBLOCK(0, 0, -1, RSM_BIT_SIDESUPPORTED, NORTH);
		TESTBLOCK(1, 0, 0, RSM_BIT_SIDESUPPORTED, EAST);
		TESTBLOCK(-1, 0, 0, RSM_BIT_SIDESUPPORTED, WEST);
#undef TESTBLOCK
		goto remesh;
	}

	/* Block placement & interaction */
	u64 metadata;
	switch (id) { /* Special software-determined variants don't fit the usual mold */
		case RSM_BLOCK_RESISTOR:
		case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
		case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
			metadata = usf_inthmget(datamap_, ASUID(id, 0)).u;
			break;
		default:
			metadata = usf_inthmget(datamap_, uid).u; /* 0 if not specified */
			break;
	}

	if (rsm_rightclick_) {
		rsm_rightclick_ = 0;

		if (rsm_forceplace_) goto place;

		switch (lookingAt->id) { /* Block interaction */
			case RSM_BLOCK_BUFFER:
				lookingAt->variant = (lookingAt->variant + 2) % 8; /* Change delay (leave powered state alone) */
				goto remesh;
			case RSM_BLOCK_AIR: /* Air placement */
				if (RSM_AIRPLACE) {
					lookingAdjacent = lookingAt; /* Place there instead */
					glm_vec3_copy(lookingBlockCoords_, adjacentBlockCoords_);
				} else return;
		}

place:
		if (lookingAdjacent == NULL) return; /* May happen right after init if spawned in a block */
		if (id == 0) return; /* Shouldn't place air or items */

		switch (id) { /* Handle software-defined variants */
			case RSM_BLOCK_RESISTOR:
			case RSM_BLOCK_CONSTANT_SOURCE_OPAQUE:
			case RSM_BLOCK_CONSTANT_SOURCE_TRANS:
				lookingAdjacent->variant = sspower_;
				break;

			case RSM_BLOCK_INVERTER: /* Prevent placing in air and find appropriate variant */
				if (lookingAt == lookingAdjacent || !(lookingAt->metadata & RSM_BIT_CULLFACES)) /* In air */
					return;

				lookingAdjacent->variant = lookingAxis_ == 1 ? 0 : 2; /* Fix variant & metadata */
				metadata = usf_inthmget(datamap_, ASUID(id, lookingAdjacent->variant)).u;

				switch (lookingAxis_) { /* Manual rotation since it is determined by looking block face */
					case 0:
						lookingAdjacent->rotation = stepAxis_ > 0 ? WEST : EAST;
						break;
					case 2:
						lookingAdjacent->rotation = stepAxis_ > 0 ? NORTH : SOUTH;
						break;
					default:
						lookingAdjacent->rotation = NONE;
						break;
				}
				break;

			/* All topsupported blocks */
			case RSM_BLOCK_TRANSISTOR_ANALOG:
			case RSM_BLOCK_TRANSISTOR_DIGITAL:
			case RSM_BLOCK_LATCH:
			case RSM_BLOCK_WIRE:
			case RSM_BLOCK_BUFFER:
				if (lookingAxis_ != 1 || lookingAt==lookingAdjacent || !(lookingAt->metadata&RSM_BIT_CULLFACES))
					return;
				[[fallthrough]];
			default:
				lookingAdjacent->variant = GETVARIANT(uid);
				break;
		}

		lookingAdjacent->id = id;
		lookingAdjacent->metadata = metadata;

		if (metadata & RSM_BIT_ROTATION) {
			/* Block is rotatable so rotate it according to placement angle.
			 * 45 degree offset because quadrants do not align with orientation placement fields. */
			f32 rot = fmodf((yaw_ - 45.0f), 360.0f);
			if (rot < 0.0f) rot = 360.0f - fabsf(rot);

			if (rot < 90.0f) lookingAdjacent->rotation = WEST;
			else if (rot < 180.0f) lookingAdjacent->rotation = NORTH;
			else if (rot < 270.0f) lookingAdjacent->rotation = EAST;
			else lookingAdjacent->rotation = SOUTH;
		} /* Rotation NONE (zero) by default */

remesh:
		usf_skiplist *toRemesh;
		toRemesh = usf_newsk();

		/* Remesh all chunks in a 3x3 grid to account for other blocks' meshes changing from the new block */
		i32 a, b, c;
		u64 chunkindex;
		for (a = -1; a < 2; a++) for (b = -1; b < 2; b++) for (c = -1; c < 2; c++) {
			cu_posToBlock(lookingBlockCoords_[0] + a, lookingBlockCoords_[1] + b, lookingBlockCoords_[2] + c,
					&chunkindex);
			usf_skset(toRemesh, chunkindex, USFTRUE);
		}

		usf_skipnode *n;
		for (n = toRemesh->base[0]; n; n = n->nextnodes[0]) cu_asyncRemeshChunk(n->index);
		usf_freesk(toRemesh);
	}
}

void rsm_checkMeshes(void) {
	/* Check if there are new meshes to be sent to the GPU */

	Rawmesh *rawmesh;
	u64 chunkindex;
	GLuint *mesh;
	GLint VBO;
	while ((rawmesh = (Rawmesh *) usf_dequeue(meshqueue_).p)) { /* For every new mesh this frame */
		chunkindex = rawmesh->chunkindex;

		if ((mesh = usf_inthmget(meshmap_, chunkindex).p) == NULL) { /* Load mesh and check its existence */
			/* Initialize GL buffers for the mesh */
			GLuint opaqueVAO, transVAO, opaqueVBO, transVBO, opaqueEBO, transEBO;
			mesh = malloc(4 * sizeof(GLuint));

			/* Generate buffers */
			glGenVertexArrays(1, &opaqueVAO); glGenVertexArrays(1, &transVAO);
			glGenBuffers(1, &opaqueVBO); glGenBuffers(1, &transVBO);
			glGenBuffers(1, &opaqueEBO); glGenBuffers(1, &transEBO);

			/* Set attributes */
			glBindVertexArray(opaqueVAO);
			glBindBuffer(GL_ARRAY_BUFFER, opaqueVBO); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, opaqueEBO);
			glEnableVertexAttribArray(0); /* Vertex position */
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (0 * sizeof(f32)));
			glEnableVertexAttribArray(1); /* Normals */
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (3 * sizeof(f32)));
			glEnableVertexAttribArray(2); /* Texture mappings */
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (6 * sizeof(f32)));
			glBindVertexArray(transVAO);
			glBindBuffer(GL_ARRAY_BUFFER, transVBO); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, transEBO);
			glEnableVertexAttribArray(0); /* Vertex position */
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (0 * sizeof(f32)));
			glEnableVertexAttribArray(1); /* Normals */
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (3 * sizeof(f32)));
			glEnableVertexAttribArray(2); /* Texture mappings */
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void *) (6 * sizeof(f32)));

			glBindVertexArray(0); /* Unbind to avoid modification */
			mesh[0] = opaqueVAO; mesh[1] = transVAO;
			usf_inthmput(meshmap_, chunkindex, USFDATAP(mesh)); /* Set mesh */
		}

		glBindVertexArray(mesh[0]); /* Opaque */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO); /* Query VBO */
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, rawmesh->nOV*sizeof(f32), rawmesh->opaqueVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rawmesh->nOI * sizeof(u32), rawmesh->opaqueIndexBuffer, GL_DYNAMIC_DRAW);

		glBindVertexArray(mesh[1]); /* Trans */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, rawmesh->nTV * sizeof(f32), rawmesh->transVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rawmesh->nTI * sizeof(u32), rawmesh->transIndexBuffer, GL_DYNAMIC_DRAW);
		glBindVertexArray(0); /* Unbind */

		/* Set mesh element counts */
		mesh[2] = rawmesh->nOI; mesh[3] = rawmesh->nTI;

		/* Free scratchpad buffers */
		free(rawmesh->opaqueVertexBuffer); /* All scratchpads stem from this one allocation */
		free(rawmesh);

		cu_updateMeshlist(); /* Include just created mesh in view */
	}
}
