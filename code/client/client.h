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
// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
#include "../ui/ui_public.h"
#include "keys.h"
#include "snd_public.h"
#include "../cgame/cg_public.h"
#include "../game/bg_public.h"

#ifdef USE_CURL
#include "cl_curl.h"
#endif /* USE_CURL */

#ifdef USE_VOIP
#include "speex/speex.h"
#include "speex/speex_preprocess.h"
#endif

// Lower this define to change max supported local clients for client/renderer,
// cgame/ui get max from client when they are initialized.
#ifndef CL_MAX_SPLITVIEW
#	ifdef IOS
#		define CL_MAX_SPLITVIEW 1
#	else
#		define CL_MAX_SPLITVIEW MAX_SPLITVIEW
#	endif
#endif

// file full of random crap that gets used to create cl_guid
#define QKEY_FILE "qkey"
#define QKEY_SIZE 2048

#define	RETRANSMIT_TIMEOUT	3000	// time between connection packet retransmits

// snapshots are a view of the server at a given time
typedef struct {
	qboolean		valid;			// cleared if delta parsing was invalid
	int				snapFlags;		// rate delayed and dropped commands

	int				serverTime;		// server time the message is valid for (in msec)

	int				messageNum;		// copied from netchan->incoming_sequence
	int				deltaNum;		// messageNum the delta is from
	int				ping;			// time from when cmdNum-1 was sent to time packet was reeceived
	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	int				cmdNum;			// the next cmdNum the server is expecting

	int				numPSs;
	darray_t		playerStates;	// complete information about the current players at this time
	int				lcIndex[MAX_SPLITVIEW];
	int				clientNums[MAX_SPLITVIEW];

	int				numEntities;			// all of the entities that need to be presented
	int				parseEntitiesNum;		// at the time of this snapshot

	int				serverCommandNum;		// execute all commands up to this before
											// making the snapshot current
} clSnapshot_t;



/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

typedef struct {
	int		p_cmdNumber;		// cl.cmdNumber when packet was sent
	int		p_serverTime;		// usercmd->serverTime when packet was sent
	int		p_realtime;			// cls.realtime when packet was sent
} outPacket_t;

extern int g_console_field_width;

// Client Active Local Client
typedef struct {
	int			mouseDx[2], mouseDy[2];	// added to by mouse events
	int			mouseIndex;

} calc_t;

typedef struct {
	int			timeoutcount;		// it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t	snap;			// latest received from server

	int			serverTime;			// may be paused during play
	int			oldServerTime;		// to prevent time from flowing bakcwards
	int			oldFrameServerTime;	// to check tournament restarts
	int			serverTimeDelta;	// cl.serverTime = cls.realtime + cl.serverTimeDelta
									// this value changes as net lag varies
	qboolean	extrapolatedSnapshot;	// set if any cgame frame has been forced to extrapolate
									// cleared when CL_AdjustTimeDelta looks at it
	qboolean	newSnapshots;		// set on parse of any valid packet

	gameState_t	gameState;			// configstrings
	char		mapname[MAX_QPATH];	// extracted from CS_SERVERINFO

	int			parseEntitiesNum;	// index (not anded off) into cl_parse_entities[]

	calc_t		localClients[CL_MAX_SPLITVIEW];

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t	cmdss[CL_MAX_SPLITVIEW][CMD_BACKUP];	// each mesage will send several old cmds
	int			cmdNumber;			// incremented each frame, because multiple
									// frames may need to be packed into a single packet

	outPacket_t	outPackets[PACKET_BACKUP];	// information about each packet we have sent out

	int			serverId;			// included in each client message so the server
												// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t	snapshots[PACKET_BACKUP];
	darray_t		tempSnapshotPS;

	darray_t		entityBaselines; // entityState_t [MAX_GENTITIES], for delta compression when not in previous frame

	// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
	// entities, so that when a delta compressed message arives from the server
	// it can be un-deltad from the original 
	darray_t		parseEntities; // entityState_t [CL_MAX_SPLITVIEW * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES]

	int				cgameEntityStateSize;
	int				cgamePlayerStateSize;

} clientActive_t;

extern	clientActive_t		cl;

sharedEntityState_t *CL_ParseEntityState( int num );

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/

#define MAX_TIMEDEMO_DURATIONS	4096

typedef struct {

	connstate_t	state;				// connection status

	int			clientNums[MAX_SPLITVIEW];
	int			lastPacketSentTime;			// for retransmits during connection
	int			lastPacketTime;				// for timeouts

	char		servername[MAX_OSPATH];		// name of server from original connect (used by reconnect)
	netadr_t	serverAddress;
	int			connectTime;				// for connection retransmits
	int			connectPacketCount;			// for display on connection dialog
	char		serverMessage[MAX_STRING_TOKENS];	// for display on connection dialog

	int			challenge;					// from the server to use for connecting

	// these are our reliable messages that go to the server
	int			reliableSequence;
	int			reliableAcknowledge;		// the last one the server has executed
	char		reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	int			serverMessageSequence;

	// reliable messages received from server
	int			serverCommandSequence;
	int			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	char		serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// file transfer from server
	fileHandle_t download;
	char		downloadTempName[MAX_OSPATH];
	char		downloadName[MAX_OSPATH];
#ifdef USE_CURL
	qboolean	cURLEnabled;
	qboolean	cURLUsed;
	qboolean	cURLDisconnected;
	char		downloadURL[MAX_OSPATH];
	CURL		*downloadCURL;
	CURLM		*downloadCURLM;
#endif /* USE_CURL */
	int		sv_allowDownload;
	char		sv_dlURL[MAX_CVAR_VALUE_STRING];
	int			downloadNumber;
	int			downloadBlock;	// block we are waiting for
	int			downloadCount;	// how many bytes we got
	int			downloadSize;	// how many bytes we got
	char		downloadList[MAX_INFO_STRING]; // list of paks we need to download
	qboolean	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak

	// demo information
	char		demoName[MAX_QPATH];
	qboolean	demorecording;
	qboolean	demoplaying;
	qboolean	demowaiting;	// don't record until a non-delta message is received
	qboolean	firstDemoFrameSkipped;
	fileHandle_t	demofile;
	int			demoLength;		// size of playback demo

	int			timeDemoFrames;		// counter of rendered frames
	int			timeDemoStart;		// cls.realtime before first frame
	int			timeDemoBaseTime;	// each frame will be at this time + frameNum * 50
	int			timeDemoLastFrame;// time the last frame was rendered
	int			timeDemoMinDuration;	// minimum frame duration
	int			timeDemoMaxDuration;	// maximum frame duration
	unsigned char	timeDemoDurations[ MAX_TIMEDEMO_DURATIONS ];	// log of frame durations

#ifdef USE_VOIP
	qboolean voipEnabled;
	qboolean speexInitialized;
	int speexFrameSize;
	int speexSampleRate;

	// incoming data...
	// !!! FIXME: convert from parallel arrays to array of a struct.
	SpeexBits speexDecoderBits[MAX_CLIENTS];
	void *speexDecoder[MAX_CLIENTS];
	byte voipIncomingGeneration[MAX_CLIENTS];
	int voipIncomingSequence[MAX_CLIENTS];
	float voipGain[MAX_CLIENTS];
	qboolean voipIgnore[MAX_CLIENTS];
	int voipLastPacketTime[MAX_CLIENTS];
	float voipPower[MAX_CLIENTS];
	qboolean voipMuteAll;

	// outgoing data...
	// if voipTargets[i / 8] & (1 << (i % 8)),
	// then we are sending to clientnum i.
	uint8_t voipTargets[(MAX_CLIENTS + 7) / 8];
	uint8_t voipFlags;
	SpeexPreprocessState *speexPreprocessor;
	SpeexBits speexEncoderBits;
	void *speexEncoder;
	int voipOutgoingDataSize;
	int voipOutgoingDataFrames;
	int voipOutgoingSequence;
	byte voipOutgoingGeneration;
	byte voipOutgoingData[1024];
#endif

#ifdef LEGACY_PROTOCOL
	qboolean compat;
#endif

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;
} clientConnection_t;

extern	clientConnection_t clc;

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all
(except when CL_Shutdown is called)

==================================================================
*/

typedef struct {
	netadr_t	adr;
	int			start;
	int			time;
	char		info[MAX_INFO_STRING];
} ping_t;

typedef struct {
	netadr_t	adr;
	char	  	hostName[MAX_NAME_LENGTH];
	char	  	mapName[MAX_NAME_LENGTH];
	char	  	game[MAX_NAME_LENGTH];
	int			netType;
	char		gameType[MAX_NAME_LENGTH];
	int		  	clients;
	int		  	maxClients;
	int			minPing;
	int			maxPing;
	int			ping;
	qboolean	visible;
	int			g_humanplayers;
	int			g_needpass;
} serverInfo_t;

typedef struct {
	// when the server clears the hunk, all of these must be restarted
	qboolean	rendererStarted;
	qboolean	soundStarted;
	qboolean	soundRegistered;
	qboolean	uiStarted;
	qboolean	cgameStarted;

	int			framecount;
	int			frametime;			// msec since last frame

	int			realtime;			// ignores pause
	int			realFrametime;		// ignoring pause, so console always works

	int			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	int			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	int			numGlobalServerAddresses;
	netadr_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		// source currently pinging or updating

	// update server info
	netadr_t	updateServer;
	char		updateChallenge[MAX_TOKEN_CHARS];
	char		updateInfoString[MAX_INFO_STRING];

	// rendering info
	glconfig_t	glconfig;
	qboolean	drawnLoadingScreen;
	qhandle_t	charSetShader;
	qhandle_t	whiteShader;
	qhandle_t	consoleShader;
} clientStatic_t;

extern	clientStatic_t		cls;

extern	char		cl_oldGame[MAX_QPATH];
extern	qboolean	cl_oldGameSet;

#define MAX_BUTTONS 20

typedef struct {
	qboolean active;
	qboolean pressed;
	qboolean initialized;
	float x, y, h, w;
	int callback;
	int menu;
} buttons_t;

typedef struct {
	qboolean clean;
	buttons_t buttons[MAX_BUTTONS];
} screenInput_t;

extern	screenInput_t		clsi;

#ifdef IOS
int cl_joyscale_x[2];
int cl_joyscale_y[2];
#endif

//=============================================================================

extern	vm_t			*cgvm;	// interface to cgame dll or vm
extern	vm_t			*uivm;	// interface to ui dll or vm
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//
extern	cvar_t	*cl_nodelta;
extern	cvar_t	*cl_noprint;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_maxpackets;
extern	cvar_t	*cl_packetdup;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showSend;
extern	cvar_t	*cl_timeNudge;
extern	cvar_t	*cl_showTimeDelta;
extern	cvar_t	*cl_freezeDemo;

extern	cvar_t	*cl_sensitivity;

extern	cvar_t	*cl_mouseAccel;
extern	cvar_t	*cl_mouseAccelOffset;
extern	cvar_t	*cl_mouseAccelStyle;
extern	cvar_t	*cl_showMouseRate;

extern	cvar_t	*m_filter;

extern	cvar_t	*cl_timedemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;

extern	cvar_t	*cl_activeAction;

extern	cvar_t	*cl_allowDownload;
extern  cvar_t  *cl_downloadMethod;
extern	cvar_t	*cl_conXOffset;
extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cl_lanForcePackets;
extern	cvar_t	*cl_autoRecordDemo;

extern	cvar_t	*con_autochat;
extern	cvar_t	*cl_consoleKeys;

#ifdef USE_MUMBLE
extern	cvar_t	*cl_useMumble;
extern	cvar_t	*cl_mumbleScale;
#endif

#ifdef USE_VOIP
// cl_voipSendTarget is a string: "all" to broadcast to everyone, "none" to
//  send to no one, or a comma-separated list of client numbers:
//  "0,7,2,23" ... an empty string is treated like "all".
extern	cvar_t	*cl_voipUseVAD;
extern	cvar_t	*cl_voipVADThreshold;
extern	cvar_t	*cl_voipSend;
extern	cvar_t	*cl_voipSendTarget;
extern	cvar_t	*cl_voipGainDuringCapture;
extern	cvar_t	*cl_voipCaptureMult;
extern	cvar_t	*cl_voip;
#endif

//=================================================

//
// cl_main
//

void CL_Init (void);
void CL_AddReliableCommand(const char *cmd, qboolean isDisconnectCmd);

void CL_StartHunkUsers( qboolean rendererOnly );

void CL_Disconnect_f (void);
void CL_GetChallengePacket (void);
void CL_Vid_Restart_f( void );
void CL_Snd_Restart_f (void);
void CL_StartDemoLoop( void );
void CL_NextDemo( void );
demoState_t CL_DemoState( void );
int CL_DemoPos( void );
void CL_DemoName( char *buffer, int size );
int CL_DemoLength( void );
void CL_ReadDemoMessage( void );
void CL_StopRecord_f(void);

void CL_InitDownloads(void);
void CL_NextDownload(void);

void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
void CL_ClearPing( int n );
int CL_GetPingQueueCount( void );

void CL_InitRef( void );
int CL_ServerStatus( char *serverAddress, char *serverStatusString, int maxLen );

qboolean CL_CheckPaused(void);

//
// cl_input
//
void CL_InitInput(void);
void CL_ShutdownInput(void);
void CL_SendCmd (void);
void CL_ClearState (void);
void CL_InitConnection (qboolean clear);
void CL_ReadPackets (void);

void CL_WritePacket( void );

void CL_VerifyCode( void );

int Key_StringToKeynum( char *str );
char *Key_KeynumToString (int keynum);

void CL_DrawTouchArea(float x, float y, float w, float h, int menu, int callback);
void CL_FlushButtons( void );

//
// cl_parse.c
//
extern int cl_connectedToCheatServer;

#ifdef USE_VOIP
void CL_Voip_f( void );
int CL_GetVoipTime( int clientNum );
float CL_GetVoipPower( int clientNum );
float CL_GetVoipGain( int clientNum );
qboolean CL_GetVoipMuteClient( int clientNum );
qboolean CL_GetVoipMuteAll( void );
#endif

void CL_SystemInfoChanged( void );
void CL_ParseServerMessage( msg_t *msg );

//====================================================================

void	CL_ServerInfoPacket( netadr_t from, msg_t *msg );
void	CL_LocalServers_f( void );
void	CL_GlobalServers_f( void );
void	CL_FavoriteServers_f( void );
void	CL_Ping_f( void );
qboolean CL_UpdateVisiblePings_f( int source );


//
// console
//
void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init(void);
void Con_Shutdown(void);
void Con_Clear_f (void);
void Con_ToggleConsole_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_RunConsole (void);
void Con_DrawConsole (void);
void Con_PageUp( void );
void Con_PageDown( void );
void Con_Top( void );
void Con_Bottom( void );
void Con_Close( void );

void CL_LoadConsoleHistory( void );
void CL_SaveConsoleHistory( void );

//
// cl_scrn.c
//
void	SCR_Init (void);
void	SCR_UpdateScreen (void);

void	SCR_DebugGraph (float value);

int		SCR_GetBigStringWidth( const char *str );	// returns in virtual 640x480 coordinates

void	SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
void	SCR_FillRect( float x, float y, float width, float height, 
					 const float *color );
void	SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader );
void	SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname );

void	SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape );			// draws a string with embedded color control characters with fade
void	SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color, qboolean noColorEscape );	// ignores embedded color control characters
void	SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, qboolean forceColor, qboolean noColorEscape );
void	SCR_DrawSmallChar( int x, int y, int ch );


//
// cl_cin.c
//

void CL_PlayCinematic_f( void );
void SCR_DrawCinematic (void);
void SCR_RunCinematic (void);
void SCR_StopCinematic (void);
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic (int handle);
void CIN_DrawCinematic (int handle);
void CIN_SetExtents (int handle, int x, int y, int w, int h);
void CIN_SetLooping (int handle, qboolean loop);
void CIN_UploadCinematic(int handle);
void CIN_CloseAllVideos(void);

//
// cl_cgame.c
//
void CL_InitCGame( void );
void CL_ShutdownCGame( void );
qboolean CL_GameCommand( void );
void CL_CGameRendering( stereoFrame_t stereo );
void CL_SetCGameTime( void );
void CL_FirstSnapshot( void );
void CL_ShaderStateChanged(void);

//
// cl_ui.c
//
void CL_InitUI( void );
void CL_ShutdownUI( void );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );
void LAN_LoadCachedServers( void );
void LAN_SaveServersToCache( void );


//
// cl_net_chan.c
//
void CL_Netchan_Transmit( netchan_t *chan, msg_t* msg);	//int length, const byte *data );
qboolean CL_Netchan_Process( netchan_t *chan, msg_t *msg );

//
// cl_avi.c
//
qboolean CL_OpenAVIForWriting( const char *filename );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qboolean CL_CloseAVI( void );
qboolean CL_VideoRecording( void );

//
// cl_main.c
//
void CL_WriteDemoMessage ( msg_t *msg, int headerBytes );
void CL_GetMapMessage(char *buf, int bufLength);
qboolean CL_GetClientLocation(char *buf, int bufLength, int localClientNum);

