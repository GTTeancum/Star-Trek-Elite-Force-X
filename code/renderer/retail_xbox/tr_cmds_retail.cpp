//Anything above this #include will be ignored by the compiler
#include "../../server/exe_headers.h"

#include "../tr_local.h"
#include "retail_renderer_contract.h"
#include "../../win32/xb_perf.h"

#ifdef _XBOX
extern "C" volatile unsigned int g_SPXBClTailStage;
#endif

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
extern "C" volatile unsigned int g_SPXBRenderCommandHighWater;
extern "C" volatile unsigned int g_SPXBRenderCommandDrops;
extern "C" volatile unsigned int g_SPXBRenderCommandLastDrop;
extern "C" volatile unsigned int g_SPXBRenderCommandCalls;
extern "C" volatile unsigned int g_SPXBRenderCommandIssueCount;
extern "C" volatile unsigned int g_SPXBRenderCommandLastUsed;
extern "C" volatile unsigned int g_SPXBHMScoreStretchCount;
extern "C" volatile unsigned int g_SPXBHMScoreStretchX;
extern "C" volatile unsigned int g_SPXBHMScoreStretchY;
extern "C" volatile unsigned int g_SPXBHMScoreStretchW;
extern "C" volatile unsigned int g_SPXBHMScoreStretchH;
extern "C" volatile unsigned int g_SPXBHMScoreQueuedCount;
extern "C" volatile unsigned int g_SPXBHMScoreQueuedShader;
#endif

STEFX_RETAIL_NAMESPACE_BEGIN

/*
=====================
R_PerformanceCounters
=====================
*/
void R_PerformanceCounters( void ) {
#ifndef _XBOX
	if ( !r_speeds->integer ) {
		// clear the counters even if we aren't printing
		Com_Memset( &tr.pc, 0, sizeof( tr.pc ) );
		Com_Memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
		return;
	}

	if (r_speeds->integer == 1) {
		const float texSize = R_SumOfUsedImages( qfalse )/(8*1048576.0f)*(r_texturebits->integer?r_texturebits->integer:glConfig.colorBits);
		Com_Printf ( "%i/%i shdrs/srfs %i leafs %i vrts %i/%i tris %.2fMB tex %.2f dc\n",
			backEnd.pc.c_shaders, backEnd.pc.c_surfaces, tr.pc.c_leafs, backEnd.pc.c_vertexes, 
			backEnd.pc.c_indexes/3, backEnd.pc.c_totalIndexes/3, 
			texSize, backEnd.pc.c_overDraw / (float)(glConfig.vidWidth * glConfig.vidHeight) ); 
	} else if (r_speeds->integer == 2) {
		Com_Printf ( "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_patch_in, tr.pc.c_sphere_cull_patch_clip, tr.pc.c_sphere_cull_patch_out, 
			tr.pc.c_box_cull_patch_in, tr.pc.c_box_cull_patch_clip, tr.pc.c_box_cull_patch_out );
		Com_Printf ( "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			tr.pc.c_sphere_cull_md3_in, tr.pc.c_sphere_cull_md3_clip, tr.pc.c_sphere_cull_md3_out, 
			tr.pc.c_box_cull_md3_in, tr.pc.c_box_cull_md3_clip, tr.pc.c_box_cull_md3_out );
	} else if (r_speeds->integer == 3) {
		Com_Printf ( "viewcluster: %i\n", tr.viewCluster );
	} else if (r_speeds->integer == 4) {
		if ( backEnd.pc.c_dlightVertexes ) {
			Com_Printf ( "dlight srf:%i  culled:%i  verts:%i  tris:%i\n", 
				tr.pc.c_dlightSurfaces, tr.pc.c_dlightSurfacesCulled,
				backEnd.pc.c_dlightVertexes, backEnd.pc.c_dlightIndexes / 3 );
		}
	} 
	else if (r_speeds->integer == 5 )
	{
		Com_Printf ("zFar: %.0f\n", tr.viewParms.zFar );
	}
	else if (r_speeds->integer == 6 )
	{
		Com_Printf ("flare adds:%i tests:%i renders:%i\n", 
			backEnd.pc.c_flareAdds, backEnd.pc.c_flareTests, backEnd.pc.c_flareRenders );
	}
	else if (r_speeds->integer == 7) {
		const float texSize = R_SumOfUsedImages(qtrue) / (1048576.0f);
		const float backBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.colorBits / (8.0f * 1024*1024);
		const float depthBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.depthBits / (8.0f * 1024*1024);
		const float stencilBuff= glConfig.vidWidth * glConfig.vidHeight * glConfig.stencilBits / (8.0f * 1024*1024);
		Com_Printf ( "Tex MB %.2f + buffers %.2f MB = Total %.2fMB\n",
			texSize, backBuff*2+depthBuff+stencilBuff, texSize+backBuff*2+depthBuff+stencilBuff); 
	}
#endif

	memset( &tr.pc, 0, sizeof( tr.pc ) );
	memset( &backEnd.pc, 0, sizeof( backEnd.pc ) );
}


/*
====================
R_InitCommandBuffers
====================
*/
void R_InitCommandBuffers( void ) {
}

/*
====================
R_ShutdownCommandBuffers
====================
*/
void R_ShutdownCommandBuffers( void ) {
}

/*
====================
R_IssueRenderCommands
====================
*/
void R_IssueRenderCommands( qboolean runPerformanceCounters ) {
	renderCommandList_t	*cmdList;

	cmdList = &backEndData->commands;
	assert(cmdList); // bk001205
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	++g_SPXBRenderCommandIssueCount;
	g_SPXBRenderCommandLastUsed = (unsigned int)cmdList->used;
	if ( (unsigned int)cmdList->used > g_SPXBRenderCommandHighWater ) {
		g_SPXBRenderCommandHighWater = (unsigned int)cmdList->used;
	}
#endif
	// add an end-of-list command
	*(int *)(cmdList->cmds + cmdList->used) = RC_END_OF_LIST;

	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

	// at this point, the back end thread is idle, so it is ok
	// to look at it's performance counters
	if ( runPerformanceCounters ) {
		R_PerformanceCounters();
	}

	// actually start the commands going
	if ( !r_skipBackEnd->integer ) {
		// let it start on the new batch
		RB_ExecuteRenderCommands( cmdList->cmds );
	}
}


/*
====================
R_SyncRenderThread

Issue any pending commands and wait for them to complete.
After exiting, the render thread will have completed its work
and will remain idle and the main thread is free to issue
OpenGL calls until R_IssueRenderCommands is called.
====================
*/
void R_SyncRenderThread( void ) {
#ifndef _XBOX
	if ( !tr.registered ) {
		return;
	}
	R_IssueRenderCommands( qfalse );
#endif
}

/*
============
R_GetCommandBuffer

make sure there is enough command space, waiting on the
render thread if needed.
============
*/
void *R_GetCommandBuffer( int bytes ) {
	renderCommandList_t	*cmdList;

#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	++g_SPXBRenderCommandCalls;
#endif
	cmdList = &backEndData->commands;

	// always leave room for the end of list command
	if ( cmdList->used + bytes + 4 > MAX_RENDER_COMMANDS ) {
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
		++g_SPXBRenderCommandDrops;
		g_SPXBRenderCommandLastDrop = ((unsigned int)(cmdList->used & 0xffff) << 16) |
			(unsigned int)(bytes & 0xffff);
#endif
#if defined(_DEBUG) && defined(_XBOX)
		Com_Printf(S_COLOR_RED"Command buffer overflow!  Tell Brian.\n");
#endif
		if ( bytes > MAX_RENDER_COMMANDS - 4 ) {
			Com_Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList->used += bytes;
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( (unsigned int)cmdList->used > g_SPXBRenderCommandHighWater ) {
		g_SPXBRenderCommandHighWater = (unsigned int)cmdList->used;
	}
#endif

	return cmdList->cmds + cmdList->used - bytes;
}


/*
=============
R_AddDrawSurfCmd

=============
*/
void	R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	cmd = (drawSurfsCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;

}


/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void	RE_SetColor( const float *rgba ) {
	setColorCommand_t	*cmd;

	cmd = (setColorCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SET_COLOR;
	if ( !rgba ) {
		static float colorWhite[4] = { 1, 1, 1, 1 };

		rgba = colorWhite;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}


/*
=============
RE_StretchPic
=============
*/
void RE_StretchPic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	stretchPicCommand_t	*cmd;

	cmd = (stretchPicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
#if defined(_XBOX) && defined(STEFX_HM_SCORE_DIAGNOSTICS)
	if ( g_SPXBHMScoreStretchCount > 0u && g_SPXBHMScoreQueuedCount == 0u ) {
		union { float f; unsigned int u; } bits;
		bits.f = x;
		if ( bits.u == g_SPXBHMScoreStretchX ) {
			bits.f = y;
			if ( bits.u == g_SPXBHMScoreStretchY ) {
				bits.f = w;
				if ( bits.u == g_SPXBHMScoreStretchW ) {
					bits.f = h;
					if ( bits.u == g_SPXBHMScoreStretchH ) {
						g_SPXBHMScoreQueuedCount = 1u;
						g_SPXBHMScoreQueuedShader = (unsigned int)cmd->shader;
					}
				}
			}
		}
	}
#endif
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}

/*
=============
RE_RotatePic
=============
*/
void RE_RotatePic ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader ) {
	rotatePicCommand_t	*cmd;

	cmd = (rotatePicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_ROTATE_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
	cmd->a = a;
}

/*
=============
RE_RotatePic2
=============
*/
void RE_RotatePic2 ( float x, float y, float w, float h, 
					  float s1, float t1, float s2, float t2,float a, qhandle_t hShader ) {
	rotatePicCommand_t	*cmd;

	cmd = (rotatePicCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_ROTATE_PIC2;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
	cmd->a = a;
}

void RE_RenderWorldEffects(void)
{
	drawBufferCommand_t	*cmd;

	cmd = (drawBufferCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_WORLD_EFFECTS;
}

void RE_LAGoggles( void )
{
	tr.refdef.rdflags |= (RDF_doLAGoggles|RDF_doFullbright);

	fog_t *fog = &tr.world->fogs[tr.world->numfogs];
	fog->parms.color[0] = 0.75f;
	fog->parms.color[1] = 0.42f + random() * 0.025f;
	fog->parms.color[2] = 0.07f;
	fog->parms.color[3] = 1.0f;
	fog->parms.depthForOpaque = 10000;
	fog->colorInt = ColorBytes4( fog->parms.color[0], fog->parms.color[1], fog->parms.color[2], fog->parms.color[3] );
	fog->tcScale = 2.0f / ( fog->parms.depthForOpaque * (1.0f + cos( tr.refdef.floatTime ) * 0.1f) );
}

void RE_Scissor( float x, float y, float w, float h )
{
	scissorCommand_t *cmd;

	cmd = (scissorCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd )
	{
		return;
	}
	cmd->commandId = RC_SCISSOR;
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
}

/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	drawBufferCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	glState.finishCalled = qfalse;

	tr.frameCount++;
	tr.frameSceneNum = 0;
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
	// CPU-only cache; Xbox render commands execute synchronously.
	extern void R_STEFX_CoopSkinBeginFrame(void);
	R_STEFX_CoopSkinBeginFrame();
#endif

	//
	// do overdraw measurement
	//
#ifndef _XBOX
	if ( r_measureOverdraw->integer )
	{
		if ( glConfig.stencilBits < 4 )
		{
			Com_Printf ("Warning: not enough stencil bits to measure overdraw: %d\n", glConfig.stencilBits );
			Cvar_Set( "r_measureOverdraw", "0" );
			r_measureOverdraw->modified = qfalse;
		}
		else if ( r_shadows->integer == 2 )
		{
			Com_Printf ("Warning: stencil shadows and overdraw measurement are mutually exclusive\n" );
			Cvar_Set( "r_measureOverdraw", "0" );
			r_measureOverdraw->modified = qfalse;
		}
		else
		{
			R_SyncRenderThread();
			qglEnable( GL_STENCIL_TEST );
			qglStencilMask( ~0U );
			qglClearStencil( 0U );
			qglStencilFunc( GL_ALWAYS, 0U, ~0U );
			qglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
		}
		r_measureOverdraw->modified = qfalse;
	}
	else
	{
		// this is only reached if it was on and is now off
		if ( r_measureOverdraw->modified ) {
			R_SyncRenderThread();
			qglDisable( GL_STENCIL_TEST );
		}
		r_measureOverdraw->modified = qfalse;
	}
#endif

	//
	// texturemode stuff
	//
	if ( r_textureMode->modified || r_ext_texture_filter_anisotropic->modified) {
		R_SyncRenderThread();
		GL_TextureMode( r_textureMode->string );
		r_textureMode->modified = qfalse;
		r_ext_texture_filter_anisotropic->modified = qfalse;
	}

	//
	// gamma stuff
	//
	if ( r_gamma->modified ) {
		r_gamma->modified = qfalse;

		R_SyncRenderThread();
		R_SetColorMappings();
	}

    // check for errors
    if ( !r_ignoreGLErrors->integer ) {
        int	err;

		R_SyncRenderThread();
        if ( ( err = qglGetError() ) != GL_NO_ERROR ) {
            Com_Error( ERR_FATAL, "RE_BeginFrame() - glGetError() failed (0x%x)!\n", err );
        }
    }

	//
	// draw buffer stuff
	//
	cmd = (drawBufferCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_BUFFER;

	if ( glConfig.stereoEnabled ) {
		if ( stereoFrame == STEREO_LEFT ) {
			cmd->buffer = (int)GL_BACK_LEFT;
		} else if ( stereoFrame == STEREO_RIGHT ) {
			cmd->buffer = (int)GL_BACK_RIGHT;
		} else {
			Com_Error( ERR_FATAL, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame );
		}
	} else {
		if ( stereoFrame != STEREO_CENTER ) {
			Com_Error( ERR_FATAL, "RE_BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame );
		}
//		if ( !Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) ) {
//			cmd->buffer = (int)GL_FRONT;
//		} else 
		{
			cmd->buffer = (int)GL_BACK;
		}
	}
}


/*
=============
RE_EndFrame

Returns the number of msec spent in the back end
=============
*/
void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	swapBuffersCommand_t	*cmd;
#ifdef _XBOX
	g_SPXBClTailStage = 0x45463030; /* 'EF00' */
#endif
#if defined(_XBOX) && defined(STEFX_HW_FRAME_DIAGNOSTICS)
	int xboxPresentStart = 0;
#endif

	if ( !tr.registered ) {
		return;
	}
	cmd = (swapBuffersCommand_t *) R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SWAP_BUFFERS;

#ifdef _XBOX
	g_SPXBClTailStage = 0x45463031; /* 'EF01' */
	if (!qglBeginFrame()) return;
	g_SPXBClTailStage = 0x45463032; /* 'EF02' */
#endif

	R_IssueRenderCommands( qtrue );

#ifdef _XBOX
	g_SPXBClTailStage = 0x45463033; /* 'EF03' */
	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (g_SPXBPerfSampleActive) {
		xboxPresentStart = Sys_Milliseconds();
	}
	#endif
	qglEndFrame();
	g_SPXBClTailStage = 0x45463034; /* 'EF04' */
	#if defined(STEFX_HW_FRAME_DIAGNOSTICS)
	if (g_SPXBPerfSampleActive) {
		g_SPXBPerfPresentMsec += (unsigned int)(Sys_Milliseconds() - xboxPresentStart);
	}
	#endif
#endif

	// use the other buffers next frame, because another CPU
	// may still be rendering into the current ones
	R_ToggleSmpFrame();
#ifdef _XBOX
	g_SPXBClTailStage = 0x45463035; /* 'EF05' */
#endif

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	tr.frontEndMsec = 0;
	if ( backEndMsec ) {
		*backEndMsec = backEnd.pc.msec;
	}
	backEnd.pc.msec = 0;
}

STEFX_RETAIL_NAMESPACE_END
