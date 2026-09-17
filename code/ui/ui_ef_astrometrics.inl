#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" { volatile unsigned int g_SPXBAstrometricsProof[8] = {0}; }
#endif
// Native Xbox qmenu adapter for the authored Virtual Voyager database.
// Keep entry data bounded and register previews only when selected.
enum { EFQ_ASTRO_MAX = 32, EFQ_ASTRO_ROWS = 8, EFQ_ASTRO_LINES = 64 };
struct efqAstroEntry_t {
    char title[64], text[1536], model[MAX_QPATH], sound[MAX_QPATH], command[128];
    int group;
};
static efqAstroEntry_t s_astroEntries[EFQ_ASTRO_MAX];
static char s_astroGroups[4][64];
static char s_astroLines[EFQ_ASTRO_LINES][128];
static int s_astroCount, s_astroGroupCount, s_astroGroup, s_astroTop, s_astroSelected;
static int s_astroLineCount, s_astroTextPage;
static qhandle_t s_astroModel;
static menuframework_s s_astroMenu;
static menuaction_s s_astroButtons[20];
static int s_astroVisible[EFQ_ASTRO_ROWS];

static void EFQ_AstroBuildButtons(void);
static void EFQ_AstroWrapText(const char *text)
{
    s_astroLineCount = 0;
    s_astroTextPage = 0;
    while (*text && s_astroLineCount < EFQ_ASTRO_LINES) {
        while (*text == ' ' || *text == '\r' || *text == '\n') ++text;
        if (!*text) break;
        char *line = s_astroLines[s_astroLineCount];
        int len = 0, lastSpace = -1;
        while (text[len] && len < 126) {
            line[len] = text[len]; line[len + 1] = 0;
            if (text[len] == ' ') lastSpace = len;
            if (EFQ_TextWidth(line, UI_TINYFONT) > 292) break;
            ++len;
        }
        if (text[len] && lastSpace > 0) len = lastSpace;
        if (!len) len = 1;
        line[len] = 0; text += len; ++s_astroLineCount;
    }
}
static void EFQ_AstroSelect(int index)
{
    if (index < 0 || index >= s_astroCount) return;
    s_astroSelected = index;
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
    g_SPXBAstrometricsProof[1] |= 1u << index;
    g_SPXBAstrometricsProof[7] = index;
#endif
    EFQ_AstroWrapText(s_astroEntries[index].text);
    s_astroModel = s_astroEntries[index].model[0] ? ui.R_RegisterModel(s_astroEntries[index].model) : 0;
    #if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
    if (s_astroEntries[index].model[0] && !s_astroModel) g_SPXBAstrometricsProof[2] |= 1u << index;
#endif
    if (s_astroEntries[index].sound[0])
        ui.S_StartLocalSound(ui.S_RegisterSound(s_astroEntries[index].sound), CHAN_LOCAL_SOUND);
#ifdef _XBOX
    XBLF("STEFX_ASTROMETRICS: selected=%d title='%s' model=%d lines=%d command='%s'",
        index, s_astroEntries[index].title, s_astroModel, s_astroLineCount, s_astroEntries[index].command);
#endif
}
static void EFQ_AstroClose(qboolean project)
{
    char command[144] = {0};
    if (project && s_astroSelected >= 0)
        Com_sprintf(command, sizeof(command), "%s\n", s_astroEntries[s_astroSelected].command);
    UI_ForceMenuOff();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
    ++g_SPXBAstrometricsProof[project ? 3 : 4];
    g_SPXBAstrometricsProof[5] = (unsigned int)ui.Cvar_VariableValue("cl_paused");
    g_SPXBAstrometricsProof[6] = ui.Key_GetCatcher();
#endif
    // Stop this menu's narration through the existing menu sound channel.
    ui.S_StartLocalSound(ui.S_RegisterSound("sound/null.wav"), CHAN_LOCAL_SOUND);
    if (command[0]) ui.Cmd_ExecuteText(EXEC_APPEND, command);
#ifdef _XBOX
    XBLF("STEFX_ASTROMETRICS: closed project=%d paused=%g catcher=%d command='%s'",
        project, ui.Cvar_VariableValue("cl_paused"), ui.Key_GetCatcher(), command);
#endif
}
static int EFQ_AstroGroupSize(void)
{
    int count = 0;
    for (int i = 0; i < s_astroCount; ++i) if (s_astroEntries[i].group == s_astroGroup) ++count;
    return count;
}
static void EFQ_AstroEvent(void *ptr, int notification)
{
    if (notification != QM_ACTIVATED) return;
    int id = ((menucommon_s *)ptr)->id;
    if (id >= 300 && id < 300 + s_astroGroupCount) {
        s_astroGroup = id - 300; s_astroTop = 0;
        for (int i = 0; i < s_astroCount; ++i) if (s_astroEntries[i].group == s_astroGroup) { EFQ_AstroSelect(i); break; }
    } else if (id >= 310 && id < 310 + EFQ_ASTRO_ROWS) {
        EFQ_AstroSelect(s_astroVisible[id - 310]);
    } else if (id == 320) {
        if (s_astroTop >= EFQ_ASTRO_ROWS) s_astroTop -= EFQ_ASTRO_ROWS;
    } else if (id == 321) {
        if (s_astroTop + EFQ_ASTRO_ROWS < EFQ_AstroGroupSize()) s_astroTop += EFQ_ASTRO_ROWS;
    } else if (id == 322) {
        s_astroTextPage = (s_astroTextPage + 1) % ((s_astroLineCount + 7) / 8 > 0 ? (s_astroLineCount + 7) / 8 : 1);
    } else if (id == 323) { EFQ_AstroClose(qtrue); return;
    } else if (id == 324) { EFQ_AstroClose(qfalse); return; }
    EFQ_AstroBuildButtons();
}
static sfxHandle_t EFQ_AstroKey(int key)
{
    if (key == A_ESCAPE || key == A_MOUSE2 || key == A_JOY13 || key == A_JOY14) { EFQ_AstroClose(qfalse); return 0; }
    if (key == A_JOY15) key = A_ENTER;
    if (key == A_JOY5) key = A_CURSOR_UP;
    if (key == A_JOY7) key = A_CURSOR_DOWN;
    return Menu_DefaultKey(&s_astroMenu, key);
}
static void EFQ_AstroBuildButtons(void)
{
    int oldCursor = s_astroMenu.cursor, used = 0, row = 0, count = 0;
    s_astroMenu.nitems = 0;
    memset(s_astroButtons, 0, sizeof(s_astroButtons));
    for (int group = 0; group < s_astroGroupCount; ++group) {
        EFQ_InitAction(&s_astroButtons[used], 300 + group, 30 + group * 145, 68, 138, 24, s_astroGroups[group], EFQ_AstroEvent);
        Menu_AddItem(&s_astroMenu, &s_astroButtons[used++]);
    }
    for (int i = 0; i < s_astroCount && row < EFQ_ASTRO_ROWS; ++i) {
        if (s_astroEntries[i].group != s_astroGroup) continue;
        if (count++ < s_astroTop) continue;
        s_astroVisible[row] = i;
        EFQ_InitAction(&s_astroButtons[used], 310 + row, 30, 116 + row * 27, 232, 23, s_astroEntries[i].title, EFQ_AstroEvent);
        if (i == s_astroSelected) s_astroButtons[used].color = CT_LTPURPLE1;
        Menu_AddItem(&s_astroMenu, &s_astroButtons[used++]); ++row;
    }
    const char *labels[5] = {"PREVIOUS", "NEXT", "MORE TEXT", "VIEW HOLOGRAM", "BACK"};
    const int xs[5] = {30,150,300,300,30};
    const int ys[5] = {348,348,394,428,428};
    const int ws[5] = {112,112,300,300,232};
    for (int action = 0; action < 5; ++action) {
        EFQ_InitAction(&s_astroButtons[used], 320 + action, xs[action], ys[action], ws[action], 24, labels[action], EFQ_AstroEvent);
        if ((action == 0 && s_astroTop == 0) ||
            (action == 1 && s_astroTop + EFQ_ASTRO_ROWS >= EFQ_AstroGroupSize()) ||
            (action == 2 && s_astroLineCount <= 8) || (action == 3 && s_astroSelected < 0))
            s_astroButtons[used].generic.flags |= QMF_GRAYED;
        Menu_AddItem(&s_astroMenu, &s_astroButtons[used++]);
    }
    // Keep focus on the activated control where possible after page changes.
    s_astroMenu.cursor = oldCursor < s_astroMenu.nitems ? oldCursor : 0;
    if (!EFQ_ItemSelectable((menucommon_s *)Menu_ItemAtCursor(&s_astroMenu))) Menu_AdjustCursor(&s_astroMenu, 1);
}
static void EFQ_AstroDrawModel(void)
{
    if (!s_astroModel) return;
    refdef_t rd; refEntity_t ent;
    vec3_t mins, maxs, angles = {0,0,0};
    memset(&rd, 0, sizeof(rd)); memset(&ent, 0, sizeof(ent));
    ui.R_ModelBounds(s_astroModel, mins, maxs);
    float radius = 1.0f;
    for (int i = 0; i < 3; ++i) if ((maxs[i] - mins[i]) * 0.5f > radius) radius = (maxs[i] - mins[i]) * 0.5f;
    angles[YAW] = (uis.realtime % 18000) * 0.02f;
    AnglesToAxis(angles, ent.axis);
    ent.hModel = s_astroModel; ent.renderfx = RF_NOSHADOW;
    // Bounding sphere fits at every rotation; recenter even off-origin props.
    for (int axis = 0; axis < 3; ++axis)
        for (int component = 0; component < 3; ++component)
            ent.origin[component] -= 0.5f * (mins[axis] + maxs[axis]) * ent.axis[axis][component];
    ent.origin[0] += radius * 6.5f;
    VectorCopy(ent.origin, ent.oldorigin);
    AxisClear(rd.viewaxis); rd.rdflags = RDF_NOWORLDMODEL;
    float x = 300, y = 130, w = 300, h = 140;
    x *= uis.glconfig.vidWidth / 640.0f; w *= uis.glconfig.vidWidth / 640.0f;
    y *= uis.glconfig.vidHeight / 480.0f; h *= uis.glconfig.vidHeight / 480.0f;
    rd.x = x; rd.y = y; rd.width = w; rd.height = h;
    rd.fov_y = 30; rd.fov_x = atan(tan(30.0f * M_PI / 360.0f) * w / h) * 360.0f / M_PI;
    rd.time = uis.realtime;
    ui.R_ClearScene(); ui.R_AddRefEntityToScene(&ent);
    vec3_t light = {0,0,80}; ui.R_AddLightToScene(light, 500, 1, 1, 1);
    ui.R_RenderScene(&rd);
}
static void EFQ_AstroDraw(void)
{
    UI_FillRect(0, 0, 640, 480, colorTable[CT_BLACK]);
    UI_FillRect(30, 52, 570, 8, colorTable[CT_LTPURPLE1]);
    EFQ_DrawText(600, 24, "ASTROMETRICS", UI_BIGFONT | UI_RIGHT, CT_LTGOLD1);
    if (s_astroSelected >= 0) {
        EFQ_DrawText(300, 106, s_astroEntries[s_astroSelected].title, UI_SMALLFONT, CT_LTGOLD1);
        EFQ_AstroDrawModel();
        for (int line = 0; line < 8 && s_astroTextPage * 8 + line < s_astroLineCount; ++line)
            EFQ_DrawText(300, 278 + line * 14, s_astroLines[s_astroTextPage * 8 + line], UI_TINYFONT, CT_WHITE);
    }
    Menu_Draw(&s_astroMenu);
    EFQ_DrawText(30, 462, "A : SELECT    Y : BACK", UI_TINYFONT, CT_LTGOLD1);
}
static qboolean EFQ_OpenAstrometrics(void)
{
    void *file = NULL;
    int bytes = ui.FS_ReadFile("ext_data/sp_astrometrics.dat", &file);
    s_astroCount = s_astroGroupCount = 0; s_astroSelected = -1;
    memset(s_astroEntries, 0, sizeof(s_astroEntries));
    memset(s_astroGroups, 0, sizeof(s_astroGroups));
    if (file && bytes > 0 && bytes < 65536) {
        const char *cursor = (const char *)file;
        while (cursor) {
            char key[64]; Q_strncpyz(key, COM_ParseExt(&cursor, qtrue), sizeof(key));
            if (!key[0]) break;
            const char *value = COM_ParseExt(&cursor, qtrue);
            if (!Q_stricmpn(key, "MAINTOPIC", 9)) {
                if (s_astroGroupCount == 4) break;
                Q_strncpyz(s_astroGroups[s_astroGroupCount++], value, sizeof(s_astroGroups[0]));
            } else if (!Q_stricmpn(key, "SUBTOPIC", 8)) {
                if (!s_astroGroupCount || s_astroCount == EFQ_ASTRO_MAX) break;
                efqAstroEntry_t *entry = &s_astroEntries[s_astroCount++];
                entry->group = s_astroGroupCount - 1;
                Q_strncpyz(entry->title, value, sizeof(entry->title));
            } else if (s_astroCount) {
                efqAstroEntry_t *entry = &s_astroEntries[s_astroCount - 1];
                if (!Q_stricmp(key, "TEXT")) Q_strncpyz(entry->text, value, sizeof(entry->text));
                else if (!Q_stricmp(key, "MODEL")) Q_strncpyz(entry->model, value, sizeof(entry->model));
                else if (!Q_stricmp(key, "SOUND")) Q_strncpyz(entry->sound, value, sizeof(entry->sound));
                else if (!Q_stricmp(key, "COMMAND")) Q_strncpyz(entry->command, value, sizeof(entry->command));
            }
        }
    }
    if (file) ui.FS_FreeFile(file);
    if (!s_astroCount) {
        UI_ForceMenuOff();
        ui.Printf("Astrometrics data unavailable; returning to gameplay.\n");
        return qtrue; // Never fall into a paused, nonexistent inherited menu.
    }
    #if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
    ++g_SPXBAstrometricsProof[0];
#endif
    UI_EFMainMenu_Deactivate(); UI_EFPauseMenu_Deactivate(); UI_EFQmenu_ClearState("astrometrics");
    memset(&s_astroMenu, 0, sizeof(s_astroMenu));
    s_astroMenu.wrapAround = qtrue; s_astroMenu.fullscreen = qtrue;
    s_astroMenu.draw = EFQ_AstroDraw; s_astroMenu.key = EFQ_AstroKey;
    s_astroGroup = s_astroTop = 0; EFQ_AstroSelect(0); EFQ_AstroBuildButtons();
    ui.Cvar_Set("cl_paused", "1"); UI_PushMenu(&s_astroMenu);
#ifdef _XBOX
    XBLF("STEFX_ASTROMETRICS: opened entries=%d groups=%d bytes=%d", s_astroCount, s_astroGroupCount, bytes);
#endif
    return qtrue;
}
