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
/**********************************************************************
	UI_ATOMS.C

	User interface building blocks and support functions.
**********************************************************************/
#include "ui_local.h"

qboolean		m_entersound;		// after a frame, so caching won't disrupt the sound

qboolean newUI = qfalse;


/*
=================
UI_ClampCvar
=================
*/
float UI_ClampCvar( float min, float max, float value )
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

/*
=================
UI_StartDemoLoop
=================
*/
void UI_StartDemoLoop( void ) {
	trap_Cmd_ExecuteText( EXEC_APPEND, "d1\n" );
}


char *UI_Argv( int arg ) {
	static char	buffer[MAX_STRING_CHARS];

	trap_Argv( arg, buffer, sizeof( buffer ) );

	return buffer;
}


char *UI_Cvar_VariableString( const char *var_name ) {
	static char	buffer[MAX_STRING_CHARS];

	trap_Cvar_VariableStringBuffer( var_name, buffer, sizeof( buffer ) );

	return buffer;
}



void UI_SetBestScores(postGameInfo_t *newInfo, qboolean postGame) {
	trap_Cvar_Set("ui_scoreAccuracy",				va("%i%%", newInfo->accuracy));
	trap_Cvar_SetValue("ui_scoreImpressives",	newInfo->impressives);
	trap_Cvar_SetValue("ui_scoreExcellents", 	newInfo->excellents);
	trap_Cvar_SetValue("ui_scoreDefends", 		newInfo->defends);
	trap_Cvar_SetValue("ui_scoreAssists", 		newInfo->assists);
	trap_Cvar_SetValue("ui_scoreGauntlets", 	newInfo->gauntlets);
	trap_Cvar_SetValue("ui_scoreScore", 		newInfo->score);
	trap_Cvar_SetValue("ui_scorePerfect",	 	newInfo->perfects);
	trap_Cvar_Set("ui_scoreTeam",					va("%i to %i", newInfo->redScore, newInfo->blueScore));
	trap_Cvar_SetValue("ui_scoreBase",			newInfo->baseScore);
	trap_Cvar_SetValue("ui_scoreTimeBonus",		newInfo->timeBonus);
	trap_Cvar_SetValue("ui_scoreSkillBonus",	newInfo->skillBonus);
	trap_Cvar_SetValue("ui_scoreShutoutBonus",	newInfo->shutoutBonus);
	trap_Cvar_Set("ui_scoreTime",					va("%02i:%02i", newInfo->time / 60, newInfo->time % 60));
	trap_Cvar_SetValue("ui_scoreCaptures",		newInfo->captures);
  if (postGame) {
		trap_Cvar_Set("ui_scoreAccuracy2",				va("%i%%", newInfo->accuracy));
		trap_Cvar_SetValue("ui_scoreImpressives2",	newInfo->impressives);
		trap_Cvar_SetValue("ui_scoreExcellents2",	newInfo->excellents);
		trap_Cvar_SetValue("ui_scoreDefends2",		newInfo->defends);
		trap_Cvar_SetValue("ui_scoreAssists2",		newInfo->assists);
		trap_Cvar_SetValue("ui_scoreGauntlets2",	newInfo->gauntlets);
		trap_Cvar_SetValue("ui_scoreScore2",		newInfo->score);
		trap_Cvar_SetValue("ui_scorePerfect2",		newInfo->perfects);
		trap_Cvar_Set("ui_scoreTeam2",					va("%i to %i", newInfo->redScore, newInfo->blueScore));
		trap_Cvar_SetValue("ui_scoreBase2",			newInfo->baseScore);
		trap_Cvar_SetValue("ui_scoreTimeBonus2",	newInfo->timeBonus);
		trap_Cvar_SetValue("ui_scoreSkillBonus2",	newInfo->skillBonus);
		trap_Cvar_SetValue("ui_scoreShutoutBonus2",	newInfo->shutoutBonus);
		trap_Cvar_Set("ui_scoreTime2",					va("%02i:%02i", newInfo->time / 60, newInfo->time % 60));
		trap_Cvar_SetValue("ui_scoreCaptures2",		newInfo->captures);
	}
}

void UI_LoadBestScores(const char *map, int game)
{
	char		fileName[MAX_QPATH];
	fileHandle_t f;
	postGameInfo_t newInfo;
	int protocol, protocolLegacy;
	
	memset(&newInfo, 0, sizeof(postGameInfo_t));
	Com_sprintf(fileName, MAX_QPATH, "games/%s_%i.game", map, game);
	if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0) {
		int size = 0;
		trap_FS_Read(&size, sizeof(int), f);
		if (size == sizeof(postGameInfo_t)) {
			trap_FS_Read(&newInfo, sizeof(postGameInfo_t), f);
		}
		trap_FS_FCloseFile(f);
	}
	UI_SetBestScores(&newInfo, qfalse);

	uiInfo.demoAvailable = qfalse;

	protocolLegacy = trap_Cvar_VariableValue("com_legacyprotocol");
	protocol = trap_Cvar_VariableValue("com_protocol");

	if(!protocol)
		protocol = trap_Cvar_VariableValue("protocol");
	if(protocolLegacy == protocol)
		protocolLegacy = 0;

	Com_sprintf(fileName, MAX_QPATH, "demos/%s_%d.%s%d", map, game, DEMOEXT, protocol);
	if(trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0)
	{
		uiInfo.demoAvailable = qtrue;
		trap_FS_FCloseFile(f);
	}
	else if(protocolLegacy > 0)
	{
		Com_sprintf(fileName, MAX_QPATH, "demos/%s_%d.%s%d", map, game, DEMOEXT, protocolLegacy);
		if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0)
		{
			uiInfo.demoAvailable = qtrue;
			trap_FS_FCloseFile(f);
		}
	} 
}

/*
===============
UI_ClearScores
===============
*/
void UI_ClearScores(void) {
	char	gameList[4096];
	char *gameFile;
	int		i, len, count, size;
	fileHandle_t f;
	postGameInfo_t newInfo;

	count = trap_FS_GetFileList( "games", "game", gameList, sizeof(gameList) );

	size = sizeof(postGameInfo_t);
	memset(&newInfo, 0, size);

	if (count > 0) {
		gameFile = gameList;
		for ( i = 0; i < count; i++ ) {
			len = strlen(gameFile);
			if (trap_FS_FOpenFile(va("games/%s",gameFile), &f, FS_WRITE) >= 0) {
				trap_FS_Write(&size, sizeof(int), f);
				trap_FS_Write(&newInfo, size, f);
				trap_FS_FCloseFile(f);
			}
			gameFile += len + 1;
		}
	}
	
	UI_SetBestScores(&newInfo, qfalse);

}



static void	UI_Cache_f( void ) {
	Display_CacheAll();
}

/*
=======================
UI_CalcPostGameStats
=======================
*/
static void UI_CalcPostGameStats( void ) {
	char		map[MAX_QPATH];
	char		fileName[MAX_QPATH];
	char		info[MAX_INFO_STRING];
	fileHandle_t f;
	int size, game, time, adjustedTime;
	postGameInfo_t oldInfo;
	postGameInfo_t newInfo;
	qboolean newHigh = qfalse;

	trap_GetConfigString( CS_SERVERINFO, info, sizeof(info) );
	Q_strncpyz( map, Info_ValueForKey( info, "mapname" ), sizeof(map) );
	game = atoi(Info_ValueForKey(info, "g_gametype"));

	// compose file name
	Com_sprintf(fileName, MAX_QPATH, "games/%s_%i.game", map, game);
	// see if we have one already
	memset(&oldInfo, 0, sizeof(postGameInfo_t));
	if (trap_FS_FOpenFile(fileName, &f, FS_READ) >= 0) {
	// if so load it
		size = 0;
		trap_FS_Read(&size, sizeof(int), f);
		if (size == sizeof(postGameInfo_t)) {
			trap_FS_Read(&oldInfo, sizeof(postGameInfo_t), f);
		}
		trap_FS_FCloseFile(f);
	}					 

	newInfo.accuracy = atoi(UI_Argv(3));
	newInfo.impressives = atoi(UI_Argv(4));
	newInfo.excellents = atoi(UI_Argv(5));
	newInfo.defends = atoi(UI_Argv(6));
	newInfo.assists = atoi(UI_Argv(7));
	newInfo.gauntlets = atoi(UI_Argv(8));
	newInfo.baseScore = atoi(UI_Argv(9));
	newInfo.perfects = atoi(UI_Argv(10));
	newInfo.redScore = atoi(UI_Argv(11));
	newInfo.blueScore = atoi(UI_Argv(12));
	time = atoi(UI_Argv(13));
	newInfo.captures = atoi(UI_Argv(14));

	newInfo.time = (time - trap_Cvar_VariableValue("ui_matchStartTime")) / 1000;
	adjustedTime = uiInfo.mapList[ui_currentMap.integer].timeToBeat[game];
	if (newInfo.time < adjustedTime) { 
		newInfo.timeBonus = (adjustedTime - newInfo.time) * 10;
	} else {
		newInfo.timeBonus = 0;
	}

	if (newInfo.redScore > newInfo.blueScore && newInfo.blueScore <= 0) {
		newInfo.shutoutBonus = 100;
	} else {
		newInfo.shutoutBonus = 0;
	}

	newInfo.skillBonus = trap_Cvar_VariableValue("g_spSkill");
	if (newInfo.skillBonus <= 0) {
		newInfo.skillBonus = 1;
	}
	newInfo.score = newInfo.baseScore + newInfo.shutoutBonus + newInfo.timeBonus;
	newInfo.score *= newInfo.skillBonus;

	// see if the score is higher for this one
	newHigh = (newInfo.redScore > newInfo.blueScore && newInfo.score > oldInfo.score);

	if  (newHigh) {
		// if so write out the new one
		uiInfo.newHighScoreTime = uiInfo.uiDC.realTime + 20000;
		if (trap_FS_FOpenFile(fileName, &f, FS_WRITE) >= 0) {
			size = sizeof(postGameInfo_t);
			trap_FS_Write(&size, sizeof(int), f);
			trap_FS_Write(&newInfo, sizeof(postGameInfo_t), f);
			trap_FS_FCloseFile(f);
		}
	}

	if (newInfo.time < oldInfo.time) {
		uiInfo.newBestTime = uiInfo.uiDC.realTime + 20000;
	}
 
	// put back all the ui overrides
	trap_Cvar_SetValue("capturelimit", trap_Cvar_VariableValue("ui_saveCaptureLimit"));
	trap_Cvar_SetValue("fraglimit", trap_Cvar_VariableValue("ui_saveFragLimit"));
	trap_Cvar_SetValue("cg_drawTimer", trap_Cvar_VariableValue("ui_drawTimer"));
	trap_Cvar_SetValue("g_doWarmup", trap_Cvar_VariableValue("ui_doWarmup"));
	trap_Cvar_SetValue("g_Warmup", trap_Cvar_VariableValue("ui_Warmup"));
	trap_Cvar_SetValue("sv_pure", trap_Cvar_VariableValue("ui_pure"));
	trap_Cvar_SetValue("g_friendlyFire", trap_Cvar_VariableValue("ui_friendlyFire"));

	UI_SetBestScores(&newInfo, qtrue);
	UI_ShowPostGame(newHigh);


}


/*
=================
UI_ConsoleCommand
=================
*/
qboolean UI_ConsoleCommand( int realTime ) {
	char	*cmd;

	uiInfo.uiDC.frameTime = realTime - uiInfo.uiDC.realTime;
	uiInfo.uiDC.realTime = realTime;

	cmd = UI_Argv( 0 );

	// ensure minimum menu data is available
	//Menu_Cache();

	if ( Q_stricmp (cmd, "ui_test") == 0 ) {
		UI_ShowPostGame(qtrue);
	}

	if ( Q_stricmp (cmd, "ui_report") == 0 ) {
		UI_Report();
		return qtrue;
	}
	
	if ( Q_stricmp (cmd, "ui_load") == 0 ) {
#ifdef MISSIONPACK_HUD
		if ( cg.connected ) {
			// if hud scripts are loaded, UI_Load() will break it. So reload hud and ui using loadhud command.
			trap_Cmd_ExecuteText( EXEC_NOW, "loadhud\n" );
			return qtrue;
		}
#endif
		UI_Load();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "remapShader") == 0 ) {
		if (trap_Argc() == 4) {
			char shader1[MAX_QPATH];
			char shader2[MAX_QPATH];
			char shader3[MAX_QPATH];
			
			Q_strncpyz(shader1, UI_Argv(1), sizeof(shader1));
			Q_strncpyz(shader2, UI_Argv(2), sizeof(shader2));
			Q_strncpyz(shader3, UI_Argv(3), sizeof(shader3));
			
			trap_R_RemapShader(shader1, shader2, shader3);
			return qtrue;
		}
	}

	if ( Q_stricmp (cmd, "postgame") == 0 ) {
		UI_CalcPostGameStats();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "ui_cache") == 0 ) {
		UI_Cache_f();
		return qtrue;
	}

	if ( Q_stricmp (cmd, "ui_teamOrders") == 0 ) {
		//UI_TeamOrdersMenu_f();
		return qtrue;
	}

	return qfalse;
}

/*
================
UI_AdjustFrom640

Adjusted for resolution and screen aspect ratio
================
*/
void UI_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	CG_AdjustFrom640( x, y, w, h );
}

void UI_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	hShader = trap_R_RegisterShaderNoMip( picname );
	UI_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

void UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader ) {
	float	s0;
	float	s1;
	float	t0;
	float	t1;

	if( w < 0 ) {	// flip about vertical
		w  = -w;
		s0 = 1;
		s1 = 0;
	}
	else {
		s0 = 0;
		s1 = 1;
	}

	if( h < 0 ) {	// flip about horizontal
		h  = -h;
		t0 = 1;
		t1 = 0;
	}
	else {
		t0 = 0;
		t1 = 1;
	}
	
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, h, s0, t0, s1, t1, hShader );
}

/*
================
UI_FillRect

Coordinates are 640*480 virtual values
=================
*/
void UI_FillRect( float x, float y, float width, float height, const float *color ) {
	trap_R_SetColor( color );

	UI_AdjustFrom640( &x, &y, &width, &height );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );

	trap_R_SetColor( NULL );
}

void UI_DrawSides(float x, float y, float w, float h) {
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x + w - 1, y, 1, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}

void UI_DrawTopBottom(float x, float y, float w, float h) {
	UI_AdjustFrom640( &x, &y, &w, &h );
	trap_R_DrawStretchPic( x, y, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
	trap_R_DrawStretchPic( x, y + h - 1, w, 1, 0, 0, 0, 0, uiInfo.uiDC.whiteShader );
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void UI_DrawRect( float x, float y, float width, float height, const float *color ) {
	trap_R_SetColor( color );

  UI_DrawTopBottom(x, y, width, height);
  UI_DrawSides(x, y, width, height);

	trap_R_SetColor( NULL );
}

void UI_SetColor( const float *rgba ) {
	trap_R_SetColor( rgba );
}

void UI_UpdateScreen( void ) {
	trap_UpdateScreen();
}


void UI_DrawTextBox (int x, int y, int width, int lines)
{
	UI_FillRect( x + BIGCHAR_WIDTH/2, y + BIGCHAR_HEIGHT/2, ( width + 1 ) * BIGCHAR_WIDTH, ( lines + 1 ) * BIGCHAR_HEIGHT, colorBlack );
	UI_DrawRect( x + BIGCHAR_WIDTH/2, y + BIGCHAR_HEIGHT/2, ( width + 1 ) * BIGCHAR_WIDTH, ( lines + 1 ) * BIGCHAR_HEIGHT, colorWhite );
}

qboolean UI_CursorInRect (int x, int y, int width, int height)
{
	if (uiInfo.uiDC.cursorx < x ||
		uiInfo.uiDC.cursory < y ||
		uiInfo.uiDC.cursorx > x+width ||
		uiInfo.uiDC.cursory > y+height)
		return qfalse;

	return qtrue;
}
