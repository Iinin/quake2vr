/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#define MAXMENUITEMS	64

#define MTYPE_SLIDER		0
#define MTYPE_LIST			1
#define MTYPE_ACTION		2
#define MTYPE_SPINCONTROL	3
#define MTYPE_SEPARATOR  	4
#define MTYPE_FIELD			5

#define	K_TAB			9
#define	K_ENTER			13
#define	K_ESCAPE		27
#define	K_SPACE			32

// normal keys should be passed as lowercased ascii

#define	K_BACKSPACE		127
#define	K_UPARROW		128
#define	K_DOWNARROW		129
#define	K_LEFTARROW		130
#define	K_RIGHTARROW	131

#define QMF_LEFT_JUSTIFY	0x00000001
#define QMF_GRAYED			0x00000002
#define QMF_NUMBERSONLY		0x00000004
#define QMF_SKINLIST		0x00000008

//
#define RCOLUMN_OFFSET  MENU_FONT_SIZE*2	// was 16
#define LCOLUMN_OFFSET -MENU_FONT_SIZE*2	// was -16

#define SLIDER_RANGE 10
//


typedef struct _tag_menuframework
{
	Sint32 x, y;
	Sint32	cursor;

	Sint32	nitems;
	Sint32 nslots;
	void *items[64];

	const char *statusbar;

	void (*cursordraw)( struct _tag_menuframework *m );
	
} menuframework_s;

typedef struct
{
	Sint32 type;
	const char *name;
	Sint32 x, y;
	menuframework_s *parent;
	Sint32 cursor_offset;
	Sint32	localdata[4];
	unsigned flags;

	const char *statusbar;

	void (*callback)( void *self );
	void (*statusbarfunc)( void *self );
	void (*ownerdraw)( void *self );
	void (*cursordraw)( void *self );
} menucommon_s;

typedef struct
{
	menucommon_s generic;

	char		buffer[80];
	Sint32			cursor;
	Sint32			length;
	Sint32			visible_length;
	Sint32			visible_offset;
} menufield_s;

typedef struct 
{
	menucommon_s generic;

	float minvalue;
	float maxvalue;
	float curvalue;

	float range;
} menuslider_s;

typedef struct
{
	menucommon_s	generic;

	Sint32			curvalue;

	const char	**itemnames;
	Sint32			numitemnames;
} menulist_s;

typedef struct
{
	menucommon_s generic;
} menuaction_s;

typedef struct
{
	menucommon_s generic;
} menuseparator_s;

typedef enum {
	LISTBOX_TEXT,
	LISTBOX_IMAGE,
	LISTBOX_TEXTIMAGE
} listboxtype_t;

typedef enum {
	SCROLL_X,
	SCROLL_Y
} scrolltype_t;

typedef struct
{
	menucommon_s	generic;

	listboxtype_t	type;
	scrolltype_t	scrolltype;
	Sint32			items_x;
	Sint32			items_y;
	Sint32			item_width;
	Sint32			item_height;
	Sint32			scrollpos;
	Sint32			curvalue;

	const char	**itemnames;
	Sint32			numitemnames;
} menulistbox_s;

typedef struct
{
	float	min[2];
	float max[2];
	Sint32 index;
} buttonmenuobject_t;

typedef struct
{
	Sint32	min[2];
	Sint32 max[2];
	void (*OpenMenu)(void);
} mainmenuobject_t;

qboolean Field_Key( menufield_s *field, Sint32 key );

void	Menu_AddItem (menuframework_s *menu, void *item);
void	Menu_AdjustCursor (menuframework_s *menu, Sint32 dir);
void	Menu_Center (menuframework_s *menu);
void	Menu_Draw (menuframework_s *menu);
void	*Menu_ItemAtCursor (menuframework_s *m);
qboolean Menu_SelectItem (menuframework_s *s);
qboolean Menu_MouseSelectItem (menucommon_s *item);
void	Menu_SetStatusBar (menuframework_s *s, const char *string);
void	Menu_SlideItem (menuframework_s *s, Sint32 dir);
Sint32		Menu_TallySlots (menuframework_s *menu);

void	Menu_DrawString (Sint32 x, Sint32 y, const char *string, Sint32 alpha);
void	Menu_DrawStringDark (Sint32 x, Sint32 y, const char *string, Sint32 alpha);
void	Menu_DrawStringR2L (Sint32 x, Sint32 y, const char *string, Sint32 alpha);
void	Menu_DrawStringR2LDark (Sint32 x, Sint32 y, const char *string, Sint32 alpha);

void	Menu_DrawTextBox (Sint32 x, Sint32 y, Sint32 width, Sint32 lines);
void	Menu_DrawBanner (char *name);

void	UI_Draw_Cursor (void);


//=======================================================

// menu_main.c

#define NUM_MAINMENU_CURSOR_FRAMES 15

#define MOUSEBUTTON1 0
#define MOUSEBUTTON2 1

#define LOADSCREEN_NAME		"/gfx/ui/unknownmap.pcx"
#define UI_BACKGROUND_NAME	"/gfx/ui/menu_background.pcx"
#define UI_NOSCREEN_NAME	"/gfx/ui/noscreen.pcx"

#define UI_MOUSECURSOR_MAIN_PIC		"/gfx/ui/cursors/m_cur_main.pcx"
#define UI_MOUSECURSOR_HOVER_PIC	"/gfx/ui/cursors/m_cur_hover.pcx"
#define UI_MOUSECURSOR_CLICK_PIC	"/gfx/ui/cursors/m_cur_click.pcx"
#define UI_MOUSECURSOR_OVER_PIC		"/gfx/ui/cursors/m_cur_over.pcx"
#define UI_MOUSECURSOR_TEXT_PIC		"/gfx/ui/cursors/m_cur_text.pcx"

#define UI_MOUSECURSOR_PIC			"/gfx/ui/cursors/m_mouse_cursor.pcx"

extern	cvar_t	*ui_cursor_scale;

static char *menu_in_sound		= "misc/menu1.wav";
static char *menu_move_sound	= "misc/menu2.wav";
static char *menu_out_sound		= "misc/menu3.wav";

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound

Sint32 MainMenuMouseHover;


cursor_t cursor;

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
		void M_Menu_PlayerConfig_f (void);
		void M_Menu_DownloadOptions_f (void);
	void M_Menu_Video_f (void);
		void M_Menu_Video_Advanced_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Options_Sound_f (void);
		void M_Menu_Options_Controls_f (void);
			void M_Menu_Keys_f (void);
		void M_Menu_Options_Screen_f (void);
		void M_Menu_Options_Effects_f (void);
		void M_Menu_Options_Interface_f (void);
		void M_Menu_Options_VR_f (void);
			void M_Menu_Options_VR_Advanced_f (void);
			void M_Menu_Options_VR_OVR_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

static char *creditsBuffer;

void ActionStartMod (char *mod);

// ui_subsystem.c
void UI_AddButton (buttonmenuobject_t *thisObj, Sint32 index, float x, float y, float w, float h);
void UI_AddMainButton (mainmenuobject_t *thisObj, Sint32 index, Sint32 x, Sint32 y, char *name);
void UI_RefreshCursorMenu (void);
void UI_RefreshCursorLink (void);
void UI_RefreshCursorButtons (void);
void UI_PushMenu ( void (*draw) (void), const char *(*key) (Sint32 k) );
void UI_ForceMenuOff (void);
void UI_PopMenu (void);
void UI_BackMenu (void *unused);
const char *Default_MenuKey( menuframework_s *m, Sint32 key );

// ui_main.c
void M_Main_Draw (void);
void UI_CheckMainMenuMouse (void);

// ui_game_saveload.c
void UI_InitSavegameData (void);

// ui_credits.c
void M_Credits_MenuDraw (void);

// ui_options_screen.c
void Options_Screen_MenuDraw (void);
void MenuCrosshair_MouseClick (void);

// ui_mp_startserver.c
void UI_LoadArenas (void);
void UI_LoadMapList (void);

// ui_mp_playersetup.c
void PlayerConfig_MenuDraw (void);
void PlayerConfig_MouseClick (void);
void PConfigAccept (void);

#endif
