// leave this at the top of all UI_xxxx files for PCH reasons.
#include "../server/exe_headers.h"

#include "ui_local.h"
#ifdef _XBOX
#include "../win32/xb_log.h"
extern "C" volatile unsigned int g_SPXBUIPauseOpenCount;
extern "C" volatile unsigned int g_SPXBUIPauseDrawCount;
extern "C" volatile unsigned int g_SPXBUIPauseActive;
#endif

#define EF_MNT_MAX 1073
#define EF_MBT_MAX 250
#define EF_MAX_MENU_TEXT 55000
#define EF_MAX_BUTTON_TEXT 14000

#define EF_MENU_BUTTON_MED_HEIGHT 18
#define EF_MENU_BUTTON_MED_WIDTH 130
#define EF_MENU_TITLE_X 611
#define EF_MENU_TITLE_Y 24
#define EF_MENU_DESC_X 100
#define EF_MENU_DESC_Y 444
#define EF_MENU_BUTTON_TEXT_X 5
#define EF_MENU_BUTTON_TEXT_Y 1
#define EF_SUIT_MAXDESC 8
#define EF_FONT_CHARMAX 256
#define EF_FONT_BUFF_LENGTH 20000
#define EF_FONT_TINY 0
#define EF_FONT_MEDIUM 1
#define EF_FONT_BIG 2
#define EF_FONT_COUNT 3

enum {
	EF_MNT_INGAMEMENU_TITLE = 249,
	EF_MNT_HAZARDSUIT_SPECS = 250,
	EF_MNT_HAZARDSUIT = 251,
	EF_MNT_FRONT = 252,
	EF_MNT_BACK = 253,
	EF_MNT_POWERCONVERTER_DESC1 = 262,
	EF_MNT_COMMBADGE_DESC1 = 263,
	EF_MNT_LOGISTICS_DESC1 = 264,
	EF_MNT_ENERGY_PACK_DESC1 = 265,
	EF_MNT_WAVEGENERATOR_DESC1 = 266,
	EF_MNT_SCANNERS_DESC1 = 267,
	EF_MNT_POUCHES_DESC1 = 268,
	EF_MNT_BUFFER_DESC1 = 269
};

enum {
	EF_MBT_SAVEGAME = 8,
	EF_MBT_IMLOADGAME = 9,
	EF_MBT_POWERCONV = 10,
	EF_MBT_COMMBADGE = 11,
	EF_MBT_LOGISTICS = 12,
	EF_MBT_ENERGY_PACK = 13,
	EF_MBT_WAVEGENERATOR = 14,
	EF_MBT_SCANNERS = 15,
	EF_MBT_POUCHES = 16,
	EF_MBT_BUFFER = 17,
	EF_MBT_QUITGAME = 18,
	EF_MBT_EXITPROG = 19,
	EF_MBT_RETURNTOGAME = 146,
	EF_MBT_CONFIGURE = 171,
	EF_MBT_SCREENSHOT = 177,
	EF_MBT_EXITTOUR = 237
};

enum {
	EF_PAUSE_RESUME,
	EF_PAUSE_SAVE,
	EF_PAUSE_LOAD,
	EF_PAUSE_CONFIGURE,
	EF_PAUSE_LEAVEGAME,
	EF_PAUSE_QUIT,
	EF_PAUSE_SCREENSHOT,
	EF_PAUSE_BUTTON_COUNT
};

typedef struct {
	int id;
	int x;
	int y;
	int w;
	int h;
	int textEnum;
	const char *fallbackText;
	const char *fallbackDesc;
	qboolean enabled;
} efPauseButton_t;

typedef struct {
	int buttonTextEnum;
	int normalDescEnum;
	const char *fallbackLabel;
	const char *fallbackDesc;
	int labelX;
	int labelY;
	int style;
	const char *picName;
	int picX;
	int picY;
	int picW;
	int picH;
	qhandle_t picShader;
	const char *lineName;
	int lineX;
	int lineY;
	int lineW;
	int lineH;
	qhandle_t lineShader;
} efPauseSystem_t;

typedef struct {
	qboolean cached;
	qboolean textLoaded;
	qhandle_t whiteShader;
	qhandle_t buttonRight;
	qhandle_t buttonLeftEnd;
	qhandle_t square;
	qhandle_t cornerUpper;
	qhandle_t cornerUpper2;
	qhandle_t cornerLower;
	qhandle_t suit;
} efPauseAssets_t;

typedef struct {
	qboolean loaded;
	qhandle_t shader[EF_FONT_COUNT];
	int propMap[EF_FONT_COUNT][EF_FONT_CHARMAX][3];
} efPauseFonts_t;

static efPauseAssets_t s_assets;
static efPauseFonts_t s_fonts;
static qboolean s_active = qfalse;
static qboolean s_loggedDraw = qfalse;
static int s_loggedTextDraws = 0;
static int s_cursor = 0;
static int s_activeSystem = 0;
static int s_nextSystemTime = 0;
static char s_menuTextBuffer[EF_MAX_MENU_TEXT];
static char s_buttonTextBuffer[EF_MAX_BUTTON_TEXT];
static char *s_normalText[EF_MNT_MAX];
static char *s_buttonText[EF_MBT_MAX][2];
static char s_suitDesc[EF_SUIT_MAXDESC][512];

static const int s_ingameButtonPositions[6][2] = {
	{129, 62},
	{129, 86},
	{129, 109},
	{305, 62},
	{305, 86},
	{305, 109}
};

static efPauseButton_t s_buttons[EF_PAUSE_BUTTON_COUNT] = {
	{ EF_PAUSE_RESUME,     129, 62,  EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_RETURNTOGAME, "RESUME GAME",  "RETURN TO CURRENT GAME", qtrue },
	{ EF_PAUSE_SAVE,       129, 86,  EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_SAVEGAME,     "SAVE GAME",    "SAVE CURRENT GAME TO DISK", qtrue },
	{ EF_PAUSE_LOAD,       129, 109, EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_IMLOADGAME,   "LOAD GAME",    "LOAD GAME FROM DISK", qtrue },
	{ EF_PAUSE_CONFIGURE,  305, 62,  EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_CONFIGURE,    "CONFIGURE",    "CREATE KEY BINDINGS", qfalse },
	{ EF_PAUSE_LEAVEGAME,  305, 86,  EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_QUITGAME,     "QUIT GAME",    "QUIT CURRENT GAME", qtrue },
	{ EF_PAUSE_QUIT,       305, 109, EF_MENU_BUTTON_MED_WIDTH, EF_MENU_BUTTON_MED_HEIGHT, EF_MBT_EXITPROG,     "EXIT PROGRAM", "QUIT ELITE FORCE PROGRAM", qtrue },
	{ EF_PAUSE_SCREENSHOT, 481, 77,  EF_MENU_BUTTON_MED_WIDTH, 36,                         EF_MBT_SCREENSHOT,  "SCREENSHOT",   "TAKE SCREENSHOT", qtrue }
};

static efPauseSystem_t s_systems[] = {
	{ EF_MBT_POWERCONV,     EF_MNT_POWERCONVERTER_DESC1, "POWER CONVERTER",              "POWER CONVERTER : used to convert alien energy into a source of power for Federation weapons.", 148, 228, UI_RIGHT | UI_TINYFONT, "menu/suit/power_conv.tga",       201, 263, 32, 32, 0, "menu/suit/power_conv_break.tga", 150, 232, 64, 32, 0 },
	{ EF_MBT_COMMBADGE,     EF_MNT_COMMBADGE_DESC1,      "COMM BADGE",                   "COMM BADGE : for use in communicating between team members and Voyager.",                       252, 185, UI_TINYFONT,            "menu/suit/combadge.tga",         219, 220, 16, 16, 0, "menu/suit/combadge_break.tga",   226, 192, 32, 32, 0 },
	{ EF_MBT_LOGISTICS,     EF_MNT_LOGISTICS_DESC1,      "DIRECTION/LOGISTICS",          "DIRECTION/LOGISTICS : supplies data to tactical display, giving heading, location, and location of team members.", 312, 320, UI_TINYFONT, "menu/suit/direct_log_circ.tga", 338, 278, 32, 16, 0, "menu/suit/direc_log_break.tga",  312, 285, 32, 32, 0 },
	{ EF_MBT_ENERGY_PACK,   EF_MNT_ENERGY_PACK_DESC1,    "ENERGY PACK",                  "ENERGY PACK : stores power for Federation weapons.",                                             466, 242, UI_TINYFONT,            "menu/suit/energy_pack.tga",      387, 266, 32, 16, 0, "menu/suit/ener_pack_break.tga",  402, 248, 64, 32, 0 },
	{ EF_MBT_WAVEGENERATOR, EF_MNT_WAVEGENERATOR_DESC1,  "MULTI-PHASIC WAVE GENERATORS", "MULTI_PHASIC WAVE GENERATOR : used to disrupt lock ons of unfriendly transporter beams.",         254, 158, UI_TINYFONT,            "menu/suit/multi_wavegen.tga",    183, 206, 64, 16, 0, "menu/suit/multi_ph_break.tga",   188, 162, 64, 64, 0 },
	{ EF_MBT_SCANNERS,      EF_MNT_SCANNERS_DESC1,       "PASSIVE/ACTIVE SCANNERS",      "ACTIVE/PASSIVE SCANNERS : gives information of the surrounding environment.",                     127, 320, UI_RIGHT | UI_TINYFONT, "menu/suit/pass_acscan.tga",      155, 278, 32, 32, 0, "menu/suit/direc_log_break.tga",  127, 285, 32, 32, 0 },
	{ EF_MBT_POUCHES,       EF_MNT_POUCHES_DESC1,        "EQUIPMENT POUCH",              "EQUIPMENT POUCHES : for holding various supplies.",                                              490, 295, UI_TINYFONT,            "menu/suit/pouches.tga",          419, 295, 16, 32, 0, "menu/suit/ener_pack_break.tga",  428, 300, 64, 32, 0 },
	{ EF_MBT_BUFFER,        EF_MNT_BUFFER_DESC1,         "TRANSPORTER BUFFER",           "TRANSPORTER BUFFER : stores equipment in a molecularized state which can be rematerialized instantly when needed.", 472, 281, UI_TINYFONT, "menu/suit/trans_buff.tga", 402, 264, 16, 16, 0, "menu/suit/trans_buff_break.tga", 421, 268, 64, 32, 0 }
};

static void EFPause_LanguageFilename(const char *baseName, const char *baseExtension, char *finalName)
{
	char language[32];
	fileHandle_t file;

	ui.Cvar_VariableStringBuffer("g_language", language, sizeof(language));
	if (language[0] == '\0' || Q_stricmp("ENGLISH", language) == 0) {
		Com_sprintf(finalName, MAX_QPATH, "%s.%s", baseName, baseExtension);
		return;
	}

	Com_sprintf(finalName, MAX_QPATH, "%s_%s.%s", baseName, language, baseExtension);
	ui.FS_FOpenFile(finalName, &file, FS_READ);
	if (file == 0) {
		Com_sprintf(finalName, MAX_QPATH, "%s.%s", baseName, baseExtension);
	} else {
		ui.FS_FCloseFile(file);
	}
}

static char *EFPause_ParseQuoted(char **cursor)
{
	char *p;
	char *start;
	char *out;

	if (!cursor || !*cursor) {
		return NULL;
	}

	p = *cursor;
	for (;;) {
		while (*p && *p <= ' ') {
			p++;
		}
		if (p[0] == '/' && p[1] == '/') {
			while (*p && *p != '\n') {
				p++;
			}
			continue;
		}
		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/')) {
				p++;
			}
			if (*p) {
				p += 2;
			}
			continue;
		}
		break;
	}

	while (*p && *p != '"') {
		p++;
	}
	if (!*p) {
		*cursor = NULL;
		return NULL;
	}

	p++;
	start = p;
	out = p;
	while (*p && *p != '"') {
		if (*p == '\\' && p[1]) {
			p++;
		}
		*out++ = *p++;
	}
	if (*p == '"') {
		p++;
	}
	*out = '\0';
	*cursor = p;
	return start;
}

static int EFPause_LoadTextFile(const char *baseName, const char *extension, char *dest, int destSize)
{
	char filename[MAX_QPATH];
	char *fileBuffer;
	int len;
	int copyLen;

	EFPause_LanguageFilename(baseName, extension, filename);
	len = ui.FS_ReadFile(filename, (void **)&fileBuffer);
	if (len < 0 || !fileBuffer) {
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_TEXT missing file='%s' len=%d", filename, len);
#endif
		return -1;
	}

	copyLen = len;
	if (copyLen >= destSize) {
		copyLen = destSize - 1;
	}
	memcpy(dest, fileBuffer, copyLen);
	dest[copyLen] = '\0';
	ui.FS_FreeFile(fileBuffer);
#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_TEXT loaded file='%s' len=%d copied=%d", filename, len, copyLen);
#endif
	return copyLen;
}

static void EFPause_LoadTextTables(void)
{
	char *cursor;
	char *token;
	int i;

	if (s_assets.textLoaded) {
		return;
	}

	memset(s_normalText, 0, sizeof(s_normalText));
	memset(s_buttonText, 0, sizeof(s_buttonText));
	s_normalText[0] = "";
	s_buttonText[0][0] = "";
	s_buttonText[0][1] = NULL;

	if (EFPause_LoadTextFile("ext_data/sp_normaltext", "dat", s_menuTextBuffer, sizeof(s_menuTextBuffer)) >= 0) {
		cursor = s_menuTextBuffer;
		i = 1;
		while (i < EF_MNT_MAX) {
			token = EFPause_ParseQuoted(&cursor);
			if (!token) {
				break;
			}
			s_normalText[i++] = token;
		}
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_TEXT parsed normal count=%d expected=%d", i, EF_MNT_MAX);
#endif
	}

	if (EFPause_LoadTextFile("ext_data/sp_buttontext", "dat", s_buttonTextBuffer, sizeof(s_buttonTextBuffer)) >= 0) {
		cursor = s_buttonTextBuffer;
		i = 1;
		while (i < EF_MBT_MAX) {
			token = EFPause_ParseQuoted(&cursor);
			if (!token) {
				break;
			}
			s_buttonText[i][0] = ((token[0] == '/') && (token[1] == '\0')) ? NULL : token;
			token = EFPause_ParseQuoted(&cursor);
			if (!token) {
				break;
			}
			s_buttonText[i][1] = ((token[0] == '/') && (token[1] == '\0')) ? NULL : token;
			i++;
		}
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_TEXT parsed button count=%d expected=%d", i, EF_MBT_MAX);
#endif
	}

	s_assets.textLoaded = qtrue;
}

static const char *EFPause_NormalText(int index, const char *fallback)
{
	if (index > 0 && index < EF_MNT_MAX && s_normalText[index] && s_normalText[index][0]) {
		return s_normalText[index];
	}
	return fallback ? fallback : "?";
}

static const char *EFPause_ButtonText(int index, int column, const char *fallback)
{
	if (index > 0 && index < EF_MBT_MAX && column >= 0 && column < 2 &&
		s_buttonText[index][column] && s_buttonText[index][column][0]) {
		return s_buttonText[index][column];
	}
	return fallback ? fallback : "?";
}

static float EFPause_PropSizeScale(int style)
{
	if (style & UI_GIANTFONT) {
		return PROP_GIANT_SIZE_SCALE;
	}
	return 1.0f;
}

static int EFPause_FontIndexForStyle(int style)
{
	if (style & UI_TINYFONT) {
		return EF_FONT_TINY;
	}
	if (style & UI_BIGFONT) {
		return EF_FONT_BIG;
	}
	return EF_FONT_MEDIUM;
}

static int EFPause_FontHeightForIndex(int fontIndex)
{
	if (fontIndex == EF_FONT_TINY) {
		return PROP_TINY_HEIGHT;
	}
	if (fontIndex == EF_FONT_BIG) {
		return PROP_BIG_HEIGHT;
	}
	return PROP_HEIGHT;
}

static int EFPause_FontGapForIndex(int fontIndex)
{
	if (fontIndex == EF_FONT_TINY) {
		return PROP_GAP_TINY_WIDTH;
	}
	if (fontIndex == EF_FONT_BIG) {
		return PROP_GAP_BIG_WIDTH;
	}
	return PROP_GAP_WIDTH;
}

static int EFPause_FontSpaceForIndex(int fontIndex)
{
	if (fontIndex == EF_FONT_TINY) {
		return PROP_SPACE_TINY_WIDTH;
	}
	if (fontIndex == EF_FONT_BIG) {
		return PROP_SPACE_BIG_WIDTH;
	}
	return PROP_SPACE_WIDTH;
}

static void EFPause_ClearFontMaps(void)
{
	int f;
	int ch;

	for (f = 0; f < EF_FONT_COUNT; f++) {
		for (ch = 0; ch < EF_FONT_CHARMAX; ch++) {
			s_fonts.propMap[f][ch][0] = 0;
			s_fonts.propMap[f][ch][1] = 0;
			s_fonts.propMap[f][ch][2] = -1;
		}
	}
}

static qboolean EFPause_ParseFontMap(const char **cursor, int fontIndex, const char *debugName)
{
	char *token;
	int ch;
	int component;

	token = COM_ParseExt(cursor, qtrue);
	if (!token[0] || Q_stricmp(token, "{")) {
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_FONT parse_fail map='%s' expected_open token='%s'",
			debugName,
			token ? token : "<null>");
#endif
		return qfalse;
	}

	for (ch = 0; ch < EF_FONT_CHARMAX; ch++) {
		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "{")) {
#ifdef _XBOX
			XBLF("STEFX_INPUT_PAUSE_FONT parse_fail map='%s' char=%d expected_char_open token='%s'",
				debugName,
				ch,
				token ? token : "<null>");
#endif
			return qfalse;
		}

		for (component = 0; component < 3; component++) {
			token = COM_ParseExt(cursor, qtrue);
			if (!token[0]) {
#ifdef _XBOX
				XBLF("STEFX_INPUT_PAUSE_FONT parse_fail map='%s' char=%d component=%d empty",
					debugName,
					ch,
					component);
#endif
				return qfalse;
			}
			s_fonts.propMap[fontIndex][ch][component] = atoi(token);
		}

		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "}")) {
#ifdef _XBOX
			XBLF("STEFX_INPUT_PAUSE_FONT parse_fail map='%s' char=%d expected_char_close token='%s'",
				debugName,
				ch,
				token ? token : "<null>");
#endif
			return qfalse;
		}
	}

	token = COM_ParseExt(cursor, qtrue);
	if (!token[0] || Q_stricmp(token, "}")) {
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_FONT parse_fail map='%s' expected_close token='%s'",
			debugName,
			token ? token : "<null>");
#endif
		return qfalse;
	}

	return qtrue;
}

static void EFPause_LoadPropFonts(void)
{
	char buffer[EF_FONT_BUFF_LENGTH];
	fileHandle_t file;
	int len;
	const char *cursor;
	qboolean ok;

	if (s_fonts.loaded) {
		return;
	}

	EFPause_ClearFontMaps();
	s_fonts.shader[EF_FONT_TINY] = ui.R_RegisterShaderNoMip("gfx/2d/chars_tiny.tga");
	s_fonts.shader[EF_FONT_MEDIUM] = ui.R_RegisterShaderNoMip("gfx/2d/chars_medium.tga");
	s_fonts.shader[EF_FONT_BIG] = ui.R_RegisterShaderNoMip("gfx/2d/chars_big.tga");

	len = ui.FS_FOpenFile("ext_data/fonts.dat", &file, FS_READ);
	if (!file) {
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_FONT missing file='ext_data/fonts.dat' len=%d tiny=%d med=%d big=%d",
			len,
			s_fonts.shader[EF_FONT_TINY],
			s_fonts.shader[EF_FONT_MEDIUM],
			s_fonts.shader[EF_FONT_BIG]);
#endif
		return;
	}

	if (len >= (int)sizeof(buffer)) {
#ifdef _XBOX
		XBLF("STEFX_INPUT_PAUSE_FONT too_large len=%d max=%d",
			len,
			(int)sizeof(buffer));
#endif
		ui.FS_FCloseFile(file);
		return;
	}

	memset(buffer, 0, sizeof(buffer));
	ui.FS_Read(buffer, len, file);
	ui.FS_FCloseFile(file);

	cursor = buffer;
	ok = EFPause_ParseFontMap(&cursor, EF_FONT_TINY, "tiny");
	if (ok) {
		ok = EFPause_ParseFontMap(&cursor, EF_FONT_MEDIUM, "medium");
	}
	if (ok) {
		ok = EFPause_ParseFontMap(&cursor, EF_FONT_BIG, "big");
	}

	s_fonts.loaded = ok;
#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_FONT loaded=%d len=%d tinyShader=%d medShader=%d bigShader=%d sampleA=%d/%d/%d",
		s_fonts.loaded,
		len,
		s_fonts.shader[EF_FONT_TINY],
		s_fonts.shader[EF_FONT_MEDIUM],
		s_fonts.shader[EF_FONT_BIG],
		s_fonts.propMap[EF_FONT_MEDIUM]['A'][0],
		s_fonts.propMap[EF_FONT_MEDIUM]['A'][1],
		s_fonts.propMap[EF_FONT_MEDIUM]['A'][2]);
#endif
}

static int EFPause_PropStringWidth(const char *text, int style)
{
	int fontIndex;
	int gap;
	int spaceWidth;
	int width;
	int charWidth;
	const unsigned char *s;

	if (!text || !text[0]) {
		return 0;
	}

	EFPause_LoadPropFonts();
	if (!s_fonts.loaded) {
		return 0;
	}

	fontIndex = EFPause_FontIndexForStyle(style);
	gap = EFPause_FontGapForIndex(fontIndex);
	spaceWidth = EFPause_FontSpaceForIndex(fontIndex);
	width = 0;
	s = (const unsigned char *)text;
	while (*s) {
		if (*s == ' ') {
			charWidth = spaceWidth;
		} else {
			charWidth = s_fonts.propMap[fontIndex][*s][2];
		}
		if (charWidth != -1) {
			width += charWidth + gap;
		}
		s++;
	}

	if (width > 0) {
		width -= gap;
	}
	return (int)(width * EFPause_PropSizeScale(style));
}

static void EFPause_DrawPropString(int x, int y, const char *text, int style, int colorIndex)
{
	const unsigned char *s;
	int fontIndex;
	int gap;
	int spaceWidth;
	int height;
	int charWidth;
	float scale;
	float drawX;
	float drawY;
	float fcol;
	float frow;
	float fwidth;
	float fheight;

	if (!text || !text[0]) {
		return;
	}

	EFPause_LoadPropFonts();
	if (!s_fonts.loaded || !s_fonts.shader[EFPause_FontIndexForStyle(style)]) {
#ifdef _XBOX
		if (s_loggedTextDraws < 4) {
			XBLF("STEFX_INPUT_PAUSE_TEXTDRAW fallback_no_font text='%.32s' loaded=%d tiny=%d med=%d big=%d",
				text,
				s_fonts.loaded,
				s_fonts.shader[EF_FONT_TINY],
				s_fonts.shader[EF_FONT_MEDIUM],
				s_fonts.shader[EF_FONT_BIG]);
		}
#endif
		return;
	}

	if (style & UI_CENTER) {
		x -= EFPause_PropStringWidth(text, style) / 2;
	} else if (style & UI_RIGHT) {
		x -= EFPause_PropStringWidth(text, style);
	}

	fontIndex = EFPause_FontIndexForStyle(style);
	gap = EFPause_FontGapForIndex(fontIndex);
	spaceWidth = EFPause_FontSpaceForIndex(fontIndex);
	height = EFPause_FontHeightForIndex(fontIndex);
	scale = EFPause_PropSizeScale(style);
	drawX = (float)x;
	drawY = (float)y;

#ifdef _XBOX
	if (s_loggedTextDraws < 8) {
		XBLF("STEFX_INPUT_PAUSE_TEXTDRAW text='%.48s' x=%d y=%d style=0x%x font=%d shader=%d width=%d color=%d",
			text,
			x,
			y,
			style,
			fontIndex,
			s_fonts.shader[fontIndex],
			EFPause_PropStringWidth(text, style),
			colorIndex);
		s_loggedTextDraws++;
	}
#endif

	ui.R_SetColor(colorTable[colorIndex]);
	s = (const unsigned char *)text;
	while (*s) {
		if (*s == ' ') {
			charWidth = spaceWidth;
		} else {
			charWidth = s_fonts.propMap[fontIndex][*s][2];
		}

		if (charWidth != -1) {
			if (*s != ' ') {
				fcol = (float)s_fonts.propMap[fontIndex][*s][0] / 256.0f;
				frow = (float)s_fonts.propMap[fontIndex][*s][1] / 256.0f;
				fwidth = (float)charWidth / 256.0f;
				fheight = (float)height / 256.0f;
				ui.R_DrawStretchPic(drawX, drawY, (float)charWidth * scale, (float)height * scale,
					fcol, frow, fcol + fwidth, frow + fheight, s_fonts.shader[fontIndex]);
			}
			drawX += ((float)charWidth + (float)gap) * scale;
		}
		s++;
	}
	ui.R_SetColor(NULL);
}

static float EFPause_TextScale(int style)
{
	if (style & UI_TINYFONT) {
		return 0.65f;
	}
	if (style & UI_SMALLFONT) {
		return 0.8f;
	}
	return 1.0f;
}

static int EFPause_Font(void)
{
	return uiInfo.uiDC.Assets.qhMediumFont;
}

static int EFPause_TextWidth(const char *text, int style)
{
	if (!text) {
		return 0;
	}
	return EFPause_PropStringWidth(text, style);
}

static void EFPause_DrawText(int x, int y, const char *text, int style, int colorIndex)
{
	if (!text || !text[0]) {
		return;
	}

	EFPause_DrawPropString(x, y, text, style, colorIndex);
}

// Shared EF atlas rendering for parser-menu adapters. These fonts use the
// existing renderer-cache invalidation path and load on demand after a map load.
int UI_EFPropTextWidth(const char *text, int style)
{
	return EFPause_PropStringWidth(text, style);
}

void UI_EFDrawPropText(int x, int y, const char *text, int style, int colorIndex)
{
	EFPause_DrawPropString(x, y, text, style, colorIndex);
}

static void EFPause_DrawPic(int x, int y, int w, int h, qhandle_t shader, int colorIndex)
{
	ui.R_SetColor(colorTable[colorIndex]);
	UI_DrawHandlePic((float)x, (float)y, (float)w, (float)h, shader);
	ui.R_SetColor(NULL);
}

static void EFPause_Cache(void)
{
	int i;

	if (s_assets.cached) {
		return;
	}

	if (!uis.whiteShader) {
		uis.whiteShader = ui.R_RegisterShader("white");
	}
	s_assets.whiteShader = uis.whiteShader;
	s_assets.buttonRight = ui.R_RegisterShaderNoMip("menu/new/bar1.tga");
	s_assets.buttonLeftEnd = ui.R_RegisterShaderNoMip("menu/common/barbuttonleft.tga");
	s_assets.square = s_assets.whiteShader;
	s_assets.cornerUpper = ui.R_RegisterShaderNoMip("menu/common/corner_ll_47_7.tga");
	s_assets.cornerUpper2 = ui.R_RegisterShaderNoMip("menu/common/corner_ul_47_7.tga");
	s_assets.cornerLower = ui.R_RegisterShaderNoMip("menu/common/corner_ll_47_18.tga");
	s_assets.suit = ui.R_RegisterShaderNoMip("menu/suit/breakout_suit.tga");

	for (i = 0; i < (int)(sizeof(s_systems) / sizeof(s_systems[0])); i++) {
		s_systems[i].picShader = ui.R_RegisterShaderNoMip(s_systems[i].picName);
		s_systems[i].lineShader = ui.R_RegisterShaderNoMip(s_systems[i].lineName);
	}

#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_CACHE white=%d bar=%d left=%d square=%d suit=%d",
		s_assets.whiteShader, s_assets.buttonRight, s_assets.buttonLeftEnd, s_assets.square, s_assets.suit);
#endif
	s_assets.cached = qtrue;
}

void UI_EFPauseMenu_Cache(void)
{
	EFPause_LoadTextTables();
	EFPause_Cache();
	EFPause_LoadPropFonts();
}

void UI_EFPauseMenu_InvalidateCache(void)
{
	memset(&s_assets, 0, sizeof(s_assets));
	memset(&s_fonts, 0, sizeof(s_fonts));
	s_loggedDraw = qfalse;
#ifdef _XBOX
	XBLog_Write("STEFX: EF pause renderer cache invalidated");
#endif
}

static void EFPause_SplitDescription(const char *text)
{
	const char *p;
	const char *wordStart;
	char word[128];
	char line[512];
	int lineIndex;
	int wordLen;
	int tryLen;
	char tryLine[512];

	memset(s_suitDesc, 0, sizeof(s_suitDesc));
	if (!text || !text[0]) {
		return;
	}

	lineIndex = 0;
	line[0] = '\0';
	p = text;
	while (*p && lineIndex < EF_SUIT_MAXDESC) {
		while (*p == ' ') {
			p++;
		}
		if (!*p) {
			break;
		}

		wordStart = p;
		while (*p && *p != ' ') {
			p++;
		}
		wordLen = (int)(p - wordStart);
		if (wordLen >= (int)sizeof(word)) {
			wordLen = sizeof(word) - 1;
		}
		memcpy(word, wordStart, wordLen);
		word[wordLen] = '\0';

		tryLine[0] = '\0';
		if (line[0]) {
			Q_strncpyz(tryLine, line, sizeof(tryLine));
			Q_strcat(tryLine, sizeof(tryLine), " ");
		}
		Q_strcat(tryLine, sizeof(tryLine), word);

		tryLen = EFPause_TextWidth(tryLine, UI_TINYFONT);
		if (tryLen >= 159 && line[0]) {
			Q_strncpyz(s_suitDesc[lineIndex], line, sizeof(s_suitDesc[lineIndex]));
			lineIndex++;
			Q_strncpyz(line, word, sizeof(line));
		} else {
			Q_strncpyz(line, tryLine, sizeof(line));
		}
	}

	if (line[0] && lineIndex < EF_SUIT_MAXDESC) {
		Q_strncpyz(s_suitDesc[lineIndex], line, sizeof(s_suitDesc[lineIndex]));
	}
}

static void EFPause_SetActiveSystem(int systemIndex, int holdMilliseconds)
{
	const char *desc;
	int maxSystems;

	maxSystems = (int)(sizeof(s_systems) / sizeof(s_systems[0]));
	if (systemIndex < 0) {
		systemIndex = maxSystems - 1;
	} else if (systemIndex >= maxSystems) {
		systemIndex = 0;
	}

	s_activeSystem = systemIndex;
	desc = EFPause_NormalText(s_systems[systemIndex].normalDescEnum, s_systems[systemIndex].fallbackDesc);
	EFPause_SplitDescription(desc);
	s_nextSystemTime = ui.Milliseconds() + holdMilliseconds;
#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_SYSTEM active=%d text='%s'", s_activeSystem, s_systems[systemIndex].fallbackLabel);
#endif
}

static void EFPause_DrawMenuFrame(void)
{
	const char *title;
	const char *footnote;
	int holdX;
	int holdLength;

	title = EFPause_NormalText(EF_MNT_INGAMEMENU_TITLE, "ELITE FORCE : INGAME MAIN MENU");
	footnote = EFPause_NormalText(EF_MNT_HAZARDSUIT_SPECS, "HAZARD SUIT SPECS");

	EFPause_DrawPic(30, 24, 47, 54, s_assets.whiteShader, CT_DKPURPLE2);
	EFPause_DrawPic(30, 81, 47, 34, s_assets.whiteShader, CT_DKPURPLE3);
	EFPause_DrawPic(30, 115, 128, 64, s_assets.cornerUpper, CT_DKPURPLE3);
	EFPause_DrawPic(109, 136, 40, 7, s_assets.whiteShader, CT_DKPURPLE3);
	EFPause_DrawPic(152, 136, 135, 7, s_assets.whiteShader, CT_LTBROWN1);
	EFPause_DrawPic(290, 136, 12, 7, s_assets.whiteShader, CT_DKPURPLE2);
	EFPause_DrawPic(305, 139, 60, 4, s_assets.whiteShader, CT_DKPURPLE2);
	EFPause_DrawPic(368, 136, 111, 7, s_assets.whiteShader, CT_LTBROWN1);
	EFPause_DrawText(EF_MENU_TITLE_X, EF_MENU_TITLE_Y, title, UI_RIGHT | UI_BIGFONT, CT_LTORANGE);

	EFPause_DrawPic(30, 147, 128, 64, s_assets.cornerUpper2, CT_DKBROWN1);
	EFPause_DrawPic(50, 147, 99, 7, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(152, 147, 135, 7, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(290, 147, 12, 7, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(305, 147, 60, 4, s_assets.whiteShader, CT_LTBROWN1);
	EFPause_DrawPic(368, 147, 111, 7, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(30, 173, 47, 27, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(30, 392, 47, 33, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(30, 425, 128, 64, s_assets.cornerLower, CT_DKBROWN1);
	EFPause_DrawPic(96, 438, 268, 18, s_assets.whiteShader, CT_LTBROWN1);

	EFPause_DrawText(EF_MENU_TITLE_X, 440, footnote, UI_RIGHT | UI_SMALLFONT, CT_LTORANGE);
	holdX = EF_MENU_TITLE_X - EFPause_TextWidth(footnote, UI_SMALLFONT);
	holdLength = (367 + 6) - holdX;
	if (holdLength > 0) {
		EFPause_DrawPic(367, 438, holdLength, 18, s_assets.whiteShader, CT_LTBROWN1);
	}
}

static void EFPause_DrawButtons(void)
{
	int i;

	for (i = 0; i < EF_PAUSE_BUTTON_COUNT; i++) {
		efPauseButton_t *button;
		int fillColor;
		int textColor;
		const char *text;

		button = &s_buttons[i];
		fillColor = (i == s_cursor) ? CT_LTPURPLE1 : CT_DKPURPLE1;
		textColor = (i == s_cursor) ? CT_WHITE : CT_BLACK;
		if (!button->enabled) {
			fillColor = CT_DKBROWN1;
			textColor = CT_DKGOLD1;
		}
		text = EFPause_ButtonText(button->textEnum, 0, button->fallbackText);

		if (button->id != EF_PAUSE_SCREENSHOT) {
			EFPause_DrawPic(button->x - 14, button->y, EF_MENU_BUTTON_MED_HEIGHT, EF_MENU_BUTTON_MED_HEIGHT, s_assets.buttonLeftEnd, fillColor);
			EFPause_DrawPic(button->x, button->y, button->w, button->h, s_assets.buttonRight, fillColor);
		} else {
			EFPause_DrawPic(button->x, button->y, button->w, button->h, s_assets.square, fillColor);
		}

		EFPause_DrawText(button->x + EF_MENU_BUTTON_TEXT_X, button->y + EF_MENU_BUTTON_TEXT_Y, text, UI_SMALLFONT, textColor);
	}

	if (s_cursor >= 0 && s_cursor < EF_PAUSE_BUTTON_COUNT) {
		const char *desc = EFPause_ButtonText(s_buttons[s_cursor].textEnum, 1, s_buttons[s_cursor].fallbackDesc);
		EFPause_DrawText(EF_MENU_DESC_X, EF_MENU_DESC_Y, desc, UI_TINYFONT, CT_BLACK);
	}
}

static void EFPause_DrawSuitSystems(void)
{
	int i;
	int y;
	efPauseSystem_t *active;

	EFPause_DrawPic(482, 136, EF_MENU_BUTTON_MED_WIDTH - 14, EF_MENU_BUTTON_MED_HEIGHT, s_assets.whiteShader, CT_LTBROWN1);
	EFPause_DrawPic(460 + EF_MENU_BUTTON_MED_WIDTH + 2, 136, -19, EF_MENU_BUTTON_MED_HEIGHT, s_assets.buttonLeftEnd, CT_LTBROWN1);
	EFPause_DrawPic(140, 170, 512, 256, s_assets.suit, CT_DKBLUE1);

	EFPause_DrawText(85, 162, EFPause_NormalText(EF_MNT_HAZARDSUIT, "HAZARD SUIT"), UI_SMALLFONT, CT_DKGOLD1);
	EFPause_DrawText(108, 406, EFPause_NormalText(EF_MNT_FRONT, "FRONT"), UI_SMALLFONT, CT_DKGOLD1);
	EFPause_DrawText(296, 406, EFPause_NormalText(EF_MNT_BACK, "BACK"), UI_SMALLFONT, CT_DKGOLD1);

	EFPause_DrawPic(445, 318, 6, 103, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(445, 318, 168, 8, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(445, 421, 168, 8, s_assets.whiteShader, CT_DKBROWN1);
	EFPause_DrawPic(30, 203, 47, 186, s_assets.whiteShader, CT_DKPURPLE3);

	EFPause_DrawText(74, 66, "80-345", UI_RIGHT | UI_TINYFONT, CT_BLACK);
	EFPause_DrawText(74, 84, "67-568", UI_RIGHT | UI_TINYFONT, CT_BLACK);
	EFPause_DrawText(74, 188, "451-05", UI_RIGHT | UI_TINYFONT, CT_BLACK);
	EFPause_DrawText(74, 206, "452", UI_RIGHT | UI_TINYFONT, CT_BLACK);
	EFPause_DrawText(74, 395, "57258", UI_RIGHT | UI_TINYFONT, CT_BLACK);
	EFPause_DrawText(592, 142, "1001001", UI_RIGHT | UI_TINYFONT, CT_BLACK);

	for (i = 0; i < (int)(sizeof(s_systems) / sizeof(s_systems[0])); i++) {
		int color = (i == s_activeSystem) ? CT_YELLOW : CT_DKBROWN1;
		EFPause_DrawText(s_systems[i].labelX, s_systems[i].labelY,
			EFPause_ButtonText(s_systems[i].buttonTextEnum, 0, s_systems[i].fallbackLabel),
			s_systems[i].style, color);
	}

	active = &s_systems[s_activeSystem];
	EFPause_DrawPic(active->lineX, active->lineY, active->lineW, active->lineH, active->lineShader, CT_WHITE);
	EFPause_DrawPic(active->picX, active->picY, active->picW, active->picH, active->picShader, CT_LTGOLD1);

	y = 329;
	for (i = 0; i < EF_SUIT_MAXDESC; i++) {
		if (s_suitDesc[i][0]) {
			EFPause_DrawText(454, y, s_suitDesc[i], UI_TINYFONT, CT_LTGOLD1);
		}
		y += 12;
	}
}

static void EFPause_OpenEFQmenu(const char *commandName)
{
	qboolean handled = qfalse;
#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_ACTION openEFQ command='%s'", commandName ? commandName : "");
#endif
	if (commandName && commandName[0]) {
		handled = UI_EFQmenu_ConsoleCommand(commandName);
	}

	// Capture-backed destinations stay on the pause screen until their real EF
	// screens exist; deactivate once either replacement UI owner is active.
	s_active = (UI_EFQmenu_IsActive() || UI_EFMainMenu_IsActive()) ? qfalse : qtrue;
	ui.Key_SetCatcher(KEYCATCH_UI);
	ui.Cvar_Set("cl_paused", "1");
#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_ACTION openEFQ result handled=%d qmenuActive=%d frontendActive=%d pauseActive=%d",
		handled ? 1 : 0,
		UI_EFQmenu_IsActive() ? 1 : 0,
		UI_EFMainMenu_IsActive() ? 1 : 0,
		s_active ? 1 : 0);
#endif
}

static void EFPause_ResumeGame(void)
{
#ifdef _XBOX
	XBLog_Write("STEFX_INPUT_PAUSE_ACTION resume");
#endif
	s_active = qfalse;
	UI_ForceMenuOff();
}

static void EFPause_ActivateButton(void)
{
	if (s_cursor < 0 || s_cursor >= EF_PAUSE_BUTTON_COUNT) {
		return;
	}

#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_ACTION activate id=%d text='%s'",
		s_buttons[s_cursor].id,
		EFPause_ButtonText(s_buttons[s_cursor].textEnum, 0, s_buttons[s_cursor].fallbackText));
#endif

	if (!s_buttons[s_cursor].enabled) {
		return;
	}

	switch (s_buttons[s_cursor].id) {
	case EF_PAUSE_RESUME:
		EFPause_ResumeGame();
		break;
	case EF_PAUSE_SAVE:
#ifdef _XBOX
		XBLog_Write("STEFX: pause save requested -> EF qmenu save");
#endif
		EFPause_OpenEFQmenu("ui_ef_savegame");
		break;
	case EF_PAUSE_LOAD:
#ifdef _XBOX
		XBLog_Write("STEFX: pause load requested -> EF qmenu load");
#endif
		EFPause_OpenEFQmenu("ui_ef_loadgame");
		break;
	case EF_PAUSE_CONFIGURE:
		EFPause_OpenEFQmenu("ui_ef_configure");
		break;
	case EF_PAUSE_LEAVEGAME:
		EFPause_OpenEFQmenu("ui_ef_leavegame");
		break;
	case EF_PAUSE_QUIT:
		EFPause_OpenEFQmenu("ui_ef_quit");
		break;
	case EF_PAUSE_SCREENSHOT:
		s_active = qfalse;
		UI_ForceMenuOff();
		ui.Cmd_ExecuteText(EXEC_APPEND, "wait; wait; wait; wait; screenshot\n");
		break;
	default:
		break;
	}
}

qboolean UI_EFPauseMenu_IsActive(void)
{
	return s_active;
}

void UI_EFPauseMenu_Deactivate(void)
{
	if (s_active) {
#ifdef _XBOX
		XBLog_Write("STEFX_INPUT_PAUSE_DEACTIVATE");
#endif
	}
	s_active = qfalse;
#ifdef _XBOX
	g_SPXBUIPauseActive = 0;
#endif
}

void UI_EFPauseMenu_Open(const char *menuID)
{
#ifdef _XBOX
	g_SPXBUIPauseOpenCount++;
	XBLF("STEFX_INPUT_PAUSE_OPEN_BEGIN menuID='%s' catcher=0x%x",
		menuID ? menuID : "",
		ui.Key_GetCatcher());
#endif
	EFPause_LoadTextTables();
	EFPause_Cache();
	EFPause_LoadPropFonts();

	s_buttons[EF_PAUSE_SAVE].enabled = ui.SG_GameAllowedToSaveHere(qfalse);
	UI_EFMainMenu_Deactivate();
	UI_EFQmenu_ClearState("ef-pause-open");
	s_cursor = 0;
	s_active = qtrue;
#ifdef _XBOX
	g_SPXBUIPauseActive = 1;
#endif
	s_loggedDraw = qfalse;
	s_loggedTextDraws = 0;
	ui.Cvar_Set("cl_paused", "1");
	ui.Key_SetCatcher(KEYCATCH_UI);
	EFPause_SetActiveSystem(0, 5000);

#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_OPEN menuID='%s' catcher=0x%x canSave=%d",
		menuID ? menuID : "",
		ui.Key_GetCatcher(),
		s_buttons[EF_PAUSE_SAVE].enabled);
#endif
}

void UI_EFPauseMenu_Draw(int realtime)
{
	if (!s_active) {
		return;
	}

#ifdef _XBOX
	g_SPXBUIPauseDrawCount++;
	if (!s_loggedDraw) {
		XBLF("STEFX_INPUT_PAUSE_DRAW first realtime=%d catcher=0x%x cursor=%d system=%d",
			realtime,
			ui.Key_GetCatcher(),
			s_cursor,
			s_activeSystem);
		s_loggedDraw = qtrue;
	}
#endif

	if (s_nextSystemTime && realtime >= s_nextSystemTime) {
		EFPause_SetActiveSystem(s_activeSystem + 1, 5000);
	}

	EFPause_DrawMenuFrame();
	EFPause_DrawButtons();
	EFPause_DrawSuitSystems();
}

void UI_EFPauseMenu_KeyEvent(int key, qboolean down)
{
	if (!s_active || !down) {
		return;
	}

#ifdef _XBOX
	XBLF("STEFX_INPUT_PAUSE_KEY key=%d cursor=%d", key, s_cursor);
#endif

	switch (key) {
	case A_ESCAPE:
	case A_JOY1:
	case A_JOY4:
	case A_JOY14:
		EFPause_ResumeGame();
		break;
	case A_ENTER:
	case A_KP_ENTER:
	case A_MOUSE1:
	case A_JOY15:
		EFPause_ActivateButton();
		break;
	case A_CURSOR_UP:
	case A_KP_8:
	case A_CURSOR_LEFT:
	case A_KP_4:
	case A_JOY5:
	case A_JOY8:
		s_cursor--;
		if (s_cursor < 0) {
			s_cursor = EF_PAUSE_BUTTON_COUNT - 1;
		}
		break;
	case A_CURSOR_DOWN:
	case A_KP_2:
	case A_CURSOR_RIGHT:
	case A_KP_6:
	case A_JOY7:
	case A_JOY6:
		s_cursor++;
		if (s_cursor >= EF_PAUSE_BUTTON_COUNT) {
			s_cursor = 0;
		}
		break;
	case A_HOME:
		EFPause_SetActiveSystem(s_activeSystem - 1, 10000);
		break;
	case A_END:
		EFPause_SetActiveSystem(s_activeSystem + 1, 10000);
		break;
	default:
		break;
	}
}
