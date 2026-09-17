// leave this at the top of all UI_xxxx files for PCH reasons.
#include "../server/exe_headers.h"

#include "ui_local.h"
#include "menudef.h"
#ifdef _XBOX
#include <xtl.h>
#include "../win32/xb_log.h"
#endif



enum
{
	EFQ_ID_STUB_RETURN = 100,
	EFQ_ID_CONFIRM_NO,
	EFQ_ID_CONFIRM_YES
};

static menuframework_s s_stubMenu;
static menuaction_s s_stubButtons[1];
static char s_stubTitle[64];
static char s_stubLine1[96];
static char s_stubLine2[96];
static qboolean s_stubReturnMain = qtrue;
static menuframework_s s_confirmMenu;
static menuaction_s s_confirmButtons[2];
static qboolean s_confirmQuitProgram = qfalse;
static qboolean s_confirmReturnToMain = qfalse;
// EF ships bitmap atlases/fonts.dat, not the inherited JA ergoec fontdat.
// Reuse the pause menu's font cache so labels survive renderer restarts.
static int EFQ_TextWidth(const char *text, int style)
{
	return UI_EFPropTextWidth(text, style);
}

static void EFQ_DrawText(float x, float y, const char *text, int style, int colorIndex)
{
	UI_EFDrawPropText((int)x, (int)y, text, style, colorIndex);
}
static void EFQ_InitAction(menuaction_s *action, int id, int x, int y, int w, int h, const char *label, void (*callback)(void *, int))
{
	memset(action, 0, sizeof(*action));
	action->generic.type = MTYPE_ACTION;
	action->generic.flags = QMF_HIGHLIGHT_IF_FOCUS;
	action->generic.x = x;
	action->generic.y = y;
	action->generic.name = label;
	action->generic.id = id;
	action->generic.callback = callback;
	action->width = w;
	action->height = h;
	action->color = CT_DKPURPLE1;
	action->color2 = CT_LTPURPLE1;
	action->textX = 5;
	action->textY = 1;
	action->textcolor = CT_BLACK;
	action->textcolor2 = CT_WHITE;
}

static const char *EFQ_ItemText(menucommon_s *item)
{
	if (!item)
	{
		return NULL;
	}

	if (item->type == MTYPE_BITMAP)
	{
		menubitmap_s *bitmap = (menubitmap_s *)item;
		if (bitmap->textPtr && bitmap->textPtr[0])
		{
			return bitmap->textPtr;
		}
	}
	else if (item->type == MTYPE_TEXT)
	{
		menutext_s *text = (menutext_s *)item;
		if (text->string && text->string[0])
		{
			return text->string;
		}
	}

	return item->name;
}

static qboolean EFQ_ItemSelectable(menucommon_s *item)
{
	if (!item)
	{
		return qfalse;
	}

	if (item->type == MTYPE_SEPARATOR)
	{
		return qfalse;
	}

	if (item->flags & (QMF_GRAYED | QMF_HIDDEN | QMF_MOUSEONLY | QMF_INACTIVE))
	{
		return qfalse;
	}

	return qtrue;
}

static void EFQ_SetBounds(menucommon_s *item, int width, int height)
{
	if (!item)
	{
		return;
	}

	item->left = item->x;
	item->top = item->y;
	item->right = item->x + width;
	item->bottom = item->y + height;
}

qboolean UI_EFQmenu_IsActive(void)
{
	return uis.activemenu != NULL;
}

void UI_EFQmenu_ClearState(const char *reason)
{
#ifdef _XBOX
	XBLF("STEFX: EF qmenu clear reason='%s' depth=%d active=%p catcher=0x%x",
		reason ? reason : "",
		uis.menusp,
		(void*)uis.activemenu,
		ui.Key_GetCatcher());
#endif
	uis.menusp = 0;
	uis.activemenu = NULL;
	memset(uis.stack, 0, sizeof(uis.stack));
	uis.firstdraw = qtrue;
}

void UI_PushMenu(menuframework_s *menu)
{
	int i;

	if (!menu)
	{
		return;
	}

	for (i = 0; i < uis.menusp; i++)
	{
		if (uis.stack[i] == menu)
		{
			uis.menusp = i;
			break;
		}
	}

	if (i == uis.menusp)
	{
		if (uis.menusp >= MAX_MENUDEPTH)
		{
			ui.Error(ERR_FATAL, "UI_PushMenu: EF qmenu stack overflow");
		}
		uis.stack[uis.menusp++] = menu;
	}

	uis.activemenu = menu;
	menu->cursor = 0;
	menu->cursor_prev = 0;
	ui.Key_SetCatcher(KEYCATCH_UI);

	for (i = 0; i < menu->nitems; i++)
	{
		if (EFQ_ItemSelectable((menucommon_s *)menu->items[i]))
		{
			menu->cursor_prev = -1;
			Menu_SetCursor(menu, i);
			break;
		}
	}

	uis.firstdraw = qtrue;
#ifdef _XBOX
	XBLF("STEFX: EF qmenu push menu=%p title='%s' nitems=%d depth=%d cursor=%d",
		(void*)menu,
		menu->title ? menu->title : "",
		menu->nitems,
		uis.menusp,
		menu->cursor);
#endif
}

void UI_PopMenu(void)
{
	if (uis.menusp <= 0)
	{
		uis.activemenu = NULL;
		UI_ForceMenuOff();
		return;
	}

	uis.menusp--;
	if (uis.menusp > 0)
	{
		uis.activemenu = uis.stack[uis.menusp - 1];
		uis.firstdraw = qtrue;
	}
	else
	{
		uis.activemenu = NULL;
		UI_ForceMenuOff();
	}

#ifdef _XBOX
	XBLF("STEFX: EF qmenu pop depth=%d active=%p catcher=0x%x",
		uis.menusp,
		(void*)uis.activemenu,
		ui.Key_GetCatcher());
#endif
}

void Menu_AddItem(menuframework_s *menu, void *item)
{
	menucommon_s *common;

	if (!menu || !item)
	{
		return;
	}

	if (menu->nitems >= MAX_QMENUITEMS)
	{
		ui.Error(ERR_FATAL, "Menu_AddItem: excessive EF qmenu items");
	}

	common = (menucommon_s *)item;
	common->parent = menu;
	common->menuPosition = menu->nitems;
	common->flags &= ~QMF_HASMOUSEFOCUS;

	if (common->type == MTYPE_BITMAP)
	{
		menubitmap_s *bitmap = (menubitmap_s *)item;
		if (!bitmap->shader && common->name && common->name[0])
		{
			bitmap->shader = ui.R_RegisterShaderNoMip(common->name);
		}
		EFQ_SetBounds(common, bitmap->width, bitmap->height);
	}
	else if (common->type == MTYPE_ACTION)
	{
		menuaction_s *action = (menuaction_s *)item;
		EFQ_SetBounds(common, action->width, action->height);
	}
	else if (common->type == MTYPE_TEXT)
	{
		menutext_s *text = (menutext_s *)item;
		int width = text->focusWidth ? text->focusWidth : EFQ_TextWidth(EFQ_ItemText(common), UI_SMALLFONT);
		int height = text->focusHeight ? text->focusHeight : 18;
		EFQ_SetBounds(common, width, height);
	}
	else if (common->type == MTYPE_SCROLLLIST)
	{
		menulist_s *list = (menulist_s *)item;
		int columns;
		int seperation;
		int width;

		if (list->columns <= 0)
		{
			list->columns = 1;
		}
		if (list->seperation < 0)
		{
			list->seperation = 0;
		}
		list->oldvalue = list->curvalue;
		if (list->curvalue < 0 || list->curvalue >= list->numitems)
		{
			list->curvalue = 0;
		}
		list->top = 0;
		columns = list->columns;
		seperation = list->seperation;
		width = ((list->width + seperation) * columns - seperation) * SMALLCHAR_WIDTH;
		EFQ_SetBounds(common, width, list->height * SMALLCHAR_HEIGHT);
	}

	menu->items[menu->nitems++] = item;
}

void Menu_CursorMoved(menuframework_s *m)
{
	void (*callback)(void *self, int notification);

	if (!m || m->cursor_prev == m->cursor)
	{
		return;
	}

	if (m->cursor_prev >= 0 && m->cursor_prev < m->nitems)
	{
		callback = ((menucommon_s *)m->items[m->cursor_prev])->callback;
		if (callback)
		{
			callback(m->items[m->cursor_prev], QM_LOSTFOCUS);
		}
	}

	if (m->cursor >= 0 && m->cursor < m->nitems)
	{
		callback = ((menucommon_s *)m->items[m->cursor])->callback;
		if (callback)
		{
			callback(m->items[m->cursor], QM_GOTFOCUS);
		}
	}
}

void Menu_SetCursor(menuframework_s *m, int cursor)
{
	if (!m)
	{
		return;
	}

	m->cursor_prev = m->cursor;
	m->cursor = cursor;
	Menu_CursorMoved(m);
}

void Menu_AdjustCursor(menuframework_s *m, int dir)
{
	qboolean wrapped = qfalse;

	if (!m || !m->nitems)
	{
		return;
	}

wrap:
	while (m->cursor >= 0 && m->cursor < m->nitems)
	{
		if (EFQ_ItemSelectable((menucommon_s *)m->items[m->cursor]))
		{
			break;
		}
		m->cursor += dir;
	}

	if (dir > 0 && m->cursor >= m->nitems)
	{
		if (m->wrapAround && !wrapped)
		{
			m->cursor = 0;
			wrapped = qtrue;
			goto wrap;
		}
		m->cursor = m->cursor_prev;
	}
	else if (dir < 0 && m->cursor < 0)
	{
		if (m->wrapAround && !wrapped)
		{
			m->cursor = m->nitems - 1;
			wrapped = qtrue;
			goto wrap;
		}
		m->cursor = m->cursor_prev;
	}
}

void *Menu_ItemAtCursor(menuframework_s *m)
{
	if (!m || m->cursor < 0 || m->cursor >= m->nitems)
	{
		return NULL;
	}

	return m->items[m->cursor];
}

sfxHandle_t Menu_ActivateItem(menuframework_s *s, menucommon_s *item)
{
	(void)s;
	if (item && item->callback)
	{
		item->callback(item, QM_ACTIVATED);
	}
	return 0;
}

void Menu_SetStatusBar(menuframework_s *m, const char *string)
{
	if (m)
	{
		m->statusbar = string;
	}
}

void Menu_Center(menuframework_s *menu)
{
	(void)menu;
}

void Menu_SlideItem(menuframework_s *s, int dir)
{
	(void)s;
	(void)dir;
}

void Menu_Focus(menucommon_s *m)
{
	(void)m;
}

static qboolean EFQ_ScrollListKey(menulist_s *list, int key)
{
	if (!list || list->numitems <= 0)
	{
		return qfalse;
	}

	switch (key)
	{
	case A_CURSOR_UP:
		if (list->curvalue <= 0)
		{
			return qfalse;
		}
		list->oldvalue = list->curvalue;
		list->curvalue--;
		if (list->curvalue < list->top)
		{
			list->top = list->curvalue;
		}
		if (list->generic.callback)
		{
			list->generic.callback(list, QM_GOTFOCUS);
		}
		return qtrue;

	case A_CURSOR_DOWN:
		if (list->curvalue >= list->numitems - 1)
		{
			return qfalse;
		}
		list->oldvalue = list->curvalue;
		list->curvalue++;
		if (list->curvalue >= list->top + list->height)
		{
			list->top = list->curvalue - list->height + 1;
		}
		if (list->generic.callback)
		{
			list->generic.callback(list, QM_GOTFOCUS);
		}
		return qtrue;

	case A_CURSOR_LEFT:
		if (list->curvalue <= 0)
		{
			return qfalse;
		}
		list->oldvalue = list->curvalue;
		list->curvalue -= list->height - 1;
		if (list->curvalue < 0)
		{
			list->curvalue = 0;
		}
		list->top = list->curvalue;
		if (list->generic.callback)
		{
			list->generic.callback(list, QM_GOTFOCUS);
		}
		return qtrue;

	case A_CURSOR_RIGHT:
		if (list->curvalue >= list->numitems - 1)
		{
			return qfalse;
		}
		list->oldvalue = list->curvalue;
		list->curvalue += list->height - 1;
		if (list->curvalue > list->numitems - 1)
		{
			list->curvalue = list->numitems - 1;
		}
		list->top = list->curvalue - list->height + 1;
		if (list->top < 0)
		{
			list->top = 0;
		}
		if (list->generic.callback)
		{
			list->generic.callback(list, QM_GOTFOCUS);
		}
		return qtrue;
	}

	return qfalse;
}

sfxHandle_t Menu_DefaultKey(menuframework_s *m, int key)
{
	menucommon_s *item;
	int cursorPrev;

	if (!m)
	{
		return 0;
	}

	item = (menucommon_s *)Menu_ItemAtCursor(m);
	if (item && item->type == MTYPE_SCROLLLIST && EFQ_ScrollListKey((menulist_s *)item, key))
	{
		return 0;
	}

	switch (key)
	{
	case A_ESCAPE:
	case A_MOUSE2:
		UI_PopMenu();
		return 0;

	case A_CURSOR_UP:
		cursorPrev = m->cursor;
		m->cursor_prev = m->cursor;
		m->cursor--;
		Menu_AdjustCursor(m, -1);
		if (cursorPrev != m->cursor)
		{
			Menu_CursorMoved(m);
		}
		return 0;

	case A_TAB:
	case A_CURSOR_DOWN:
		cursorPrev = m->cursor;
		m->cursor_prev = m->cursor;
		m->cursor++;
		Menu_AdjustCursor(m, 1);
		if (cursorPrev != m->cursor)
		{
			Menu_CursorMoved(m);
		}
		return 0;

	case A_ENTER:
	case A_KP_ENTER:
	case A_MOUSE1:
		if (EFQ_ItemSelectable(item))
		{
			return Menu_ActivateItem(m, item);
		}
		return 0;
	}

	return 0;
}

static void EFQ_DrawBitmap(menubitmap_s *bitmap, qboolean focused)
{
	int color;
	qhandle_t shader;
	const char *text;

	if (!bitmap)
	{
		return;
	}

	color = focused ? bitmap->color2 : bitmap->color;
	if (color <= CT_NONE || color >= CT_MAX)
	{
		color = CT_WHITE;
	}

	shader = bitmap->shader;
	if (!shader && bitmap->generic.name && bitmap->generic.name[0])
	{
		shader = ui.R_RegisterShaderNoMip(bitmap->generic.name);
		bitmap->shader = shader;
	}

	if (shader)
	{
		ui.R_SetColor(colorTable[color]);
		UI_DrawHandlePic((float)bitmap->generic.x, (float)bitmap->generic.y, (float)bitmap->width, (float)bitmap->height, shader);
		ui.R_SetColor(NULL);
	}

	text = EFQ_ItemText(&bitmap->generic);
	if (text && text[0])
	{
		int textColor = focused ? bitmap->textcolor2 : bitmap->textcolor;
		if (textColor <= CT_NONE || textColor >= CT_MAX)
		{
			textColor = focused ? CT_WHITE : CT_BLACK;
		}
		EFQ_DrawText((float)(bitmap->generic.x + bitmap->textX), (float)(bitmap->generic.y + bitmap->textY),
			text, bitmap->textStyle ? bitmap->textStyle : UI_SMALLFONT, textColor);
	}
}

static void EFQ_DrawTextItem(menutext_s *text, qboolean focused)
{
	int color;
	const char *label;

	if (!text)
	{
		return;
	}

	color = focused ? text->color2 : text->color;
	if (color <= CT_NONE || color >= CT_MAX)
	{
		color = focused ? CT_WHITE : CT_LTGOLD1;
	}

	label = EFQ_ItemText(&text->generic);
	EFQ_DrawText((float)text->generic.x, (float)text->generic.y, label, text->style, color);
}

static void EFQ_DrawAction(menuaction_s *action, qboolean focused)
{
	int color;
	int textColor;
	int style;
	const char *label;

	if (!action)
	{
		return;
	}

	color = focused ? action->color2 : action->color;
	textColor = focused ? action->textcolor2 : action->textcolor;
	if (color <= CT_NONE || color >= CT_MAX)
	{
		color = focused ? CT_LTPURPLE1 : CT_DKPURPLE1;
	}
	if (textColor <= CT_NONE || textColor >= CT_MAX)
	{
		textColor = focused ? CT_WHITE : CT_BLACK;
	}

	ui.R_SetColor(colorTable[color]);
	UI_DrawHandlePic((float)action->generic.x, (float)action->generic.y, (float)action->width, (float)action->height, uis.whiteShader);
	ui.R_SetColor(NULL);

	label = EFQ_ItemText(&action->generic);
	style = UI_SMALLFONT;
	EFQ_DrawText((float)(action->generic.x + action->textX), (float)(action->generic.y + action->textY),
		label, style, textColor);
}

static void EFQ_DrawScrollList(menulist_s *list, qboolean focused)
{
	int row;
	int maxRows;
	int textColor;
	int highlightColor;

	if (!list || !list->itemnames || list->numitems <= 0)
	{
		return;
	}

	if (list->top < 0)
	{
		list->top = 0;
	}
	if (list->top > list->numitems - 1)
	{
		list->top = list->numitems - 1;
	}

	maxRows = list->height;
	if (maxRows <= 0)
	{
		maxRows = 1;
	}

	for (row = 0; row < maxRows; row++)
	{
		int itemIndex;
		int y;
		const char *label;

		itemIndex = list->top + row;
		if (itemIndex >= list->numitems)
		{
			break;
		}

		y = list->generic.y + row * SMALLCHAR_HEIGHT;
		label = list->itemnames[itemIndex];
		if (!label)
		{
			label = "";
		}

		if (itemIndex == list->curvalue)
		{
			highlightColor = focused ? CT_LTPURPLE1 : CT_DKPURPLE1;
			textColor = focused ? CT_WHITE : CT_BLACK;
			ui.R_SetColor(colorTable[highlightColor]);
			UI_DrawHandlePic((float)(list->generic.x - 2), (float)y, (float)(list->width * SMALLCHAR_WIDTH), (float)(SMALLCHAR_HEIGHT + 2), uis.whiteShader);
			ui.R_SetColor(NULL);
		}
		else
		{
			textColor = CT_DKGOLD1;
		}

		EFQ_DrawText((float)list->generic.x, (float)(y + 1), label, UI_SMALLFONT, textColor);
	}
}

void Menu_Draw(menuframework_s *menu)
{
	int i;

	if (!menu)
	{
		return;
	}

	if (menu->title && menu->title[0])
	{
		EFQ_DrawText(320.0f, 24.0f, menu->title, UI_BIGFONT | UI_CENTER, CT_LTGOLD1);
	}

	for (i = 0; i < menu->nitems; i++)
	{
		menucommon_s *item = (menucommon_s *)menu->items[i];
		qboolean focused;

		if (!item || (item->flags & QMF_HIDDEN))
		{
			continue;
		}

		if (item->ownerdraw)
		{
			item->ownerdraw(item);
			continue;
		}

		focused = (i == menu->cursor);
		switch (item->type)
		{
		case MTYPE_BITMAP:
			EFQ_DrawBitmap((menubitmap_s *)item, focused);
			break;
		case MTYPE_ACTION:
			EFQ_DrawAction((menuaction_s *)item, focused);
			break;
		case MTYPE_TEXT:
			EFQ_DrawTextItem((menutext_s *)item, focused);
			break;
		case MTYPE_SCROLLLIST:
			EFQ_DrawScrollList((menulist_s *)item, focused);
			break;
		default:
			break;
		}
	}
}

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
static void EFQ_AstroRecordDraw(void);
#endif

void UI_EFQmenu_Draw(int realtime)
{
	if (!uis.activemenu)
	{
		return;
	}

	uis.frametime = realtime - uis.realtime;
	uis.realtime = realtime;

	if (uis.activemenu->opening)
	{
		uis.activemenu->opening();
	}

	if (uis.activemenu && uis.activemenu->draw)
	{
		uis.activemenu->draw();
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
        EFQ_AstroRecordDraw();
#endif
	}
	else if (uis.activemenu)
	{
		Menu_Draw(uis.activemenu);
	}
}

void UI_EFQmenu_KeyEvent(int key, qboolean down)
{
	sfxHandle_t sound;

	if (!down || !uis.activemenu)
	{
		return;
	}

	if (uis.activemenu->key)
	{
		sound = uis.activemenu->key(key);
	}
	else
	{
		sound = Menu_DefaultKey(uis.activemenu, key);
	}

	if (sound > 0)
	{
		ui.S_StartLocalSound(sound, CHAN_LOCAL_SOUND);
	}

#ifdef _XBOX
	XBLF("STEFX: EF qmenu key key=%d active=%p depth=%d cursor=%d",
		key,
		(void*)uis.activemenu,
		uis.menusp,
		uis.activemenu ? uis.activemenu->cursor : -1);
#endif
}

static void EFQ_ReturnFromStubMenu(void)
{
#ifdef _XBOX
	XBLF("STEFX: EF qmenu stub return title='%s' main=%d", s_stubTitle, s_stubReturnMain ? 1 : 0);
#endif
	UI_ForceMenuOff();
	if (s_stubReturnMain)
	{
		UI_EFMainMenu_Open();
	}
	else
	{
		UI_EFPauseMenu_Open(NULL);
	}
}

static void EFQ_StubEvent(void *ptr, int notification)
{
	(void)ptr;
	if (notification != QM_ACTIVATED)
	{
		return;
	}
	EFQ_ReturnFromStubMenu();
}

static sfxHandle_t EFQ_StubKey(int key)
{
	switch (key)
	{
	case A_ESCAPE:
	case A_MOUSE2:
		EFQ_ReturnFromStubMenu();
		return 0;
	}

	return Menu_DefaultKey(&s_stubMenu, key);
}

static void EFQ_StubDraw(void)
{
	UI_FillRect(0, 0, 640, 480, colorTable[CT_BLACK]);
	EFQ_DrawText(611, 24, s_stubTitle, UI_BIGFONT | UI_RIGHT, CT_LTGOLD1);
	EFQ_DrawText(320, 198, s_stubLine1, UI_SMALLFONT | UI_CENTER, CT_LTORANGE);
	EFQ_DrawText(320, 232, s_stubLine2, UI_TINYFONT | UI_CENTER, CT_DKGOLD1);
	Menu_Draw(&s_stubMenu);
}

static void EFQ_OpenComingSoonMenu(const char *title, const char *line1, const char *line2)
{
#ifdef _XBOX
	XBLF("STEFX: EF qmenu opening friendly stub title='%s'", title ? title : "");
#endif
	s_stubReturnMain = UI_EFMainMenu_IsActive();
	UI_EFMainMenu_Deactivate();
	UI_EFPauseMenu_Deactivate();
	memset(&s_stubMenu, 0, sizeof(s_stubMenu));
	memset(s_stubButtons, 0, sizeof(s_stubButtons));
	Q_strncpyz(s_stubTitle, title ? title : "ELITE FORCE", sizeof(s_stubTitle));
	Q_strncpyz(s_stubLine1, line1 ? line1 : "COMING SOON", sizeof(s_stubLine1));
	Q_strncpyz(s_stubLine2, line2 ? line2 : "", sizeof(s_stubLine2));

	s_stubMenu.wrapAround = qtrue;
	s_stubMenu.fullscreen = qtrue;
	s_stubMenu.draw = EFQ_StubDraw;
	s_stubMenu.key = EFQ_StubKey;

	EFQ_InitAction(&s_stubButtons[0], EFQ_ID_STUB_RETURN, 255, 318, 130, 18, s_stubReturnMain ? "MAIN MENU" : "INGAME MENU", EFQ_StubEvent);
	Menu_AddItem(&s_stubMenu, &s_stubButtons[0]);
	UI_PushMenu(&s_stubMenu);
}

static void EFQ_KeepMainMenuDumb(const char *cmd, const char *title)
{
#ifdef _XBOX
	XBLF("STEFX: EF qmenu consumed dumb main-menu command cmd='%s' title='%s'", cmd ? cmd : "", title ? title : "");
#else
	(void)cmd;
	(void)title;
#endif
}

static void EFQ_ReturnToPauseMenu(void)
{
#ifdef _XBOX
	XBLF("STEFX: EF confirmation return quitProgram=%d main=%d",
		s_confirmQuitProgram ? 1 : 0, s_confirmReturnToMain ? 1 : 0);
#endif
	if (s_confirmReturnToMain)
	{
		UI_EFMainMenu_Open();
	}
	else
	{
		UI_EFPauseMenu_Open(NULL);
	}
}

static void EFQ_ConfirmEvent(void *ptr, int notification)
{
	menucommon_s *item = (menucommon_s *)ptr;

	if (notification != QM_ACTIVATED || !item)
	{
		return;
	}

	if (item->id == EFQ_ID_CONFIRM_NO)
	{
		EFQ_ReturnToPauseMenu();
		return;
	}

	if (item->id != EFQ_ID_CONFIRM_YES)
	{
		return;
	}

#ifdef _XBOX
	XBLF("STEFX: EF confirmation accepted quitProgram=%d", s_confirmQuitProgram ? 1 : 0);
#endif
	UI_ForceMenuOff();
	ui.Cmd_ExecuteText(EXEC_APPEND, s_confirmQuitProgram ? "quit\n" : "disconnect\n");
}

static sfxHandle_t EFQ_ConfirmKey(int key)
{
	if (key == A_ESCAPE || key == A_MOUSE2 || key == A_JOY13 || key == A_JOY14 || key == A_BACKSPACE)
	{
		EFQ_ReturnToPauseMenu();
		return 0;
	}
	if (key == A_JOY15)
	{
		return Menu_ActivateItem(&s_confirmMenu, (menucommon_s *)Menu_ItemAtCursor(&s_confirmMenu));
	}
	if (key == A_JOY5)
	{
		return Menu_DefaultKey(&s_confirmMenu, A_CURSOR_UP);
	}
	if (key == A_JOY7)
	{
		return Menu_DefaultKey(&s_confirmMenu, A_CURSOR_DOWN);
	}
	return Menu_DefaultKey(&s_confirmMenu, key);
}

static void EFQ_ConfirmDraw(void)
{
	UI_FillRect(0, 0, 640, 480, colorTable[CT_BLACK]);
	EFQ_DrawText(611, 24, s_confirmQuitProgram ? "EXIT PROGRAM" : "QUIT GAME", UI_BIGFONT | UI_RIGHT, CT_LTGOLD1);
	EFQ_DrawText(320, 198,
		s_confirmQuitProgram ? "EXIT ELITE FORCE?" : "QUIT THE CURRENT GAME?",
		UI_SMALLFONT | UI_CENTER, CT_LTORANGE);
	EFQ_DrawText(320, 232, "THIS ACTION CANNOT BE UNDONE", UI_TINYFONT | UI_CENTER, CT_DKGOLD1);
	Menu_Draw(&s_confirmMenu);
}

static void EFQ_OpenConfirmation(qboolean quitProgram)
{
	s_confirmQuitProgram = quitProgram;
	s_confirmReturnToMain = UI_EFMainMenu_IsActive();
	UI_EFMainMenu_Deactivate();
	UI_EFPauseMenu_Deactivate();
	UI_EFQmenu_ClearState(quitProgram ? "ef-quit-confirm" : "ef-leave-confirm");
	memset(&s_confirmMenu, 0, sizeof(s_confirmMenu));
	memset(s_confirmButtons, 0, sizeof(s_confirmButtons));

	s_confirmMenu.wrapAround = qtrue;
	s_confirmMenu.fullscreen = qtrue;
	s_confirmMenu.draw = EFQ_ConfirmDraw;
	s_confirmMenu.key = EFQ_ConfirmKey;

	EFQ_InitAction(&s_confirmButtons[0], EFQ_ID_CONFIRM_NO, 220, 302, 200, 24, "NO - RETURN", EFQ_ConfirmEvent);
	EFQ_InitAction(&s_confirmButtons[1], EFQ_ID_CONFIRM_YES, 220, 340, 200, 24,
		quitProgram ? "YES - EXIT" : "YES - QUIT GAME", EFQ_ConfirmEvent);
	Menu_AddItem(&s_confirmMenu, &s_confirmButtons[0]);
	Menu_AddItem(&s_confirmMenu, &s_confirmButtons[1]);
	UI_PushMenu(&s_confirmMenu);
#ifdef _XBOX
	XBLF("STEFX: EF confirmation opened quitProgram=%d", quitProgram ? 1 : 0);
#endif
}

// These menus execute the original data-file commands. In particular, deck
// changes use authored targets, preserving ICARUS travel and hub transitions.
static menuframework_s s_tourTravelMenu;
static menuaction_s s_tourTravelButtons[18];
static char s_tourTravelLabels[16][128];
static char s_tourTravelCommands[16][256];
static char s_tourTravelSounds[16][MAX_QPATH];
static int s_tourTravelCount;
static int s_tourTravelSelected;
static qboolean s_tourTravelHolodeck;
static qboolean s_tourTravelTransporter;

static void EFQ_TourTravelEvent(void *ptr, int notification)
{
	if (notification != QM_ACTIVATED) return;
	int id = ((menucommon_s *)ptr)->id;
	if (id == 201)
	{
		UI_ForceMenuOff();
		return;
	}
	if (id >= 210 && id < 210 + s_tourTravelCount)
	{
		s_tourTravelSelected = id - 210;
		for (int i = 0; i < s_tourTravelCount; ++i)
		{
			s_tourTravelButtons[i].color = i == s_tourTravelSelected ? CT_LTPURPLE1 : CT_DKPURPLE1;
			s_tourTravelButtons[i].textcolor = i == s_tourTravelSelected ? CT_WHITE : CT_BLACK;
		}
		s_tourTravelButtons[16].generic.flags &= ~QMF_GRAYED;
		const char *sound = s_tourTravelSounds[s_tourTravelSelected];
		if (sound[0]) ui.S_StartLocalSound(ui.S_RegisterSound(sound), CHAN_LOCAL_SOUND);
		return;
	}
	if (id == 200 && s_tourTravelSelected >= 0)
	{
		char command[272];
		Com_sprintf(command, sizeof(command), "%s\n", s_tourTravelCommands[s_tourTravelSelected]);
#ifdef _XBOX
		XBLF("STEFX_VIRTUAL_VOYAGER: travel holodeck=%d selection=%d command='%s'",
			s_tourTravelHolodeck, s_tourTravelSelected, command);
#endif
		UI_ForceMenuOff();
		ui.Cmd_ExecuteText(EXEC_APPEND, command);
	}
}

static sfxHandle_t EFQ_TourTravelKey(int key)
{
	if (key == A_ESCAPE || key == A_MOUSE2 || key == A_JOY13 || key == A_JOY14)
	{
		UI_ForceMenuOff();
		return 0;
	}
	if (key == A_JOY15) key = A_ENTER;
	if (key == A_JOY5) key = A_CURSOR_UP;
	if (key == A_JOY7) key = A_CURSOR_DOWN;
	return Menu_DefaultKey(&s_tourTravelMenu, key);
}

static void EFQ_TourTravelDraw(void)
{
	UI_FillRect(0, 0, 640, 480, colorTable[CT_BLACK]);
	UI_FillRect(32, 52, 576, 8, colorTable[CT_LTPURPLE1]);
	EFQ_DrawText(608, 24, s_tourTravelTransporter ? "TRANSPORTER" : s_tourTravelHolodeck ? "HOLODECK" : "TURBOLIFT",
		UI_BIGFONT | UI_RIGHT, CT_LTGOLD1);
	EFQ_DrawText(32, 438, "A : SELECT    Y : BACK", UI_SMALLFONT, CT_LTGOLD1);
	Menu_Draw(&s_tourTravelMenu);
}

static qboolean EFQ_OpenTourTravel(int kind)
{
	qboolean holodeck = (qboolean)(kind == 1);
	qboolean transporter = (qboolean)(kind == 2);
	void *file = NULL;
	const char *dataPath = transporter ? "ext_data/sp_transporter.dat" : holodeck ? "ext_data/sp_holodeck.dat" : "ext_data/sp_turbolift.dat";
	if (ui.FS_ReadFile(dataPath, &file) <= 0 || !file) return qfalse;
	const char *cursor = (const char *)file;
	char mapName[MAX_QPATH];
	ui.Cvar_VariableStringBuffer("mapname", mapName, sizeof(mapName));
	qboolean inHolodeck = (qboolean)!Q_stricmpn(mapName, "_holodeck_", 10);
	s_tourTravelCount = 0;
	s_tourTravelSelected = -1;
	s_tourTravelHolodeck = holodeck;
	s_tourTravelTransporter = transporter;
	memset(s_tourTravelSounds, 0, sizeof(s_tourTravelSounds));
	while (cursor && s_tourTravelCount < 16)
	{
		const char *token = COM_ParseExt(&cursor, qtrue);
		if (!token[0]) break;
		qboolean isReturn = (qboolean)!Q_stricmp(token, "RETURNBUTTON");
		if (Q_stricmpn(token, holodeck ? "MAP" : transporter ? "SITE" : "DECK", holodeck ? 3 : 4) && !isReturn) continue;
		char fields[4][256] = {0};
		int field;
		for (field = 0; field < (transporter ? 2 : 4); ++field)
			Q_strncpyz(fields[field], COM_ParseExt(&cursor, qfalse), sizeof(fields[field]));
		if (isReturn && !inHolodeck) continue;
		const char *command = fields[transporter ? 1 : holodeck && inHolodeck ? 3 : 2];
		if (!command[0]) continue;
		int index = s_tourTravelCount++;
		Com_sprintf(s_tourTravelLabels[index], sizeof(s_tourTravelLabels[index]), "%s%s%s",
			fields[0], !transporter && fields[1][0] ? " " : "", transporter ? "" : fields[1]);
		Q_strncpyz(s_tourTravelCommands[index], command, sizeof(s_tourTravelCommands[index]));
		if (!holodeck && !transporter) Q_strncpyz(s_tourTravelSounds[index], fields[3], sizeof(s_tourTravelSounds[index]));
	}
	ui.FS_FreeFile(file);
	if (!s_tourTravelCount) return qfalse;
	UI_EFMainMenu_Deactivate();
	UI_EFPauseMenu_Deactivate();
	UI_EFQmenu_ClearState("virtual-voyager-travel");
	memset(&s_tourTravelMenu, 0, sizeof(s_tourTravelMenu));
	s_tourTravelMenu.wrapAround = qtrue;
	s_tourTravelMenu.fullscreen = qtrue;
	s_tourTravelMenu.draw = EFQ_TourTravelDraw;
	s_tourTravelMenu.key = EFQ_TourTravelKey;
	int index;
	for (index = 0; index < s_tourTravelCount; ++index)
	{
		EFQ_InitAction(&s_tourTravelButtons[index], 210 + index, 100, 78 + index * 29, 480, 24,
			s_tourTravelLabels[index], EFQ_TourTravelEvent);
		Menu_AddItem(&s_tourTravelMenu, &s_tourTravelButtons[index]);
	}
	EFQ_InitAction(&s_tourTravelButtons[16], 200, 360, 386, 220, 26, "ENGAGE", EFQ_TourTravelEvent);
	s_tourTravelButtons[16].generic.flags |= QMF_GRAYED;
	EFQ_InitAction(&s_tourTravelButtons[17], 201, 100, 386, 220, 26, "BACK", EFQ_TourTravelEvent);
	Menu_AddItem(&s_tourTravelMenu, &s_tourTravelButtons[16]);
	Menu_AddItem(&s_tourTravelMenu, &s_tourTravelButtons[17]);
	ui.Cvar_Set("cl_paused", "1");
	UI_PushMenu(&s_tourTravelMenu);
#ifdef _XBOX
	XBLF("STEFX_VIRTUAL_VOYAGER: travel menu opened holodeck=%d inside=%d options=%d map='%s'",
		holodeck, inHolodeck, s_tourTravelCount, mapName);
#endif
	return qtrue;
}

#include "ui_ef_astrometrics.inl"

#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
extern "C" { volatile unsigned int g_SPXBAstroHarnessProof[4] = {0}; }
static void EFQ_AstroTestSelect(int selection);
static void EFQ_AstroRecordDraw(void)
{
    static unsigned int sweepOpen = 0;
    static int sweepNext = 0, sweepIndex = 0;
    if (uis.activemenu != &s_astroMenu) return;
    if (s_astroSelected >= 0 && s_astroModel)
        g_SPXBAstroHarnessProof[0] |= 1u << s_astroSelected;
    if ((int)ui.Cvar_VariableValue("stefx_astro_sweep") != 1) return;
    if (sweepOpen != g_SPXBAstrometricsProof[0]) {
        sweepOpen = g_SPXBAstrometricsProof[0];
        sweepIndex = 0; sweepNext = uis.realtime + 3000;
    } else if (uis.realtime >= sweepNext && sweepIndex + 1 < s_astroCount) {
        EFQ_AstroTestSelect(++sweepIndex);
        sweepNext = uis.realtime + 3000;
    }
}
// Test input stays inside the game and takes the same D-pad/A route as a controller.
static qboolean EFQ_AstroTestActivate(int id)
{
    for (int step = 0; step <= s_astroMenu.nitems; ++step) {
        menucommon_s *item = (menucommon_s *)Menu_ItemAtCursor(&s_astroMenu);
        if (item && item->id == id && EFQ_ItemSelectable(item)) {
            EFQ_AstroKey(A_JOY15);
            ++g_SPXBAstroHarnessProof[1];
            return qtrue;
        }
        EFQ_AstroKey(A_JOY7);
    }
    ++g_SPXBAstroHarnessProof[2];
    return qfalse;
}
static void EFQ_AstroTestSelect(int selection)
{
    if (selection < 0 || selection >= s_astroCount) return;
    if (s_astroGroup != s_astroEntries[selection].group &&
        !EFQ_AstroTestActivate(300 + s_astroEntries[selection].group)) return;
    int ordinal = 0;
    for (int i = 0; i < selection; ++i)
        if (s_astroEntries[i].group == s_astroGroup) ++ordinal;
    int top = (ordinal / EFQ_ASTRO_ROWS) * EFQ_ASTRO_ROWS;
    while (s_astroTop > top) if (!EFQ_AstroTestActivate(320)) return;
    while (s_astroTop < top) if (!EFQ_AstroTestActivate(321)) return;
    EFQ_AstroTestActivate(310 + ordinal - top);
}
#endif


qboolean UI_EFQmenu_RouteMenuName(const char *menuName)
{
	if (!menuName || !menuName[0])
	{
		return qfalse;
	}
	if (ui.Cvar_VariableValue("cg_virtualVoyager") != 0)
	{
		if (!Q_stricmp(menuName, "astrometrics")) return EFQ_OpenAstrometrics();
		if (!Q_stricmp(menuName, "turbolift")) return EFQ_OpenTourTravel(0);
		if (!Q_stricmp(menuName, "transporter")) return EFQ_OpenTourTravel(2);
		if (!Q_stricmp(menuName, "holodeck") || !Q_stricmp(menuName, "endholomenu"))
			return EFQ_OpenTourTravel(qtrue);
	}

	if (!Q_stricmp(menuName, "main") || !Q_stricmp(menuName, "mainMenu") || !Q_stricmp(menuName, "splashMenu"))
	{
#ifdef _XBOX
		XBLF("STEFX: EF route parser menu '%s' -> main menu", menuName);
#endif
		UI_EFMainMenu_Open();
		return qtrue;
	}

	if (!Q_stricmp(menuName, "ingameMainMenu"))
	{
		extern void S_StopAllSoundsExceptMusic( void );
#ifdef _XBOX
		XBLF("STEFX: EF route parser menu '%s' -> pause menu", menuName);
#endif
		S_StopAllSoundsExceptMusic();
		UI_EFPauseMenu_Open(menuName);
		return qtrue;
	}

	if (!Q_stricmp(menuName, "newgame") || !Q_stricmp(menuName, "characterMenu") ||
		!Q_stricmp(menuName, "ingameMissionSelect1") || !Q_stricmp(menuName, "ingameMissionSelect2") ||
		!Q_stricmp(menuName, "ingameMissionSelect3"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_newgame");
	}
	if (!Q_stricmp(menuName, "loadgame") || !Q_stricmp(menuName, "loadMenu") ||
		!Q_stricmp(menuName, "loadgameMenu") || !Q_stricmp(menuName, "ingameloadMenu") ||
		!Q_stricmp(menuName, "missionfailed_menu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_loadgame");
	}
	if (!Q_stricmp(menuName, "savegame") || !Q_stricmp(menuName, "saveMenu") ||
		!Q_stricmp(menuName, "savegameMenu") || !Q_stricmp(menuName, "ingamesaveMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_savegame");
	}
	if (!Q_stricmp(menuName, "configure") || !Q_stricmp(menuName, "controlsMenu") ||
		!Q_stricmp(menuName, "setupMenu") || !Q_stricmp(menuName, "ingamecontrolsMenu") ||
		!Q_stricmp(menuName, "ingamesetupMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_configure");
	}
	if (!Q_stricmp(menuName, "holomatch") || !Q_stricmp(menuName, "holomatchMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_holomatch");
	}
	if (!Q_stricmp(menuName, "crew") || !Q_stricmp(menuName, "crewMenu") || !Q_stricmp(menuName, "voyagerCrew"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_crew");
	}
	if (!Q_stricmp(menuName, "credits") || !Q_stricmp(menuName, "creditsMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_credits");
	}
	if (!Q_stricmp(menuName, "tour") || !Q_stricmp(menuName, "tourMenu") || !Q_stricmp(menuName, "virtualVoyager"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_tour");
	}
	if (!Q_stricmp(menuName, "mods") || !Q_stricmp(menuName, "modsMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_mods");
	}
	if (!Q_stricmp(menuName, "leavegame") || !Q_stricmp(menuName, "ingamequitMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_leavegame");
	}
	if (!Q_stricmp(menuName, "quitgame") || !Q_stricmp(menuName, "quitMenu"))
	{
		return UI_EFQmenu_ConsoleCommand("ui_ef_quit");
	}

	return qfalse;
}

qboolean UI_EFQmenu_ConsoleCommand(const char *cmd)
{
	// Route EF-owned menu commands away from inherited JA parser screens.
	if (!cmd || !cmd[0])
	{
		return qfalse;
	}

#if defined(STEFX_HW_FRAME_DIAGNOSTICS) && !defined(STEFX_SP_HOSTED_MP)
	if (!Q_stricmp(cmd, "ui_ef_test_astro"))
    {
        if (uis.activemenu == &s_astroMenu) {
            int selection = (int)ui.Cvar_VariableValue("stefx_astro_selection");
            int action = (int)ui.Cvar_VariableValue("stefx_astro_action");
            if (action == 1) EFQ_AstroTestActivate(323);
            else if (action == 2) EFQ_AstroKey(A_JOY14);
            else if (action == 3) { if (EFQ_AstroTestActivate(322)) ++g_SPXBAstroHarnessProof[3]; }
            else if (selection >= 0 && selection < s_astroCount) {
                EFQ_AstroTestSelect(selection);
            }
        }
        return qtrue;
    }
    // Opt-in process-local fixtures use the same save-list APIs as the UI.
	if (!Q_stricmp(cmd, "ui_ef_test_vv_travel"))
	{
		// Exercise the same callbacks as the displayed menu, retaining authored use commands.
		int selection = (int)ui.Cvar_VariableValue("stefx_vv_travel_selection");
		if (uis.activemenu == &s_tourTravelMenu && selection >= 0 && selection < s_tourTravelCount)
		{
			EFQ_TourTravelEvent(&s_tourTravelButtons[selection], QM_ACTIVATED);
			EFQ_TourTravelEvent(&s_tourTravelButtons[16], QM_ACTIVATED);
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_test_vv_save"))
	{
		ui.Cvar_Set("stefx_vv_save_fixture", "1");
		qboolean queued = UI_EFSave_CreateNew();
		ui.Cvar_Set("stefx_vv_save_fixture", "0");
		XBLog_WriteRingMarkerf("STEFX_VV_SAVE_UI: create queued=%d", queued);
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_test_vv_load"))
	{
		char name[MAX_QPATH];
		ui.Cvar_VariableStringBuffer("stefx_vv_save_fixture_name", name, sizeof(name));
		int count = UI_EFSave_Count();
		qboolean queued = qfalse;
		for (int i = 0; name[0] && i < count; ++i)
		{
			if (!Q_stricmp(UI_EFSave_Name(i), name))
			{
				XBLog_WriteRingMarkerf("STEFX_VV_SAVE_UI: index=%d name='%s' map='%s' corrupt=%d",
					i, name, UI_EFSave_Map(i), UI_EFSave_IsCorrupt(i));
				ui.Cvar_Set("cg_virtualVoyager", "0");
				UI_ForceMenuOff();
				queued = UI_EFSave_Load(i);
				break;
			}
		}
		XBLog_WriteRingMarkerf("STEFX_VV_SAVE_UI: load queued=%d name='%s' count=%d", queued, name, count);
		return qtrue;
	}
#endif

	if (!Q_stricmp(cmd, "ui_ef_newgame"))
	{
		UI_EFMainMenu_OpenNewGame();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_loadgame"))
	{
		if (UI_EFPauseMenu_IsActive())
		{
			UI_EFMainMenu_OpenLoadGameFromPause();
		}
		else
		{
			UI_EFMainMenu_OpenLoadGame();
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_savegame"))
	{
		UI_EFMainMenu_OpenSaveGame();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_configure"))
	{
		if (UI_EFPauseMenu_IsActive())
		{
			UI_EFMainMenu_OpenConfigureFromPause();
		}
		else
		{
			UI_EFMainMenu_OpenConfigure();
		}
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_audio"))
	{
		UI_EFMainMenu_OpenAudio();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_video"))
	{
		UI_EFMainMenu_OpenVideo();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_controller"))
	{
		UI_EFMainMenu_OpenController();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_coop"))
	{
		UI_EFMainMenu_StartSplitScreenBaseline();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_holomatch"))
	{
		UI_EFMainMenu_StartHolomatchBaseline();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_crew"))
	{
		UI_EFMainMenu_OpenCrew();
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_credits"))
	{
		EFQ_KeepMainMenuDumb(cmd, "CREDITS");
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_tour"))
	{
		EFQ_KeepMainMenuDumb(cmd, "VIRTUAL VOYAGER");
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_mods"))
	{
		EFQ_KeepMainMenuDumb(cmd, "MODS");
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_quit"))
	{
		EFQ_OpenConfirmation(qtrue);
		return qtrue;
	}
	if (!Q_stricmp(cmd, "ui_ef_leavegame"))
	{
		EFQ_OpenConfirmation(qfalse);
		return qtrue;
	}

	return qfalse;
}
