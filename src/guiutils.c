#include "guiutils.h"

GLuint guiVAOs_[MAX_GUI_MESHID];
u64 nGUIIndices_[MAX_GUI_MESHID];

void gu_initGUI(void) {
	/* Create the appropriate OpenGL structures for the GUI */

	GLuint guiVBOs[MAX_GUI_MESHID], guiEBOs[MAX_GUI_MESHID];
	glGenVertexArrays(MAX_GUI_MESHID, guiVAOs_);
	glGenBuffers(MAX_GUI_MESHID, guiVBOs);
	glGenBuffers(MAX_GUI_MESHID, guiEBOs);

	/* Attributes */
	u32 i;
	for (i = 0; i < MAX_GUI_MESHID; i++) {
		glBindVertexArray(guiVAOs_[i]);
		glBindBuffer(GL_ARRAY_BUFFER, guiVBOs[i]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, guiEBOs[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void *) (0 * sizeof(f32)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void *) (3 * sizeof(f32)));
	}
	glBindVertexArray(0);
}

f32 gu_guiAtlasAdjust(f32 v, GUITextureID textureid) {
	/* Adjusts V texture coord from local (inside GUI texture) to global (inside GUI texture atlas) */

	return (v * RSM_GUI_TEXTURE_SIZE_PIXELS + RSM_GUI_TEXTURE_SIZE_PIXELS * textureid)
		/ ((MAX_GUI_TEXTUREID + NCHARTEXTURES) * RSM_GUI_TEXTURE_SIZE_PIXELS);
}

void gu_meshSet(u64 meshid, f32 *v, u64 nvertices, u32 *i, u64 nindices) {
	/* Send GUI mesh to GPU buffers */

	GLint VBO;
	glBindVertexArray(guiVAOs_[meshid]);
	glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint) VBO);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (nvertices * sizeof(f32)), v, GL_DYNAMIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (nindices * sizeof(i32)), i, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	nGUIIndices_[meshid] = nindices;
}

u64 gu_drawText(char *str, f32 *vbuf, u32 *ibuf, u64 ioffset, f32 x, f32 y, f32 scale, GUIMeshID priority) {
	/* Add the adjusted quads to a vbuf and ibuf for text rendering of string str. Note that both of these
	 * buffers should be big enough to handle the string (4 vertices + 6 indices per char.
	 * Indices start at offset ioffset.
	 * Important: vbuf and ibuf must point to the place where the quads will be rendered! */

	Textchar *textchar;
	char *c;
	u64 nchars;
	f32 xoffset, yoffset;
	for (xoffset = yoffset = 0.0f, nchars = 0, c = str; *c; c++) {
		if (*c < 0) continue; /* Redsim currently only supports ASCII 0-127 */

		textchar = &TEXTCHARS[(u8) *c];

		if (*c == '\n') {
			xoffset = 0.0f;
			yoffset -= LINEHEIGHT * scale;
			continue;
		}

		if (isspace(*c)) { /* Do not print anything; simply advance */
			xoffset += textchar->advance * scale;
			continue;
		}

		f32 xpos, ypos, width, height;
		xpos = x + xoffset + textchar->bearing[0] * scale;
		ypos = y + yoffset - (textchar->size[1] - textchar->bearing[1]) * scale;
		width = textchar->size[0] * scale; height = textchar->size[1] * scale;

		f32 v[5 * 4] = {
			xpos, ypos, priority, textchar->uv[0], textchar->uv[1],
			xpos, ypos + height, priority, textchar->uv[0], textchar->uv[3],
			xpos + width, ypos + height, priority, textchar->uv[2], textchar->uv[3],
			xpos + width, ypos, priority, textchar->uv[2], textchar->uv[1]
		};
		u32 i[6] = {0 + ioffset, 1 + ioffset, 2 + ioffset, 0 + ioffset, 2 + ioffset, 3 + ioffset};

		memcpy(vbuf, v, sizeof(v));
		memcpy(ibuf, i, sizeof(i));

		xoffset += textchar->advance * scale;
		ioffset += 4;
		vbuf += sizeof(v)/sizeof(f32);
		ibuf += sizeof(i)/sizeof(u32);

		nchars++; /* Count drawn characters */
	}

	return nchars;
}
