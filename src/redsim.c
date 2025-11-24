#include "redsim.h"

usf_hashmap *datamap;

Gamestate gamestate = NORMAL;
Blockdata *lookingAt, *lookingAdjacent;
uint64_t lookingChunkIndex, lookingAdjChunkIndex;
GLuint wiremesh[2];

vec3 *playerBBOffsets;
unsigned int nPlayerBBOffsets;

void rsm_move(vec3 position) {
	/* Change player position each frame according to movement) */

	/* Persistent momentum across multiple frames */
	static vec3 speed = {0.0f, 0.0f, 0.0f};

	/* Keep track of delta time passed since last frame */
	static float lastTime = 0.0f; /* OK since glfwGetTime() is seconds since init. */
	float deltaTime;

	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	/* Deltas to travel this frame */
	vec3 movement;

	/* Now update speed for next frame */
	if (RSM_RIGHT) speed[0] += RSM_FLY_X_ACCELERATION * deltaTime;
	if (RSM_LEFT) speed[0] -= RSM_FLY_X_ACCELERATION * deltaTime;
	if (RSM_UP) speed[1] += RSM_FLY_Y_ACCELERATION * deltaTime;
	if (RSM_DOWN) speed[1] -= RSM_FLY_Y_ACCELERATION * deltaTime;
	if (RSM_BACKWARD) speed[2] += RSM_FLY_Z_ACCELERATION * deltaTime;
	if (RSM_FORWARD) speed[2] -= RSM_FLY_Z_ACCELERATION * deltaTime;

	/* Cap speed */
	if (glm_vec3_norm(speed) > RSM_FLY_SPEED_CAP) glm_vec3_scale_as(speed, RSM_FLY_SPEED_CAP, speed);

	/* Air deceleration */
	glm_vec3_scale_as(speed, glm_vec3_norm(speed) * powf(RSM_FLY_FRICTION, deltaTime), speed);

	glm_vec3_copy(speed, movement);
	glm_vec3_scale(movement, deltaTime, movement);
	glm_vec3_rotate(movement, -glm_rad(yaw), UPVECTOR);

	/* Collision handling */
	Blockdata *blockdata;
	float boundingbox[6];
	vec3 blockposition, *offset, newPosition, playerCornerNew, playerCornerOld, boxcenter;
	mat4 rotation;
	int axis;

	/* Old player position bounding box base */
	glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, position, playerCornerOld);

	/* Move in X, Y and Z to prevent edge collision bugs. This does mean that the player needs a certain
	 * clearance to move which depends on the framerate, but this value should be small at normal FPS and
	 * the normal player BB size isn't a multiple of 1 so it's fine */
	for (axis = 0; axis < 3; axis++) {
		/* Future position of the player */
		glm_vec3_copy(position, newPosition); newPosition[axis] += movement[axis];
		glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, newPosition, playerCornerNew);

		for (offset = playerBBOffsets; offset - playerBBOffsets < nPlayerBBOffsets; offset++) {
			/* Blockposition now contains the absolute world position of the block in question.
			 * To retrieve it, first get the chunk (and check if it exists) then the offset inside that */
			glm_vec3_add(playerCornerNew, *offset, blockposition);

			blockdata = coordsToBlock(blockposition, NULL);
			if (!(blockdata->metadata & RSM_BIT_COLLISION)) continue; /* Block has no collision */

			/* Get bounding box data and check it against player bounding box */
			memcpy(boundingbox, boundingboxes[blockdata->id][blockdata->variant], sizeof(float [6]));
			glm_vec3_floor(blockposition, blockposition);
			glm_vec3_add(boundingbox, blockposition, boundingbox);

			/* To prevent phasing through a float imprecision-induced offset boundingbox poking in an
			 * adjacent block, downsize it by an adjusted offset first */
			glm_vec3_adds(boundingbox, BLOCK_BOUNDINGBOX_ADJUST_OFFSET, boundingbox);
			glm_vec3_subs(boundingbox+3, BLOCK_BOUNDINGBOX_ADJUST_OFFSET, boundingbox+3);

			if (blockdata->rotation > NORTH) { /* NONE is 0, and both NORTH and NONE do not modify base model */
				/* Find diametrically opposite corner */
				glm_vec3_add(boundingbox, boundingbox+3, boundingbox+3);

				glm_vec3_adds(boundingbox, 0.5f, boxcenter);
				rotationMatrix(rotation, blockdata->rotation, boxcenter);
				glm_mat4_mulv3(rotation, boundingbox, 1.0f, boundingbox);
				glm_mat4_mulv3(rotation, boundingbox+3, 1.0f, boundingbox+3);

				/* Find new dimensions */
				glm_vec3_sub(boundingbox+3, boundingbox, boundingbox+3);
			}

			/* Landing position is not inside a block; no collision occurs */
			if (!AABBIntersect(boundingbox, boundingbox+3, playerCornerNew, PLAYER_BOUNDINGBOX_DIMENSIONS))
				continue;

			/* Find axis which did not match before movement (axis to cancel movement on) */
			if (AABBIntersect(boundingbox, boundingbox+3, playerCornerOld, PLAYER_BOUNDINGBOX_DIMENSIONS))
				continue; /* Player is moving inside block; do not block movement */

			if (boundingbox[axis] + boundingbox[axis+3] < playerCornerOld[axis]) {
				/* Player is "above" */
				movement[axis] = -(playerCornerOld[axis] - (boundingbox[axis] + boundingbox[axis+3]))
					+ BLOCK_BOUNDINGBOX_SAFE_DISTANCE;
			} else {
				/* Player is "under */
				movement[axis] = boundingbox[axis] - (playerCornerOld[axis] +
					PLAYER_BOUNDINGBOX_DIMENSIONS[axis]) - BLOCK_BOUNDINGBOX_SAFE_DISTANCE;
			}

			/* Match speed to orientation, kill component and replace it */
			glm_vec3_rotate(speed, -glm_rad(yaw), UPVECTOR);
			speed[axis] = 0.0f; /* Kill speed for that component */
			glm_vec3_rotate(speed, glm_rad(yaw), UPVECTOR);
		}
	}

	glm_vec3_add(position, movement, position); /* This affects player position */
}

int AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2) {
	/* Returns whether or not two 3D boxes intersect on all 3 axes */

	/* First normalize dimensions and corner */
#define NORMALIZEDIM(dim, corner, i) if (dim[i] < 0.0f) { dim[i] = fabsf(dim[i]); corner[i] -= dim[i]; }
	NORMALIZEDIM(dim1, corner1, 0); NORMALIZEDIM(dim1, corner1, 1); NORMALIZEDIM(dim1, corner1, 2);
	NORMALIZEDIM(dim2, corner2, 0); NORMALIZEDIM(dim2, corner2, 1); NORMALIZEDIM(dim2, corner2, 2);

#define AXISCOMPARE(i) \
	if ((corner1[i]) + (dim1[i]) < (corner2[i]) || (corner1[i]) > (corner2[i]) + (dim2[i])) return 0;
	AXISCOMPARE(0); AXISCOMPARE(1); AXISCOMPARE(2);

	return 1;
}

int64_t chunkOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its chunk offset equivalent */
	return (int64_t) floorf(absoluteComponent / CHUNKSIZE);
}

uint64_t blockOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its block offset (within a chunk) equivalent */
	return absoluteComponent < 0 ?
		CHUNKSIZE - 1 - ((uint64_t) -floorf(absoluteComponent) - 1) % CHUNKSIZE :
		(uint64_t) absoluteComponent % CHUNKSIZE;
}

void initWiremesh(void) {
	GLuint wiremeshVBO, wiremeshEBO;

	glGenVertexArrays(1, wiremesh); /* VAO at index 0 */
	glGenBuffers(1, &wiremeshVBO);
	glGenBuffers(1, &wiremeshEBO);

	glBindVertexArray(*wiremesh);
	glBindBuffer(GL_ARRAY_BUFFER, wiremeshVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wiremeshEBO);

	/* Need to match attributes with normal rendering as we use the same shaders (weird, but simpler this way
	 * as wiremeshes are used only for rendering highlighting) */
	glEnableVertexAttribArray(0); /* Vertex position */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1); /* Normals */
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
	glEnableVertexAttribArray(2); /* Texture mappings */
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

	glBindVertexArray(0);
}

void rsm_updateWiremesh(void) {
	/* Update wiremesh to reflect current block highlighting and selection. Note that the colors are taken from
	 * the air.png (or whatever name ID 0 is) as UID 0:0 is the only ID 0 which loads a texture (for this
	 * purpose). By convention, the left side is selection while the right side is block highlighting */
	float sx, sy, sz, sa, sb, sc;

	/* Selection coords and dimensions */
	sa = selection[0]; sb = selection[1]; sc = selection[2];
	sx = selection[3] - sa; sy = selection[4] - sb; sz = selection[5] - sc;

	/* Block highlighting coords and dimensions */
	vec3 looking, pos;
	glm_vec3_copy(position, looking); glm_vec3_copy(position, pos);
	glm_vec3_muladds(orientation, RSM_REACH, looking);

	/* 3D Bresenham's */
	vec3 gridpos, direction;
	glm_vec3_floor(pos, gridpos);
	glm_vec3_sub(looking, pos, direction);

#define STEP(i) direction[i] > 0 ? 1.0f : -1.0f
	vec3 step = { STEP(0), STEP(1), STEP(2) };

	/* tDelta is the step size (in tspace) per unit moved in realspace */
#define TDELTA(i) direction[i] == 0 ? INFINITY : fabsf(1.0f / direction[i])
	vec3 tDelta = { TDELTA(0), TDELTA(1), TDELTA(2) };

	/* tMax is the current progress (from 0.0f to 1.0f) until we reach looking in tspace */
#define TMAX(i) tDelta[i] * ((step[i] > 0 ? (gridpos[i] + 1) - pos[i] : pos[i] - gridpos[i]))
	vec3 tMax = { TMAX(0), TMAX(1), TMAX(2) };

	int axis;
	do {
		lookingAt = coordsToBlock(gridpos, &lookingChunkIndex);

		if (lookingAt->id) goto brk; /* Block exists and isn't air */

		/* Determine which axis is crossed next, update t */
		if (tMax[0] < tMax[1]) axis = (tMax[0] < tMax[2]) ? 0 : 2;
		else axis = (tMax[1] < tMax[2]) ? 1 : 2;

		/* Update face position with the one we just entered */
		lookingAdjChunkIndex = lookingChunkIndex;
		lookingAdjacent = lookingAt;

		gridpos[axis] += step[axis];
		tMax[axis] += tDelta[axis];
	} while (glm_vec3_min(tMax) < 1.0f);
	/* Once more to catch final step */
	lookingAt = coordsToBlock(gridpos, &lookingChunkIndex);
brk:

	/* Normals set to point upwards as to not be illegal (still processed by opaque fragment shader) */
	float vertices[8 * 8 * 2] = {
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

	unsigned int indices[3 * 8 * 2] = {
		0, 1, 1, 2, 2, 3, 3, 0, /* Bottom square */
		4, 5, 5, 6, 6, 7, 7, 4, /* Top square */
		0, 4, 1, 5, 2, 6, 3, 7, /* Side squares */

		8, 9, 9, 10, 10, 11, 11, 8,
		12, 13, 13, 14, 14, 15, 15, 12,
		8, 12, 9, 13, 10, 14, 11, 15
	};

	/* Dump to GL buffers */
	GLint buf;
	glBindVertexArray(wiremesh[0]);
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buf);
	glBindBuffer(GL_ARRAY_BUFFER, buf);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
	glBindVertexArray(0);

	wiremesh[1] = sizeof(indices)/sizeof(unsigned int);
}

void rsm_interact(void) {
	/* Place, destroy or interact with the block at lookingAt */
	uint64_t uid, metadata;
	uid = ASUID(hotbar[hotbarIndex][hotslotIndex][0], hotbar[hotbarIndex][hotslotIndex][1]);

	if (RSM_LEFTCLICK) {
		RSM_LEFTCLICK = 0; /* Consume */
		memset(lookingAt, 0, sizeof(Blockdata)); /* Reset to air */
		async_remeshChunk(lookingChunkIndex);
		return;
	}

	metadata = usf_inthmget(datamap, uid).u; /* 0 if not specified */
	if (RSM_RIGHTCLICK) {
		RSM_RIGHTCLICK = 0;

		if (lookingAt->id) {
			/* TODO block interaction */
		} else lookingAdjacent = lookingAt; /* Place there instead */

		if (lookingAdjacent == NULL) return; /* May happen right after init if spawned in a block */

		if ((lookingAdjacent->id = GETID(uid)) == 0) return; /* Avoid littering when 'placing' items (id 0) */
		lookingAdjacent->variant = GETVARIANT(uid);
		lookingAdjacent->metadata = metadata;

		if (metadata & RSM_BIT_ROTATION) {
			/* Block is rotatable so rotate it according to placement angle.
			 * 45 degree offset because quadrants do not align with orientation placement fields. */
			float rot = fmodf((yaw - 45.0f), 360.0f);
			if (rot < 0.0f) rot = 360.0f - fabsf(rot);

			if (rot < 90.0f) lookingAdjacent->rotation = WEST;
			else if (rot < 180.0f) lookingAdjacent->rotation = NORTH;
			else if (rot < 270.0f) lookingAdjacent->rotation = EAST;
			else lookingAdjacent->rotation = SOUTH;
		} else lookingAdjacent->rotation = NONE;

		async_remeshChunk(lookingAdjChunkIndex);
		async_remeshChunk(lookingChunkIndex);
	}

	if (RSM_MIDDLECLICK && lookingAt->id) { /* Pipette tool; don't consume as it only modifies a hotbar slot */
		hotbar[hotbarIndex][hotslotIndex][0] = lookingAt->id;
		hotbar[hotbarIndex][hotslotIndex][1] = lookingAt->variant;
		updateGUI();
	}
}

void rsm_checkMeshes(void) {
	/* Check if there are new meshes to be sent to the GPU */
	Rawmesh *rawmesh;
	uint64_t chunkindex;
	GLuint *mesh;
	GLint VBO;

	pthread_mutex_lock(&meshlock);

	while ((rawmesh = (Rawmesh *) usf_dequeue(meshqueue).p)) { /* For every new mesh this frame */
		chunkindex = rawmesh->chunkindex;
		mesh = (GLuint *) usf_inthmget(meshmap, chunkindex).p;

		if (mesh == NULL) {
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
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
			glEnableVertexAttribArray(1); /* Normals */
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
			glEnableVertexAttribArray(2); /* Texture mappings */
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));
			glBindVertexArray(transVAO);
			glBindBuffer(GL_ARRAY_BUFFER, transVBO); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, transEBO);
			glEnableVertexAttribArray(0); /* Vertex position */
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) 0);
			glEnableVertexAttribArray(1); /* Normals */
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (3 * sizeof(float)));
			glEnableVertexAttribArray(2); /* Texture mappings */
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *) (6 * sizeof(float)));

			glBindVertexArray(0); /* Unbind to avoid modification */
			mesh[0] = opaqueVAO; mesh[1] = transVAO;
			usf_inthmput(meshmap, chunkindex, USFDATAP(mesh)); /* Set mesh */

			updateMeshlist(); /* Include just created mesh in view */
		}

		glBindVertexArray(mesh[0]); /* Opaque */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO); /* Query VBO */
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, rawmesh->nOV*sizeof(float), rawmesh->opaqueVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rawmesh->nOI * sizeof(unsigned int), rawmesh->opaqueIndexBuffer, GL_DYNAMIC_DRAW);
		glBindVertexArray(mesh[1]); /* Trans */
		glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, rawmesh->nTV * sizeof(float), rawmesh->transVertexBuffer, GL_DYNAMIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, rawmesh->nTI * sizeof(unsigned int), rawmesh->transIndexBuffer, GL_DYNAMIC_DRAW);
		glBindVertexArray(0); /* Unbind */

		/* Set mesh element counts */
		mesh[2] = rawmesh->nOI; mesh[3] = rawmesh->nTI;

		/* Free scratchpad buffers */
		free(rawmesh->opaqueVertexBuffer); /* All scratchpads stem from this one allocation */
		free(rawmesh);
	}

	pthread_mutex_unlock(&meshlock);
}
