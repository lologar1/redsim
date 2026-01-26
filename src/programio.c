#include "programio.h"

u64 MAX_BLOCK_ID;
u64 *MAX_BLOCK_VARIANT;
u64 NBLOCKTEXTURES;
u64 OV_BUFSZ, TV_BUFSZ, OI_BUFSZ, TI_BUFSZ; /* Max buffer sizes */
Blockmesh **BLOCKMESHES;
f32 (**BOUNDINGBOXES)[6];
u64 **SPRITEIDS;

u64 SUBMENUS[RSM_INVENTORY_SUBMENUS][RSM_INVENTORY_SLOTS_HORIZONTAL][RSM_INVENTORY_SLOTS_VERTICAL];
f32 LINEHEIGHT;
Textchar *TEXTCHARS; /* Hold rendering info for each ASCII 0-127 */
u64 NCHARTEXTURES;

GLuint blockTextureAtlas_;
GLuint guiTextureAtlas_;

void pio_parseBlockdata(void) {
	/* Parse all block data from disk and populate structures which hold them.
	 * Also generate the block texture atlas from associated texture files. */

#define BLOCKRESOURCES_PATH RESOURCE_BASE_PATH TEXTURE_BLOCK_PATH
#define BLOCKMAPFILE_PATH RESOURCE_BASE_PATH BLOCKMAP_PATH
	fprintf(stderr, "Loading blockmeshes from directory %s\n", BLOCKRESOURCES_PATH);
	fprintf(stderr, "Loading blockmap from file %s\n", BLOCKMAPFILE_PATH);

	char **blockmap; /* Contains block definitions, number of textures used, default metadata and sprite usage */
	blockmap = usf_ftost(BLOCKMAPFILE_PATH, &MAX_BLOCK_ID);
	MAX_BLOCK_VARIANT = malloc(MAX_BLOCK_ID * sizeof(u64));

	if (blockmap == NULL) {
		fprintf(stderr, "Error reading blockmap at %s (Does it exist?), aborting.\n", BLOCKMAPFILE_PATH);
		exit(RSM_EXIT_NOBLOCKMAP);
	}

	/* Alloc ID indirection layer */
	BLOCKMESHES = malloc(MAX_BLOCK_ID * sizeof(Blockmesh *));
	BOUNDINGBOXES = malloc(MAX_BLOCK_ID * sizeof(f32 (*)[6]));
	SPRITEIDS = malloc(MAX_BLOCK_ID * sizeof(u64 *));

	/* Precompute number of textures to get right UV coordinate mappings */
	char *override;
	u64 id, noverrides;
	for (NBLOCKTEXTURES = id = 1; id < MAX_BLOCK_ID; id++) { /* Skip id 0 (texture used for wiremesh) */
		/* For each $ override, add its texture tile count, then add 1 for each default handle */
		for (noverrides = 0, override = strchr(blockmap[id], '$'); override; override = strchr(override, '$')) {
			NBLOCKTEXTURES += strtou64(++override, NULL, 10);
			noverrides++;
		}
		NBLOCKTEXTURES += usf_scount(blockmap[id], ' ') + 1 - noverrides;
	}

	/* Block atlas */
	u8 *blockatlas = NULL; /* CPU-side block atlas */
	GLsizei blockatlassz = 0; /* Block atlas size in bytes (passed to OpenGL) */

	/* Read block data for each entry in blockmap */
	char **blockspecs;
	u64 spriteid, texid, nvariants, variant;
	for (spriteid = texid = id = 0; id < MAX_BLOCK_ID; id++) {
		blockspecs = usf_scsplit(blockmap[id], ' ', &nvariants); /* Read all variants */
		MAX_BLOCK_VARIANT[id] = nvariants;

		BOUNDINGBOXES[id] = calloc(nvariants, sizeof(float [6]));
		BLOCKMESHES[id] = calloc(nvariants, sizeof(Blockmesh));
		SPRITEIDS[id] = calloc(nvariants, sizeof(u64));

		char *blockspec, *specification;
		u64 uid, ntextiles;
		for (variant = 0; variant < nvariants; variant++, texid += ntextiles) {
			uid = ASUID(id, variant); /* Unique id:variant identifier */
			blockspec = blockspecs[variant];

			/* Block specification handling
			 * '*' means a sprite is associated with the given UID (spriteids given incrementally)
			 * ':' specifies default block metadata
			 * '$' specifies the number of textures used
			 *
			 * Note that the ':' metadata specification must come before the '$' texture count specification */

			if (blockspec[0] == '*') {
				SPRITEIDS[id][variant] = spriteid++;
				blockspec++; /* Consume char */
			}
			if ((specification = strchr(blockspec, ':'))) {
				*specification++ = '\0'; /* Cut spec string here (this is why ':' must come before '$') */
				usf_inthmput(datamap_, uid, USFDATAU(strtou64(specification, NULL, 10)));
			}
			if ((specification = strchr(blockspec, '$'))) {
				*specification++ = '\0';
				if ((ntextiles = strtou64(specification, NULL, 10)) > RSM_MAX_BLOCKMESH_TEXTURETILES) {
					fprintf(stderr, "Block %"PRIu64" variant %"PRIu64" exceeds maximum texture tile count "
							"at %"PRIu64" > %"PRId32"aborting.\n",
							id, variant, ntextiles, RSM_MAX_BLOCKMESH_TEXTURETILES);
					exit(RSM_EXIT_EXCBUF);
				}
			} else ntextiles = 1; /* One texture tile used by default */

			usf_strhmput(namemap_, blockspec, USFDATAU(uid)); /* By now blockspec contains only its name */

			/* ID 0 (except RSM_AIR, which has variant 0) are placeholders for tools;
			 * As such, no bounding boxes nor blockmeshes nor textures are allocated. */
			if (id == 0 && variant) { ntextiles = 0; continue; }

			/* Parse bounding box, if it exists */
			char filepath[RSM_MAX_PATH_NAME_LENGTH], *bbspec;
			f32 *bb;
			pio_pathcat(filepath, 3, BLOCKRESOURCES_PATH, blockspec, BOUNDINGBOX_EXTENSION);
			if ((bbspec = usf_ftos(filepath, NULL)) != NULL) {
				bb = BOUNDINGBOXES[id][variant];
				if (sscanf(bbspec, "%f %f %f %f %f %f", &bb[0], &bb[1], &bb[2], &bb[3], &bb[4], &bb[5]) == EOF) {
					fprintf(stderr, "Error parsing bounding box data for line %s, aborting.\n", bbspec);
					exit(RSM_EXIT_BADBOUNDINGBOXDATA);
				}
				usf_free(bbspec); /* Alloc'd by usf_ftos */
			}

			/* Add texture(s) to block atlas
			 * Note: ntextiles = 0 simply skips (ru_atlasAppend has no effect) */
			pio_pathcat(filepath, 3, BLOCKRESOURCES_PATH, blockspec, TEXTURE_EXTENSION);
			ru_atlasAppend(filepath, RSM_BLOCK_TEXTURE_SIZE_PIXELS,
					RSM_BLOCK_TEXTURE_SIZE_PIXELS * ntextiles, &blockatlas, &blockatlassz);

			/* Load mesh data and build block template */
			Blockmesh template = {.opaqueVertices = malloc(16), .transVertices = malloc(16),
				.opaqueIndices = malloc(16), .transIndices = malloc(16)}; /* Ensure buffers exist */
			char **meshdata, *meshspec;
			u64 meshdatalen, textureoffset, i;
			pio_pathcat(filepath, 3, BLOCKRESOURCES_PATH, blockspec, MESH_EXTENSION);
			if ((meshdata = usf_ftost(filepath, &meshdatalen)) == NULL) {
				fprintf(stderr, "Error reading raw mesh data at %s (Does it exist?), aborting.\n", filepath);
				exit(RSM_EXIT_NOMESHDATA);
			}

			for (textureoffset = i = 0; i < meshdatalen; i++) {
				meshspec = meshdata[i];
				switch (meshspec[0]) { /* First char determines type (vertex, index, opaque, trans) */
					f32 v[NMEMB_VERTEX], *y;
#define _ADJUSTY(_Y) /* Adjust in-texture coordinate to in-atlas value */ \
	((RSM_BLOCK_TEXTURE_SIZE_PIXELS * texid) + ((_Y) * RSM_BLOCK_TEXTURE_SIZE_PIXELS)) \
	/ (NBLOCKTEXTURES * RSM_BLOCK_TEXTURE_SIZE_PIXELS)
#define _VERTEXADJUST(_COUNT, _VERTEX) \
	if (sscanf(meshspec+1, "%f %f %f %f %f %f %f %f", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7])==EOF) { \
		fprintf(stderr, "Error parsing vertex data for line %s, aborting.\n", meshspec); \
		exit(RSM_EXIT_BADVERTEXDATA); \
	} \
	template._VERTEX = realloc(template._VERTEX, (template.count[_COUNT] + NMEMB_VERTEX) * sizeof(f32)); \
	memcpy(template._VERTEX + template.count[_COUNT], v, sizeof(v)); \
	y = template._VERTEX + (template.count[_COUNT] += NMEMB_VERTEX) - 1; /* Add to count, save to adjust */ \
	*y = _ADJUSTY(USF_CLAMP(*y, RSM_TEXTURE_PADDING, 1 - RSM_TEXTURE_PADDING) + textureoffset); /* Pad, offset */
					case 'o':
						_VERTEXADJUST(0, opaqueVertices);
						break;
					case 't':
						_VERTEXADJUST(1, transVertices);
						break;
#undef _ADJUSTY
#undef _VERTEXADJUST

					char **indices;
					u64 nindices, n;
#define _INDEXADJUST(_COUNT, _INDEX) \
	indices = usf_scsplit(meshspec + 1, ' ', &nindices); \
	template._INDEX = realloc(template._INDEX, (template.count[_COUNT] + nindices) * sizeof(u32)); \
	for (n = 0; n < nindices; n++) /* Write all indices to proper index buffer */ \
		template._INDEX[template.count[_COUNT] + n] = strtou32(indices[n], NULL, 10); \
	template.count[_COUNT] += nindices; \
	usf_free(indices); /* Alloc'd using usf_scsplit */
					case 'i':
						_INDEXADJUST(2, opaqueIndices);
						break;
					case 'e':
						_INDEXADJUST(3, transIndices);
						break;
#undef _INDEXADJUST
					case '$':
						textureoffset = strtou64(meshspec + 1, NULL, 10);
						break;
					case '#': /* Allow comments */
					case '\0': /* Allow empty lines */
						continue;
					default:
						fprintf(stderr, "Unknown data format %c at line %"PRIu64" in mesh data file "
								"%s, skipping.\n", meshspec[0], i, filepath);
						continue;
				}
			}

			/* Guard against buffer overflow when copying to scratchpad on chunk remeshing
			 * (guarantee RSM_MAX_BLOCKMESH_VERTICES and RSM_MAX_BLOCKMESH_INDICES) */
			if (template.count[0]>RSM_MAX_BLOCKMESH_VERTICES || template.count[1]>RSM_MAX_BLOCKMESH_VERTICES) {
				fprintf(stderr, "Blockmesh for ID %"PRIu64" variant %"PRIu64" exceeds maximum blockmesh"
						"vertex count (%"PRIu64" or %"PRIu64") > %"PRId32", aborting.\n",
						id, variant, template.count[0], template.count[1], RSM_MAX_BLOCKMESH_VERTICES);
				exit(RSM_EXIT_EXCBUF);
			}
			if (template.count[2]>RSM_MAX_BLOCKMESH_INDICES || template.count[3]>RSM_MAX_BLOCKMESH_INDICES) {
				fprintf(stderr, "Blockmesh for ID %"PRIu64" variant %"PRIu64" exceeds maximum blockmesh"
						"index count (%"PRIu64" or %"PRIu64") > %"PRId32", aborting.\n",
						id, variant, template.count[2], template.count[3], RSM_MAX_BLOCKMESH_INDICES);
				exit(RSM_EXIT_EXCBUF);
			}

			/* Adjust maximum sizes for buffer allocation */
			OV_BUFSZ = USF_MAX(OV_BUFSZ, template.count[0]); TV_BUFSZ = USF_MAX(TV_BUFSZ, template.count[1]);
			OI_BUFSZ = USF_MAX(OI_BUFSZ, template.count[2]); TI_BUFSZ = USF_MAX(TI_BUFSZ, template.count[3]);
			usf_freetxt(meshdata, 1); /* Done with this block mesh */

			/* Set to blockmeshes */
			BLOCKMESHES[id][variant] = template;
		}
		usf_free(blockspecs); /* Alloc'd using usf_scsplit */
	}
	usf_freetxt(blockmap, 1);

	/* Convert to byte sizes, per chunk */
	OV_BUFSZ *= sizeof(f32) * CHUNKVOLUME; TV_BUFSZ *= sizeof(f32) * CHUNKVOLUME;
	OI_BUFSZ *= sizeof(u32) * CHUNKVOLUME; TI_BUFSZ *= sizeof(u32) * CHUNKVOLUME;

	fprintf(stderr, "Scratchpad buffer sizes for remeshing (vertex opaque/trans, index opaque/trans): "
			"%.2fMB, %.2fMB, %.2fMB, %.2fMB\n", OV_BUFSZ/1e6, TV_BUFSZ/1e6, OI_BUFSZ/1e6, TI_BUFSZ/1e6);

	/* Generate and bind atlas to OpenGL renderer */
	glGenTextures(1, &blockTextureAtlas_);
	glBindTexture(GL_TEXTURE_2D, blockTextureAtlas_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_BLOCK_TEXTURE_SIZE_PIXELS,
			blockatlassz / (4 * RSM_BLOCK_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, blockatlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5); /* Needed to avoid blending the whole atlas */
	glGenerateMipmap(GL_TEXTURE_2D);
	free(blockatlas); /* CPU-side buffer */
}

void pio_parseGUIdata(void) {
	/* Parse all GUI data from disk and populate structures which hold them.
	 * Also generate the GUI texture atlas from associated texture files. */

#define GUIRESOURCES_PATH RESOURCE_BASE_PATH TEXTURE_GUI_PATH
#define GUIMAPFILE_PATH RESOURCE_BASE_PATH GUIMAP_PATH
	char **guimap;
	u64 nelements;
	if ((guimap = usf_ftost(GUIMAPFILE_PATH, &nelements)) == NULL) {
		fprintf(stderr, "Error reading guimap at %s (Does it exist?), aborting.\n", GUIMAPFILE_PATH);
		exit(RSM_EXIT_NOGUIMAP);
	}

	u8 *guiatlas = NULL;
	GLsizei guiatlassz = 0;

	u64 nelement;
	char *elementname, filepath[RSM_MAX_PATH_NAME_LENGTH];
	for (nelement = 0; nelement < nelements; nelement++) {
		elementname = guimap[nelement];

		/* Append texture for this element to the atlas */
		pio_pathcat(filepath, 3, GUIRESOURCES_PATH, elementname, TEXTURE_EXTENSION);
		ru_atlasAppend(filepath, RSM_GUI_TEXTURE_SIZE_PIXELS, RSM_GUI_TEXTURE_SIZE_PIXELS,
				&guiatlas, &guiatlassz);

		/* If element is a submenu: load corresponding layout file and populate it */
		if (nelement < TICONID || nelement >= TICONID + RSM_INVENTORY_SUBMENUS) continue;

		pio_pathcat(filepath, 3, GUIRESOURCES_PATH, elementname, LAYOUT_EXTENSION);

		u64 nsprites;
		char **submenulayout;
		if ((submenulayout = usf_ftost(filepath, &nsprites)) == NULL) {
			fprintf(stderr, "Cannot find inventory submenu layout at %s, while "
					"%s is declared as a submenu icon, aborting.\n", filepath, elementname);
			exit(RSM_EXIT_NOLAYOUT);
		}

		u64 i, uid, xslotoffset, yslotoffset;
		for (i = 0; i < nsprites; i++) {
			uid = usf_strhmget(namemap_, submenulayout[i]).u;

			xslotoffset = i % RSM_INVENTORY_SLOTS_HORIZONTAL;
			yslotoffset = i / RSM_INVENTORY_SLOTS_HORIZONTAL;

			SUBMENUS[nelement - TICONID][xslotoffset][RSM_INVENTORY_SLOTS_VERTICAL - yslotoffset-1] = uid;
		}
		usf_freetxt(submenulayout, 1);
	}
	usf_freetxt(guimap, 1);

	/* Load font using FreeType */
	TEXTCHARS = malloc(128 * sizeof(Textchar));
	FT_Library freetype;
	FT_Face typeface;
	FT_GlyphSlot glyph;

	if (FT_Init_FreeType(&freetype)) {
		fprintf(stderr, "Error initializing FreeType library, aborting.\n");
		exit(RSM_EXIT_TEXTFAIL);
	}

#define FONTFILE_PATH TYPEFACE_PATH FONT_PATH
	if (FT_New_Face(freetype, FONTFILE_PATH, 0, &typeface)) {
		fprintf(stderr, "Error retrieving typeface at %s, aborting.\n", TYPEFACE_PATH);
		exit(RSM_EXIT_TEXTFAIL);
	}
	glyph = typeface->glyph;

	/* Request glyph sizes in pixels. Resulting glyphs will approach this size, but aren't constrained by it! */
	FT_Set_Pixel_Sizes(typeface, RSM_CHARACTER_TEXTURE_SIZE_PIXELS, RSM_CHARACTER_TEXTURE_SIZE_PIXELS);
	LINEHEIGHT = typeface->size->metrics.height / 64; /* Divide by 64 as metrics are in 26.6 format */

#define CHAR_TEXTURESZ (RSM_CHARACTER_TEXTURE_SIZE_PIXELS * RSM_CHARACTER_TEXTURE_SIZE_PIXELS)
#define CHARS_PER_TEXTURE ((RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS) / CHAR_TEXTURESZ)
	NCHARTEXTURES = 128 / CHARS_PER_TEXTURE; /* Division yields whole number */
	u8 chartexture[CHARS_PER_TEXTURE][CHAR_TEXTURESZ]; /* Grayscale: single GUI texture */
	u8 chartextureatlas[CHARS_PER_TEXTURE * CHAR_TEXTURESZ * NCHARTEXTURES * 4]; /* RGBA match GUI atlas */

	u8 c;
	u64 nchartexture, nsubtexture;
	for (c = 0; c < 128; c++) {
		nchartexture = c / CHARS_PER_TEXTURE;
		nsubtexture = c % CHARS_PER_TEXTURE;

		if (FT_Load_Char(typeface, c, FT_LOAD_RENDER)) {
			fprintf(stderr, "Error loading or rendering character %c (%"PRIu8"), aborting,\n", c, c);
			exit(RSM_EXIT_TEXTFAIL);
		}

		/* Create char subtexture within the chartexture GUI texture */
		memset(chartexture[nsubtexture], 0, CHAR_TEXTURESZ);

		u64 row, col;
		for (row = 0; row < glyph->bitmap.rows; row++)
		for (col = 0; col < glyph->bitmap.width; col++) {
#define XOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS \
			- USF_MIN(glyph->bitmap.width, RSM_CHARACTER_TEXTURE_SIZE_PIXELS)) / 2)
#define YOFFSET ((RSM_CHARACTER_TEXTURE_SIZE_PIXELS \
			- USF_MIN(glyph->bitmap.rows, RSM_CHARACTER_TEXTURE_SIZE_PIXELS)) / 2)
			chartexture[nsubtexture][(row + YOFFSET) * RSM_CHARACTER_TEXTURE_SIZE_PIXELS + col + XOFFSET] =
				glyph->bitmap.buffer[row * (u64) USF_ABS(glyph->bitmap.pitch) + col];
		}

#define CHARS_PER_LINE (RSM_GUI_TEXTURE_SIZE_PIXELS / RSM_CHARACTER_TEXTURE_SIZE_PIXELS)
#define CHAR_STRIDE ((f32) RSM_CHARACTER_TEXTURE_SIZE_PIXELS / (f32) RSM_GUI_TEXTURE_SIZE_PIXELS)
		f32 cox, coy;
		cox = (nsubtexture % CHARS_PER_LINE) * CHAR_STRIDE;
		coy = (nsubtexture / CHARS_PER_LINE) * CHAR_STRIDE + CHAR_STRIDE;

		/* Compute textchar values */
#define UOFFSET ((f32) XOFFSET / RSM_GUI_TEXTURE_SIZE_PIXELS)
#define VOFFSET ((f32) YOFFSET / RSM_GUI_TEXTURE_SIZE_PIXELS)
		Textchar *textchar;
		textchar = &TEXTCHARS[c];
		textchar->uv[0] = cox + UOFFSET;
		textchar->uv[1] = gu_guiAtlasAdjust(coy - VOFFSET, MAX_GUI_TEXTUREID + nchartexture);
		textchar->uv[2] = cox + CHAR_STRIDE - UOFFSET;
		textchar->uv[3] = gu_guiAtlasAdjust(coy - CHAR_STRIDE + VOFFSET, MAX_GUI_TEXTUREID + nchartexture);

		textchar->size[0] = glyph->bitmap.width;
		textchar->size[1] = glyph->bitmap.rows;

		textchar->bearing[0] = glyph->bitmap_left;
		textchar->bearing[1] = glyph->bitmap_top;

		textchar->advance = glyph->advance.x / 64; /* 26.6 pixel format */

		if (nsubtexture != CHARS_PER_TEXTURE - 1) continue;

		/* Dump to full GUI texture */
		u64 charoffset, pixeloffset;
		for (charoffset = 0; charoffset < CHARS_PER_TEXTURE; charoffset++)
		for (pixeloffset = 0; pixeloffset < CHAR_TEXTURESZ; pixeloffset++) {
/* Using integer division as floor: expressions like (a / b) * b are intentional */
#define GUITEXTUREOFFSET (nchartexture * RSM_GUI_TEXTURE_SIZE_PIXELS * RSM_GUI_TEXTURE_SIZE_PIXELS * 4)
#define CHARTEXTUREOFFSET (((charoffset / CHARS_PER_LINE) * (CHARS_PER_LINE * CHAR_TEXTURESZ) \
			+ (charoffset % CHARS_PER_LINE) * RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * 4)
#define PIXELOFFSET (((pixeloffset / RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * RSM_GUI_TEXTURE_SIZE_PIXELS \
			+ pixeloffset % RSM_CHARACTER_TEXTURE_SIZE_PIXELS) * 4)
			memset(&chartextureatlas[GUITEXTUREOFFSET + CHARTEXTUREOFFSET + PIXELOFFSET],
					chartexture[charoffset][pixeloffset], 3); /* RGB from grayscale */
			chartextureatlas[GUITEXTUREOFFSET + CHARTEXTUREOFFSET + PIXELOFFSET + 3] =
				chartexture[charoffset][pixeloffset] ? 255 : 0; /* Alpha either opaque or transparent */
#undef GUITEXTUREOFFSET
#undef CHARTEXTUREOFFSET
#undef PIXELOFFSET
		}
	}

	/* Copy char atlas to GUI atlas */
	guiatlas = realloc(guiatlas, (u64) guiatlassz + sizeof(chartextureatlas));
	memcpy(guiatlas + guiatlassz, chartextureatlas, sizeof(chartextureatlas));
	guiatlassz += (i64) sizeof(chartextureatlas);

	/* Free FreeType */
	FT_Done_Face(typeface);
	FT_Done_FreeType(freetype);

	glGenTextures(1, &guiTextureAtlas_);
	glBindTexture(GL_TEXTURE_2D, guiTextureAtlas_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RSM_GUI_TEXTURE_SIZE_PIXELS,
			guiatlassz / (4 * RSM_GUI_TEXTURE_SIZE_PIXELS), 0, GL_RGBA, GL_UNSIGNED_BYTE, guiatlas);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	free(guiatlas); /* CPU-side buffer */
}

void pio_pathcat(char *destination, u64 n, ...) {
	/* Concatenates a path into a destination, checking against RSM_MAX_PATH_NAME_LENGTH */

	va_list args;
	va_start(args, n);

	if (usf_vstrcat(destination, RSM_MAX_PATH_NAME_LENGTH, n, args)) {
		fprintf(stderr, "Concatenation of %"PRIu64" paths exceeds max buffer length %"PRId32", aborting.\n",
				n, RSM_MAX_PATH_NAME_LENGTH);
		exit(RSM_EXIT_EXCBUF);
	}

	va_end(args);
}
