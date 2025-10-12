#include "gui.h"

GLuint guiAtlas;
GLuint guiVBO, guiEBO, guiVAO;
unsigned int nGUIIndices;

void initGUI(void) {
	/* Create the appropriate OpenGL structures for the GUI */
	nGUIIndices = 0;
	glGenVertexArrays(1, &guiVAO);
	glGenBuffers(1, &guiVBO);
	glGenBuffers(1, &guiEBO);

	/* Attributes */
	glBindVertexArray(guiVAO);
	glBindBuffer(GL_ARRAY_BUFFER, guiVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, guiEBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));

	glBindVertexArray(0);
}

void parseGUIdata(void) {
	/* Create GUI texture atlas and parse GUI elements from disk */
	char guiPath[sizeof(textureBasePath) + sizeof(textureGuiPath)];
	char guimapPath[sizeof(textureBasePath) + sizeof(textureGuimapPath)];

	strcpy(guiPath, textureBasePath);
	strcat(guiPath, textureGuiPath);

	strcpy(guimapPath, textureBasePath);
	strcat(guimapPath, textureGuimapPath);

	char *guimap, **elements, *element;
	uint64_t nelement, nelements;

	guimap = usf_ftos(guimapPath, "r", NULL);
	elements = usf_scsplit(guimap, '\n', &nelements);
	nelements--; /* One more because terminating \n is included as separator */

	char guitexturepath[RSM_MAX_PATH_NAME_LENGTH];
	unsigned char *guiAtlasData = NULL;
	GLsizei guiAtlasSize = 0;

	for (nelement = 0; nelement < nelements; nelement++) {
		element = elements[nelement];

		/* Append texture for this element to the atlas */
		if (sizeof(guiPath) + strlen(element) + sizeof(textureFormatExtension) > RSM_MAX_PATH_NAME_LENGTH) {
			fprintf(stderr, "GUI texture name too long at %s exceeding %u (with extensions), aborting.\n",
					element, RSM_MAX_PATH_NAME_LENGTH);
			exit(RSM_EXIT_EXCBUF);
		}

		strcpy(guitexturepath, guiPath);
		strcat(guitexturepath, element);
		strcat(guitexturepath, textureFormatExtension);

		atlasAppend(guitexturepath, RSM_GUI_TEXTURE_SIZE_PIXELS, &guiAtlasData, &guiAtlasSize);
	}

	free(elements);
	free(guimap); /* Will free the char **elements split substrings */

	glGenTextures(1, &guiAtlas);
	glBindTexture(GL_TEXTURE_2D, guiAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_GUI_TEXTURE_SIZE_PIXELS, guiAtlasSize / (4 * RSM_GUI_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, guiAtlasData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);

	free(guiAtlasData);
}

float atlasAdjust(float y, GUIPriority priority) {
	return (y * RSM_GUI_TEXTURE_SIZE_PIXELS + RSM_GUI_TEXTURE_SIZE_PIXELS * priority)
		/ (MAX_GUI_PRIORITY * RSM_GUI_TEXTURE_SIZE_PIXELS);
}

void meshAppend(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei,
		float v[], unsigned int i[], size_t sv, size_t si) {
	*vertices = realloc(*vertices, *sizev * sizeof(float) + sv);
	*indices = realloc(*indices, *sizei * sizeof(unsigned int) + si);

	memcpy(*vertices + *sizev, v, sv);
	for (unsigned int j = 0; j < si/sizeof(unsigned int); j++)
		(*indices + *sizei)[j] = i[j] + (*sizev / 5);

	*sizev += sv/sizeof(float);
	*sizei += si/sizeof(unsigned int);
}

void renderGUI(void) {
	/* Redraw the GUI. Every GUI element is currently encoded in this .c file.
	 * As remeshing the GUI is an inefficient process (dynamic arrays) it is better to only call
	 * this function when necessary (e.g. user input which may modify GUI) rather than every frame.
	 * CURRENT GUI ELEMENTS :
	 * Crosshair
	 * Hotbar */

	/* Temporary 'dynamic' buffers for GUI */
	unsigned int *indices = NULL;
	float *vertices = NULL;
	unsigned int sizei, sizev; sizei = sizev = 0;

	renderCrosshair(&vertices, &sizev, &indices, &sizei);
	renderHotbar(&vertices, &sizev, &indices, &sizei);

	/* Dump to GL buffers */
	glBindVertexArray(guiVAO);
	glBindBuffer(GL_ARRAY_BUFFER, guiVBO);
	glBufferData(GL_ARRAY_BUFFER, sizev * sizeof(float), vertices, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizei * sizeof(unsigned int), indices, GL_DYNAMIC_DRAW);
	glBindVertexArray(0);

	nGUIIndices = sizei;

	free(indices);
	free(vertices);
}

void renderCrosshair(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei) {
	/* Draw crosshair */
	float v[] = {
		WINDOW_WIDTH/2 - 32.0f, WINDOW_HEIGHT/2 - 32.0f, pCrosshair, 0.0f, atlasAdjust(0.0f, pCrosshair),
		WINDOW_WIDTH/2 - 32.0f, WINDOW_HEIGHT/2 + 32.0f, pCrosshair, 0.0f, atlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + 32.0f, WINDOW_HEIGHT/2 + 32.0f, pCrosshair, 1.0f, atlasAdjust(1.0f, pCrosshair),
		WINDOW_WIDTH/2 + 32.0f, WINDOW_HEIGHT/2 - 32.0f, pCrosshair, 1.0f, atlasAdjust(0.0f, pCrosshair),
	};

	unsigned int i[] = {
		0, 1, 2,
		0, 3, 2
	};

	meshAppend(vertices, sizev, indices, sizei, v, i, sizeof(v), sizeof(i));
}

void renderHotbar(float **vertices, unsigned int *sizev, unsigned int **indices, unsigned int *sizei) {
	/* Draw hotbar */
	float v[(2 * RSM_HOTBAR_SLOTS + 2) * 5];

	for (int i = 0; i < RSM_HOTBAR_SLOTS + 1; i++) {
		int offset = i * 10;
		v[offset + 0] = WINDOW_WIDTH/2 - RSM_GUI_TEXTURE_SIZE_PIXELS * ((float) RSM_HOTBAR_SLOTS/2)
			+ RSM_GUI_TEXTURE_SIZE_PIXELS * i;
		v[offset + 1] = 0.0f;
		v[offset + 2] = pHotbar;
		v[offset + 3] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 4] = atlasAdjust(0.0f, pHotbar);

		v[offset + 5] = WINDOW_WIDTH/2 - RSM_GUI_TEXTURE_SIZE_PIXELS * ((float) RSM_HOTBAR_SLOTS/2)
			+ RSM_GUI_TEXTURE_SIZE_PIXELS * i;
		v[offset + 6] = RSM_GUI_TEXTURE_SIZE_PIXELS;
		v[offset + 7] = pHotbar;
		v[offset + 8] = i % 2 == 0 ? 0.0f : 1.0f; v[offset + 9] = atlasAdjust(1.0f, pHotbar);
	}

	unsigned int i[6 * RSM_HOTBAR_SLOTS];

	for (int j = 0; j < RSM_HOTBAR_SLOTS * 2; j++) {
		int offset = j * 3;
		i[offset + 0] = 0 + j; i[offset + 1] = 1 + j; i[offset + 2] = 2 + j;
	}

	meshAppend(vertices, sizev, indices, sizei, v, i, sizeof(v), sizeof(i));
}
