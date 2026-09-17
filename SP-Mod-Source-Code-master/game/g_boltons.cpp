#include "b_local.h"
#include "boltOns.h"
#include "../../code/win32/xb_log.h"
//#include "b_public.h"

//extern void G_ParseBoltOnList( boltOn_t *boltOn );
extern gentity_t *G_CreateObject ( gentity_t *owner, vec3_t origin, vec3_t angles, int modelIndex, int frame, trType_t trType );
extern qboolean G_ParseLiteral( char **data, const char *string );
extern qboolean G_ParseString( char **data, char **s ) ;
extern qboolean G_ParseInt( char **data, int *i );
extern qboolean G_ParseFloat( char **data, float *f );

#define BOLTON_NONE MAX_BOLT_ONS

boltOn_t	knownBoltOns[MAX_GAME_BOLTONS];
int			numBoltOns;
char	boltOnList[0x10000];

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
static qboolean s_stefxHelmetMirrorGuard = qfalse;

extern "C" volatile unsigned int g_SPXBHelmetBoltOnLoadLen;
extern "C" volatile unsigned int g_SPXBHelmetBoltOnCount;
extern "C" volatile unsigned int g_SPXBHelmetBoltOnHelmetIndex;
extern "C" volatile unsigned int g_SPXBHelmetAddAttempts;
extern "C" volatile unsigned int g_SPXBHelmetAddKnownIndex;
extern "C" volatile unsigned int g_SPXBHelmetAddFailCode;

static qboolean STEFX_BoltOnNameContainsNoCase( const char *name, const char *needle )
{
	int needleLen;

	if ( !name || !needle || !needle[0] )
	{
		return qfalse;
	}

	needleLen = strlen( needle );
	while ( *name )
	{
		if ( !Q_stricmpn( name, needle, needleLen ) )
		{
			return qtrue;
		}
		++name;
	}

	return qfalse;
}

static qboolean STEFX_BoltOnIsHelmet( const char *boltOnName )
{
	return (qboolean)( boltOnName &&
		( !Q_stricmp( boltOnName, "helmet" ) || !Q_stricmp( boltOnName, "helmet_lhand" ) ) );
}

static qboolean STEFX_BoltOnIsWornHelmet( const char *boltOnName )
{
	return (qboolean)( boltOnName && !Q_stricmp( boltOnName, "helmet" ) );
}

static qboolean STEFX_BoltOnEntityIsMunroScriptTarget( gentity_t *ent )
{
	if ( !ent )
	{
		return qfalse;
	}

	if ( ent == &g_entities[0] )
	{
		return qtrue;
	}

	if ( STEFX_BoltOnNameContainsNoCase( ent->targetname, "munro" ) ||
		STEFX_BoltOnNameContainsNoCase( ent->script_targetname, "munro" ) ||
		STEFX_BoltOnNameContainsNoCase( ent->NPC_type, "munro" ) )
	{
		return qtrue;
	}

	if ( ent->client && STEFX_BoltOnNameContainsNoCase( ent->client->squadname, "munro" ) )
	{
		return qtrue;
	}

	return qfalse;
}

static void STEFX_BoltOnTraceHelmetOp( const char *op, gentity_t *ent, const char *boltOnName, int slot, int knownIndex )
{
	static int s_logBudget = 512;
	const char *squadName;
	int p2EntNum;

	if ( s_logBudget <= 0 || !STEFX_BoltOnIsHelmet( boltOnName ) )
	{
		return;
	}

	squadName = ( ent && ent->client && ent->client->squadname ) ? ent->client->squadname : "<null>";
	p2EntNum = gi.Cvar_VariableIntegerValue( "stefx_splitScreenP2Entity" );

	XBLog_Writef( "STEFX: helmet bolton %s trace ent=%d inuse=%d class='%s' target='%s' scriptTarget='%s' npc='%s' squad='%s' client=%d munroTarget=%d boltOn='%s' slot=%d index=%d active=%d p2Cvar=%d time=%d",
		op ? op : "<null>",
		ent ? ent->s.number : -1,
		ent ? ent->inuse : 0,
		ent && ent->classname ? ent->classname : "<null>",
		ent && ent->targetname ? ent->targetname : "<null>",
		ent && ent->script_targetname ? ent->script_targetname : "<null>",
		ent && ent->NPC_type ? ent->NPC_type : "<null>",
		squadName,
		( ent && ent->client ) ? 1 : 0,
		STEFX_BoltOnEntityIsMunroScriptTarget( ent ),
		boltOnName ? boltOnName : "<null>",
		slot,
		knownIndex,
		ent ? ent->activeBoltOn : -1,
		p2EntNum,
		level.time );
	--s_logBudget;
}

static gentity_t *STEFX_BoltOnGetP2( void )
{
	int split = gi.Cvar_VariableIntegerValue( "stefx_splitScreen" );
	int players = gi.Cvar_VariableIntegerValue( "stefx_splitScreenPlayers" );
	int entNum = gi.Cvar_VariableIntegerValue( "stefx_splitScreenP2Entity" );

	if ( !split || players < 2 || entNum <= 0 || entNum >= MAX_GENTITIES )
	{
		return NULL;
	}

	if ( !g_entities[entNum].inuse || !g_entities[entNum].client )
	{
		return NULL;
	}

	return &g_entities[entNum];
}

static void STEFX_BoltOnMirrorHelmetToPlayer( gentity_t *player, gentity_t *source, const char *boltOnName, qboolean add )
{
	byte slot;

	if ( !player || !player->client || player == source )
	{
		return;
	}

	if ( add )
	{
		slot = G_BoltOnNumberForName( player, boltOnName );
		if ( slot >= MAX_BOLT_ONS )
		{
			slot = G_AddBoltOn( player, boltOnName );
			if ( slot >= MAX_BOLT_ONS )
			{
				slot = G_BoltOnNumberForName( player, boltOnName );
			}
		}
		if ( slot < MAX_BOLT_ONS )
		{
			player->activeBoltOn = slot;
		}
	}
	else
	{
		G_RemoveBoltOn( player, boltOnName );
	}
}

static void STEFX_BoltOnMirrorPlayerHelmet( gentity_t *source, const char *boltOnName, qboolean add )
{
	static int s_logBudget = 96;
	gentity_t *p1;
	gentity_t *p2;

	if ( s_stefxHelmetMirrorGuard || !STEFX_BoltOnIsWornHelmet( boltOnName ) ||
		!STEFX_BoltOnEntityIsMunroScriptTarget( source ) )
	{
		return;
	}

	p1 = &g_entities[0];
	p2 = STEFX_BoltOnGetP2();

	s_stefxHelmetMirrorGuard = qtrue;
	STEFX_BoltOnMirrorHelmetToPlayer( p1, source, boltOnName, add );
	STEFX_BoltOnMirrorHelmetToPlayer( p2, source, boltOnName, add );
	s_stefxHelmetMirrorGuard = qfalse;

	if ( s_logBudget > 0 )
	{
		XBLog_Writef( "STEFX: helmet bolton mirror %s source=%d target='%s' scriptTarget='%s' npc='%s' boltOn='%s' p2=%d time=%d",
			add ? "add" : "remove",
			source ? source->s.number : -1,
			source && source->targetname ? source->targetname : "<null>",
			source && source->script_targetname ? source->script_targetname : "<null>",
			source && source->NPC_type ? source->NPC_type : "<null>",
			boltOnName ? boltOnName : "<null>",
			p2 ? p2->s.number : -1,
			level.time );
		--s_logBudget;
	}
}
#endif

int G_GetBoltOnIndex( const char *boltOnName ) 
{
	for ( int i = 0; i < numBoltOns; i++ )
	{
		if ( Q_stricmp( knownBoltOns[i].name, boltOnName ) == 0 )
		{//found it!
			return i;
		}
	}

#ifdef _XBOX
	gi.Printf( S_COLOR_YELLOW"WARNING: Unknown boltOn name: %s; skipping on Xbox\n", boltOnName );
	return MAX_GAME_BOLTONS;
#else
	G_Error( "ERROR: Unknown boltOn name: %s!\n", boltOnName );

	return MAX_GAME_BOLTONS;
#endif
}

void G_RegisterBoltOns (void)
{
	boltOn_t *boltOn = NULL;
	char	*token = NULL;
	char	*value;
	char	*p;
	int		n;
	float	f;

	p = boltOnList;
	COM_BeginParseSession();

	while(1)
	{
		if ( numBoltOns >= MAX_GAME_BOLTONS )
		{
			G_Error( "ERROR:  Too many boltOns (%d) in boltOn.cfg!\n", MAX_GAME_BOLTONS );
			return;
		}

		boltOn = &knownBoltOns[numBoltOns];

		while ( 1 )
		{
			token = COM_ParseExt( &p, qtrue );
			if ( token[0] == 0 )
			{
				//reached EOF
				return;
			}
			else if ( token[0] == '{' )
			{
				//WTF?  Unnamed boltOn?
				continue;
			}
			else if ( !p ) 
			{
				//???
				return;
			}
			else
			{
				//found a boltOn name
				break;
			}
		}

		Q_strncpyz( (char *)&boltOn->name, token, sizeof(boltOn->name), qtrue );

		if ( G_ParseLiteral( &p, "{" ) ) 
		{//couldn't find an open brace "{"!
			return;
		}

		//Set default scale
		VectorSet(boltOn->model.scaleXYZ, 100, 100, 100);
			
		// parse the boltOn info block
		while ( 1 ) 
		{
			token = COM_ParseExt( &p, qtrue );
			if ( !token[0] ) 
			{
				gi.Printf( S_COLOR_RED"ERROR: unexpected EOF while parsing '%s'\n", boltOn->name );
				return;
			}

			if ( !Q_stricmp( token, "}" ) ) 
			{
				//reached end of boltOn, on to next
				break;
			}

			// modelName
			if ( !Q_stricmp( token, "modelName" ) ) 
			{
				if ( G_ParseString( &p, &value ) ) 
				{
					continue;
				}
				boltOn->model.modelIndex = G_ModelIndex( value );
				continue;
			}
			
			// target model to attach to, head, torso, legs or weapon
			if ( !Q_stricmp( token, "targetModel" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					continue;
				}

				if ( n < 0 || n >= NUM_TARGET_MODELS )
				{
					gi.Printf("WARNING boltOn %s has targetModel out of range (<0 or >= %d)\n", boltOn->name, NUM_TARGET_MODELS);
					return;
				}

				boltOn->targetModel = (targetModel_t)n;			
				continue;
			}

			// target tag to attach to
			if ( !Q_stricmp( token, "targetTag" ) ) 
			{
				if ( G_ParseString( &p, &value ) ) 
				{
					continue;
				}
				//boltOn->targetTag = G_NewString(value);
				Q_strncpyz( boltOn->targetTag, value, sizeof(boltOn->targetTag), qtrue);
				continue;
			}

			// pitch offset
			if ( !Q_stricmp( token, "pitchOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->angleOffsets[0] = f;
				continue;
			}

			// yaw offset
			if ( !Q_stricmp( token, "yawOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->angleOffsets[1] = f;
				continue;
			}

			// roll offset
			if ( !Q_stricmp( token, "rollOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->angleOffsets[2] = f;
				continue;
			}

			// x offset
			if ( !Q_stricmp( token, "xOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->originOffsets[0] = f;
				continue;
			}

			// y offset
			if ( !Q_stricmp( token, "yOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->originOffsets[1] = f;
				continue;
			}

			// z offset
			if ( !Q_stricmp( token, "zOffset" ) ) 
			{
				if ( G_ParseFloat( &p, &f ) ) 
				{
					continue;
				}
				boltOn->originOffsets[2] = f;
				continue;
			}

			// Uniform XYZ scale
			if ( !Q_stricmp( token, "scale" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.scaleXYZ[0] = boltOn->model.scaleXYZ[1] = boltOn->model.scaleXYZ[2] = n;
				continue;
			}

			//X scale
			if ( !Q_stricmp( token, "scaleX" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.scaleXYZ[0] = n;
				continue;
			}

			//Y scale
			if ( !Q_stricmp( token, "scaleY" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.scaleXYZ[1] = n;
				continue;
			}

			//Z scale
			if ( !Q_stricmp( token, "scaleZ" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.scaleXYZ[2] = n;
				continue;
			}
			
			// Uniform RGB tint
			if ( !Q_stricmp( token, "tint" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.customRGB[0] = boltOn->model.customRGB[1] = boltOn->model.customRGB[2] = n;
				continue;
			}

			//red tint
			if ( !Q_stricmp( token, "tintRed" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.customRGB[0] = n;
				continue;
			}

			//Green tint
			if ( !Q_stricmp( token, "tintGreen" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.customRGB[1] = n;
				continue;
			}

			//Blue tint
			if ( !Q_stricmp( token, "tintBlue" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.customRGB[2] = n;
				continue;
			}

			//alpha
			if ( !Q_stricmp( token, "alpha" ) ) 
			{
				if ( G_ParseInt( &p, &n ) ) 
				{
					SkipRestOfLine( &p );
					continue;
				}
				if ( n < 0 ) 
				{
					gi.Printf(  "bad %s in boltOn '%s'\n", token, boltOn->name );
					continue;
				}
				boltOn->model.customAlpha = n;
				continue;
			}
		}

		numBoltOns++;
	}
}

void G_ClearBoltOnInfo( boltOnInfo_t *bOInfo )
{
	bOInfo->index = MAX_GAME_BOLTONS;
	VectorClear( bOInfo->lastOrigin );
	VectorClear( bOInfo->lastAngles );
	bOInfo->frame = bOInfo->startFrame = bOInfo->endFrame = 0;
	bOInfo->loopAnim = qfalse;
}

void G_LoadBoltOns( void ) 
{
	int			len;
	const char	filename[] = "ext_data/boltOns.cfg";
	char		*buffer;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	int helmetIndex = MAX_GAME_BOLTONS;
#endif

	// The Xbox game module stays resident across saves and deck transitions.
	// Rebuild the level-owned attachment table instead of appending it again.
	numBoltOns = 0;
	memset(knownBoltOns, 0, sizeof(knownBoltOns));
	boltOnList[0] = '\0';
	gi.Printf( "Parsing %s\n", filename );
	len = gi.FS_ReadFile( filename, (void **) &buffer );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	g_SPXBHelmetBoltOnLoadLen = ( len >= 0 ) ? (unsigned int)len : 0xFFFFFFFF;
#endif
	if ( len == -1 ) 
	{
		gi.Printf( "file not found\n" );
		return;
	}

	if ( len >= sizeof( boltOnList ) ) 
	{
		G_Error( "ext_data/boltOns.cfg is too large" );
	}
	strncpy( boltOnList, buffer, sizeof( boltOnList ) - 1 );
	gi.FS_FreeFile( buffer );

	G_RegisterBoltOns();
#ifdef _XBOX
	int scoutbotIndex = -1;
	int warriorbotIndex = -1;
	for ( int i = 0; i < numBoltOns; i++ )
	{
		if ( Q_stricmp( knownBoltOns[i].name, "helmet" ) == 0 )
		{
			helmetIndex = i;
		}
		if ( Q_stricmp( knownBoltOns[i].name, "headbot_scoutbot" ) == 0 )
		{
			scoutbotIndex = i;
		}
		else if ( Q_stricmp( knownBoltOns[i].name, "headbot_warriorbot" ) == 0 )
		{
			warriorbotIndex = i;
		}
	}
	g_SPXBHelmetBoltOnCount = (unsigned int)numBoltOns;
	g_SPXBHelmetBoltOnHelmetIndex = (unsigned int)helmetIndex;
	gi.Printf( "STEFX: G_LoadBoltOns loaded count=%d first='%s' last='%s' warrior=%d scout=%d\n",
		numBoltOns,
		numBoltOns > 0 ? knownBoltOns[0].name : "<none>",
		numBoltOns > 0 ? knownBoltOns[numBoltOns - 1].name : "<none>",
		warriorbotIndex,
		scoutbotIndex );
#endif
}

byte G_AddBoltOn( gentity_t *ent, const char *boltOnName )
{
	int freeSlot, newIndex;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	qboolean traceHelmet = STEFX_BoltOnIsHelmet( boltOnName );
	if ( traceHelmet )
	{
		g_SPXBHelmetAddAttempts++;
		g_SPXBHelmetAddFailCode = 0;
	}
#endif

	if ( !ent || !boltOnName || !boltOnName[0] )
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( traceHelmet )
		{
			g_SPXBHelmetAddFailCode = 1;
		}
#endif
		return BOLTON_NONE;
	}

	newIndex = G_GetBoltOnIndex( boltOnName );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	if ( traceHelmet )
	{
		g_SPXBHelmetAddKnownIndex = (unsigned int)newIndex;
	}
#endif
	if ( newIndex < 0 || newIndex >= numBoltOns )
	{
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
		if ( traceHelmet )
		{
			g_SPXBHelmetAddFailCode = 2;
		}
#endif
		return BOLTON_NONE;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_BoltOnTraceHelmetOp( "add-begin", ent, boltOnName, -1, newIndex );
#endif

	if ( !ent->client )
	{
		freeSlot = 0;

		G_ClearBoltOnInfo( &ent->boltOn );
		ent->boltOn.index = newIndex;
	}
	else
	{
		for ( freeSlot = 0; freeSlot < MAX_BOLT_ONS; freeSlot++ )
		{
			if ( ent->client->renderInfo.boltOns[freeSlot].index == MAX_GAME_BOLTONS )
			{//Found a free slot
				break;
			}
			else if ( ent->client->renderInfo.boltOns[freeSlot].index == newIndex )
			{
#ifndef FINAL_BUILD
				gi.Printf("WARNING: %s already has boltOn turned on!\n", ent->targetname, boltOnName );
#endif
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
				STEFX_BoltOnTraceHelmetOp( "add-duplicate", ent, boltOnName, freeSlot, newIndex );
				STEFX_BoltOnMirrorPlayerHelmet( ent, boltOnName, qtrue );
#endif
				return freeSlot;
			}
		}

		if ( freeSlot >= MAX_BOLT_ONS )
		{
			gi.Printf("WARNING: %s out of free boltOn slots! (MAX = %d)\n", ent->targetname, MAX_BOLT_ONS );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
			if ( traceHelmet )
			{
				g_SPXBHelmetAddFailCode = 3;
			}
#endif
			return BOLTON_NONE;
		}

		G_ClearBoltOnInfo( &ent->client->renderInfo.boltOns[freeSlot] );
		ent->client->renderInfo.boltOns[freeSlot].index = newIndex;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_BoltOnTraceHelmetOp( "add-applied", ent, boltOnName, freeSlot, newIndex );
	STEFX_BoltOnMirrorPlayerHelmet( ent, boltOnName, qtrue );
#endif

	return freeSlot;
}

void G_RemoveBoltOn( gentity_t *ent, const char *boltOnName )
{
	int	namedIndex;
	
	if ( !ent || !boltOnName || !boltOnName[0] )
	{
		return;
	}

	namedIndex = G_GetBoltOnIndex( boltOnName );
	if ( namedIndex < 0 || namedIndex >= numBoltOns )
	{
		return;
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_BoltOnTraceHelmetOp( "remove-begin", ent, boltOnName, -1, namedIndex );
#endif

	if ( !ent->client )
	{
		if ( ent->boltOn.index == namedIndex )
		{
			G_ClearBoltOnInfo( &ent->boltOn );
		}
	}
	else
	{
		for ( int i = 0; i < MAX_BOLT_ONS; i++ )
		{
			if ( ent->client->renderInfo.boltOns[i].index == namedIndex )
			{//Found it, clear it
				G_ClearBoltOnInfo( &ent->client->renderInfo.boltOns[i] );
			}
		}
	}

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
	STEFX_BoltOnTraceHelmetOp( "remove-applied", ent, boltOnName, -1, namedIndex );
	STEFX_BoltOnMirrorPlayerHelmet( ent, boltOnName, qfalse );
#endif
}

void G_DropBoltOn( gentity_t *ent, const char *boltOnName )
{
	boltOn_t	*boltOn;
	int			namedIndex;

	if ( !ent || !boltOnName || !boltOnName[0] )
	{
		return;
	}

	namedIndex = G_GetBoltOnIndex( boltOnName );
	if ( namedIndex < 0 || namedIndex >= numBoltOns )
	{
		return;
	}

	if ( !ent->client )
	{
		if ( ent->boltOn.index != namedIndex )
		{
			//FIXME: error msg
			return;
		}

		if ( ent->boltOn.index < 0 || ent->boltOn.index >= numBoltOns )
		{
			return;
		}

		boltOn = &knownBoltOns[ent->boltOn.index];
		//FIXME: what about tint, alpha and scale?  And animation?
		gentity_t *newObject = G_CreateObject( ent, ent->boltOn.lastOrigin, ent->boltOn.lastAngles,
			boltOn->model.modelIndex, ent->boltOn.frame, TR_GRAVITY );
		if ( newObject )
		{
			newObject->targetname = G_NewString( va( "%s_%s", ent->targetname, boltOnName ) );
		}

		//Remove it
		G_RemoveBoltOn( ent, boltOnName );
	}
	else
	{
		for ( int i = 0; i < MAX_BOLT_ONS; i++ )
		{
			if ( ent->client->renderInfo.boltOns[i].index < 0 || ent->client->renderInfo.boltOns[i].index >= numBoltOns )
			{
				continue;
			}

			if ( ent->client->renderInfo.boltOns[i].index == namedIndex )
			{//Found it
				boltOn = &knownBoltOns[ent->client->renderInfo.boltOns[i].index];
				//GUH!!! Move these onto ENT!!!
				gentity_t *newObject = G_CreateObject( ent, ent->client->renderInfo.boltOns[i].lastOrigin,
					ent->client->renderInfo.boltOns[i].lastAngles,
					boltOn->model.modelIndex,
					ent->client->renderInfo.boltOns[i].frame, 
					TR_GRAVITY );//TR_STATIONARY );//
				if ( newObject )
				{
					newObject->targetname = G_NewString( va( "%s_%s", ent->targetname, boltOnName ) );
				}
				//FIXME: what about tint, alpha and scale?  And animation?

				//Remove it
				G_RemoveBoltOn( ent, boltOnName );
				//Done
				return;
			}
		}
	}
}

byte G_BoltOnNumberForName( gentity_t *ent, const char *boltOnName )
{
	if ( !ent || !boltOnName || !boltOnName[0] )
	{
		return MAX_BOLT_ONS;
	}

	if ( !ent->client )
	{//non-clients only have one
		return 0;
	}

	for ( int i = 0; i < MAX_BOLT_ONS; i++ )
	{
		int boltOnIndex = ent->client->renderInfo.boltOns[i].index;
		if ( boltOnIndex < 0 || boltOnIndex >= numBoltOns )
		{
			continue;
		}

		if ( !Q_stricmp( boltOnName, knownBoltOns[boltOnIndex].name ) )
		{//Found it, return it
			return i;
		}
	}

	return MAX_BOLT_ONS;
}

void G_InitBoltOnData ( gentity_t *ent )
{
	//UG, crappy here, but need to initialize boltOn index to invalid number
	ent->activeBoltOn = MAX_BOLT_ONS;

	G_ClearBoltOnInfo( &ent->boltOn );
	if ( ent->client )
	{
		for ( int i = 0; i < MAX_BOLT_ONS; i++ )
		{
			G_ClearBoltOnInfo( &ent->client->renderInfo.boltOns[i] );
		}
	}
}
