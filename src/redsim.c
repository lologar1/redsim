#include "redsim.h"

Gamestate gamestate = NORMAL;

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
	vec3 momentum;

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

	glm_vec3_copy(speed, momentum);
	glm_vec3_scale(momentum, deltaTime, momentum);
	glm_vec3_rotate(momentum, -glm_rad(yaw), upvector);

	/* Collision handling */
	Chunkdata *chunkdata;
	Blockdata *blockdata;
	float boundingbox[6];
	vec3 blockposition, offset, newPosition, playerCornerNew, playerCornerOld;
	mat4 rotation;

	/* Future position of the player */
	glm_vec3_add(position, momentum, newPosition);

	/* Position of the base of the player bounding box */
	glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, newPosition, playerCornerNew);
	glm_vec3_add(PLAYER_BOUNDINGBOX_RELATIVE_CORNER, position, playerCornerOld);

	/* Iterate through all blocks which might impede the player ; this algo. is inefficient for
	 * large bounding boxes because it also iterates on blocks inside, but it is easier to understand
	 * and implement and since the default player bounding box is small enough that it's the same, I'm
	 * using volumetric iteration */
	for (offset[0] = PLAYER_BOUNDINGBOX_DIMENSIONS[0]; offset[0] > -1.0f; offset[0]--) {
		for (offset[1] = PLAYER_BOUNDINGBOX_DIMENSIONS[1]; offset[1] > -1.0f; offset[1]--) {
			for (offset[2] = PLAYER_BOUNDINGBOX_DIMENSIONS[2]; offset[2] > -1.0f; offset[2]--) {
				/* Blockposition now contains the absolute world position of the block in question.
				 * To retrieve it, first get the chunk (and check if it exists) then the offset inside that */
				blockposition[0] = playerCornerNew[0] + offset[0];
				blockposition[1] = playerCornerNew[1] + offset[1];
				blockposition[2] = playerCornerNew[2] + offset[2];

				chunkdata = (Chunkdata *) usf_inthmget(chunkmap, TOCHUNKINDEX(
							chunkOffsetConvertFloat(blockposition[0]),
							chunkOffsetConvertFloat(blockposition[1]),
							chunkOffsetConvertFloat(blockposition[2]))).p;

				if (chunkdata == NULL) continue; /* Chunk is empty */

				blockdata = &(*chunkdata)
							[blockOffsetConvertFloat(blockposition[0])]
							[blockOffsetConvertFloat(blockposition[1])]
							[blockOffsetConvertFloat(blockposition[2])];

				if (!(blockdata->metadata & RSM_BIT_COLLISION)) continue; /* Block has no collision */

				/* Get bounding box data and check it against player bounding box */
				memcpy(boundingbox, boundingboxes[blockdata->id][blockdata->variant], sizeof(float [6]));
				boundingbox[0] += floor(blockposition[0]);
				boundingbox[1] += floor(blockposition[1]);
				boundingbox[2] += floor(blockposition[2]);

				if (blockdata->rotation != NONE) {
					/* Bounding boxes may be asymmetric */
					rotationMatrix(rotation, blockdata->rotation);
					glm_mat4_mulv3(rotation, boundingbox, 1.0f, boundingbox);
				}

				if (AABBIntersect(boundingbox, boundingbox+3, playerCornerNew,
							PLAYER_BOUNDINGBOX_DIMENSIONS) != -1) /* If all axes intersect */
					continue;

				/* Axis which will intersect in the future. Note that a perfect 45 degree angle can
				 * enable a player to phase through a box as only one axis is checked, but that is either
				 * too unlikely (and the player can get out safely) or simply not permitted as pitch/yaw are
				 * ultimately decoded from discrete mouse inputs, which, due to float precision,
				 * may not be able to produce this effect */
				int miss = AABBIntersect(boundingbox, boundingbox+3, playerCornerOld,
						PLAYER_BOUNDINGBOX_DIMENSIONS);

				if (miss == -1) continue; /* Moving inside a block ; let the player escape! */

				if (boundingbox[miss] + boundingbox[miss+3] < playerCornerOld[miss]) {
					/* Player is "above" */
					momentum[miss] = -(playerCornerOld[miss] - (boundingbox[miss] + boundingbox[miss+3]))
						+ BLOCK_BOUNDINGBOX_SAFE_DISTANCE;
				}
				else
					/* Player is "under */
					momentum[miss] = boundingbox[miss] - (playerCornerOld[miss] +
						PLAYER_BOUNDINGBOX_DIMENSIONS[miss]) - BLOCK_BOUNDINGBOX_SAFE_DISTANCE;

				/* Match speed to orientation, kill component and replace it */
				glm_vec3_rotate(speed, -glm_rad(yaw), upvector);
				speed[miss] = 0.0f; /* Kill speed for that component */
				glm_vec3_rotate(speed, glm_rad(yaw), upvector);
			}
		}
	}

	glm_vec3_add(position, momentum, position); /* This affects player position */
}

int AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2) {
	/* Returns either the axis which a bounding box doesn't intersect another (prio xyz)
	 * Or -1 if all axes intersect */
	if (corner1[0] + dim1[0] < corner2[0] || corner1[0] > corner2[0] + dim2[0]) return 0;
	if (corner1[1] + dim1[1] < corner2[1] || corner1[1] > corner2[1] + dim2[1]) return 1;
	if (corner1[2] + dim1[2] < corner2[2] || corner1[2] > corner2[2] + dim2[2]) return 2;
	return -1; /* Does intersect */
}

int64_t chunkOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its chunk offset equivalent */
	return (int64_t) floor(absoluteComponent / CHUNKSIZE);
}

uint64_t blockOffsetConvertFloat(float absoluteComponent) {
	/* Converts a float absoluteComponent to its block offset (within a chunk) equivalent */
	return absoluteComponent < 0 ?
		CHUNKSIZE - 1 - (uint64_t) fabsf(absoluteComponent) % CHUNKSIZE :
		(uint64_t) fabsf(absoluteComponent) % CHUNKSIZE;
}
