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
// cl_parse.c  -- parse a message received from the server

#include "client.h"

char *svc_strings[256] = {
	"svc_bad",

	"svc_nop",
	"svc_gamestate",
	"svc_configstring",
	"svc_baseline",	
	"svc_serverCommand",
	"svc_download",
	"svc_snapshot",
	"svc_EOF",
	"svc_voip",
};

void SHOWNET( msg_t *msg, char *s) {
	if ( cl_shownet->integer >= 2) {
		Com_Printf ("%3i:%s\n", msg->readcount-1, s);
	}
}

/*
=========================================================================

MESSAGE PARSING

=========================================================================
*/

/*
==================
CL_LocalClientAdded
==================
*/
void CL_LocalClientAdded(int localClientNum, int clientNum) {
	if (clientNum < 0 || clientNum >= MAX_CLIENTS)
		return;

	clc.clientNums[localClientNum] = clientNum;
}

/*
==================
CL_LocalClientRemoved
==================
*/
void CL_LocalClientRemoved(int localClientNum) {
	if (clc.clientNums[localClientNum] == -1)
		return;

	clc.clientNums[localClientNum] = -1;
}

/*
==================
CL_ParseEntityState

Client only looks at shared part of entityState_t
==================
*/
sharedEntityState_t *CL_ParseEntityState( int num ) {
	return DA_ElementPointer( cl.parseEntities, num % cl.parseEntities.maxElements );
}

/*
==================
CL_DeltaEntity

Parses deltas from the given base and adds the resulting entity
to the current frame
==================
*/
void CL_DeltaEntity (msg_t *msg, clSnapshot_t *frame, int newnum, sharedEntityState_t *old, 
					 qboolean unchanged) {
	sharedEntityState_t	*state;

	// save the parsed entity state into the big circular buffer so
	// it can be used as the source for a later delta
	state = CL_ParseEntityState( cl.parseEntitiesNum );

	if ( unchanged ) {
		Com_Memcpy( state, old, cl.cgameEntityStateSize );
	} else {
		MSG_ReadDeltaEntity( msg, old, state, newnum );
	}

	if ( state->number == (MAX_GENTITIES-1) ) {
		return;		// entity was delta removed
	}
	cl.parseEntitiesNum++;
	frame->numEntities++;
}

/*
==================
CL_ParsePacketEntities

==================
*/
void CL_ParsePacketEntities( msg_t *msg, clSnapshot_t *oldframe, clSnapshot_t *newframe) {
	int			newnum;
	sharedEntityState_t	*oldstate;
	int			oldindex, oldnum;

	newframe->parseEntitiesNum = cl.parseEntitiesNum;
	newframe->numEntities = 0;

	// delta from the entities present in oldframe
	oldindex = 0;
	oldstate = NULL;
	if (!oldframe) {
		oldnum = 99999;
	} else {
		if ( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = CL_ParseEntityState(oldframe->parseEntitiesNum + oldindex);
			oldnum = oldstate->number;
		}
	}

	while ( 1 ) {
		// read the entity index number
		newnum = MSG_ReadBits( msg, GENTITYNUM_BITS );

		if ( newnum == (MAX_GENTITIES-1) ) {
			break;
		}

		if ( msg->readcount > msg->cursize ) {
			Com_Error (ERR_DROP,"CL_ParsePacketEntities: end of message");
		}

		while ( oldnum < newnum ) {
			// one or more entities from the old packet are unchanged
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  unchanged: %i\n", msg->readcount, oldnum);
			}
			CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );
			
			oldindex++;

			if ( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = CL_ParseEntityState(oldframe->parseEntitiesNum + oldindex);
				oldnum = oldstate->number;
			}
		}
		if (oldnum == newnum) {
			// delta from previous state
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  delta: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( msg, newframe, newnum, oldstate, qfalse );

			oldindex++;

			if ( oldindex >= oldframe->numEntities ) {
				oldnum = 99999;
			} else {
				oldstate = CL_ParseEntityState(oldframe->parseEntitiesNum + oldindex);
				oldnum = oldstate->number;
			}
			continue;
		}

		if ( oldnum > newnum ) {
			// delta from baseline
			if ( cl_shownet->integer == 3 ) {
				Com_Printf ("%3i:  baseline: %i\n", msg->readcount, newnum);
			}
			CL_DeltaEntity( msg, newframe, newnum, DA_ElementPointer( cl.entityBaselines, newnum ), qfalse );
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while ( oldnum != 99999 ) {
		// one or more entities from the old packet are unchanged
		if ( cl_shownet->integer == 3 ) {
			Com_Printf ("%3i:  unchanged: %i\n", msg->readcount, oldnum);
		}
		CL_DeltaEntity( msg, newframe, oldnum, oldstate, qtrue );
		
		oldindex++;

		if ( oldindex >= oldframe->numEntities ) {
			oldnum = 99999;
		} else {
			oldstate = CL_ParseEntityState(oldframe->parseEntitiesNum + oldindex);
			oldnum = oldstate->number;
		}
	}
}


/*
================
CL_ParseSnapshot

If the snapshot is parsed properly, it will be copied to
cl.snap and saved in cl.snapshots[].  If the snapshot is invalid
for any reason, no changes to the state will be made at all.
================
*/
void CL_ParseSnapshot( msg_t *msg ) {
	int			len;
	clSnapshot_t	*old;
	clSnapshot_t	newSnap;
	sharedPlayerState_t *newPS, *oldPS;
	int			deltaNum;
	int			oldMessageNum;
	int			i, packetNum;

	if ( !cgvm ) {
		Com_Error( ERR_DROP, "Received unexpected snapshot" );
	}

	if ( !cl.cgamePlayerStateSize || !cl.cgamePlayerStateSize ) {
		Com_Error( ERR_DROP, "cgame needs to call trap_SetNetFields" );
	}

	// get the reliable sequence acknowledge number
	// NOTE: now sent with all server to client messages
	//clc.reliableAcknowledge = MSG_ReadLong( msg );

	// read in the new snapshot to a temporary buffer
	// we will only copy to cl.snap if it is valid
	Com_Memset (&newSnap, 0, sizeof(newSnap));

	// we will have read any new server commands in this
	// message before we got to svc_snapshot
	newSnap.serverCommandNum = clc.serverCommandSequence;

	newSnap.serverTime = MSG_ReadLong( msg );

	// if we were just unpaused, we can only *now* really let the
	// change come into effect or the client hangs.
	cl_paused->modified = 0;

	newSnap.messageNum = clc.serverMessageSequence;

	deltaNum = MSG_ReadByte( msg );
	if ( !deltaNum ) {
		newSnap.deltaNum = -1;
	} else {
		newSnap.deltaNum = newSnap.messageNum - deltaNum;
	}
	newSnap.snapFlags = MSG_ReadByte( msg );

	// If the frame is delta compressed from data that we
	// no longer have available, we must suck up the rest of
	// the frame, but not use it, then ask for a non-compressed
	// message 
	if ( newSnap.deltaNum <= 0 ) {
		newSnap.valid = qtrue;		// uncompressed frame
		old = NULL;
		clc.demowaiting = qfalse;	// we can start recording now
	} else {
		old = &cl.snapshots[newSnap.deltaNum & PACKET_MASK];
		if ( !old->valid ) {
			// should never happen
			Com_Printf ("Delta from invalid frame (not supposed to happen!).\n");
		} else if ( old->messageNum != newSnap.deltaNum ) {
			// The frame that the server did the delta from
			// is too old, so we can't reconstruct it properly.
			Com_Printf ("Delta frame too old.\n");
		} else if ( cl.parseEntitiesNum - old->parseEntitiesNum > cl.parseEntities.maxElements - MAX_SNAPSHOT_ENTITIES * CL_MAX_SPLITVIEW ) {
			Com_Printf ("Delta parseEntitiesNum too old.\n");
		} else {
			newSnap.valid = qtrue;	// valid delta parse
		}
	}

	DA_Clear( &cl.tempSnapshotPS );

	// read playerinfo
	SHOWNET( msg, "playerstate" );

	newSnap.numPSs = MSG_ReadByte( msg );
	if (newSnap.numPSs > MAX_SPLITVIEW) {
		Com_DPrintf(S_COLOR_YELLOW "Warning: Got numPSs as %d (max=%d)\n", newSnap.numPSs, MAX_SPLITVIEW);
		newSnap.numPSs = MAX_SPLITVIEW;
	}

	for (i = 0; i < MAX_SPLITVIEW; i++) {
		newSnap.lcIndex[i] = MSG_ReadByte( msg );
		newSnap.clientNums[i] = MSG_ReadByte( msg );

		// -1 gets converted to 255 should be set to -1 (and so should all invalid values)
		if ( newSnap.lcIndex[i] >= newSnap.numPSs || newSnap.clientNums[i] >= MAX_CLIENTS ) {
			newSnap.lcIndex[i] = -1;
			newSnap.clientNums[i] = -1;
		}

		// read areamask
		len = MSG_ReadByte( msg );

		if(len > sizeof(newSnap.areamask[0]))
		{
			Com_Error (ERR_DROP,"CL_ParseSnapshot: Invalid size %d for areamask", len);
			return;
		}

		MSG_ReadData( msg, &newSnap.areamask[i], len);
	}

	for (i = 0; i < MAX_SPLITVIEW; i++) {
		// Read player states
		if (newSnap.lcIndex[i] != -1) {
			newPS = (sharedPlayerState_t *) DA_ElementPointer( cl.tempSnapshotPS, newSnap.lcIndex[i] );

			if ( old && old->valid && old->lcIndex[i] != -1 ) {
				oldPS = (sharedPlayerState_t *) DA_ElementPointer( old->playerStates, old->lcIndex[i] );

				MSG_ReadDeltaPlayerstate( msg, oldPS, newPS, newSnap.clientNums[i] );
			} else {
				MSG_ReadDeltaPlayerstate( msg, NULL, newPS, newSnap.clientNums[i] );
			}
		}

		// Server added or removed local client
		if ( old && old->clientNums[i] != newSnap.clientNums[i] ) {
			CL_LocalClientRemoved( i );

			if ( newSnap.clientNums[i] != -1 ) {
				CL_LocalClientAdded( i, newSnap.clientNums[i] );
			}
		}
	}

	// read packet entities
	SHOWNET( msg, "packet entities" );
	CL_ParsePacketEntities( msg, old, &newSnap );

	// if not valid, dump the entire thing now that it has
	// been properly read
	if ( !newSnap.valid ) {
		return;
	}

	// clear the valid flags of any snapshots between the last
	// received and this one, so if there was a dropped packet
	// it won't look like something valid to delta from next
	// time we wrap around in the buffer
	oldMessageNum = cl.snap.messageNum + 1;

	if ( newSnap.messageNum - oldMessageNum >= PACKET_BACKUP ) {
		oldMessageNum = newSnap.messageNum - ( PACKET_BACKUP - 1 );
	}
	for ( ; oldMessageNum < newSnap.messageNum ; oldMessageNum++ ) {
		cl.snapshots[oldMessageNum & PACKET_MASK].valid = qfalse;
	}

	// copy player states from temp to snapshot
	DA_Copy( cl.tempSnapshotPS, &cl.snapshots[newSnap.messageNum & PACKET_MASK].playerStates );
	newSnap.playerStates = cl.snapshots[newSnap.messageNum & PACKET_MASK].playerStates;

	// copy to the current good spot
	cl.snap = newSnap;
	cl.snap.ping = 999;
	// calculate ping time
	for ( i = 0 ; i < PACKET_BACKUP ; i++ ) {
		packetNum = ( clc.netchan.outgoingSequence - 1 - i ) & PACKET_MASK;
		newPS = (sharedPlayerState_t *) DA_ElementPointer( cl.snap.playerStates, 0 );
		if ( newPS->commandTime >= cl.outPackets[ packetNum ].p_serverTime ) {
			cl.snap.ping = cls.realtime - cl.outPackets[ packetNum ].p_realtime;
			break;
		}
	}
	// save the frame off in the backup array for later delta comparisons
	cl.snapshots[cl.snap.messageNum & PACKET_MASK] = cl.snap;

	if (cl_shownet->integer == 3) {
		Com_Printf( "   snapshot:%i  delta:%i  ping:%i\n", cl.snap.messageNum,
		cl.snap.deltaNum, cl.snap.ping );
	}

	cl.newSnapshots = qtrue;
}


//=====================================================================

int cl_connectedToCheatServer;

/*
==================
CL_SystemInfoChanged

The systeminfo configstring has been changed, so parse
new information out of it.  This will happen at every
gamestate, and possibly during gameplay.
==================
*/
void CL_SystemInfoChanged( void ) {
	char			*systemInfo;
	const char		*s, *t;
	char			key[BIG_INFO_KEY];
	char			value[BIG_INFO_VALUE];
	qboolean		gameSet;

	systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	// NOTE TTimo:
	// when the serverId changes, any further messages we send to the server will use this new serverId
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=475
	// in some cases, outdated cp commands might get sent with this news serverId
	cl.serverId = atoi( Info_ValueForKey( systemInfo, "sv_serverid" ) );

#ifdef USE_VOIP
	s = Info_ValueForKey( systemInfo, "sv_voip" );
	clc.voipEnabled = atoi(s);
#endif

	// don't set any vars when playing a demo
	if ( clc.demoplaying ) {
		return;
	}

	s = Info_ValueForKey( systemInfo, "sv_cheats" );
	cl_connectedToCheatServer = atoi( s );
	if ( !cl_connectedToCheatServer ) {
		Cvar_SetCheatState();
	}

	// check pure server string
	s = Info_ValueForKey( systemInfo, "sv_paks" );
	t = Info_ValueForKey( systemInfo, "sv_pakNames" );
	FS_PureServerSetLoadedPaks( s, t );

	s = Info_ValueForKey( systemInfo, "sv_referencedPaks" );
	t = Info_ValueForKey( systemInfo, "sv_referencedPakNames" );
	FS_PureServerSetReferencedPaks( s, t );

	gameSet = qfalse;
	// scan through all the variables in the systeminfo and locally set cvars to match
	s = systemInfo;
	while ( s ) {
		Info_NextPair( &s, key, value );
		if ( !key[0] ) {
			break;
		}
		
		// ehw!
		if (!Q_stricmp(key, "fs_game"))
		{
			if(FS_CheckDirTraversal(value))
			{
				Com_Printf(S_COLOR_YELLOW "WARNING: Server sent invalid fs_game value %s\n", value);
				continue;
			}
				
			gameSet = qtrue;
		}

		Cvar_Server_Set(key, value);
	}
	// game folder must be set
	if ( !gameSet ) {
		Com_Error( ERR_DROP, "fs_game not set on server" );
	}
}

/*
==================
CL_ParseServerInfo
==================
*/
static void CL_ParseServerInfo(void)
{
	const char *serverInfo;

	serverInfo = cl.gameState.stringData
		+ cl.gameState.stringOffsets[ CS_SERVERINFO ];

	clc.sv_allowDownload = atoi(Info_ValueForKey(serverInfo,
		"sv_allowDownload"));
	Q_strncpyz(clc.sv_dlURL,
		Info_ValueForKey(serverInfo, "sv_dlURL"),
		sizeof(clc.sv_dlURL));
}

/*
==================
CL_ParseGamestate
==================
*/
void CL_ParseGamestate( msg_t *msg ) {
	int				i;
	int				newnum;
	int				cmd;
	char			*s;
	char oldGame[MAX_QPATH];

	Con_Close();

	clc.connectPacketCount = 0;

	// wipe local client state
	CL_ClearState();

	// a gamestate always marks a server command sequence
	clc.serverCommandSequence = MSG_ReadLong( msg );

	// parse all the configstrings and baselines
	cl.gameState.dataCount = 1;	// leave a 0 at the beginning for uninitialized configstrings
	while ( 1 ) {
		cmd = MSG_ReadByte( msg );

		if ( cmd == svc_EOF ) {
			break;
		}
		
		if ( cmd == svc_configstring ) {
			int		len;

			i = MSG_ReadShort( msg );
			if ( i < 0 || i >= MAX_CONFIGSTRINGS ) {
				Com_Error( ERR_DROP, "configstring > MAX_CONFIGSTRINGS" );
			}
			s = MSG_ReadBigString( msg );
			len = strlen( s );

			if ( len + 1 + cl.gameState.dataCount > MAX_GAMESTATE_CHARS ) {
				Com_Error( ERR_DROP, "MAX_GAMESTATE_CHARS exceeded" );
			}

			// append it to the gameState string buffer
			cl.gameState.stringOffsets[ i ] = cl.gameState.dataCount;
			Com_Memcpy( cl.gameState.stringData + cl.gameState.dataCount, s, len + 1 );
			cl.gameState.dataCount += len + 1;
		} else {
			Com_Error( ERR_DROP, "CL_ParseGamestate: bad command byte" );
		}
	}

	// read clientNums
	for ( i = 0; i < MAX_SPLITVIEW; i++ ) {
		newnum = MSG_ReadLong(msg);
		if (newnum >= 0 && newnum < MAX_CLIENTS)
			CL_LocalClientAdded(i, newnum);
		else
			CL_LocalClientRemoved(i);
	}

	// save old gamedir
	Cvar_VariableStringBuffer("fs_game", oldGame, sizeof(oldGame));

	// parse useful values out of CS_SERVERINFO
	CL_ParseServerInfo();

	// parse serverId and other cvars
	CL_SystemInfoChanged();

	// stop recording now so the demo won't have an unnecessary level load at the end.
	if(cl_autoRecordDemo->integer && clc.demorecording)
		CL_StopRecord_f();
	
	// reinitialize the filesystem if the game directory has changed
	if(!cl_oldGameSet && (Cvar_Flags("fs_game") & CVAR_MODIFIED))
	{
		cl_oldGameSet = qtrue;
		Q_strncpyz(cl_oldGame, oldGame, sizeof(cl_oldGame));
	}

	FS_ConditionalRestart(qfalse);

	// This used to call CL_StartHunkUsers, but now we enter the download state before loading the
	// cgame
	CL_InitDownloads();

	// make sure the game starts
	Cvar_Set( "cl_paused", "0" );
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline( msg_t *msg ) {
	sharedEntityState_t	*es;
	int					newnum;

	if ( !cgvm ) {
		Com_Error( ERR_DROP, "Received unexpected baseline" );
	}

	if ( !cl.entityBaselines.pointer ) {
		Com_Error( ERR_DROP, "cgame needs to call trap_SetNetFields" );
	}

	newnum = MSG_ReadBits( msg, GENTITYNUM_BITS );
	if ( newnum < 0 || newnum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "Baseline number out of range: %i", newnum );
	}
	es = DA_ElementPointer( cl.entityBaselines, newnum );
	MSG_ReadDeltaEntity( msg, NULL, es, newnum );
}


//=====================================================================

/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload ( msg_t *msg ) {
	int		size;
	unsigned char data[MAX_MSGLEN];
	uint16_t block;

	if (!*clc.downloadTempName) {
		Com_Printf("Server sending download, but no download was requested\n");
		CL_AddReliableCommand("stopdl", qfalse);
		return;
	}

	// read the data
	block = MSG_ReadShort ( msg );

	if(!block && !clc.downloadBlock)
	{
		// block zero is special, contains file size
		clc.downloadSize = MSG_ReadLong ( msg );

		Cvar_SetValue( "cl_downloadSize", clc.downloadSize );

		if (clc.downloadSize < 0)
		{
			Com_Error( ERR_DROP, "%s", MSG_ReadString( msg ) );
			return;
		}
	}

	size = MSG_ReadShort ( msg );
	if (size < 0 || size > sizeof(data))
	{
		Com_Error(ERR_DROP, "CL_ParseDownload: Invalid size %d for download chunk", size);
		return;
	}
	
	MSG_ReadData(msg, data, size);

	if((clc.downloadBlock & 0xFFFF) != block)
	{
		Com_DPrintf( "CL_ParseDownload: Expected block %d, got %d\n", (clc.downloadBlock & 0xFFFF), block);
		return;
	}

	// open the file if not opened yet
	if (!clc.download)
	{
		clc.download = FS_SV_FOpenFileWrite( clc.downloadTempName );

		if (!clc.download) {
			Com_Printf( "Could not create %s\n", clc.downloadTempName );
			CL_AddReliableCommand("stopdl", qfalse);
			CL_NextDownload();
			return;
		}
	}

	if (size)
		FS_Write( data, size, clc.download );

	CL_AddReliableCommand(va("nextdl %d", clc.downloadBlock), qfalse);
	clc.downloadBlock++;

	clc.downloadCount += size;

	// So UI gets access to it
	Cvar_SetValue( "cl_downloadCount", clc.downloadCount );

	if (!size) { // A zero length block means EOF
		if (clc.download) {
			FS_FCloseFile( clc.download );
			clc.download = 0;

			// rename the file
			FS_SV_Rename ( clc.downloadTempName, clc.downloadName, qfalse );
		}

		// send intentions now
		// We need this because without it, we would hold the last nextdl and then start
		// loading right away.  If we take a while to load, the server is happily trying
		// to send us that last block over and over.
		// Write it twice to help make sure we acknowledge the download
		CL_WritePacket();
		CL_WritePacket();

		// get another file if needed
		CL_NextDownload ();
	}
}

#ifdef USE_VOIP
static
qboolean CL_ShouldIgnoreVoipSender(int sender)
{
	int i;

	if (!cl_voip->integer)
		return qtrue;  // VoIP is disabled.
	else if (clc.voipMuteAll)
		return qtrue;  // all channels are muted with extreme prejudice.
	else if (clc.voipIgnore[sender])
		return qtrue;  // just ignoring this guy.
	else if (clc.voipGain[sender] == 0.0f)
		return qtrue;  // too quiet to play.

	if (!clc.demoplaying) {
		for ( i = 0; i < CL_MAX_SPLITVIEW; i++ ) {
			if (sender == clc.clientNums[i])
				return qtrue;  // ignore own voice (unless playing back a demo).
		}
	}

	return qfalse;
}

/*
=====================
CL_PlayVoip

Play raw data
=====================
*/

static void CL_PlayVoip(int sender, int samplecnt, const byte *data, int flags)
{
	if(flags & VOIP_DIRECT)
	{
		S_RawSamples(sender + 1, samplecnt, clc.speexSampleRate, 2, 1,
	             data, clc.voipGain[sender], -1);
	}

	if(flags & VOIP_SPATIAL)
	{
		S_RawSamples(sender + MAX_CLIENTS + 1, samplecnt, clc.speexSampleRate, 2, 1,
	             data, 1.0f, sender);
	}
}

/*
=====================
CL_ParseVoip

A VoIP message has been received from the server
=====================
*/
static
void CL_ParseVoip ( msg_t *msg ) {
	static short decoded[4096];  // !!! FIXME: don't hardcode.

	const int sender = MSG_ReadShort(msg);
	const int generation = MSG_ReadByte(msg);
	const int sequence = MSG_ReadLong(msg);
	const int frames = MSG_ReadByte(msg);
	const int packetsize = MSG_ReadShort(msg);
	const int flags = MSG_ReadBits(msg, VOIP_FLAGCNT);
	char encoded[1024];
	int seqdiff = sequence - clc.voipIncomingSequence[sender];
	int written = 0;
	float voipPower = 0.0f;
	int i, j;

	Com_DPrintf("VoIP: %d-byte packet from client %d\n", packetsize, sender);

	if (sender < 0)
		return;   // short/invalid packet, bail.
	else if (generation < 0)
		return;   // short/invalid packet, bail.
	else if (sequence < 0)
		return;   // short/invalid packet, bail.
	else if (frames < 0)
		return;   // short/invalid packet, bail.
	else if (packetsize < 0)
		return;   // short/invalid packet, bail.

	if (packetsize > sizeof (encoded)) {  // overlarge packet?
		int bytesleft = packetsize;
		while (bytesleft) {
			int br = bytesleft;
			if (br > sizeof (encoded))
				br = sizeof (encoded);
			MSG_ReadData(msg, encoded, br);
			bytesleft -= br;
		}
		return;   // overlarge packet, bail.
	}

	if (!clc.speexInitialized) {
		MSG_ReadData(msg, encoded, packetsize);  // skip payload.
		return;   // can't handle VoIP without libspeex!
	} else if (sender >= MAX_CLIENTS) {
		MSG_ReadData(msg, encoded, packetsize);  // skip payload.
		return;   // bogus sender.
	} else if (CL_ShouldIgnoreVoipSender(sender)) {
		MSG_ReadData(msg, encoded, packetsize);  // skip payload.
		return;   // Channel is muted, bail.
	}

	// !!! FIXME: make sure data is narrowband? Does decoder handle this?

	Com_DPrintf("VoIP: packet accepted!\n");

	// This is a new "generation" ... a new recording started, reset the bits.
	if (generation != clc.voipIncomingGeneration[sender]) {
		Com_DPrintf("VoIP: new generation %d!\n", generation);
		speex_bits_reset(&clc.speexDecoderBits[sender]);
		clc.voipIncomingGeneration[sender] = generation;
		seqdiff = 0;
	} else if (seqdiff < 0) {   // we're ahead of the sequence?!
		// This shouldn't happen unless the packet is corrupted or something.
		Com_DPrintf("VoIP: misordered sequence! %d < %d!\n",
		            sequence, clc.voipIncomingSequence[sender]);
		// reset the bits just in case.
		speex_bits_reset(&clc.speexDecoderBits[sender]);
		seqdiff = 0;
	} else if (seqdiff * clc.speexFrameSize * 2 >= sizeof (decoded)) { // dropped more than we can handle?
		// just start over.
		Com_DPrintf("VoIP: Dropped way too many (%d) frames from client #%d\n",
		            seqdiff, sender);
		speex_bits_reset(&clc.speexDecoderBits[sender]);
		seqdiff = 0;
	}

	if (seqdiff != 0) {
		Com_DPrintf("VoIP: Dropped %d frames from client #%d\n",
		            seqdiff, sender);
		// tell speex that we're missing frames...
		for (i = 0; i < seqdiff; i++) {
			assert((written + clc.speexFrameSize) * 2 < sizeof (decoded));
			speex_decode_int(clc.speexDecoder[sender], NULL, decoded + written);
			written += clc.speexFrameSize;
		}
	}

	for (i = 0; i < frames; i++) {
		const int16_t *sampptr;
		const int len = MSG_ReadByte(msg);
		if (len < 0) {
			Com_DPrintf("VoIP: Short packet!\n");
			break;
		}
		MSG_ReadData(msg, encoded, len);

		// shouldn't happen, but just in case...
		if ((written + clc.speexFrameSize) * 2 > sizeof (decoded)) {
			Com_DPrintf("VoIP: playback %d bytes, %d samples, %d frames\n",
			            written * 2, written, i);
			
			CL_PlayVoip(sender, written, (const byte *) decoded, flags);
			written = 0;
		}

		speex_bits_read_from(&clc.speexDecoderBits[sender], encoded, len);
		speex_decode_int(clc.speexDecoder[sender],
		                 &clc.speexDecoderBits[sender], decoded + written);

		#if 0
		static FILE *encio = NULL;
		if (encio == NULL) encio = fopen("voip-incoming-encoded.bin", "wb");
		if (encio != NULL) { fwrite(encoded, len, 1, encio); fflush(encio); }
		static FILE *decio = NULL;
		if (decio == NULL) decio = fopen("voip-incoming-decoded.bin", "wb");
		if (decio != NULL) { fwrite(decoded+written, clc.speexFrameSize*2, 1, decio); fflush(decio); }
		#endif

		sampptr = (const int16_t *)decoded + written;

		// calculate the "power" of this packet...
		for (j = 0; j < clc.speexFrameSize; j++) {
			const float flsamp = (float) sampptr[j];
			const float s = fabs(flsamp);
			voipPower += s * s;
		}

		written += clc.speexFrameSize;
	}

	Com_DPrintf("VoIP: playback %d bytes, %d samples, %d frames\n",
	            written * 2, written, i);

	if(written > 0)
		CL_PlayVoip(sender, written, (const byte *) decoded, flags);

	clc.voipPower[sender] = (voipPower / (32768.0f * 32768.0f *
			                 ((float) (clc.speexFrameSize * i)))) *
			                 100.0f;

	clc.voipIncomingSequence[sender] = sequence + frames;
	clc.voipLastPacketTime[sender] = cl.serverTime;
}
#endif


/*
=====================
CL_ParseCommandString

Command strings are just saved off until cgame asks for them
when it transitions a snapshot
=====================
*/
void CL_ParseCommandString( msg_t *msg ) {
	char	*s;
	int		seq;
	int		index;

	seq = MSG_ReadLong( msg );
	s = MSG_ReadString( msg );

	// see if we have already executed stored it off
	if ( clc.serverCommandSequence >= seq ) {
		return;
	}
	clc.serverCommandSequence = seq;

	index = seq & (MAX_RELIABLE_COMMANDS-1);
	Q_strncpyz( clc.serverCommands[ index ], s, sizeof( clc.serverCommands[ index ] ) );
}


/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage( msg_t *msg ) {
	int			cmd;

	if ( cl_shownet->integer == 1 ) {
		Com_Printf ("%i ",msg->cursize);
	} else if ( cl_shownet->integer >= 2 ) {
		Com_Printf ("------------------\n");
	}

	MSG_Bitstream(msg);

	// get the reliable sequence acknowledge number
	clc.reliableAcknowledge = MSG_ReadLong( msg );
	// 
	if ( clc.reliableAcknowledge < clc.reliableSequence - MAX_RELIABLE_COMMANDS ) {
		clc.reliableAcknowledge = clc.reliableSequence;
	}

	//
	// parse the message
	//
	while ( 1 ) {
		if ( msg->readcount > msg->cursize ) {
			Com_Error (ERR_DROP,"CL_ParseServerMessage: read past end of server message");
			break;
		}

		cmd = MSG_ReadByte( msg );

		if (cmd == svc_EOF) {
			SHOWNET( msg, "END OF MESSAGE" );
			break;
		}

		if ( cl_shownet->integer >= 2 ) {
			if ( (cmd < 0) || (!svc_strings[cmd]) ) {
				Com_Printf( "%3i:BAD CMD %i\n", msg->readcount-1, cmd );
			} else {
				SHOWNET( msg, svc_strings[cmd] );
			}
		}
	
	// other commands
		switch ( cmd ) {
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message");
			break;			
		case svc_nop:
			break;
		case svc_serverCommand:
			CL_ParseCommandString( msg );
			break;
		case svc_gamestate:
			CL_ParseGamestate( msg );
			break;
		case svc_baseline:
			CL_ParseBaseline( msg );
			break;
		case svc_snapshot:
			CL_ParseSnapshot( msg );
			break;
		case svc_download:
			CL_ParseDownload( msg );
			break;
		case svc_voip:
#ifdef USE_VOIP
			CL_ParseVoip( msg );
#endif
			break;
		}
	}
}


