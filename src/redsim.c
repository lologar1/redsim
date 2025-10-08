#include "redsim.h"

Gamestate gamestate;

void rsm_move(vec3 position, vec3 orientation) {
	/* Change player position each frame according to movement) */

	/* Persistent momentum across multiple frames */
	static float momentumX = 0.0f, momentumY = 0.0f, momentumZ = 0.0f;

	/* Keep track of delta time passed since last frame */
	static float lastTime = 0.0f; /* OK since glfwGetTime() is seconds since init. */
	float deltaTime;

	deltaTime = glfwGetTime() - lastTime;
	lastTime = glfwGetTime();

	/* Get current speed */
	float cameraSpeedX, cameraSpeedY, cameraSpeedZ;
	cameraSpeedX = momentumX * deltaTime;
	cameraSpeedY = momentumY * deltaTime;
	cameraSpeedZ = momentumZ * deltaTime;

	/* Deltas to travel this frame */
	vec3 zMovement, xMovement, yMovement, momentum;

	/* Now update speed for next frame */

	/* X movement */
	if (RSM_RIGHT) momentumX = fmin(momentumX + RSM_FLY_X_ACCELERATION * deltaTime, RSM_FLY_X_CAP);
	if (RSM_LEFT) momentumX = fmax(momentumX - RSM_FLY_X_ACCELERATION * deltaTime, -RSM_FLY_X_CAP);
	if (!(RSM_RIGHT && RSM_LEFT)) momentumX = momentumX < 0.0f ?
		fmin(0.0f, momentumX + RSM_FLY_X_DECELERATION * deltaTime) :
		fmax(0.0f, momentumX - RSM_FLY_X_DECELERATION * deltaTime);

	/* Y movement */
	if (RSM_UP) momentumY = fmin(momentumY + RSM_FLY_Y_ACCELERATION * deltaTime, RSM_FLY_Y_CAP);
	if (RSM_DOWN) momentumY = fmax(momentumY - RSM_FLY_Y_ACCELERATION * deltaTime, -RSM_FLY_Y_CAP);
	if (!(RSM_UP && RSM_DOWN)) momentumY = momentumY < 0.0f ?
		fmin(0.0f, momentumY + RSM_FLY_Y_DECELERATION * deltaTime) :
		fmax(0.0f, momentumY - RSM_FLY_Y_DECELERATION * deltaTime);


	/* Z movement */
	if (RSM_FORWARD) momentumZ = fmin(momentumZ + RSM_FLY_Z_ACCELERATION * deltaTime, RSM_FLY_Z_CAP);
	if (RSM_BACKWARD) momentumZ = fmax(momentumZ - RSM_FLY_Z_ACCELERATION * deltaTime, -RSM_FLY_Z_CAP);
	if (!(RSM_FORWARD && RSM_BACKWARD)) momentumZ = momentumZ < 0.0f ?
		fmin(0.0f, momentumZ + RSM_FLY_Z_DECELERATION * deltaTime) :
		fmax(0.0f, momentumZ - RSM_FLY_Z_DECELERATION * deltaTime);

	/* Convert movement to momentum vector */
	glm_vec3_cross(orientation, upvector, xMovement); /* Get relative right vector */
	glm_vec3_normalize(xMovement);
	glm_vec3_scale(xMovement, cameraSpeedX, xMovement);

	glm_vec3_scale(upvector, cameraSpeedY, yMovement);

	glm_vec3_copy(orientation, zMovement);
	zMovement[1] = 0.0f; /* Lock vertical movement because template is orientation */
	glm_vec3_normalize(zMovement);
	glm_vec3_scale(zMovement, cameraSpeedZ, zMovement);

	/* Create combined movement vector */
	glm_vec3_copy(xMovement, momentum);
	glm_vec3_addadd(yMovement, zMovement, momentum);

	/* Collision handling */
	Chunkdata *chunkdata;
	Blockdata *blockdata;
	float boundingbox[6];
	vec3 blockposition, offset, newPosition, playerCornerNew, playerCornerOld, mask;
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
				mask[0] = floor(blockposition[0]); /* Temporary buffer to snap boundingbox to grid */
				mask[1] = floor(blockposition[1]);
				mask[2] = floor(blockposition[2]);
				glm_vec3_add(boundingbox, mask, boundingbox);

				if (blockdata->rotation != NONE) {
					/* Bounding boxes may be asymmetric */
					rotationMatrix(rotation, blockdata->rotation);
					glm_mat4_mulv3(rotation, boundingbox, 1.0f, boundingbox);
				}

				AABBIntersect(boundingbox, boundingbox+3, playerCornerNew, PLAYER_BOUNDINGBOX_DIMENSIONS, mask);

				if (!(mask[0] == 1.0f && mask[1] == 1.0f && mask[2] == 1.0f)) continue; /* No collision */

				AABBIntersect(boundingbox, boundingbox+3, playerCornerOld, PLAYER_BOUNDINGBOX_DIMENSIONS, mask);
				glm_vec3_mul(momentum, mask, momentum); /* Filter */
			}
		}
	}

	glm_vec3_add(position, momentum, position); /* This affects player position */
}

void AABBIntersect(vec3 corner1, vec3 dim1, vec3 corner2, vec3 dim2, vec3 mask) {
    mask[0] = !((corner1[0] + dim1[0] < corner2[0]) || (corner1[0] > corner2[0] + dim2[0])) ? 1.0f : 0.0f;
    mask[1] = !((corner1[1] + dim1[1] < corner2[1]) || (corner1[1] > corner2[1] + dim2[1])) ? 1.0f : 0.0f;
    mask[2] = !((corner1[2] + dim1[2] < corner2[2]) || (corner1[2] > corner2[2] + dim2[2])) ? 1.0f : 0.0f;
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
