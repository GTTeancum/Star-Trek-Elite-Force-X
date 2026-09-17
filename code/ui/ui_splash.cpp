
#include "../client/client.h"
#include "../renderer/tr_local.h"
#include "../win32/glw_win_dx8.h"
#include "../win32/win_local.h"
#include "../win32/win_file.h"
#include "../ui/ui_splash.h"

#ifdef _XBOX
#include <xb_log.h>
#endif

extern bool Sys_QuickStart( void );

/*********
Globals
*********/
static bool SP_LicenseDone = false;

#ifdef _XBOX
static const char *s_spDrawTextureContext = "unknown";

static void SP_SetDrawTextureContext(const char *context)
{
	s_spDrawTextureContext = context ? context : "unknown";
}
#endif

typedef struct
{
	qhandle_t piece[9];
	qhandle_t circle;
	qhandle_t quarter;
	qhandle_t white;
	qhandle_t bigFont;
	int tinyFont;
	int bigFontMap[256][3];
	qboolean bigFontLoaded;
	qboolean initialized;
} spEfLoadingAssets_t;

static spEfLoadingAssets_t s_spEfLoadingAssets;
static char s_spEfLoadingFontBuffer[20000];
static int s_spEfLoadingDiscardFontMap[256][3];
static int s_spEfLoadingPulse;
static qboolean s_spEfLoadingFontWarned;
static char s_spEfLoadingTitleMap[MAX_QPATH];
static char s_spEfLoadingTitleLanguage[32];
static char s_spEfLoadingTitle[96];
static char s_spEfLoadingTitleWarnedMap[MAX_QPATH];

typedef struct
{
	char filename[MAX_QPATH];
	char text[32768];
	int length;
	qboolean loaded;
	qboolean attempted;
} spEfLoadingInfoCache_t;

static spEfLoadingInfoCache_t s_spEfLoadingInfoCache[2];

typedef struct
{
	const char *mapName;
	const char *title;
} spEfLoadingTitle_t;

// Canonical English II_<MAP> values from BaseEF/ext_data/infostrings.dat.
// Loading begins while Xbox file search paths are being rebuilt, so these
// small labels must not depend on a file open during the transition.
static const spEfLoadingTitle_t s_spEfLoadingTitles[] =
{
	{ "borg1", "The Rescue" },
	{ "borg2", "Incursion" },
	{ "holodeck", "Tactical Decision" },
	{ "voy1", "Condition" },
	{ "voy2", "Unavoidable Delays" },
	{ "voy3", "Hazard Duty" },
	{ "voy4", "Defense" },
	{ "voy5", "Hazard Ops" },
	{ "stasis1", "Data Retrieval" },
	{ "stasis2", "Deep Echoes" },
	{ "stasis3", "Encounters" },
	{ "voy6", "Renewal" },
	{ "voy7", "Union" },
	{ "voy8", "Departure" },
	{ "scav1", "The Visit" },
	{ "scav2", "Dangerous Ground" },
	{ "scav3", "Conflicting Views" },
	{ "scav3b", "Conflicting Views (pt 2)" },
	{ "scav4", "Disorder" },
	{ "scav5", "Infiltration" },
	{ "scavboss", "The Hunter" },
	{ "voy9", "Fallout" },
	{ "borg3", "Proving Ground" },
	{ "borg4", "Information" },
	{ "borg5", "Covenant" },
	{ "borg6", "Infestation" },
	{ "voy13", "R & R" },
	{ "voy14", "Visual Confirmation" },
	{ "voy15", "Offense" },
	{ "dn1", "The Breach" },
	{ "dn2", "Command" },
	{ "dn3", "Primary Encounter" },
	{ "dn4", "The Skirmish" },
	{ "dn5", "Defensive Measures" },
	{ "dn6", "Attunement" },
	{ "dn8", "Array" },
	{ "voy16", "Invasion" },
	{ "voy17", "Decisions" },
	{ "forge1", "External Stimuli" },
	{ "forge2", "Matrix" },
	{ "forge3", "Onslaught" },
	{ "forge4", "Visual Magnitude" },
	{ "forge5", "Dissolution" },
	{ "forgeboss", "Command Decision" },
	{ "voy20", "Epilogue" },
	{ "tutorial", "Tutorial" }
};

extern "C"
{
	volatile unsigned int g_SPXBLoadingTitleMagic = 0x4c44544c;
	volatile unsigned int g_SPXBLoadingTitleStatus = 0x4c445401;
	volatile unsigned int g_SPXBLoadingTitleShader = 0x4c445402;
	volatile unsigned int g_SPXBLoadingTitleDraws = 0x4c445403;
	volatile unsigned int g_SPXBLoadingTitleLastChar = 0x4c445404;
	volatile unsigned int g_SPXBLoadingTitleMapHash = 0x4c445405;
	volatile unsigned int g_SPXBLoadingTitleTextHash = 0x4c445406;
}

#define SP_EF_LOAD_TEXT_RIGHT 0x0001
#define SP_EF_BIG_FONT_HEIGHT 24
#define SP_EF_BIG_FONT_GAP 3
#define SP_EF_BIG_FONT_SPACE 6

static unsigned int SP_EFLoadingHash(const char *text, int maxLength)
{
	unsigned int hash = 2166136261u;
	int i;

	if (!text)
	{
		return 0;
	}
	for (i = 0; text[i] && i < maxLength; ++i)
	{
		hash ^= (unsigned char)text[i];
		hash *= 16777619u;
	}
	return hash;
}

#if defined(STEFX_SP_HOSTED_MP)
typedef struct
{
	qhandle_t piece[9];
	qhandle_t circle;
	qhandle_t quarter;
	qhandle_t white;
	qhandle_t charset;
	int tinyFont;
	qboolean initialized;
} spEfMPLoadingAssets_t;

static spEfMPLoadingAssets_t s_spEfMPLoadingAssets;
static int s_spEfMPLoadingPulse;
static qboolean s_spEfMPLoadingFontWarned;

#define SP_EF_LOAD_TEXT_CENTER 0x0002
#endif

void SP_InvalidateEFLoadingAssets(void)
{
	memset(&s_spEfLoadingAssets, 0, sizeof(s_spEfLoadingAssets));
	s_spEfLoadingPulse = 0;
	s_spEfLoadingFontWarned = qfalse;
#if defined(STEFX_SP_HOSTED_MP)
	memset(&s_spEfMPLoadingAssets, 0, sizeof(s_spEfMPLoadingAssets));
	s_spEfMPLoadingPulse = 0;
	s_spEfMPLoadingFontWarned = qfalse;
#endif
#ifdef _XBOX
	XBLog_Write("SPL: EF loading renderer cache invalidated");
#endif
}

static qboolean SP_ParseEFLoadingFontMap(const char **cursor, int map[256][3])
{
	const char *token;
	int ch;
	int component;

	token = COM_ParseExt(cursor, qtrue);
	if (!token[0] || Q_stricmp(token, "{"))
	{
		return qfalse;
	}

	for (ch = 0; ch < 256; ch++)
	{
		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "{"))
		{
			return qfalse;
		}
		for (component = 0; component < 3; component++)
		{
			token = COM_ParseExt(cursor, qtrue);
			if (!token[0])
			{
				return qfalse;
			}
			map[ch][component] = atoi(token);
		}
		token = COM_ParseExt(cursor, qtrue);
		if (!token[0] || Q_stricmp(token, "}"))
		{
			return qfalse;
		}
	}

	token = COM_ParseExt(cursor, qtrue);
	return token[0] && !Q_stricmp(token, "}");
}

static void SP_RegisterEFLoadingBigFont(void)
{
	fileHandle_t f = 0;
	const char *parse;
	int len;
	int i;

	for (i = 0; i < 256; i++)
	{
		s_spEfLoadingAssets.bigFontMap[i][0] = 0;
		s_spEfLoadingAssets.bigFontMap[i][1] = 0;
		s_spEfLoadingAssets.bigFontMap[i][2] = -1;
	}
	s_spEfLoadingAssets.bigFont =
		re.RegisterShaderNoMip("gfx/2d/chars_big.tga");

	len = FS_FOpenFileRead("ext_data/fonts.dat", &f, qfalse);
	if (len <= 0 || !f || len >= (int)sizeof(s_spEfLoadingFontBuffer))
	{
		if (f)
		{
			FS_FCloseFile(f);
		}
#ifdef _XBOX
		XBLF("SPL: SP loading big font unavailable len=%d shader=%d",
			len, s_spEfLoadingAssets.bigFont);
#endif
		return;
	}

	FS_Read(s_spEfLoadingFontBuffer, len, f);
	s_spEfLoadingFontBuffer[len] = '\0';
	FS_FCloseFile(f);
	parse = s_spEfLoadingFontBuffer;
	s_spEfLoadingAssets.bigFontLoaded =
		SP_ParseEFLoadingFontMap(&parse, s_spEfLoadingDiscardFontMap) &&
		SP_ParseEFLoadingFontMap(&parse, s_spEfLoadingDiscardFontMap) &&
		SP_ParseEFLoadingFontMap(&parse, s_spEfLoadingAssets.bigFontMap);
#ifdef _XBOX
	XBLF("SPL: SP loading big font loaded=%d len=%d shader=%d",
		s_spEfLoadingAssets.bigFontLoaded ? 1 : 0,
		len,
		s_spEfLoadingAssets.bigFont);
#endif
}

static void SP_RegisterEFLoadingAssets(void)
{
	if (s_spEfLoadingAssets.initialized)
	{
		return;
	}

	s_spEfLoadingAssets.piece[0] = re.RegisterShaderNoMip("menu/loading/smpiece1.tga");
	s_spEfLoadingAssets.piece[1] = re.RegisterShaderNoMip("menu/loading/smpiece2.tga");
	s_spEfLoadingAssets.piece[2] = re.RegisterShaderNoMip("menu/loading/smpiece3.tga");
	s_spEfLoadingAssets.piece[3] = re.RegisterShaderNoMip("menu/loading/smpiece4.tga");
	s_spEfLoadingAssets.piece[4] = re.RegisterShaderNoMip("menu/loading/smpiece5.tga");
	s_spEfLoadingAssets.piece[5] = re.RegisterShaderNoMip("menu/loading/smpiece6.tga");
	s_spEfLoadingAssets.piece[6] = re.RegisterShaderNoMip("menu/loading/smpiece7.tga");
	s_spEfLoadingAssets.piece[7] = re.RegisterShaderNoMip("menu/loading/smpiece8.tga");
	s_spEfLoadingAssets.piece[8] = re.RegisterShaderNoMip("menu/loading/smpiece9.tga");
	s_spEfLoadingAssets.circle = re.RegisterShaderNoMip("menu/loading/arrowpiece.tga");
	s_spEfLoadingAssets.quarter = re.RegisterShaderNoMip("menu/loading/quarter.tga");
	s_spEfLoadingAssets.white = re.RegisterShaderNoMip("white");
	s_spEfLoadingAssets.tinyFont = re.RegisterFont("ergoec");
	SP_RegisterEFLoadingBigFont();
	s_spEfLoadingAssets.initialized = qtrue;

#ifdef _XBOX
	XBLF("SPL: EF loading assets handles %d %d %d %d %d %d %d %d %d circle=%d quarter=%d white=%d tinyFont=%d",
		s_spEfLoadingAssets.piece[0], s_spEfLoadingAssets.piece[1],
		s_spEfLoadingAssets.piece[2], s_spEfLoadingAssets.piece[3],
		s_spEfLoadingAssets.piece[4], s_spEfLoadingAssets.piece[5],
		s_spEfLoadingAssets.piece[6], s_spEfLoadingAssets.piece[7],
		s_spEfLoadingAssets.piece[8], s_spEfLoadingAssets.circle,
		s_spEfLoadingAssets.quarter, s_spEfLoadingAssets.white,
		s_spEfLoadingAssets.tinyFont);
#endif
}

static qboolean SP_EFLoadingAssetsReady(void)
{
	int i;

	for (i = 0; i < 9; i++)
	{
		if (!s_spEfLoadingAssets.piece[i])
		{
#ifdef _XBOX
			XBLF("SPL: EF loading asset missing piece=%d", i + 1);
#endif
			return qfalse;
		}
	}

	if (!s_spEfLoadingAssets.circle || !s_spEfLoadingAssets.quarter || !s_spEfLoadingAssets.white)
	{
#ifdef _XBOX
		XBLF("SPL: EF loading asset missing circle=%d quarter=%d white=%d",
			s_spEfLoadingAssets.circle, s_spEfLoadingAssets.quarter,
			s_spEfLoadingAssets.white);
#endif
		return qfalse;
	}

	return qtrue;
}

static const char *SP_GetEFLoadingMapName(void)
{
	const char *mapName = Cvar_VariableString("ui_mapname");

	if (mapName && mapName[0])
	{
		return mapName;
	}

	mapName = Cvar_VariableString("mapname");
	if (mapName && mapName[0])
	{
		return mapName;
	}

	// cl_mapname has a nonempty engine default before the first server map is
	// published, so it is intentionally the final fallback.
	mapName = Cvar_VariableString("cl_mapname");
	return (mapName && mapName[0]) ? mapName : NULL;
}

static qboolean SP_ParseEFLoadingToken(const char **data, char *out, int outSize)
{
	const char *p = *data;
	int length = 0;

	out[0] = '\0';
	while (p && *p)
	{
		if ((unsigned char)*p <= ' ')
		{
			++p;
			continue;
		}
		if (p[0] == '/' && p[1] == '/')
		{
			p += 2;
			while (*p && *p != '\n')
			{
				++p;
			}
			continue;
		}
		if (p[0] == '/' && p[1] == '*')
		{
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/'))
			{
				++p;
			}
			if (*p)
			{
				p += 2;
			}
			continue;
		}
		break;
	}

	if (!p || !*p)
	{
		*data = p;
		return qfalse;
	}

	if (*p == '{' || *p == '}')
	{
		out[0] = *p++;
		out[1] = '\0';
		*data = p;
		return qtrue;
	}

	if (*p == '"')
	{
		++p;
		while (*p && *p != '"')
		{
			if (length + 1 < outSize)
			{
				out[length++] = *p;
			}
			++p;
		}
		if (*p == '"')
		{
			++p;
		}
	}
	else
	{
		while (*p && (unsigned char)*p > ' ' && *p != '{' && *p != '}')
		{
			if (length + 1 < outSize)
			{
				out[length++] = *p;
			}
			++p;
		}
	}

	out[length] = '\0';
	*data = p;
	return qtrue;
}

static const char *SP_GetEFLoadingInfoText(const char *filename)
{
	spEfLoadingInfoCache_t *cache = NULL;
	fileHandle_t f = 0;
	int i;
	int len;

	for (i = 0; i < (int)(sizeof(s_spEfLoadingInfoCache) /
		sizeof(s_spEfLoadingInfoCache[0])); ++i)
	{
		if (s_spEfLoadingInfoCache[i].filename[0] &&
			!Q_stricmp(s_spEfLoadingInfoCache[i].filename, filename))
		{
			cache = &s_spEfLoadingInfoCache[i];
			break;
		}
		if (!cache && !s_spEfLoadingInfoCache[i].filename[0])
		{
			cache = &s_spEfLoadingInfoCache[i];
		}
	}

	if (!cache)
	{
		return NULL;
	}
	if (cache->attempted)
	{
		return cache->loaded ? cache->text : NULL;
	}

	Q_strncpyz(cache->filename, filename, sizeof(cache->filename));
	cache->attempted = qtrue;
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || !f)
	{
		if (f)
		{
			FS_FCloseFile(f);
		}
		return NULL;
	}

	if (len >= (int)sizeof(cache->text))
	{
		len = (int)sizeof(cache->text) - 1;
	}
	g_SPXBLoadingTitleStatus |= 0x00000020;
	FS_Read(cache->text, len, f);
	cache->text[len] = '\0';
	cache->length = len;
	cache->loaded = qtrue;
	FS_FCloseFile(f);

#ifdef _XBOX
	XBLF("SPL: cached loading title table file='%s' bytes=%d",
		filename, len);
#endif
	return cache->text;
}

void SP_PreloadEFLoadingTitles(void)
{
	const char *language = Cvar_VariableString("g_language");

	SP_GetEFLoadingInfoText("ext_data/infostrings.dat");
	if (language && language[0] && Q_stricmp(language, "ENGLISH"))
	{
		char localizedFile[MAX_QPATH];
		Com_sprintf(localizedFile, sizeof(localizedFile),
			"ext_data/infostrings_%s.dat", language);
		SP_GetEFLoadingInfoText(localizedFile);
	}
}

static qboolean SP_FindEFLoadingTitle(const char *filename, const char *infoItem, char *out, int outSize)
{
	const char *infoText = SP_GetEFLoadingInfoText(filename);
	const char *parse;
	char token[96];

	if (!infoText)
	{
		return qfalse;
	}

	g_SPXBLoadingTitleStatus |= 0x00000020;
	parse = infoText;
	while (1)
	{
		char entryItem[64];
		char entryTitle[80];

		if (!SP_ParseEFLoadingToken(&parse, token, sizeof(token)))
		{
			break;
		}
		if (Q_stricmp(token, "{"))
		{
			continue;
		}

		entryItem[0] = '\0';
		entryTitle[0] = '\0';
		while (1)
		{
			char key[32];

			if (!SP_ParseEFLoadingToken(&parse, token, sizeof(token)) ||
				!Q_stricmp(token, "}"))
			{
				break;
			}

			Q_strncpyz(key, token, sizeof(key));
			if (!SP_ParseEFLoadingToken(&parse, token, sizeof(token)))
			{
				break;
			}

			if (!Q_stricmp(key, "infoitem"))
			{
				Q_strncpyz(entryItem, token, sizeof(entryItem));
			}
			else if (!Q_stricmp(key, "infostring"))
			{
				Q_strncpyz(entryTitle, token, sizeof(entryTitle));
			}
		}

		if (!Q_stricmp(entryItem, infoItem))
		{
			g_SPXBLoadingTitleStatus |= 0x00000040;
			if (entryTitle[0])
			{
				g_SPXBLoadingTitleStatus |= 0x00000080;
				Q_strncpyz(out, entryTitle, outSize);
				Q_CleanStr(out);
				return qtrue;
			}
		}
	}

	return qfalse;
}

static const char *SP_GetEFLoadingTitle(void)
{
	const char *publishedTitle = Cvar_VariableString("ui_sp_levelname");
	const char *mapName = SP_GetEFLoadingMapName();
	const char *language = Cvar_VariableString("g_language");
	const char *baseName;
	const char *slash;
	const char *backslash;
	char cleanMap[MAX_QPATH];
	char infoItem[64];
	char localizedFile[MAX_QPATH];
	char title[80];
	char *extension;

	if (publishedTitle && publishedTitle[0])
	{
		g_SPXBLoadingTitleStatus |= 0x00000003;
		g_SPXBLoadingTitleTextHash = SP_EFLoadingHash(publishedTitle, 96);
		Com_sprintf(s_spEfLoadingTitle, sizeof(s_spEfLoadingTitle),
			"\"%s\"", publishedTitle);
		return s_spEfLoadingTitle;
	}

	if (!mapName || !mapName[0])
	{
		return NULL;
	}
	g_SPXBLoadingTitleStatus |= 0x00000001;
	g_SPXBLoadingTitleMapHash = SP_EFLoadingHash(mapName, MAX_QPATH);

	if (!language)
	{
		language = "";
	}
	if (!Q_stricmp(s_spEfLoadingTitleMap, mapName) &&
		!Q_stricmp(s_spEfLoadingTitleLanguage, language))
	{
		if (s_spEfLoadingTitle[0])
		{
			g_SPXBLoadingTitleStatus |= 0x00000002;
			g_SPXBLoadingTitleTextHash =
				SP_EFLoadingHash(s_spEfLoadingTitle, sizeof(s_spEfLoadingTitle));
		}
		return s_spEfLoadingTitle[0] ? s_spEfLoadingTitle : NULL;
	}

	Q_strncpyz(s_spEfLoadingTitleMap, mapName, sizeof(s_spEfLoadingTitleMap));
	Q_strncpyz(s_spEfLoadingTitleLanguage, language, sizeof(s_spEfLoadingTitleLanguage));
	s_spEfLoadingTitle[0] = '\0';

	slash = strrchr(mapName, '/');
	backslash = strrchr(mapName, '\\');
	baseName = mapName;
	if (slash && slash + 1 > baseName)
	{
		baseName = slash + 1;
	}
	if (backslash && backslash + 1 > baseName)
	{
		baseName = backslash + 1;
	}
	Q_strncpyz(cleanMap, baseName, sizeof(cleanMap));
	extension = strrchr(cleanMap, '.');
	if (extension)
	{
		*extension = '\0';
	}
	Q_strupr(cleanMap);
	// Expansion deck labels use II_DECK_02 while BSPs use tour/deck02.
	if (!Q_stricmpn(cleanMap, "DECK", 4) && strlen(cleanMap) == 6 &&
		cleanMap[4] >= '0' && cleanMap[4] <= '9' &&
		cleanMap[5] >= '0' && cleanMap[5] <= '9')
	{
		Com_sprintf(infoItem, sizeof(infoItem), "II_DECK_%s", cleanMap + 4);
	}
	else
	{
		Com_sprintf(infoItem, sizeof(infoItem), "II_%s", cleanMap);
	}

	title[0] = '\0';
	if (!language[0] || !Q_stricmp(language, "ENGLISH"))
	{
		int titleIndex;
		for (titleIndex = 0;
			titleIndex < (int)(sizeof(s_spEfLoadingTitles) /
				sizeof(s_spEfLoadingTitles[0]));
			++titleIndex)
		{
			if (!Q_stricmp(s_spEfLoadingTitles[titleIndex].mapName, cleanMap))
			{
				Q_strncpyz(title, s_spEfLoadingTitles[titleIndex].title,
					sizeof(title));
				g_SPXBLoadingTitleStatus |= 0x00000100;
				break;
			}
		}
	}
	if (language[0] && Q_stricmp(language, "ENGLISH"))
	{
		Com_sprintf(localizedFile, sizeof(localizedFile),
			"ext_data/infostrings_%s.dat", language);
		SP_FindEFLoadingTitle(localizedFile, infoItem, title, sizeof(title));
	}
	if (!title[0])
	{
		SP_FindEFLoadingTitle("ext_data/infostrings.dat", infoItem, title, sizeof(title));
	}

	if (title[0])
	{
		g_SPXBLoadingTitleStatus |= 0x00000002;
		g_SPXBLoadingTitleTextHash = SP_EFLoadingHash(title, sizeof(title));
		Com_sprintf(s_spEfLoadingTitle, sizeof(s_spEfLoadingTitle), "\"%s\"", title);
		s_spEfLoadingTitleWarnedMap[0] = '\0';
#ifdef _XBOX
		XBLF("SPL: SP loading title map='%s' key='%s' text='%s'",
			mapName, infoItem, s_spEfLoadingTitle);
#endif
	}
	else
	{
		// Loading draws can begin before the filesystem is ready. Do not cache
		// that transient miss for the rest of the map load.
		s_spEfLoadingTitleMap[0] = '\0';
		s_spEfLoadingTitleLanguage[0] = '\0';
#ifdef _XBOX
		if (Q_stricmp(s_spEfLoadingTitleWarnedMap, mapName))
		{
			Q_strncpyz(s_spEfLoadingTitleWarnedMap, mapName,
				sizeof(s_spEfLoadingTitleWarnedMap));
			XBLF("SPL: SP loading title missing map='%s' key='%s'",
				mapName, infoItem);
		}
#endif
	}

	return s_spEfLoadingTitle[0] ? s_spEfLoadingTitle : NULL;
}

void SP_PrecacheEFLoadingTitle(void)
{
	const char *title = SP_GetEFLoadingTitle();
#ifdef _XBOX
	if (title && title[0])
	{
		XBLF("SPL: precached SP loading title %s", title);
	}
	else
	{
		XBLog_Write("SPL: SP loading title precache unavailable");
	}
#endif
}

static void SP_DrawEFLoadingTitle(int x, int y, const char *text)
{
	const unsigned char *s = (const unsigned char *)text;
	float drawX = (float)x;

	if (!text || !text[0] ||
		!s_spEfLoadingAssets.bigFontLoaded ||
		!s_spEfLoadingAssets.bigFont)
	{
		return;
	}

	g_SPXBLoadingTitleStatus |= 0x00000010;
	re.SetColor(colorTable[CT_WHITE]);
	while (*s)
	{
		const int ch = *s++;
		const int sx = s_spEfLoadingAssets.bigFontMap[ch][0];
		const int sy = s_spEfLoadingAssets.bigFontMap[ch][1];
		const int sw = s_spEfLoadingAssets.bigFontMap[ch][2];

		if (ch == ' ')
		{
			drawX += (float)(SP_EF_BIG_FONT_SPACE + SP_EF_BIG_FONT_GAP);
			continue;
		}
		if (sw < 0)
		{
			continue;
		}

		re.DrawStretchPic(
			drawX, (float)y, (float)sw, (float)SP_EF_BIG_FONT_HEIGHT,
			(float)sx / 256.0f,
			(float)sy / 256.0f,
			(float)(sx + sw) / 256.0f,
			(float)(sy + SP_EF_BIG_FONT_HEIGHT) / 256.0f,
			s_spEfLoadingAssets.bigFont);
		++g_SPXBLoadingTitleDraws;
		g_SPXBLoadingTitleLastChar = ch;
		drawX += (float)(sw + SP_EF_BIG_FONT_GAP);
	}
	re.SetColor(NULL);
}

static void SP_DrawEFLoadingPic(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	float s0 = 0.0f;
	float s1 = 1.0f;
	float t0 = 0.0f;
	float t1 = 1.0f;

	if (!shader)
	{
		return;
	}

	if (w < 0.0f)
	{
		w = -w;
		s0 = 1.0f;
		s1 = 0.0f;
	}
	if (h < 0.0f)
	{
		h = -h;
		t0 = 1.0f;
		t1 = 0.0f;
	}

	re.SetColor(colorTable[colorIndex]);
	re.DrawStretchPic(x, y, w, h, s0, t0, s1, t1, shader);
}

static void SP_DrawEFLoadingPicStage(float x, float y, float w, float h, qhandle_t shader, int stage, int threshold, int darkColor, int litColor)
{
	SP_DrawEFLoadingPic(x, y, w, h, shader, (stage < threshold) ? darkColor : litColor);
}

static void SP_DrawEFLoadingText(int x, int y, const char *text, int style)
{
	const float scale = 0.65f;

	if (!s_spEfLoadingAssets.tinyFont)
	{
#ifdef _XBOX
		if (!s_spEfLoadingFontWarned)
		{
			XBLog_Write("SPL: EF loading tiny font missing; drawing LCARS art without numeric labels\n");
			s_spEfLoadingFontWarned = qtrue;
		}
#endif
		return;
	}

	if (style & SP_EF_LOAD_TEXT_RIGHT)
	{
		x -= re.Font_StrLenPixels(text, s_spEfLoadingAssets.tinyFont, scale);
	}

	re.Font_DrawString(x, y, text, colorTable[CT_BLACK], s_spEfLoadingAssets.tinyFont, -1, scale);
}

static qboolean SP_DrawEFLoadingScreen(void)
{
	const int x = 10;
	const int y = 310;
	const int stage = 0;

	g_SPXBLoadingTitleStatus = 0;
	g_SPXBLoadingTitleShader = 0;
	g_SPXBLoadingTitleDraws = 0;
	g_SPXBLoadingTitleLastChar = 0;
	g_SPXBLoadingTitleMapHash = 0;
	g_SPXBLoadingTitleTextHash = 0;
	SP_RegisterEFLoadingAssets();
	g_SPXBLoadingTitleShader = (unsigned int)s_spEfLoadingAssets.bigFont;
	if (s_spEfLoadingAssets.bigFontLoaded)
	{
		g_SPXBLoadingTitleStatus |= 0x00000004;
	}
	if (s_spEfLoadingAssets.bigFont)
	{
		g_SPXBLoadingTitleStatus |= 0x00000008;
	}
	if (!SP_EFLoadingAssetsReady())
	{
#ifdef _XBOX
		XBLog_Write("SPL: EF loading assets missing; JA LoadSP fallback removed");
#endif
		return qfalse;
	}

#ifdef _XBOX
	XBLF("SPL: drawing EF LCARS load screen from menu/loading assets stage=%d", stage);
#endif

	re.SetColor(colorTable[CT_BLACK]);
	re.DrawStretchPic(0, 0, 640, 480, 0, 0, 0, 0, s_spEfLoadingAssets.white);

	SP_DrawEFLoadingPicStage(x + 18, y + 102, 128, 64, s_spEfLoadingAssets.piece[0], stage, 1, CT_VDKPURPLE3, CT_VLTPURPLE3);
	SP_DrawEFLoadingPicStage(x,      y + 37,   64, 64, s_spEfLoadingAssets.piece[1], stage, 2, CT_VDKBLUE1, CT_VLTBLUE1);
	SP_DrawEFLoadingPicStage(x + 17, y,       128, 64, s_spEfLoadingAssets.piece[2], stage, 3, CT_VDKPURPLE1, CT_LTPURPLE1);
	SP_DrawEFLoadingPicStage(x + 99, y,       128,128, s_spEfLoadingAssets.piece[3], stage, 4, CT_VDKPURPLE2, CT_LTPURPLE2);
	SP_DrawEFLoadingPicStage(x +137, y + 81,   64, 64, s_spEfLoadingAssets.piece[4], stage, 5, CT_VDKBLUE2, CT_VLTBLUE2);
	SP_DrawEFLoadingPicStage(x + 45, y + 99,  128, 64, s_spEfLoadingAssets.piece[5], stage, 6, CT_VDKORANGE, CT_LTORANGE);
	SP_DrawEFLoadingPicStage(x + 38, y + 24,   64,128, s_spEfLoadingAssets.piece[6], stage, 7, CT_VDKBLUE2, CT_LTBLUE2);
	SP_DrawEFLoadingPicStage(x + 78, y + 20,  128, 64, s_spEfLoadingAssets.piece[7], stage, 8, CT_VDKPURPLE1, CT_LTPURPLE1);
	SP_DrawEFLoadingPicStage(x +112, y + 66,   64,128, s_spEfLoadingAssets.piece[8], stage, 9, CT_VDKBROWN1, CT_VLTBROWN1);
	SP_DrawEFLoadingPicStage(x + 62, y + 44,  128,128, s_spEfLoadingAssets.circle, stage, 9, CT_DKBLUE2, CT_LTBLUE2);

	SP_DrawEFLoadingPic(x +  61, y + 43,  32,  32, s_spEfLoadingAssets.quarter, CT_DKPURPLE2);
	SP_DrawEFLoadingPic(x + 135, y + 43, -32,  32, s_spEfLoadingAssets.quarter, CT_DKPURPLE2);
	SP_DrawEFLoadingPic(x + 135, y +117, -32, -32, s_spEfLoadingAssets.quarter, CT_DKPURPLE2);
	SP_DrawEFLoadingPic(x +  61, y +117,  32, -32, s_spEfLoadingAssets.quarter, CT_DKPURPLE2);

	s_spEfLoadingPulse++;
	if (s_spEfLoadingPulse > 3)
	{
		s_spEfLoadingPulse = 0;
	}

	switch (s_spEfLoadingPulse)
	{
	case 0:
		SP_DrawEFLoadingPic(x +  61, y + 43,  32,  32, s_spEfLoadingAssets.quarter, CT_LTPURPLE2);
		break;
	case 1:
		SP_DrawEFLoadingPic(x + 135, y + 43, -32,  32, s_spEfLoadingAssets.quarter, CT_LTPURPLE2);
		break;
	case 2:
		SP_DrawEFLoadingPic(x + 135, y +117, -32, -32, s_spEfLoadingAssets.quarter, CT_LTPURPLE2);
		break;
	default:
		SP_DrawEFLoadingPic(x +  61, y +117,  32, -32, s_spEfLoadingAssets.quarter, CT_LTPURPLE2);
		break;
	}

	SP_DrawEFLoadingText(x +  21, y + 150, "0987", 0);
	SP_DrawEFLoadingText(x +   3, y +  90, "18", 0);
	SP_DrawEFLoadingText(x +  24, y +  20, "7", 0);
	SP_DrawEFLoadingText(x +  93, y +   5, "51", SP_EF_LOAD_TEXT_RIGHT);
	SP_DrawEFLoadingText(x + 103, y +   5, "35", 0);
	SP_DrawEFLoadingText(x + 165, y +  83, "21", 0);
	SP_DrawEFLoadingText(x + 101, y + 149, "67", 0);
	SP_DrawEFLoadingText(x + 123, y +  36, "8", 0);
	SP_DrawEFLoadingText(x +  90, y +  65, "1", SP_EF_LOAD_TEXT_RIGHT);
	SP_DrawEFLoadingText(x + 105, y +  65, "2", 0);
	SP_DrawEFLoadingText(x + 105, y +  87, "3", 0);
	SP_DrawEFLoadingText(x +  91, y +  87, "4", SP_EF_LOAD_TEXT_RIGHT);

	SP_DrawEFLoadingTitle(15, 20, SP_GetEFLoadingTitle());
	re.SetColor(NULL);
	return qtrue;
}

#if defined(STEFX_SP_HOSTED_MP)
static const char *SP_MPGetLoadingMapName(void)
{
	const char *mapName = Cvar_VariableString("mapname");
	const char *uiMapName = Cvar_VariableString("ui_mapname");

	if (uiMapName && uiMapName[0])
	{
		if (!mapName || !mapName[0] || Q_stricmp(uiMapName, mapName))
		{
			return uiMapName;
		}
	}

	if (mapName && mapName[0])
	{
		return mapName;
	}

	mapName = Cvar_VariableString("cl_mapname");
	if (mapName && mapName[0])
	{
		return mapName;
	}

	return "unknownmap";
}

static void SP_MPCleanLoadingText(char *text)
{
	if (!text)
	{
		return;
	}

	Q_CleanStr(text);
	Q_strupr(text);
}

static qboolean SP_MPFindArenaLongNameInFile(const char *filename, const char *mapName, const char *language, char *out, int outSize)
{
	static char arenaText[65536];
	fileHandle_t f;
	int len;
	const char *parse;
	const char *token;
	char localizedKey[64];

	if (!filename || !mapName || !mapName[0] || !out || outSize <= 0)
	{
		return qfalse;
	}

	f = 0;
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || !f)
	{
		if (f)
		{
			FS_FCloseFile(f);
		}
		return qfalse;
	}

	if (len >= (int)sizeof(arenaText))
	{
		len = (int)sizeof(arenaText) - 1;
	}
	FS_Read(arenaText, len, f);
	arenaText[len] = '\0';
	FS_FCloseFile(f);

	localizedKey[0] = '\0';
	if (language && language[0])
	{
		Com_sprintf(localizedKey, sizeof(localizedKey), "longname_%s", language);
	}

	parse = arenaText;
	while (1)
	{
		char entryMap[MAX_QPATH];
		char longName[128];
		char localizedName[128];

		token = COM_ParseExt(&parse, qtrue);
		if (!token[0])
		{
			break;
		}
		if (Q_stricmp(token, "{"))
		{
			continue;
		}

		entryMap[0] = '\0';
		longName[0] = '\0';
		localizedName[0] = '\0';

		while (1)
		{
			char key[64];

			token = COM_ParseExt(&parse, qtrue);
			if (!token[0] || !Q_stricmp(token, "}"))
			{
				break;
			}

			Q_strncpyz(key, token, sizeof(key));
			token = COM_ParseExt(&parse, qtrue);
			if (!token[0])
			{
				break;
			}

			if (!Q_stricmp(key, "map"))
			{
				Q_strncpyz(entryMap, token, sizeof(entryMap));
			}
			else if (!Q_stricmp(key, "longname"))
			{
				Q_strncpyz(longName, token, sizeof(longName));
			}
			else if (localizedKey[0] && !Q_stricmp(key, localizedKey))
			{
				Q_strncpyz(localizedName, token, sizeof(localizedName));
			}
		}

		if (entryMap[0] && !Q_stricmp(entryMap, mapName))
		{
			const char *chosenName = localizedName[0] ? localizedName : longName;
			if (chosenName && chosenName[0])
			{
				Q_strncpyz(out, chosenName, outSize);
				Q_CleanStr(out);
				return qtrue;
			}
			return qfalse;
		}
	}

	return qfalse;
}

static void SP_MPGetLocalizedLoadingMapTitle(const char *mapName, char *out, int outSize)
{
	const char *language;

	if (!out || outSize <= 0)
	{
		return;
	}

	out[0] = '\0';
	language = Cvar_VariableString("ui_language");

	if (SP_MPFindArenaLongNameInFile("scripts/arenas.txt", mapName, language, out, outSize))
	{
		return;
	}
	if (SP_MPFindArenaLongNameInFile("scripts/xpack.arena", mapName, language, out, outSize))
	{
		return;
	}

	Q_strncpyz(out, mapName ? mapName : "unknownmap", outSize);
	SP_MPCleanLoadingText(out);
}

static void SP_MPRegisterLoadingAssets(void)
{
	if (s_spEfMPLoadingAssets.initialized)
	{
		return;
	}

	s_spEfMPLoadingAssets.piece[0] = re.RegisterShaderNoMip("menu/loading/smpiece1.dds");
	s_spEfMPLoadingAssets.piece[1] = re.RegisterShaderNoMip("menu/loading/smpiece2.dds");
	s_spEfMPLoadingAssets.piece[2] = re.RegisterShaderNoMip("menu/loading/smpiece3.dds");
	s_spEfMPLoadingAssets.piece[3] = re.RegisterShaderNoMip("menu/loading/smpiece4.dds");
	s_spEfMPLoadingAssets.piece[4] = re.RegisterShaderNoMip("menu/loading/smpiece5.dds");
	s_spEfMPLoadingAssets.piece[5] = re.RegisterShaderNoMip("menu/loading/smpiece6.dds");
	s_spEfMPLoadingAssets.piece[6] = re.RegisterShaderNoMip("menu/loading/smpiece7.dds");
	s_spEfMPLoadingAssets.piece[7] = re.RegisterShaderNoMip("menu/loading/smpiece8.dds");
	s_spEfMPLoadingAssets.piece[8] = re.RegisterShaderNoMip("menu/loading/smpiece9.dds");
	s_spEfMPLoadingAssets.circle = re.RegisterShaderNoMip("menu/loading/arrowpiece.dds");
	s_spEfMPLoadingAssets.quarter = re.RegisterShaderNoMip("menu/loading/quarter.dds");
	s_spEfMPLoadingAssets.white = re.RegisterShaderNoMip("white");
	s_spEfMPLoadingAssets.charset = re.RegisterShaderNoMip("gfx/2d/charsgrid_med");
	s_spEfMPLoadingAssets.tinyFont = re.RegisterFont("ergoec");
	s_spEfMPLoadingAssets.initialized = qtrue;

#ifdef _XBOX
	XBLF("SPL: EF MP loading assets handles %d %d %d %d %d %d %d %d %d circle=%d quarter=%d white=%d charset=%d tinyFont=%d",
		s_spEfMPLoadingAssets.piece[0], s_spEfMPLoadingAssets.piece[1],
		s_spEfMPLoadingAssets.piece[2], s_spEfMPLoadingAssets.piece[3],
		s_spEfMPLoadingAssets.piece[4], s_spEfMPLoadingAssets.piece[5],
		s_spEfMPLoadingAssets.piece[6], s_spEfMPLoadingAssets.piece[7],
		s_spEfMPLoadingAssets.piece[8], s_spEfMPLoadingAssets.circle,
		s_spEfMPLoadingAssets.quarter, s_spEfMPLoadingAssets.white,
		s_spEfMPLoadingAssets.charset, s_spEfMPLoadingAssets.tinyFont);
#endif
}

static qboolean SP_MPLoadingAssetsReady(void)
{
	int i;

	for (i = 0; i < 9; i++)
	{
		if (!s_spEfMPLoadingAssets.piece[i])
		{
#ifdef _XBOX
			XBLF("SPL: EF MP loading asset missing piece=%d", i + 1);
#endif
			return qfalse;
		}
	}

	if (!s_spEfMPLoadingAssets.circle || !s_spEfMPLoadingAssets.quarter ||
		!s_spEfMPLoadingAssets.white || !s_spEfMPLoadingAssets.charset)
	{
#ifdef _XBOX
		XBLF("SPL: EF MP loading asset missing circle=%d quarter=%d white=%d charset=%d",
			s_spEfMPLoadingAssets.circle, s_spEfMPLoadingAssets.quarter,
			s_spEfMPLoadingAssets.white, s_spEfMPLoadingAssets.charset);
#endif
		return qfalse;
	}

	return qtrue;
}

static void SP_MPDrawLoadingPic(float x, float y, float w, float h, qhandle_t shader, int colorIndex)
{
	float s0 = 0.0f;
	float s1 = 1.0f;
	float t0 = 0.0f;
	float t1 = 1.0f;

	if (!shader)
	{
		return;
	}

	if (w < 0.0f)
	{
		x += w;
		w = -w;
		s0 = 1.0f;
		s1 = 0.0f;
	}
	if (h < 0.0f)
	{
		y += h;
		h = -h;
		t0 = 1.0f;
		t1 = 0.0f;
	}

	re.SetColor(colorTable[colorIndex]);
	re.DrawStretchPic(x, y, w, h, s0, t0, s1, t1, shader);
}

static void SP_MPDrawLoadingPicStage(float x, float y, float w, float h, qhandle_t shader, int stage, int threshold, int darkColor, int litColor)
{
	SP_MPDrawLoadingPic(x, y, w, h, shader, (stage < threshold) ? darkColor : litColor);
}

static void SP_MPDrawWheelText(int x, int y, const char *text, int style)
{
	const float scale = 0.65f;

	if (!s_spEfMPLoadingAssets.tinyFont)
	{
#ifdef _XBOX
		if (!s_spEfMPLoadingFontWarned)
		{
			XBLog_Write("SPL: EF MP loading tiny font missing; drawing LCARS art without numeric labels");
			s_spEfMPLoadingFontWarned = qtrue;
		}
#endif
		return;
	}

	if (style & SP_EF_LOAD_TEXT_RIGHT)
	{
		x -= re.Font_StrLenPixels(text, s_spEfMPLoadingAssets.tinyFont, scale);
	}

	re.Font_DrawString(x, y, text, colorTable[CT_BLACK], s_spEfMPLoadingAssets.tinyFont, -1, scale);
}

static void SP_MPDrawText(int x, int y, const char *text, int style, const float *color, float scale)
{
	const char *s;
	int textWidth = 0;
	int charW;
	int charH;

	if (!text || !text[0] || !s_spEfMPLoadingAssets.charset)
	{
		return;
	}

	charW = (int)(16.0f * scale);
	charH = (int)(24.0f * scale);
	if (charW < 4)
	{
		charW = 4;
	}
	if (charH < 6)
	{
		charH = 6;
	}

	for (s = text; *s; ++s)
	{
		textWidth += charW;
	}

	if (style & SP_EF_LOAD_TEXT_RIGHT)
	{
		x -= textWidth;
	}
	else if (style & SP_EF_LOAD_TEXT_CENTER)
	{
		x -= textWidth / 2;
	}

	re.SetColor(color);
	for (s = text; *s; ++s)
	{
		int ch = *s & 255;
		if (ch != ' ')
		{
			int row = ch >> 4;
			int col = ch & 15;
			float frow = row * 0.0625f;
			float fcol = col * 0.0625f;
			re.DrawStretchPic((float)x, (float)y, (float)charW, (float)charH,
				fcol, frow, fcol + 0.0625f, frow + 0.0625f,
				s_spEfMPLoadingAssets.charset);
		}
		x += charW;
	}
	re.SetColor(NULL);
}

static void SP_MPDrawRect(float x, float y, float w, float h, int colorIndex)
{
	re.SetColor(colorTable[colorIndex]);
	re.DrawStretchPic(x, y, w, h, 0, 0, 1, 1, s_spEfMPLoadingAssets.white);
}

static void SP_MPDrawLoadingQuarter(float wheelX, float wheelY, int quadrant, int colorIndex)
{
	const float circleX = wheelX + 62.0f;
	const float circleY = wheelY + 44.0f;
	const float circleVisibleSize = 73.0f;
	const float quarterSize = 32.0f;
	const float edgeBleed = 1.0f;
	const float left = circleX - edgeBleed;
	const float top = circleY - edgeBleed;
	const float right = circleX + circleVisibleSize;
	const float bottom = circleY + circleVisibleSize;

	switch (quadrant & 3)
	{
	case 0:
		SP_MPDrawLoadingPic(left, top, quarterSize, quarterSize, s_spEfMPLoadingAssets.quarter, colorIndex);
		break;
	case 1:
		SP_MPDrawLoadingPic(right, top, -quarterSize, quarterSize, s_spEfMPLoadingAssets.quarter, colorIndex);
		break;
	case 2:
		SP_MPDrawLoadingPic(right, bottom, -quarterSize, -quarterSize, s_spEfMPLoadingAssets.quarter, colorIndex);
		break;
	default:
		SP_MPDrawLoadingPic(left, bottom, quarterSize, -quarterSize, s_spEfMPLoadingAssets.quarter, colorIndex);
		break;
	}
}

static void SP_MPDrawLoadingQuarters(float wheelX, float wheelY, int colorIndex)
{
	int quadrant;

	for (quadrant = 0; quadrant < 4; quadrant++)
	{
		SP_MPDrawLoadingQuarter(wheelX, wheelY, quadrant, colorIndex);
	}
}

static void SP_MPDrawWheel(float x, float y)
{
	const int stage = 0;

	SP_MPDrawLoadingPicStage(x + 18, y + 102, 128, 64, s_spEfMPLoadingAssets.piece[0], stage, 1, CT_VDKPURPLE3, CT_VLTPURPLE3);
	SP_MPDrawLoadingPicStage(x,      y + 37,   64, 64, s_spEfMPLoadingAssets.piece[1], stage, 2, CT_VDKBLUE1, CT_VLTBLUE1);
	SP_MPDrawLoadingPicStage(x + 17, y,       128, 64, s_spEfMPLoadingAssets.piece[2], stage, 3, CT_VDKPURPLE1, CT_LTPURPLE1);
	SP_MPDrawLoadingPicStage(x + 99, y,       128,128, s_spEfMPLoadingAssets.piece[3], stage, 4, CT_VDKPURPLE2, CT_LTPURPLE2);
	SP_MPDrawLoadingPicStage(x +137, y + 81,   64, 64, s_spEfMPLoadingAssets.piece[4], stage, 5, CT_VDKBLUE2, CT_VLTBLUE2);
	SP_MPDrawLoadingPicStage(x + 45, y + 99,  128, 64, s_spEfMPLoadingAssets.piece[5], stage, 6, CT_VDKORANGE, CT_LTORANGE);
	SP_MPDrawLoadingPicStage(x + 38, y + 24,   64,128, s_spEfMPLoadingAssets.piece[6], stage, 7, CT_VDKBLUE2, CT_LTBLUE2);
	SP_MPDrawLoadingPicStage(x + 78, y + 20,  128, 64, s_spEfMPLoadingAssets.piece[7], stage, 8, CT_VDKPURPLE1, CT_LTPURPLE1);
	SP_MPDrawLoadingPicStage(x +112, y + 66,   64,128, s_spEfMPLoadingAssets.piece[8], stage, 9, CT_VDKBROWN1, CT_VLTBROWN1);
	SP_MPDrawLoadingPicStage(x + 62, y + 44,  128,128, s_spEfMPLoadingAssets.circle, stage, 9, CT_DKBLUE2, CT_LTBLUE2);

	SP_MPDrawLoadingQuarters(x, y, CT_DKPURPLE2);

	s_spEfMPLoadingPulse++;
	if (s_spEfMPLoadingPulse > 3)
	{
		s_spEfMPLoadingPulse = 0;
	}
	SP_MPDrawLoadingQuarter(x, y, s_spEfMPLoadingPulse, CT_LTPURPLE2);

	SP_MPDrawWheelText(x +  21, y + 150, "0987", 0);
	SP_MPDrawWheelText(x +   3, y +  90, "18", 0);
	SP_MPDrawWheelText(x +  24, y +  20, "7", 0);
	SP_MPDrawWheelText(x +  93, y +   5, "51", SP_EF_LOAD_TEXT_RIGHT);
	SP_MPDrawWheelText(x + 103, y +   5, "35", 0);
	SP_MPDrawWheelText(x + 165, y +  83, "21", 0);
	SP_MPDrawWheelText(x + 101, y + 149, "67", 0);
	SP_MPDrawWheelText(x + 123, y +  36, "8", 0);
	SP_MPDrawWheelText(x +  90, y +  65, "1", SP_EF_LOAD_TEXT_RIGHT);
	SP_MPDrawWheelText(x + 105, y +  65, "2", 0);
	SP_MPDrawWheelText(x + 105, y +  87, "3", 0);
	SP_MPDrawWheelText(x +  91, y +  87, "4", SP_EF_LOAD_TEXT_RIGHT);
}

static qboolean SP_DrawEFMPLoadingScreen(void)
{
	const char *mapName = SP_MPGetLoadingMapName();
	char mapTitle[MAX_QPATH];
	char gametypeText[64];
	qhandle_t levelshot;
	int gametype;
#ifdef _XBOX
	static char s_lastLoggedMap[MAX_QPATH];
#endif

	SP_MPRegisterLoadingAssets();
	if (!SP_MPLoadingAssetsReady())
	{
		return qfalse;
	}

	SP_MPGetLocalizedLoadingMapTitle(mapName, mapTitle, sizeof(mapTitle));
	gametype = Cvar_VariableIntegerValue("g_gametype");
	switch (gametype)
	{
	case 1:
		Q_strncpyz(gametypeText, "TOURNAMENT", sizeof(gametypeText));
		break;
	case 3:
		Q_strncpyz(gametypeText, "TEAM HOLOMATCH", sizeof(gametypeText));
		break;
	case 4:
		Q_strncpyz(gametypeText, "CAPTURE THE FLAG", sizeof(gametypeText));
		break;
	default:
		Q_strncpyz(gametypeText, "FREE FOR ALL", sizeof(gametypeText));
		break;
	}

	levelshot = re.RegisterShaderNoMip(va("levelshots/%s.dds", mapName));

#ifdef _XBOX
	if (Q_stricmp(s_lastLoggedMap, mapName))
	{
		XBLF("SPL: drawing EF MP load screen map='%s' title='%s' levelshot=%d gametype=%d",
			mapName, mapTitle, levelshot, gametype);
		if (!levelshot)
		{
			XBLF("SPL: missing DDS MP levelshot for map='%s'", mapName);
		}
		Q_strncpyz(s_lastLoggedMap, mapName, sizeof(s_lastLoggedMap));
	}
#endif

	SP_MPDrawRect(0, 0, 640, 480, CT_BLACK);
	if (levelshot)
	{
		re.SetColor(colorTable[CT_WHITE]);
		re.DrawStretchPic(0, 0, 640, 480, 0, 0, 1, 1, levelshot);
	}

	SP_MPDrawRect(0, 0, 640, 54, CT_BLACK);
	SP_MPDrawRect(0, 392, 640, 88, CT_BLACK);
	SP_MPDrawRect(0, 54, 10, 338, CT_BLACK);
	SP_MPDrawRect(630, 54, 10, 338, CT_BLACK);

	SP_MPDrawText(18, 18, "HOLOMATCH", 0, colorTable[CT_LTORANGE], 1.25f);
	SP_MPDrawText(622, 22, "ELITE FORCE", SP_EF_LOAD_TEXT_RIGHT, colorTable[CT_LTGOLD1], 0.82f);
	SP_MPDrawText(604, 96, mapTitle, SP_EF_LOAD_TEXT_RIGHT, colorTable[CT_LTGOLD1], 1.05f);
	SP_MPDrawText(604, 132, gametypeText, SP_EF_LOAD_TEXT_RIGHT, colorTable[CT_WHITE], 0.78f);
	SP_MPDrawText(320, 438, "LOADING SIMULATION", SP_EF_LOAD_TEXT_CENTER, colorTable[CT_LTGOLD1], 0.86f);

	SP_MPDrawWheel(10, 244);
	re.SetColor(NULL);
	return qtrue;
}

extern "C" void STEFX_DrawHolomatchLoadingScreen(void)
{
	if (SP_DrawEFMPLoadingScreen())
	{
		return;
	}

#ifdef _XBOX
	static qboolean s_loggedUnavailable = qfalse;
	if (!s_loggedUnavailable)
	{
		XBLog_Write("SPL: Holomatch cgame requested loading screen before shared assets were ready");
		s_loggedUnavailable = qtrue;
	}
#endif

	SP_MPRegisterLoadingAssets();
	if (s_spEfMPLoadingAssets.white)
	{
		SP_MPDrawRect(0, 0, 640, 480, CT_BLACK);
		re.SetColor(NULL);
	}
}
#endif

/*********
SP_DisplayIntros
Draws intro movies to the screen
*********/
void SP_DisplayLogos(void)
{
	if( !Sys_QuickStart() )
	{
		CIN_PlayAllFrames( "eflogo", 0, 0, 640, 480, 0, true );
		CIN_PlayAllFrames( "intro", 0, 0, 640, 480, 0, true );
	}
}

/*********
SP_DrawTexture
*********/
void SP_DrawTexture(void* pixels, float width, float height, float vShift)
{
#ifdef _XBOX
	static int s_drawTextureCount = 0;
	bool logDetailed = (s_drawTextureCount < 3) || ((s_drawTextureCount & 63) == 0);
	{ char b[160]; _snprintf(b, sizeof(b), "SDT: entry #%d context=%s\n", s_drawTextureCount, s_spDrawTextureContext); b[sizeof(b)-1]=0; XBLog_Write(b); }
	s_drawTextureCount++;
#endif
	if (!pixels)
	{
		// Ug.  We were not even able to load the error message texture.
#ifdef _XBOX
		XBLog_Write("SDT: pixels NULL, return\n");
#endif
		return;
	}

	// Create a texture from the buffered file
	GLuint texid;
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glGenTextures...\n");
#endif
	glGenTextures(1, &texid);
#ifdef _XBOX
	if (logDetailed) { char b[64]; _snprintf(b, sizeof(b), "SDT: glGenTextures -> texid=%u\n", (unsigned)texid); b[sizeof(b)-1]=0; XBLog_Write(b); }
	if (logDetailed) XBLog_Write("SDT: glBindTexture...\n");
#endif
	glBindTexture(GL_TEXTURE_2D, texid);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glTexImage2D (DDS1)...\n");
#endif
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DDS1_EXT, width, height, 0, GL_DDS1_EXT, GL_UNSIGNED_BYTE, pixels);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glTexParameterf x4...\n");
#endif

	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );

	// Reset every GL state we've got.  Who knows what state
	// the renderer could be in when this function gets called.
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glColor3f...\n");
#endif
	glColor3f(1.f, 1.f, 1.f);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glViewport...\n");
	if(glw_state->isWidescreen)
		glViewport(0, 0, 720, 480);
	else
#endif
	glViewport(0, 0, 640, 480);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glViewport done\n");
#endif

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glIsEnabled x10...\n");
#endif
	GLboolean alpha = glIsEnabled(GL_ALPHA_TEST);
	glDisable(GL_ALPHA_TEST);

	GLboolean blend = glIsEnabled(GL_BLEND);
	glDisable(GL_BLEND);

	GLboolean cull = glIsEnabled(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);

	GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);

	GLboolean fog = glIsEnabled(GL_FOG);
	glDisable(GL_FOG);

	GLboolean lighting = glIsEnabled(GL_LIGHTING);
	glDisable(GL_LIGHTING);

	GLboolean offset = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_POLYGON_OFFSET_FILL);

	GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
	glDisable(GL_SCISSOR_TEST);

	GLboolean stencil = glIsEnabled(GL_STENCIL_TEST);
	glDisable(GL_STENCIL_TEST);

	GLboolean texture = glIsEnabled(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_2D);

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: matrix setup (MV+PROJ+ortho)...\n");
#endif
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
#ifdef _XBOX
	if(glw_state->isWidescreen)
        glOrtho(0, 720, 0, 480, 0, 1);
	else
#endif
	glOrtho(0, 640, 0, 480, 0, 1);

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glMatrixMode(GL_TEXTURE0) [non-std arg]...\n");
#endif
	glMatrixMode(GL_TEXTURE0);
	glLoadIdentity();
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glMatrixMode(GL_TEXTURE1) [non-std arg]...\n");
#endif
	glMatrixMode(GL_TEXTURE1);
	glLoadIdentity();

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glActiveTextureARB(GL_TEXTURE0_ARB)...\n");
#endif
	glActiveTextureARB(GL_TEXTURE0_ARB);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glClientActiveTextureARB(GL_TEXTURE0_ARB)...\n");
#endif
	glClientActiveTextureARB(GL_TEXTURE0_ARB);

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: memset(&tess)...\n");
#endif
	memset(&tess, 0, sizeof(tess));

	// Draw the error message
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glBeginFrame...\n");
#endif
	glBeginFrame();
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glBeginFrame done\n");
#endif

	if (!SP_LicenseDone)
	{
		// clear the screen if we haven't done the
		// license yet...
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	float x1, x2, y1, y2;
#ifdef _XBOX
	if(glw_state->isWidescreen)
	{
		x1 = 0;
		x2 = 720;
		y1 = 0;
		y2 = 480;
	}
	else {
#endif
	x1 = 0;
	x2 = 640;
	y1 = 0;
	y2 = 480;
#ifdef _XBOX
	}
#endif

	y1 += vShift;
	y2 += vShift;

#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glBeginEXT(GL_TRIANGLE_STRIP, 4)...\n");
#endif
	glBeginEXT (GL_TRIANGLE_STRIP, 4, 0, 0, 4, 0);
#ifdef _XBOX
		glTexCoord2f( 0,  1 );
		glVertex2f(x1, y1);
		glTexCoord2f( 1,  1 );
		glVertex2f(x2, y1);
		glTexCoord2f( 0, 0 );
		glVertex2f(x1, y2);
		glTexCoord2f( 1, 0 );
		glVertex2f(x2, y2);
#else
		glTexCoord2f( 0,  0 );
		glVertex2f(x1, y1);
		glTexCoord2f( 1 ,  0 );
		glVertex2f(x2, y1);
		glTexCoord2f( 0, 1 );
		glVertex2f(x1, y2);
		glTexCoord2f( 1, 1 );
		glVertex2f(x2, y2);
#endif
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glEnd...\n");
#endif
	glEnd();
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glEndFrame...\n");
#endif
	glEndFrame();
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glFlush...\n");
#endif
	glFlush();
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: restore states (Enable/Disable x10)...\n");
#endif

	// Restore (most) of the render states we reset
	if (alpha) glEnable(GL_ALPHA_TEST);
	else glDisable(GL_ALPHA_TEST);

	if (blend) glEnable(GL_BLEND);
	else glDisable(GL_BLEND);

	if (cull) glEnable(GL_CULL_FACE);
	else glDisable(GL_CULL_FACE);

	if (depth) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (fog) glEnable(GL_FOG);
	else glDisable(GL_FOG);

	if (lighting) glEnable(GL_LIGHTING);
	else glDisable(GL_LIGHTING);

	if (offset) glEnable(GL_POLYGON_OFFSET_FILL);
	else glDisable(GL_POLYGON_OFFSET_FILL);

	if (scissor) glEnable(GL_SCISSOR_TEST);
	else glDisable(GL_SCISSOR_TEST);

	if (stencil) glEnable(GL_STENCIL_TEST);
	else glDisable(GL_STENCIL_TEST);

	if (texture) glEnable(GL_TEXTURE_2D);
	else glDisable(GL_TEXTURE_2D);

	// Kill the texture
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: glDeleteTextures...\n");
#endif
	glDeleteTextures(1, &texid);
#ifdef _XBOX
	if (logDetailed) XBLog_Write("SDT: exit\n");
#endif
}


/*********
SP_GetLanguageExt

Retuns the extension for the current language, or
english if the language is unknown.
*********/
char* SP_GetLanguageExt()
{
	switch(XGetLanguage())
	{
	case XC_LANGUAGE_ENGLISH:
		return "EN";
//	case XC_LANGUAGE_JAPANESE:
//		return "JA";
	case XC_LANGUAGE_GERMAN:
		return "GE";
//	case XC_LANGUAGE_SPANISH:
//		return "SP";
//	case XC_LANGUAGE_ITALIAN:
//		return "IT";
//	case XC_LANGUAGE_KOREAN:
//		return "KO";
//	case XC_LANGUAGE_TCHINESE:
//		return "CH";
//	case XC_LANGUAGE_PORTUGUESE:
//		return "PO";
	case XC_LANGUAGE_FRENCH:
		return "FR";
	default:
		return "EN";
	}
}

/*********
SP_LoadFileWithLanguage

Loads a screen with the appropriate language
*********/
void *SP_LoadFileWithLanguage(const char *name)
{
	char fullname[MAX_QPATH];
	void *buffer = NULL;
	char *ext;

	// get the language extension
	ext = SP_GetLanguageExt();

	// creat the fullpath name and try to load the texture
	sprintf(fullname, "%s_%s.dds", name, ext);
	buffer = SP_LoadFile(fullname);

	if (!buffer)
	{
		sprintf(fullname, "%s.dds", name);
		buffer = SP_LoadFile(fullname);
	}

	return buffer;
}

/*********
SP_LoadFile
*********/
void* SP_LoadFile(const char* name)
{
	wfhandle_t h = WF_Open(name, true, false);
	if (h < 0) return NULL;

	if (WF_Seek(0, SEEK_END, h))
	{
		WF_Close(h);
		return NULL;
	}

	int len = WF_Tell(h);
	
	if (WF_Seek(0, SEEK_SET, h))
	{
		WF_Close(h);
		return NULL;
	}

	void *buf = Z_Malloc(len, TAG_TEMP_WORKSPACE, false, 32);

	if (WF_Read(buf, len, h) != len)
	{
		Z_Free(buf);
		WF_Close(h);
		return NULL;
	}

	WF_Close(h);

	return buf;
}

/********
SP_DoLicense

Draws the license splash to the screen
*********/
void SP_DoLicense(void)
{
#ifdef _XBOX
	XBLog_Write("SPL: SP_DoLicense entry\n");
#endif
	if( Sys_QuickStart() )
	{
#ifdef _XBOX
		XBLog_Write("SPL: Sys_QuickStart returned true \xe2\x80\x94 early return\n");
#endif
		return;
	}
#ifdef _XBOX
	XBLog_Write("SPL: Sys_QuickStart false\n");
#endif

#ifdef _XBOX
	XBLog_Write("SPL: EF movies own intro; standalone legacy license screen removed\n");
#endif
	SP_LicenseDone = true;
	return;
}

/*
SP_DrawMPLoadScreen

Draws the Multiplayer loading screen
*/
void SP_DrawMPLoadScreen( void )
{
#ifdef _XBOX
	XBLog_Write("SPL: SP_DrawMPLoadScreen entry\n");
	SP_SetDrawTextureContext("LoadMP");
#endif
#if defined(STEFX_SP_HOSTED_MP)
	if (SP_DrawEFMPLoadingScreen())
	{
		return;
	}
#ifdef _XBOX
	XBLog_Write("SPL: EF MP load screen unavailable; drawing clean fallback");
#endif
	SP_MPRegisterLoadingAssets();
	if (s_spEfMPLoadingAssets.white)
	{
		SP_MPDrawRect(0, 0, 640, 480, CT_BLACK);
		re.SetColor(NULL);
	}
#else
	// Load the texture:
	void *image = SP_LoadFileWithLanguage("d:\\base\\media\\LoadMP");

	if( image )
	{
		SP_DrawTexture(image, 512, 512, 0);
		Z_Free(image);
	}
#endif
}

/*
SP_DrawSPLoadScreen

Draws the single player loading screen - used when skipping the logo movies
*/
void SP_DrawSPLoadScreen( void )
{
#ifdef _XBOX
	XBLog_Write("SPL: SP_DrawSPLoadScreen entry\n");
	SP_SetDrawTextureContext("LoadSP");
#endif
	if (SP_DrawEFLoadingScreen())
	{
		return;
	}
#ifdef _XBOX
	XBLog_Write("SPL: EF LCARS load screen unavailable; JA LoadSP fallback removed");
#endif
	return;
}

/*
ERR_DiscFail

Draws the damaged/dirty disc message, looping forever
*/
void ERR_DiscFail(bool poll)
{
#ifdef _XBOX
	{ char b[80]; _snprintf(b, sizeof(b), "ERR_DiscFail entry poll=%d\n", poll ? 1 : 0); b[sizeof(b)-1]=0; XBLog_Write(b); }
	SP_SetDrawTextureContext("DiscErr");
#endif
	// Load the texture:
	extern const char *Sys_RemapPath( const char *filename );
	void *image = SP_LoadFileWithLanguage( Sys_RemapPath("base\\media\\DiscErr") );

	if( image )
	{
		SP_DrawTexture(image, 512, 512, 0);
		Z_Free(image);
	}

	for (;;)
	{
		extern void MuteBinkSystem(void);
		MuteBinkSystem();

		extern void S_Update_(void);
		S_Update_();
	}
}
