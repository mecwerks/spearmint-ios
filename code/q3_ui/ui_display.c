/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
//
/*
=======================================================================

DISPLAY OPTIONS MENU

=======================================================================
*/

#include "ui_local.h"


#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"
#define ART_ACCEPT0			"menu/art/accept_0"
#define ART_ACCEPT1			"menu/art/accept_1"

#define ID_GRAPHICS			10
#define ID_DISPLAY			11
#define ID_SOUND			12
#define ID_NETWORK			13
#define ID_BRIGHTNESS		14
#define ID_SCREENSIZE		15
#define ID_BACK				16
#define ID_APPLY			17
#define ID_ANAGLYPH			18
#define ID_GREYSCALE		19


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutext_s		graphics;
	menutext_s		display;
	menutext_s		sound;
	menutext_s		network;

	menuslider_s	brightness;
	menuslider_s	screensize;
	menulist_s		anaglyph;
	menulist_s		greyscale;

	menubitmap_s	back;
	menubitmap_s	apply;

	int				greyscale_default;
} displayOptionsInfo_t;

static displayOptionsInfo_t	displayOptionsInfo;


/*
=================
UI_DisplayOptionsMenu_Event
=================
*/
static void UI_DisplayOptionsMenu_Event( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GRAPHICS:
		UI_PopMenu();
		UI_GraphicsOptionsMenu();
		break;

	case ID_DISPLAY:
		break;

	case ID_SOUND:
		UI_PopMenu();
		UI_SoundOptionsMenu();
		break;

	case ID_NETWORK:
		UI_PopMenu();
		UI_NetworkOptionsMenu();
		break;

	case ID_BRIGHTNESS:
		trap_Cvar_SetValue( "r_gamma", displayOptionsInfo.brightness.curvalue / 10.0f );
		break;
	
	case ID_SCREENSIZE:
		trap_Cvar_SetValue( "cg_viewsize", displayOptionsInfo.screensize.curvalue * 10 );
		break;

	case ID_ANAGLYPH:
		trap_Cvar_SetValue( "r_anaglyphMode", displayOptionsInfo.anaglyph.curvalue );
		break;

	case ID_GREYSCALE:
		break;

	case ID_APPLY:
		trap_Cvar_SetValue( "r_greyscale", displayOptionsInfo.greyscale.curvalue );

		UI_ForceMenuOff();
		trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
		break;

	case ID_BACK:
		UI_PopMenu();
		break;
	}
}

/*
=================
DisplayOptions_UpdateMenuItems
=================
*/
static void DisplayOptions_UpdateMenuItems( void )
{
	displayOptionsInfo.apply.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;

	if ( displayOptionsInfo.greyscale_default != displayOptionsInfo.greyscale.curvalue )
	{
		displayOptionsInfo.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
}

/*
================
DisplayOptions_MenuDraw
================
*/
void DisplayOptions_MenuDraw (void)
{
//APSFIX - rework this
	DisplayOptions_UpdateMenuItems();

	Menu_Draw( &displayOptionsInfo.menu );
}

/*
===============
UI_DisplayOptionsMenu_Init
===============
*/
static void UI_DisplayOptionsMenu_Init( void ) {
	int		y;

	static const char *anaglyph_names[] =
	{
		"Off",
		"red-cyan",
		"red-blue",
		"red-green",
		"green-magenta",
		"cyan-red",
		"blue-red",
		"green-red",
		"magenta-green",
		NULL
	};

	const int numAnaglyphModes = ARRAY_LEN(anaglyph_names) - 2;

	static const char *offOn_names[] =
	{
		"Off",
		"On",
		NULL
	};

	memset( &displayOptionsInfo, 0, sizeof(displayOptionsInfo) );

	UI_DisplayOptionsMenu_Cache();
	displayOptionsInfo.menu.wrapAround = qtrue;
	displayOptionsInfo.menu.fullscreen = qtrue;
	displayOptionsInfo.menu.draw		= DisplayOptions_MenuDraw;

	displayOptionsInfo.banner.generic.type		= MTYPE_BTEXT;
	displayOptionsInfo.banner.generic.flags		= QMF_CENTER_JUSTIFY;
	displayOptionsInfo.banner.generic.x			= 320;
	displayOptionsInfo.banner.generic.y			= 16;
	displayOptionsInfo.banner.string			= "SYSTEM SETUP";
	displayOptionsInfo.banner.color				= text_banner_color;
	displayOptionsInfo.banner.style				= UI_CENTER;

	displayOptionsInfo.framel.generic.type		= MTYPE_BITMAP;
	displayOptionsInfo.framel.generic.name		= ART_FRAMEL;
	displayOptionsInfo.framel.generic.flags		= QMF_INACTIVE;
	displayOptionsInfo.framel.generic.x			= 0;  
	displayOptionsInfo.framel.generic.y			= 78;
	displayOptionsInfo.framel.width				= 256;
	displayOptionsInfo.framel.height			= 329;

	displayOptionsInfo.framer.generic.type		= MTYPE_BITMAP;
	displayOptionsInfo.framer.generic.name		= ART_FRAMER;
	displayOptionsInfo.framer.generic.flags		= QMF_INACTIVE;
	displayOptionsInfo.framer.generic.x			= 376;
	displayOptionsInfo.framer.generic.y			= 76;
	displayOptionsInfo.framer.width				= 256;
	displayOptionsInfo.framer.height			= 334;

	displayOptionsInfo.graphics.generic.type		= MTYPE_PTEXT;
	displayOptionsInfo.graphics.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	displayOptionsInfo.graphics.generic.id			= ID_GRAPHICS;
	displayOptionsInfo.graphics.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.graphics.generic.x			= 216;
	displayOptionsInfo.graphics.generic.y			= 240 - 2 * PROP_HEIGHT;
	displayOptionsInfo.graphics.string				= "GRAPHICS";
	displayOptionsInfo.graphics.style				= UI_RIGHT;
	displayOptionsInfo.graphics.color				= text_big_color;

	displayOptionsInfo.display.generic.type			= MTYPE_PTEXT;
	displayOptionsInfo.display.generic.flags		= QMF_RIGHT_JUSTIFY;
	displayOptionsInfo.display.generic.id			= ID_DISPLAY;
	displayOptionsInfo.display.generic.callback		= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.display.generic.x			= 216;
	displayOptionsInfo.display.generic.y			= 240 - PROP_HEIGHT;
	displayOptionsInfo.display.string				= "DISPLAY";
	displayOptionsInfo.display.style				= UI_RIGHT;
	displayOptionsInfo.display.color				= text_big_color;

	displayOptionsInfo.sound.generic.type			= MTYPE_PTEXT;
	displayOptionsInfo.sound.generic.flags			= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	displayOptionsInfo.sound.generic.id				= ID_SOUND;
	displayOptionsInfo.sound.generic.callback		= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.sound.generic.x				= 216;
	displayOptionsInfo.sound.generic.y				= 240;
	displayOptionsInfo.sound.string					= "SOUND";
	displayOptionsInfo.sound.style					= UI_RIGHT;
	displayOptionsInfo.sound.color					= text_big_color;

	displayOptionsInfo.network.generic.type			= MTYPE_PTEXT;
	displayOptionsInfo.network.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	displayOptionsInfo.network.generic.id			= ID_NETWORK;
	displayOptionsInfo.network.generic.callback		= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.network.generic.x			= 216;
	displayOptionsInfo.network.generic.y			= 240 + PROP_HEIGHT;
	displayOptionsInfo.network.string				= "NETWORK";
	displayOptionsInfo.network.style				= UI_RIGHT;
	displayOptionsInfo.network.color				= text_big_color;

	y = 240 - 2 * (BIGCHAR_HEIGHT+2);
	displayOptionsInfo.brightness.generic.type		= MTYPE_SLIDER;
	displayOptionsInfo.brightness.generic.name		= "Brightness:";
	displayOptionsInfo.brightness.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	displayOptionsInfo.brightness.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.brightness.generic.id		= ID_BRIGHTNESS;
	displayOptionsInfo.brightness.generic.x			= 400;
	displayOptionsInfo.brightness.generic.y			= y;
	displayOptionsInfo.brightness.minvalue			= 5;
	displayOptionsInfo.brightness.maxvalue			= 20;
	if( !cgs.glconfig.deviceSupportsGamma ) {
		displayOptionsInfo.brightness.generic.flags |= QMF_GRAYED;
	}

	y += BIGCHAR_HEIGHT+2;
	displayOptionsInfo.screensize.generic.type		= MTYPE_SLIDER;
	displayOptionsInfo.screensize.generic.name		= "Screen Size:";
	displayOptionsInfo.screensize.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	displayOptionsInfo.screensize.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.screensize.generic.id		= ID_SCREENSIZE;
	displayOptionsInfo.screensize.generic.x			= 400;
	displayOptionsInfo.screensize.generic.y			= y;
	displayOptionsInfo.screensize.minvalue			= 3;
    displayOptionsInfo.screensize.maxvalue			= 10;

	y += BIGCHAR_HEIGHT+2;
	// references/modifies "r_anaglyphMode"
	displayOptionsInfo.anaglyph.generic.type		= MTYPE_SPINCONTROL;
	displayOptionsInfo.anaglyph.generic.name		= "Anaglyph:";
	displayOptionsInfo.anaglyph.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	displayOptionsInfo.anaglyph.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.anaglyph.generic.id			= ID_ANAGLYPH;
	displayOptionsInfo.anaglyph.generic.x			= 400;
	displayOptionsInfo.anaglyph.generic.y			= y;
	displayOptionsInfo.anaglyph.itemnames     		= anaglyph_names;

	y += BIGCHAR_HEIGHT+2;
	// references/modifies "r_greyscale"
	displayOptionsInfo.greyscale.generic.type		= MTYPE_SPINCONTROL;
	displayOptionsInfo.greyscale.generic.name		= "Grey Scale:";
	displayOptionsInfo.greyscale.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	displayOptionsInfo.greyscale.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.greyscale.generic.id			= ID_GREYSCALE;
	displayOptionsInfo.greyscale.generic.x			= 400;
	displayOptionsInfo.greyscale.generic.y			= y;
	displayOptionsInfo.greyscale.itemnames     		= offOn_names;

	displayOptionsInfo.back.generic.type		= MTYPE_BITMAP;
	displayOptionsInfo.back.generic.name		= ART_BACK0;
	displayOptionsInfo.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	displayOptionsInfo.back.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.back.generic.id			= ID_BACK;
	displayOptionsInfo.back.generic.x			= 0;
	displayOptionsInfo.back.generic.y			= 480-64;
	displayOptionsInfo.back.width				= 128;
	displayOptionsInfo.back.height				= 64;
	displayOptionsInfo.back.focuspic			= ART_BACK1;

	displayOptionsInfo.apply.generic.type		= MTYPE_BITMAP;
	displayOptionsInfo.apply.generic.name		= ART_ACCEPT0;
	displayOptionsInfo.apply.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIDDEN|QMF_INACTIVE;
	displayOptionsInfo.apply.generic.callback	= UI_DisplayOptionsMenu_Event;
	displayOptionsInfo.apply.generic.id			= ID_APPLY;
	displayOptionsInfo.apply.generic.x			= 640;
	displayOptionsInfo.apply.generic.y			= 480-64;
	displayOptionsInfo.apply.width				= 128;
	displayOptionsInfo.apply.height				= 64;
	displayOptionsInfo.apply.focuspic			= ART_ACCEPT1;

	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.banner );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.framel );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.framer );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.graphics );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.display );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.sound );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.network );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.brightness );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.screensize );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.anaglyph );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.greyscale );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.back );
	Menu_AddItem( &displayOptionsInfo.menu, ( void * ) &displayOptionsInfo.apply );

	displayOptionsInfo.brightness.curvalue  = trap_Cvar_VariableValue("r_gamma") * 10;
	displayOptionsInfo.screensize.curvalue  = trap_Cvar_VariableValue( "cg_viewsize")/10;
	displayOptionsInfo.anaglyph.curvalue    = trap_Cvar_VariableValue("r_anaglyphMode");
	if (displayOptionsInfo.anaglyph.curvalue < 0)
		displayOptionsInfo.anaglyph.curvalue = 0;
	else if (displayOptionsInfo.anaglyph.curvalue > numAnaglyphModes)
		displayOptionsInfo.anaglyph.curvalue = numAnaglyphModes;

	displayOptionsInfo.greyscale.curvalue = displayOptionsInfo.greyscale_default
			= (trap_Cvar_VariableValue( "r_greyscale") != 0);
}


/*
===============
UI_DisplayOptionsMenu_Cache
===============
*/
void UI_DisplayOptionsMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
}


/*
===============
UI_DisplayOptionsMenu
===============
*/
void UI_DisplayOptionsMenu( void ) {
	UI_DisplayOptionsMenu_Init();
	UI_PushMenu( &displayOptionsInfo.menu );
	Menu_SetCursorToItem( &displayOptionsInfo.menu, &displayOptionsInfo.display );
}
