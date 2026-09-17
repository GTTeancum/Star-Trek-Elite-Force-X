// tr_surf.c

// leave this as first line for PCH reasons...
//		
#include "../server/exe_headers.h"


#include "tr_local.h"

// The shipping JA Xbox surface module owns the shared renderer ABI.  Keep
// this translation unit for Elite Force-only procedural surfaces and MDR,
// but give its legacy copies private names so they cannot win link ownership.
#ifdef STEFX_RETAIL_SURFACE_ACTIVE
#define RB_CheckOverflow       STEFX_EF_RB_CheckOverflow
#define RB_AddQuadStampExt     STEFX_EF_RB_AddQuadStampExt
#define RB_AddQuadStamp        STEFX_EF_RB_AddQuadStamp
#define RB_SurfacePolychain    STEFX_EF_RB_SurfacePolychain
#define RB_SurfaceTriangles    STEFX_EF_RB_SurfaceTriangles
#define RB_SurfaceMesh         STEFX_EF_RB_SurfaceMesh
#define RB_SurfaceFace         STEFX_EF_RB_SurfaceFace
#define RB_SurfaceGrid         STEFX_EF_RB_SurfaceGrid
#define RB_SurfaceEntity       STEFX_EF_RB_SurfaceEntity
#define RB_SurfaceBad          STEFX_EF_RB_SurfaceBad
#define RB_SurfaceFlare        STEFX_EF_RB_SurfaceFlare
#define RB_SurfaceDisplayList  STEFX_EF_RB_SurfaceDisplayList
#define RB_SurfaceSkip         STEFX_EF_RB_SurfaceSkip
#define rb_surfaceTable        stefx_ef_surfaceTable
#endif

#ifdef _XBOX
#include "../win32/glw_win_dx8.h"
#include "../win32/xb_log.h"
#endif

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/


//============================================================================


/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow( int verts, int indexes ) {
	if ( tess.shader == tr.shadowShader ) {
		if (tess.numVertexes + verts < SHADER_MAX_VERTEXES/2
			&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
			return;
		}
	} else 
		if (tess.numVertexes + verts < SHADER_MAX_VERTEXES
			&& tess.numIndexes + indexes < SHADER_MAX_INDEXES) {
			return;
		}

	RB_EndSurface();

	if ( verts >= SHADER_MAX_VERTEXES ) {
		Com_Error(ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, SHADER_MAX_VERTEXES );
	} 
	if ( indexes >= SHADER_MAX_INDEXES ) {
		Com_Error(ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, SHADER_MAX_INDEXES );
	} 

	RB_BeginSurface(tess.shader, tess.fogNum );
}


/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 ) {
	vec3_t		normal;
	int			ndx;

	RB_CHECKOVERFLOW( 4, 6 );

	ndx = tess.numVertexes;

	// triangle indexes for a simple quad
	tess.indexes[ tess.numIndexes ] = ndx;
	tess.indexes[ tess.numIndexes + 1 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 2 ] = ndx + 3;

	tess.indexes[ tess.numIndexes + 3 ] = ndx + 3;
	tess.indexes[ tess.numIndexes + 4 ] = ndx + 1;
	tess.indexes[ tess.numIndexes + 5 ] = ndx + 2;

	tess.xyz[ndx][0] = origin[0] + left[0] + up[0];
	tess.xyz[ndx][1] = origin[1] + left[1] + up[1];
	tess.xyz[ndx][2] = origin[2] + left[2] + up[2];

	tess.xyz[ndx+1][0] = origin[0] - left[0] + up[0];
	tess.xyz[ndx+1][1] = origin[1] - left[1] + up[1];
	tess.xyz[ndx+1][2] = origin[2] - left[2] + up[2];

	tess.xyz[ndx+2][0] = origin[0] - left[0] - up[0];
	tess.xyz[ndx+2][1] = origin[1] - left[1] - up[1];
	tess.xyz[ndx+2][2] = origin[2] - left[2] - up[2];

	tess.xyz[ndx+3][0] = origin[0] + left[0] - up[0];
	tess.xyz[ndx+3][1] = origin[1] + left[1] - up[1];
	tess.xyz[ndx+3][2] = origin[2] + left[2] - up[2];


	// constant normal all the way around
	VectorSubtract( vec3_origin, backEnd.viewParms.or.axis[0], normal );

	tess.normal[ndx][0] = tess.normal[ndx+1][0] = tess.normal[ndx+2][0] = tess.normal[ndx+3][0] = normal[0];
	tess.normal[ndx][1] = tess.normal[ndx+1][1] = tess.normal[ndx+2][1] = tess.normal[ndx+3][1] = normal[1];
	tess.normal[ndx][2] = tess.normal[ndx+1][2] = tess.normal[ndx+2][2] = tess.normal[ndx+3][2] = normal[2];

	// standard square texture coordinates
	tess.texCoords[ndx][0][0] = tess.texCoords[ndx][1][0] = s1;
	tess.texCoords[ndx][0][1] = tess.texCoords[ndx][1][1] = t1;

	tess.texCoords[ndx+1][0][0] = tess.texCoords[ndx+1][1][0] = s2;
	tess.texCoords[ndx+1][0][1] = tess.texCoords[ndx+1][1][1] = t1;

	tess.texCoords[ndx+2][0][0] = tess.texCoords[ndx+2][1][0] = s2;
	tess.texCoords[ndx+2][0][1] = tess.texCoords[ndx+2][1][1] = t2;

	tess.texCoords[ndx+3][0][0] = tess.texCoords[ndx+3][1][0] = s1;
	tess.texCoords[ndx+3][0][1] = tess.texCoords[ndx+3][1][1] = t2;

	// constant color all the way around
	// should this be identity and let the shader specify from entity?
	* ( unsigned int * ) &tess.vertexColors[ndx] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+1] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+2] = 
	* ( unsigned int * ) &tess.vertexColors[ndx+3] = 
		* ( unsigned int * )color;

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color ) {
	RB_AddQuadStampExt( origin, left, up, color, 0, 0, 1, 1 );
}

/*
==============
RB_SurfaceSprite
==============
*/
static void RB_SurfaceSprite( void ) {
	vec3_t		left, up;
	float		radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	if ( backEnd.currentEntity->e.rotation == 0 ) {
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} else {
		float	s, c;
		float	ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
		VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

		VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
		VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );
	} 
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

/*
=======================
RB_SurfaceOrientedQuad
=======================
*/
static void RB_SurfaceOrientedQuad( void )
{ 
	vec3_t	left, up;
	float	radius;

	// calculate the xyz locations for the four corners
	radius = backEnd.currentEntity->e.radius;
	MakeNormalVectors( backEnd.currentEntity->e.axis[0], left, up );

	if ( backEnd.currentEntity->e.rotation == 0 ) 
	{
		VectorScale( left, radius, left );
		VectorScale( up, radius, up );
	} 
	else 
	{
		vec3_t	tempLeft, tempUp;
		float	s, c;
		float	ang;

		ang = M_PI * backEnd.currentEntity->e.rotation / 180;
		s = sin( ang );
		c = cos( ang );

		// Use a temp so we don't trash the values we'll need later
		VectorScale( left, c * radius, tempLeft );
		VectorMA( tempLeft, -s * radius, up, tempLeft );

		VectorScale( up, c * radius, tempUp );
		VectorMA( tempUp, s * radius, left, up ); // no need to use the temp anymore, so copy into the dest vector ( up )

		// This was copied for safekeeping, we're done, so we can move it back to left
		VectorCopy( tempLeft, left );
	} 

	if ( backEnd.viewParms.isMirror ) 
	{
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( backEnd.currentEntity->e.origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

/*
==============
RB_SurfaceLine
==============
*/
//		
//	Values for a proper line render primitive...
//		Width
//		STScale (how many times to loop a texture)
//		alpha
//		RGB
//		
//  Values for proper line object...
//		lifetime
//		dscale
//		startalpha, endalpha
//		startRGB, endRGB
//		

static void DoLine( const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth )
{ 
	float		spanWidth2;
	int			vbase;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	spanWidth2 = -spanWidth;

	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];// * 0.25;//wtf??not sure why the code would be doing this
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];// * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];// * 0.25;
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( start, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[tess.numVertexes][0][1] = 0;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[tess.numVertexes][0][1] = 1;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

static void DoLine2( const vec3_t start, const vec3_t end, const vec3_t up, float spanWidth, float spanWidth2, const float tcStart, const float tcEnd )
{ 
	int			vbase;

	RB_CHECKOVERFLOW( 4, 6 );

	vbase = tess.numVertexes;

	VectorMA( start, spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = tcStart;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];// * 0.25;//wtf??not sure why the code would be doing this
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];// * 0.25;
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];// * 0.25;
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];// * 0.25;
	tess.numVertexes++;

	VectorMA( start, -spanWidth, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[tess.numVertexes][0][1] = tcStart;
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, spanWidth2, up, tess.xyz[tess.numVertexes] );

	tess.texCoords[tess.numVertexes][0][0] = 0;
	tess.texCoords[tess.numVertexes][0][1] = tcEnd;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	VectorMA( end, -spanWidth2, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 1;//backEnd.currentEntity->e.shaderTexCoord[0];
	tess.texCoords[tess.numVertexes][0][1] = tcEnd;//backEnd.currentEntity->e.shaderTexCoord[1];
	tess.vertexColors[tess.numVertexes][0] = backEnd.currentEntity->e.shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = backEnd.currentEntity->e.shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = backEnd.currentEntity->e.shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = backEnd.currentEntity->e.shaderRGBA[3];
	tess.numVertexes++;

	tess.indexes[tess.numIndexes++] = vbase;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 2;

	tess.indexes[tess.numIndexes++] = vbase + 2;
	tess.indexes[tess.numIndexes++] = vbase + 1;
	tess.indexes[tess.numIndexes++] = vbase + 3;
}

//-----------------
// RB_SurfaceLine
//-----------------
static void RB_SurfaceLine( void ) 
{ 
	refEntity_t *e;
	vec3_t		right;
	vec3_t		start, end;
	vec3_t		v1, v2;

	e = &backEnd.currentEntity->e;

	VectorCopy( e->oldorigin, end );
	VectorCopy( e->origin, start );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	DoLine( start, end, right, e->radius);
}

#if defined(STEFX_SP_HOSTED_MP)
static void RB_EFLine( const vec3_t start, const vec3_t end, const vec3_t left,
	vec3_t *corners, float startTex, float endTex, const refEntity_t *e )
{ 
	int ndx;
	int numIndexes;
	color4ub_t *vertexColors;

	RB_CHECKOVERFLOW( 4, 6 );
	ndx = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.indexes[numIndexes] = ndx;
	tess.indexes[numIndexes + 1] = ndx + 1;
	tess.indexes[numIndexes + 2] = ndx + 3;
	tess.indexes[numIndexes + 3] = ndx + 3;
	tess.indexes[numIndexes + 4] = ndx + 1;
	tess.indexes[numIndexes + 5] = ndx + 2;

	if ( corners )
	{
		VectorCopy( corners[0], tess.xyz[ndx] );
		VectorCopy( corners[1], tess.xyz[ndx + 1] );
	} 
	else 
	{
		VectorAdd( start, left, tess.xyz[ndx] );
		VectorSubtract( start, left, tess.xyz[ndx + 1] );
	} 
	VectorSubtract( end, left, tess.xyz[ndx + 2] );
	VectorAdd( end, left, tess.xyz[ndx + 3] );

	if ( corners )
	{
		VectorCopy( tess.xyz[ndx + 3], corners[0] );
		VectorCopy( tess.xyz[ndx + 2], corners[1] );
	} 

	tess.texCoords[ndx][0][0] = 0.0f;
	tess.texCoords[ndx][0][1] = startTex;
	tess.texCoords[ndx + 1][0][0] = 1.0f;
	tess.texCoords[ndx + 1][0][1] = startTex;
	tess.texCoords[ndx + 2][0][0] = 1.0f;
	tess.texCoords[ndx + 2][0][1] = endTex;
	tess.texCoords[ndx + 3][0][0] = 0.0f;
	tess.texCoords[ndx + 3][0][1] = endTex;

	vertexColors = tess.vertexColors;
	for ( int component = 0; component < 4; ++component )
	{
		vertexColors[ndx][component] = vertexColors[ndx + 1][component] =
			vertexColors[ndx + 2][component] = vertexColors[ndx + 3][component] =
			e->shaderRGBA[component];
	} 

	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

static void RB_EFLineUnitNormal( const vec3_t viewDirection, const vec3_t lineDirection,
	vec3_t result )
{ 
	CrossProduct( viewDirection, lineDirection, result );
	if ( VectorNormalize( result ) == 0.0f )
	{
		vec3_t normalizedLine;
		vec3_t unused;

		VectorCopy( lineDirection, normalizedLine );
		if ( VectorNormalize( normalizedLine ) == 0.0f )
		{
			VectorSet( result, 0.0f, 1.0f, 0.0f );
			return;
		}

		MakeNormalVectors( normalizedLine, result, unused );
	} 
}

static void RB_EFLineNormal( const vec3_t viewDirection, const vec3_t lineDirection,
	float width, vec3_t result )
{ 
	RB_EFLineUnitNormal( viewDirection, lineDirection, result );
	VectorScale( result, width * 0.5f, result );
}

static void RB_SurfaceTexturedLine( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t lineDirection;
	vec3_t startToView;
	vec3_t left;
	shader_t *shader;

	VectorSubtract( e->oldorigin, e->origin, lineDirection );
	VectorSubtract( backEnd.viewParms.or.origin, e->origin, startToView );
	RB_EFLineNormal( startToView, lineDirection, e->radius, left );
	RB_EFLine( e->origin, e->oldorigin, left, NULL, 0.0f, e->rotation, e );

	shader = R_GetShaderByHandle( e->customShader );
	if ( shader )
	{
		shader->cullType = CT_TWO_SIDED;
	} 
}

static void RB_SurfaceOrientedLine( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left;
	shader_t *shader;

	VectorCopy( e->axis[1], left );
	VectorNormalize( left );
	VectorScale( left, e->radius * 0.5f, left );
	RB_EFLine( e->origin, e->oldorigin, left, NULL, 0.0f, 1.0f, e );

	shader = R_GetShaderByHandle( e->customShader );
	if ( shader )
	{
		shader->cullType = CT_TWO_SIDED;
	} 
}

static void RB_SurfaceTaperedLine( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t lineDirection;
	vec3_t startToView;
	vec3_t startLeft;
	vec3_t endLeft;
	color4ub_t *vertexColors;
	int ndx;
	int numIndexes;

	RB_CHECKOVERFLOW( 6, 12 );
	ndx = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.indexes[numIndexes] = ndx;
	tess.indexes[numIndexes + 1] = ndx + 1;
	tess.indexes[numIndexes + 2] = ndx + 5;
	tess.indexes[numIndexes + 3] = ndx + 5;
	tess.indexes[numIndexes + 4] = ndx + 4;
	tess.indexes[numIndexes + 5] = ndx + 1;
	tess.indexes[numIndexes + 6] = ndx + 1;
	tess.indexes[numIndexes + 7] = ndx + 4;
	tess.indexes[numIndexes + 8] = ndx + 3;
	tess.indexes[numIndexes + 9] = ndx + 3;
	tess.indexes[numIndexes + 10] = ndx + 2;
	tess.indexes[numIndexes + 11] = ndx + 1;

	VectorSubtract( e->oldorigin, e->origin, lineDirection );
	VectorSubtract( backEnd.viewParms.or.origin, e->origin, startToView );
	RB_EFLineUnitNormal( startToView, lineDirection, startLeft );
	VectorScale( startLeft, e->backlerp * 0.5f, endLeft );
	VectorScale( startLeft, e->radius * 0.5f, startLeft );

	VectorAdd( e->origin, startLeft, tess.xyz[ndx] );
	VectorCopy( e->origin, tess.xyz[ndx + 1] );
	VectorSubtract( e->origin, startLeft, tess.xyz[ndx + 2] );
	VectorSubtract( e->oldorigin, endLeft, tess.xyz[ndx + 3] );
	VectorCopy( e->oldorigin, tess.xyz[ndx + 4] );
	VectorAdd( e->oldorigin, endLeft, tess.xyz[ndx + 5] );

	tess.texCoords[ndx][0][0] = 0.0f;
	tess.texCoords[ndx][0][1] = 0.0f;
	tess.texCoords[ndx + 1][0][0] = 0.5f;
	tess.texCoords[ndx + 1][0][1] = 0.0f;
	tess.texCoords[ndx + 2][0][0] = 1.0f;
	tess.texCoords[ndx + 2][0][1] = 0.0f;
	tess.texCoords[ndx + 3][0][0] = 1.0f;
	tess.texCoords[ndx + 3][0][1] = 1.0f;
	tess.texCoords[ndx + 4][0][0] = 0.5f;
	tess.texCoords[ndx + 4][0][1] = 1.0f;
	tess.texCoords[ndx + 5][0][0] = 0.0f;
	tess.texCoords[ndx + 5][0][1] = 1.0f;

	vertexColors = tess.vertexColors;
	for ( int component = 0; component < 4; ++component )
	{
		vertexColors[ndx][component] = vertexColors[ndx + 1][component] =
			vertexColors[ndx + 2][component] = vertexColors[ndx + 3][component] =
			vertexColors[ndx + 4][component] = vertexColors[ndx + 5][component] =
			e->shaderRGBA[component];
	} 

	tess.numVertexes += 6;
	tess.numIndexes += 12;
}

#define STEFX_BEZIER_SEGMENTS 32
#define STEFX_BEZIER_STEP (1.0 / STEFX_BEZIER_SEGMENTS)

static void RB_SurfaceBezier( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	double t = 0.0;
	vec3_t segmentStart;
	vec3_t segmentEnd;
	vec3_t previousEnd[2];

	VectorCopy( e->origin, segmentStart );
	VectorCopy( segmentStart, previousEnd[0] );
	VectorCopy( segmentStart, previousEnd[1] );

	while ( t <= 1.0 )
	{
		float startTex = (float)(t * 0.5);
		float endTex;
		double oneMinusT;
		vec3_t lineDirection;
		vec3_t startToView;
		vec3_t left;

		t += STEFX_BEZIER_STEP;
		endTex = (float)(t * 0.5);
		oneMinusT = 1.0 - t;

		for ( int component = 0; component < 3; ++component )
		{
			segmentEnd[component] = (float)(
				e->origin[component] * oneMinusT * oneMinusT * oneMinusT +
				3.0 * e->axis[0][component] * t * oneMinusT * oneMinusT +
				3.0 * e->axis[1][component] * t * t * oneMinusT +
				e->oldorigin[component] * t * t * t );
		}

		VectorSubtract( segmentEnd, segmentStart, lineDirection );
		VectorSubtract( backEnd.viewParms.or.origin, segmentStart, startToView );
		RB_EFLineNormal( startToView, lineDirection, e->radius, left );
		RB_EFLine( segmentStart, segmentEnd, left, previousEnd, startTex, endTex, e );
		VectorCopy( segmentEnd, segmentStart );
	} 
}

static void RB_SurfaceEFOrientedSprite( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left;
	vec3_t up;
	float radius = e->stefxData.sprite.radius;
	float rotation = e->stefxData.sprite.rotation;

	if ( rotation == 0.0f )
	{
		VectorScale( e->axis[1], radius, left );
		VectorScale( e->axis[2], radius, up );
	} 
	else 
	{
		float angle = M_PI * rotation / 180.0f;
		float sine = sin( angle );
		float cosine = cos( angle );

		VectorScale( e->axis[1], cosine * radius, left );
		VectorMA( left, -sine * radius, e->axis[2], left );
		VectorScale( e->axis[2], cosine * radius, up );
		VectorMA( up, sine * radius, e->axis[1], up );
	} 

	if ( backEnd.viewParms.isMirror ) 
	{
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( e->origin, left, up, e->shaderRGBA );
}

static void RB_SurfaceEFAlphaVertPoly( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t left;
	vec3_t up;
	float radius = e->stefxData.sprite.radius;
	float rotation = e->stefxData.sprite.rotation;
	int firstVertex;
	int vertex;

	if ( rotation == 0.0f )
	{
		VectorScale( backEnd.viewParms.or.axis[1], radius, left );
		VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	} 
	else 
	{
		float angle = M_PI * rotation / 180.0f;
		float sine = sin( angle );
		float cosine = cos( angle );

		VectorScale( backEnd.viewParms.or.axis[1], cosine * radius, left );
		VectorMA( left, -sine * radius, backEnd.viewParms.or.axis[2], left );
		VectorScale( backEnd.viewParms.or.axis[2], cosine * radius, up );
		VectorMA( up, sine * radius, backEnd.viewParms.or.axis[1], up );
	} 

	if ( backEnd.viewParms.isMirror ) 
	{
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( e->origin, left, up, e->shaderRGBA );
	// RB_AddQuadStamp may flush a full batch before appending this quad.
	firstVertex = tess.numVertexes - 4;
	for ( vertex = 0; vertex < 4; ++vertex )
	{
		memcpy( tess.vertexColors[firstVertex + vertex],
			e->stefxData.sprite.vertRGBA[vertex], sizeof( color4ub_t ) );
	} 
}

static void RB_EFLightningCore( const vec3_t start, const vec3_t end,
	const vec3_t up, float length, float width, const refEntity_t *e )
{ 
	int firstVertex;
	float textureLength = length / 256.0f;

	RB_CHECKOVERFLOW( 4, 6 );
	firstVertex = tess.numVertexes;

	VectorMA( start, width, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 0.0f;
	tess.texCoords[tess.numVertexes][0][1] = 0.0f;
	memcpy( tess.vertexColors[tess.numVertexes], e->shaderRGBA, sizeof( color4ub_t ) );
	tess.vertexColors[tess.numVertexes][0] = (byte)( e->shaderRGBA[0] * 0.25f );
	tess.vertexColors[tess.numVertexes][1] = (byte)( e->shaderRGBA[1] * 0.25f );
	tess.vertexColors[tess.numVertexes][2] = (byte)( e->shaderRGBA[2] * 0.25f );
	++tess.numVertexes;

	VectorMA( start, -width, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = 0.0f;
	tess.texCoords[tess.numVertexes][0][1] = 1.0f;
	memcpy( tess.vertexColors[tess.numVertexes], e->shaderRGBA, sizeof( color4ub_t ) );
	++tess.numVertexes;

	VectorMA( end, width, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = textureLength;
	tess.texCoords[tess.numVertexes][0][1] = 0.0f;
	memcpy( tess.vertexColors[tess.numVertexes], e->shaderRGBA, sizeof( color4ub_t ) );
	++tess.numVertexes;

	VectorMA( end, -width, up, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = textureLength;
	tess.texCoords[tess.numVertexes][0][1] = 1.0f;
	memcpy( tess.vertexColors[tess.numVertexes], e->shaderRGBA, sizeof( color4ub_t ) );
	++tess.numVertexes;

	tess.indexes[tess.numIndexes++] = firstVertex;
	tess.indexes[tess.numIndexes++] = firstVertex + 1;
	tess.indexes[tess.numIndexes++] = firstVertex + 2;
	tess.indexes[tess.numIndexes++] = firstVertex + 2;
	tess.indexes[tess.numIndexes++] = firstVertex + 1;
	tess.indexes[tess.numIndexes++] = firstVertex + 3;
}

static void RB_SurfaceEFLightning( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t start;
	vec3_t end;
	vec3_t direction;
	vec3_t startToView;
	vec3_t endToView;
	vec3_t right;
	float length;
	int strip;

	VectorCopy( e->origin, start );
	VectorCopy( e->oldorigin, end );
	VectorSubtract( end, start, direction );
	length = VectorNormalize( direction );
	VectorSubtract( start, backEnd.viewParms.or.origin, startToView );
	VectorNormalize( startToView );
	VectorSubtract( end, backEnd.viewParms.or.origin, endToView );
	VectorNormalize( endToView );
	CrossProduct( startToView, endToView, right );
	if ( VectorNormalize( right ) == 0.0f )
	{
		vec3_t unused;
		MakeNormalVectors( direction, right, unused );
	} 

	for ( strip = 0; strip < 4; ++strip )
	{
		vec3_t rotated;
		RB_EFLightningCore( start, end, right, length, 8.0f, e );
		RotatePointAroundVector( rotated, direction, right, 45.0f );
		VectorCopy( rotated, right );
	} 
}

#define STEFX_EF_CYLINDER_MAX_PLANES 64
#define STEFX_EF_CYLINDER_LOD 4.0f

static void RB_SurfaceEFCylinder( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t bottom;
	vec3_t top;
	vec3_t axis;
	vec3_t bottomToTop;
	vec3_t zeroBottom;
	vec3_t zeroTop;
	vec3_t bottomCorner;
	vec3_t topCorner;
	vec3_t bottomToView;
	vec3_t topToView;
	vec3_t projection;
	vec3_t projectionToView;
	float height = e->stefxData.cylinder.height;
	float width = e->stefxData.cylinder.width;
	float width2 = e->stefxData.cylinder.width2;
	float maxRadius = width > width2 ? width : width2;
	float distance;
	float startDistance;
	float endDistance;
	float angleStep;
	float textureStep = 0.0f;
	int planes;
	int index;
	int vertex;
	int numIndexes;

	VectorCopy( e->axis[0], axis );
	VectorCopy( e->origin, bottom );
	VectorScale( axis, height, bottomToTop );
	VectorAdd( bottom, bottomToTop, top );
	VectorSubtract( backEnd.viewParms.or.origin, bottom, bottomToView );
	VectorSubtract( backEnd.viewParms.or.origin, top, topToView );
	distance = DotProduct( bottomToView, axis );
	startDistance = VectorLength( bottomToView );
	endDistance = VectorLength( topToView );

	if ( distance < 0.0f || distance > height )
	{
		distance = startDistance < endDistance ? startDistance : endDistance;
	} 
	else 
	{
		VectorMA( bottom, distance, axis, projection );
		VectorSubtract( backEnd.viewParms.or.origin, projection, projectionToView );
		distance = VectorLength( projectionToView );
		if ( startDistance < distance ) distance = startDistance;
		if ( endDistance < distance ) distance = endDistance;
	} 
	if ( distance < 1.0f ) distance = 1.0f;

	planes = (int)( STEFX_EF_CYLINDER_LOD * maxRadius / distance * STEFX_EF_CYLINDER_MAX_PLANES );
	if ( planes < 8 ) planes = 8;
	else if ( planes > STEFX_EF_CYLINDER_MAX_PLANES ) planes = STEFX_EF_CYLINDER_MAX_PLANES;

	RB_CHECKOVERFLOW( 4 * planes, 6 * planes );
	PerpendicularVector( zeroBottom, axis );
	VectorScale( zeroBottom, width2 * 0.5f, zeroTop );
	VectorScale( zeroBottom, width * 0.5f, zeroBottom );
	angleStep = 360.0f / planes;
	if ( e->stefxData.cylinder.wrap )
	{
		textureStep = e->stefxData.cylinder.stscale / planes;
	} 

	vertex = tess.numVertexes;
	numIndexes = tess.numIndexes;
	for ( index = 1; index <= planes; ++index )
	{
		int base = vertex;
		tess.indexes[numIndexes++] = base;
		tess.indexes[numIndexes++] = base + 2;
		tess.indexes[numIndexes++] = base + 3;
		tess.indexes[numIndexes++] = base;
		tess.indexes[numIndexes++] = base + 3;
		tess.indexes[numIndexes++] = base + 1;

		if ( index == 1 )
		{
			VectorAdd( bottom, zeroBottom, tess.xyz[base] );
			VectorAdd( top, zeroTop, tess.xyz[base + 1] );
		}
		else
		{
			VectorCopy( bottomCorner, tess.xyz[base] );
			VectorCopy( topCorner, tess.xyz[base + 1] );
		}

		if ( index == planes )
		{
			VectorAdd( bottom, zeroBottom, bottomCorner );
			VectorAdd( top, zeroTop, topCorner );
		}
		else
		{
			RotatePointAroundVector( bottomCorner, axis, zeroBottom, angleStep * index );
			VectorAdd( bottom, bottomCorner, bottomCorner );
			RotatePointAroundVector( topCorner, axis, zeroTop, angleStep * index );
			VectorAdd( top, topCorner, topCorner );
		}
		VectorCopy( bottomCorner, tess.xyz[base + 2] );
		VectorCopy( topCorner, tess.xyz[base + 3] );

		if ( e->stefxData.cylinder.wrap )
		{
			tess.texCoords[base][0][0] = textureStep * ( index - 1 );
			tess.texCoords[base + 1][0][0] = textureStep * ( index - 1 );
			tess.texCoords[base + 2][0][0] = textureStep * index;
			tess.texCoords[base + 3][0][0] = textureStep * index;
		}
		else
		{
			tess.texCoords[base][0][0] = 0.0f;
			tess.texCoords[base + 1][0][0] = 0.0f;
			tess.texCoords[base + 2][0][0] = e->stefxData.cylinder.stscale;
			tess.texCoords[base + 3][0][0] = e->stefxData.cylinder.stscale;
		}
		tess.texCoords[base][0][1] = 0.0f;
		tess.texCoords[base + 1][0][1] = 1.0f;
		tess.texCoords[base + 2][0][1] = 0.0f;
		tess.texCoords[base + 3][0][1] = 1.0f;

		for ( int color = 0; color < 3; ++color )
		{
			tess.vertexColors[base][color] = tess.vertexColors[base + 1][color] =
				tess.vertexColors[base + 2][color] = tess.vertexColors[base + 3][color] =
				e->shaderRGBA[color];
		}
		tess.vertexColors[base][3] = tess.vertexColors[base + 1][3] =
			tess.vertexColors[base + 2][3] = tess.vertexColors[base + 3][3] = 0xff;
		vertex += 4;
	} 

	tess.numVertexes = vertex;
	tess.numIndexes = numIndexes;
}

#define STEFX_EF_ELECTRICITY_SEGMENTS 16
#define STEFX_EF_ELECTRICITY_STEP (1.0f / STEFX_EF_ELECTRICITY_SEGMENTS)

static void RB_SurfaceEFElectricity( void )
{ 
	refEntity_t *e = &backEnd.currentEntity->e;
	vec3_t start;
	vec3_t end;
	vec3_t startToEnd;
	vec3_t segmentStart;
	vec3_t segmentEnd;
	vec3_t lineDirection;
	vec3_t startToView;
	vec3_t previousEnd[2];
	vec3_t left;
	float length;
	int segment;

	VectorCopy( e->origin, start );
	VectorCopy( e->oldorigin, end );
	VectorSubtract( end, start, startToEnd );
	length = VectorLength( startToEnd );
	VectorCopy( start, segmentEnd );

	for ( segment = 0; segment < STEFX_EF_ELECTRICITY_SEGMENTS; ++segment )
	{
		VectorCopy( segmentEnd, segmentStart );
		if ( segment > STEFX_EF_ELECTRICITY_SEGMENTS - 2 )
		{
			VectorCopy( end, segmentEnd );
		}
		else
		{
			if ( segment > STEFX_EF_ELECTRICITY_SEGMENTS - 3 )
			{
				VectorAdd( segmentStart, end, segmentEnd );
				VectorScale( segmentEnd, 0.5f, segmentEnd );
			}
			segmentEnd[0] += ( startToEnd[0] + crandom() * e->stefxData.electricity.deviation * length ) * STEFX_EF_ELECTRICITY_STEP;
			segmentEnd[1] += ( startToEnd[1] + crandom() * e->stefxData.electricity.deviation * length ) * STEFX_EF_ELECTRICITY_STEP;
			segmentEnd[2] += ( startToEnd[2] + crandom() * e->stefxData.electricity.deviation * length * 0.5f ) * STEFX_EF_ELECTRICITY_STEP;
		}

		VectorSubtract( segmentEnd, segmentStart, lineDirection );
		VectorSubtract( backEnd.viewParms.or.origin, segmentStart, startToView );
		RB_EFLineNormal( startToView, lineDirection, e->stefxData.electricity.width, left );
		if ( segment == 0 )
		{
			VectorAdd( segmentStart, left, previousEnd[0] );
			VectorSubtract( segmentStart, left, previousEnd[1] );
		}
		RB_EFLine( segmentStart, segmentEnd, left, previousEnd, 0.0f,
			e->stefxData.electricity.stscale, e );
	} 
}
#endif

/*
==============
RB_SurfaceCylinder
==============
*/

#define NUM_CYLINDER_SEGMENTS 40

// e->origin holds the bottom point
// e->oldorigin holds the top point
// e->radius holds the radius

// If a cylinder has a tapered end that has a very small radius, the engine converts it to a cone.  Not a huge savings, but the texture mapping is slightly better
//	and it uses half as many indicies as the cylinder version
//-------------------------------------
static void RB_SurfaceCone( void )
//-------------------------------------
{ 
	static vec3_t points[NUM_CYLINDER_SEGMENTS];
	vec3_t		vr, vu, midpoint;
	vec3_t		tapered, base;
	float		detail, length;
	int			i;
	int			segments;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	//Work out the detail level of this cylinder
	VectorAdd( e->origin, e->oldorigin, midpoint );
	VectorScale(midpoint, 0.5, midpoint);		// Average start and end

	VectorSubtract( midpoint, backEnd.viewParms.or.origin, midpoint );
	length = VectorNormalize( midpoint );

	// this doesn't need to be perfect....just a rough compensation for zoom level is enough
	length *= (backEnd.viewParms.fovX / 90.0f);

	detail = 1 - ((float) length / 2048 );
	segments = NUM_CYLINDER_SEGMENTS * detail;

	// 3 is the absolute minimum, but the pop between 3-8 is too noticeable
	if ( segments < 8 )
	{
		segments = 8;
	} 

	if ( segments > NUM_CYLINDER_SEGMENTS )
	{
		segments = NUM_CYLINDER_SEGMENTS;
	} 

	// Get the direction vector
	MakeNormalVectors( e->axis[0], vr, vu );

	// we only need to rotate around the larger radius, the smaller radius get's welded
	if ( e->radius < e->backlerp )
	{
		VectorScale( vu, e->backlerp, vu );
		VectorCopy( e->origin, base );
		VectorCopy( e->oldorigin, tapered );
	} 
	else 
	{
		VectorScale( vu, e->radius, vu );
		VectorCopy( e->origin, tapered );
		VectorCopy( e->oldorigin, base );
	} 


	// Calculate the step around the cylinder
	detail = 360.0f / (float)segments;

	for ( i = 0; i < segments; i++ )
	{
		// ring
		RotatePointAroundVector( points[i], e->axis[0], vu, detail * i );
		VectorAdd( points[i], base, points[i] );
	} 

	// Calculate the texture coords so the texture can wrap around the whole cylinder
	detail = 1.0f / (float)segments;

	RB_CHECKOVERFLOW( 2 * (segments+1), 3 * segments ); // this isn't 100% accurate

	int vbase = tess.numVertexes;

	for ( i = 0; i < segments; i++ )
	{
 		VectorCopy( points[i], tess.xyz[tess.numVertexes] );
		tess.texCoords[tess.numVertexes][0][0] = detail * i;
		tess.texCoords[tess.numVertexes][0][1] = 1.0f;
		tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
		tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
		tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
		tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
		tess.numVertexes++;

		// We could add this vert once, but using the given texture mapping method, we need to generate different texture coordinates
		VectorCopy( tapered, tess.xyz[tess.numVertexes] );
		tess.texCoords[tess.numVertexes][0][0] = detail * i + detail * 0.5f; // set the texture coordinates to the point half-way between the untapered ends....but on the other end of the texture
		tess.texCoords[tess.numVertexes][0][1] = 0.0f;
		tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
		tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
		tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
		tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
		tess.numVertexes++;
	} 

	// last point has the same verts as the first, but does not share the same tex coords, so we have to duplicate it
 	VectorCopy( points[0], tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = detail * i;
	tess.texCoords[tess.numVertexes][0][1] = 1.0f;
	tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
	tess.numVertexes++;

 	VectorCopy( tapered, tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = detail * i + detail * 0.5f;
	tess.texCoords[tess.numVertexes][0][1] = 0.0f;
	tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
	tess.numVertexes++;

	// do the welding
	for ( i = 0; i < segments; i++ )
	{
		tess.indexes[tess.numIndexes++] = vbase;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 2;

		vbase += 2;
	} 
}

//-------------------------------------
static void RB_SurfaceCylinder( void )
//-------------------------------------
{ 
	static vec3_t lower_points[NUM_CYLINDER_SEGMENTS], upper_points[NUM_CYLINDER_SEGMENTS];
	vec3_t		vr, vu, midpoint, v1;
	float		detail, length;
	int			i;
	int			segments;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	// check for tapering
	if ( !( e->radius < 0.3f && e->backlerp < 0.3f) && ( e->radius < 0.3f || e->backlerp < 0.3f ))
	{
		// One end is sufficiently tapered to consider changing it to a cone
		RB_SurfaceCone();
		return;
	} 

	//Work out the detail level of this cylinder
	VectorAdd( e->origin, e->oldorigin, midpoint );
	VectorScale(midpoint, 0.5, midpoint);		// Average start and end

	VectorSubtract( midpoint, backEnd.viewParms.or.origin, midpoint );
	length = VectorNormalize( midpoint );

	// this doesn't need to be perfect....just a rough compensation for zoom level is enough
	length *= (backEnd.viewParms.fovX / 90.0f);

	detail = 1 - ((float) length / 2048 );
	segments = NUM_CYLINDER_SEGMENTS * detail;

	// 3 is the absolute minimum, but the pop between 3-8 is too noticeable
	if ( segments < 8 )
	{
		segments = 8;
	} 

	if ( segments > NUM_CYLINDER_SEGMENTS )
	{
		segments = NUM_CYLINDER_SEGMENTS;
	} 

	//Get the direction vector
	MakeNormalVectors( e->axis[0], vr, vu );

	VectorScale( vu, e->radius, v1 );	// size1
	VectorScale( vu, e->backlerp, vu );	// size2

	// Calculate the step around the cylinder
	detail = 360.0f / (float)segments;

	for ( i = 0; i < segments; i++ )
	{
		//Upper ring
		RotatePointAroundVector( upper_points[i], e->axis[0], vu, detail * i );
		VectorAdd( upper_points[i], e->origin, upper_points[i] );

		//Lower ring
		RotatePointAroundVector( lower_points[i], e->axis[0], v1, detail * i );
		VectorAdd( lower_points[i], e->oldorigin, lower_points[i] );
	} 

	// Calculate the texture coords so the texture can wrap around the whole cylinder
	detail = 1.0f / (float)segments;

	RB_CHECKOVERFLOW( 2 * (segments+1), 6 * segments ); // this isn't 100% accurate

	int vbase = tess.numVertexes;

	for ( i = 0; i < segments; i++ )
	{
 		VectorCopy( upper_points[i], tess.xyz[tess.numVertexes] );
		tess.texCoords[tess.numVertexes][0][0] = detail * i;
		tess.texCoords[tess.numVertexes][0][1] = 1.0f;
		tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
		tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
		tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
		tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
		tess.numVertexes++;

		VectorCopy( lower_points[i], tess.xyz[tess.numVertexes] );
		tess.texCoords[tess.numVertexes][0][0] = detail * i;
		tess.texCoords[tess.numVertexes][0][1] = 0.0f;
		tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
		tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
		tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
		tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
		tess.numVertexes++;
	} 

	// last point has the same verts as the first, but does not share the same tex coords, so we have to duplicate it
 	VectorCopy( upper_points[0], tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = detail * i;
	tess.texCoords[tess.numVertexes][0][1] = 1.0f;
	tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
	tess.numVertexes++;

 	VectorCopy( lower_points[0], tess.xyz[tess.numVertexes] );
	tess.texCoords[tess.numVertexes][0][0] = detail * i;
	tess.texCoords[tess.numVertexes][0][1] = 0.0f;
	tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
	tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
	tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
	tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
	tess.numVertexes++;

	// glue the verts
	for ( i = 0; i < segments; i++ )
	{
		tess.indexes[tess.numIndexes++] = vbase;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 2;

		tess.indexes[tess.numIndexes++] = vbase + 2;
		tess.indexes[tess.numIndexes++] = vbase + 1;
		tess.indexes[tess.numIndexes++] = vbase + 3; 

		vbase += 2;
	} 
}

static vec3_t sh1, sh2;
static int f_count;

// Up front, we create a random "shape", then apply that to each line segment...and then again to each of those segments...kind of like a fractal
//----------------------------------------------------------------------------
static void CreateShape()
//----------------------------------------------------------------------------
{ 
	VectorSet( sh1, 0.66f,// + crandom() * 0.1f,	// fwd
				0.08f + crandom() * 0.02f,
				0.08f + crandom() * 0.02f );

	// it seems to look best to have a point on one side of the ideal line, then the other point on the other side.
	VectorSet( sh2, 0.33f,// + crandom() * 0.1f,	// fwd
					-sh1[1] + crandom() * 0.02f,	// forcing point to be on the opposite side of the line -- right
					-sh1[2] + crandom() * 0.02f );// up
}

//----------------------------------------------------------------------------
static void ApplyShape( vec3_t start, vec3_t end, vec3_t right, float sradius, float eradius, int count, float startPerc, float endPerc )
//----------------------------------------------------------------------------
{ 
	vec3_t	point1, point2, fwd;
	vec3_t	rt, up;
	float	perc, dis;

    if ( count < 1 )
	{
		// done recursing
		DoLine2( start, end, right, sradius, eradius, startPerc, endPerc );
		return;
	} 

    CreateShape();

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd ) * 0.7f;
	MakeNormalVectors( fwd, rt, up );

	perc = sh1[0];

	VectorScale( start, perc, point1 );
	VectorMA( point1, 1.0f - perc, end, point1 );
	VectorMA( point1, dis * sh1[1], rt, point1 );
	VectorMA( point1, dis * sh1[2], up, point1 );

	// do a quick and dirty interpolation of the radius at that point
	float rads1, rads2;

	rads1 = sradius * 0.666f + eradius * 0.333f;
	rads2 = sradius * 0.333f + eradius * 0.666f;

	// recursion
    ApplyShape( start, point1, right, sradius, rads1, count - 1, startPerc, startPerc * 0.666f + endPerc * 0.333f );

	perc = sh2[0];

	VectorScale( start, perc, point2 );
	VectorMA( point2, 1.0f - perc, end, point2 );
	VectorMA( point2, dis * sh2[1], rt, point2 );
	VectorMA( point2, dis * sh2[2], up, point2 );

	// recursion
    ApplyShape( point2, point1, right, rads1, rads2, count - 1, startPerc * 0.333f + endPerc * 0.666f, startPerc * 0.666f + endPerc * 0.333f );
	ApplyShape( point2, end, right, rads2, eradius, count - 1, startPerc * 0.333f + endPerc * 0.666f, endPerc );
}

//----------------------------------------------------------------------------
static void DoBoltSeg( vec3_t start, vec3_t end, vec3_t right, float radius )
//----------------------------------------------------------------------------
{ 
	refEntity_t *e;
	vec3_t fwd, old;
	vec3_t cur, off={10,10,10};
	vec3_t rt, up;
	vec3_t temp;
	int		i;
    float dis, oldPerc = 0.0f, perc, oldRadius, newRadius;

	e = &backEnd.currentEntity->e;

	VectorSubtract( end, start, fwd );
	dis = VectorNormalize( fwd );

	if (dis > 2000)	//freaky long
	{
//		VID_Printf( PRINT_WARNING, "DoBoltSeg: insane distance.\n" );
		dis = 2000;
	} 
	MakeNormalVectors( fwd, rt, up );

	VectorCopy( start, old );

	newRadius = oldRadius = radius;

    for ( i = 16; i <= dis; i+= 16 )
	{
		// because of our large step size, we may not actually draw to the end.  In this case, fudge our percent so that we are basically complete
		if ( i + 16 > dis )
		{
			perc = 1.0f;
		}
		else
		{
			// percentage of the amount of line completed
			perc = (float)i / dis;
		}

		// create our level of deviation for this point
		VectorScale( fwd, Q_crandom(&e->frame) * 3.0f, temp );				// move less in fwd direction, chaos also does not affect this
		VectorMA( temp, Q_crandom(&e->frame) * 7.0f * e->angles[0], rt, temp );	// move more in direction perpendicular to line, angles is really the chaos
		VectorMA( temp, Q_crandom(&e->frame) * 7.0f * e->angles[0], up, temp );	// move more in direction perpendicular to line

		// track our total level of offset from the ideal line
		VectorAdd( off, temp, off );

        // Move from start to end, always adding our current level of offset from the ideal line
		//	Even though we are adding a random offset.....by nature, we always move from exactly start....to end
		VectorAdd( start, off, cur );
		VectorScale( cur, 1.0f - perc, cur );
		VectorMA( cur, perc, end, cur );

		if ( e->renderfx & RF_TAPERED )
		{
			// This does pretty close to perfect tapering since apply shape interpolates the old and new as it goes along.
			//	by using one minus the square, the radius stays fairly constant, then drops off quickly at the very point of the bolt
			oldRadius = radius * (1.0f-oldPerc*oldPerc);
			newRadius = radius * (1.0f-perc*perc);
		}

		// Apply the random shape to our line seg to give it some micro-detail-jaggy-coolness.
		ApplyShape( cur, old, right, newRadius, oldRadius, 2 - r_lodbias->integer, 0, 1 );

		// randomly split off to create little tendrils, but don't do it too close to the end and especially if we are not even of the forked variety
        if (( e->renderfx & RF_FORKED ) && f_count > 0 && Q_random(&e->frame) > 0.93f && (1.0f - perc) > 0.8f )
		{
			vec3_t newDest;

			f_count--;

			// Pick a point somewhere between the current point and the final endpoint
			VectorAdd( cur, e->oldorigin, newDest );
			VectorScale( newDest, 0.5f, newDest );

			// And then add some crazy offset
			for ( int t = 0; t < 3; t++ )
			{
				newDest[t] += Q_crandom(&e->frame) * 80;
			}

			// we could branch off using OLD and NEWDEST, but that would allow multiple forks...whereas, we just want simpler brancing
            DoBoltSeg( cur, newDest, right, newRadius );
		}

		// Current point along the line becomes our new old attach point
		VectorCopy( cur, old );
		oldPerc = perc;
	} 
}

//------------------------------------------
static void RB_SurfaceElectricity()
//------------------------------------------
{ 
	refEntity_t *e;
	vec3_t		right, fwd;
	vec3_t		start, end;
	vec3_t		v1, v2;
	float		radius, perc = 1.0f, dis;

	e = &backEnd.currentEntity->e;
	radius = e->radius;

	VectorCopy( e->origin, start );

	VectorSubtract( e->oldorigin, start, fwd );
	dis = VectorNormalize( fwd );

	// see if we should grow from start to end
	if ( e->renderfx & RF_GROW )
	{
//		perc = 1.0f - ( e->endTime - tr.refdef.time ) / e->angles[1]/*duration*/;
		// Hack to make this effect non-framerate dependant
		perc = 1.0f - ( Q_irand(0, 5) ) / e->angles[1];

		if ( perc > 1.0f )
		{
			perc = 1.0f;
		}
		else if ( perc < 0.0f )
		{
			perc = 0.0f;
		}
	} 

	VectorMA( start, perc * dis, fwd, e->oldorigin );
	VectorCopy( e->oldorigin, end );

	// compute side vector
	VectorSubtract( start, backEnd.viewParms.or.origin, v1 );
	VectorSubtract( end, backEnd.viewParms.or.origin, v2 );
	CrossProduct( v1, v2, right );
	VectorNormalize( right );

	// allow now more than three branches on branch type electricity
	f_count = 3;
    DoBoltSeg( start, end, right, radius );
}

/*
=============
RB_SurfacePolychain
=============
*/
/* // we could try to do something similar to this to get better normals into the tess for these types of surfs.  As it stands, any shader pass that
//	requires a normal ( env map ) will not work properly since the normals seem to essentially be random garbage.
void RB_SurfacePolychain( srfPoly_t *p ) {
	int		i;
	int		numv;
	vec3_t	a,b,normal={1,0,0};

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );

	if ( p->numVerts >= 3 )
	{
		VectorSubtract( p->verts[0].xyz, p->verts[1].xyz, a );
		VectorSubtract( p->verts[2].xyz, p->verts[1].xyz, b );
		CrossProduct( a,b, normal );
		VectorNormalize( normal );
	} 

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[numv][0][0] = p->verts[i].st[0];
		tess.texCoords[numv][0][1] = p->verts[i].st[1];
		VectorCopy( normal, tess.normal[numv] );
		*(int *)&tess.vertexColors[numv] = *(int *)p->verts[ i ].modulate;

		numv++;
	} 

	// generate fan indexes into the tess array
	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	} 

	tess.numVertexes = numv;
}
*/
void RB_SurfacePolychain( srfPoly_t *p ) {
	int		i;
	int		numv;

	RB_CHECKOVERFLOW( p->numVerts, 3*(p->numVerts - 2) );
#ifdef _XBOX
	if ( cls.state == CA_ACTIVE && tess.shader && tess.shader->name &&
		( strstr( tess.shader->name, "gfx/effects/" ) ||
		  strstr( tess.shader->name, "gfx/misc/" ) ||
		  strstr( tess.shader->name, "gfx/interface/" ) ||
		  strstr( tess.shader->name, "crosshair" ) ) )
	{
		static int s_stefxSurfacePolyBudget = 0;
		if ( s_stefxSurfacePolyBudget > 0 )
		{
			XBLF( "STEFX: RB_SurfacePolychain shader='%s' verts=%d tessBefore=%d/%d v0=(%g,%g,%g) color0=%u,%u,%u,%u",
				tess.shader->name, p->numVerts, tess.numVertexes, tess.numIndexes,
				p->numVerts > 0 ? p->verts[0].xyz[0] : 0.0f,
				p->numVerts > 0 ? p->verts[0].xyz[1] : 0.0f,
				p->numVerts > 0 ? p->verts[0].xyz[2] : 0.0f,
				p->numVerts > 0 ? (unsigned int)p->verts[0].modulate[0] : 0,
				p->numVerts > 0 ? (unsigned int)p->verts[0].modulate[1] : 0,
				p->numVerts > 0 ? (unsigned int)p->verts[0].modulate[2] : 0,
				p->numVerts > 0 ? (unsigned int)p->verts[0].modulate[3] : 0 );
			--s_stefxSurfacePolyBudget;
		}
	} 
#endif

	// fan triangles into the tess array
	numv = tess.numVertexes;
	for ( i = 0; i < p->numVerts; i++ ) {
		VectorCopy( p->verts[i].xyz, tess.xyz[numv] );
		tess.texCoords[numv][0][0] = p->verts[i].st[0];
		tess.texCoords[numv][0][1] = p->verts[i].st[1];
		*(int *)&tess.vertexColors[numv] = *(int *)p->verts[ i ].modulate;

		numv++;
	} 

	// generate fan indexes into the tess array
	for ( i = 0; i < p->numVerts-2; i++ ) {
		tess.indexes[tess.numIndexes + 0] = tess.numVertexes;
		tess.indexes[tess.numIndexes + 1] = tess.numVertexes + i + 1;
		tess.indexes[tess.numIndexes + 2] = tess.numVertexes + i + 2;
		tess.numIndexes += 3;
	} 

	tess.numVertexes = numv;
}

inline static ulong ComputeFinalVertexColor(const byte *colors)
{ 
	int			k;
	byte		result[4];
	byte		original[4];
	ulong		r, g, b;
#ifdef _XBOX
	qboolean	xboxBlendBmodelLight;
	int			xboxAmbient;
#endif

	*(int *)result = *(int *)colors;
#ifdef _XBOX
	*(int *)original = *(int *)colors;
	xboxBlendBmodelLight = ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity &&
		backEnd.currentEntity->e.hModel && tess.shader->lightmapIndex[0] == LIGHTMAP_BY_VERTEX );
	xboxAmbient = xboxBlendBmodelLight ? backEnd.currentEntity->ambientLightInt : 0;
#endif
	if (tess.shader->lightmapIndex[0] != LIGHTMAP_BY_VERTEX )
	{
		return *(ulong *)result;
	} 
	if (r_fullbright->integer)
	{
		result[0] = 255;
		result[1] = 255;
		result[2] = 255;
		return *(ulong *)result;
	} 
	// an optimization could be added here to compute the style[0] (which is always the world normal light)
	r = g = b = 0;
	for(k = 0; k < MAXLIGHTMAPS; k++)
	{
		if (tess.shader->styles[k] < LS_UNUSED)
		{
			byte	*styleColor = styleColors[tess.shader->styles[k]];

			// Slightly faster version, as suggested by MS:
			r += colors[0] * styleColor[0];
			g += colors[1] * styleColor[1];
			b += colors[2] * styleColor[2];
			colors += 4;


			/*r += (ulong)(*colors++) * (ulong)(*styleColor++);
			g += (ulong)(*colors++) * (ulong)(*styleColor++);
			b += (ulong)(*colors++) * (ulong)(*styleColor);
			colors++;*/

		}
		else
		{
			break;
		}
	} 

	// Again, faster version suggested by MS (avoids int->float->int mess):
	/*result[0] = r >> 8;
	result[1] = g >> 8;
	result[2] = b >> 8;

	result[0] = result[0] > 255 ? 255 : result[0];
	result[1] = result[1] > 255 ? 255 : result[1];
	result[2] = result[2] > 255 ? 255 : result[2];*/

	// The faster way sometimes produces wrong values
	result[0] = Com_Clamp(0, 255, r >> 8);
	result[1] = Com_Clamp(0, 255, g >> 8);
	result[2] = Com_Clamp(0, 255, b >> 8);

#ifdef _XBOX
	if ( xboxBlendBmodelLight )
	{
		static int s_xboxBmodelColorLogBudget = 0;
		const byte xboxBmodelLightFloor = 64;
		byte *ambient = (byte *)&xboxAmbient;
		byte effectiveAmbient[3];
		byte baked[4];

		*(int *)baked = *(int *)result;
		effectiveAmbient[0] = max( ambient[0], xboxBmodelLightFloor );
		effectiveAmbient[1] = max( ambient[1], xboxBmodelLightFloor );
		effectiveAmbient[2] = max( ambient[2], xboxBmodelLightFloor );
		result[0] = max( result[0], effectiveAmbient[0] );
		result[1] = max( result[1], effectiveAmbient[1] );
		result[2] = max( result[2], effectiveAmbient[2] );
		result[3] = original[3];
		if ( s_xboxBmodelColorLogBudget > 0 )
		{
			XBLF("JA: BMODEL_VERTEX_LIGHT_BLEND ent=%d hModel=%d shader='%s' ambient=%u,%u,%u floor=%u,%u,%u baked=%u,%u,%u src=%u,%u,%u out=%u,%u,%u",
				backEnd.currentEntity->e.number,
				backEnd.currentEntity->e.hModel,
				tess.shader ? tess.shader->name : "<null>",
				(unsigned int)ambient[0],
				(unsigned int)ambient[1],
				(unsigned int)ambient[2],
				(unsigned int)effectiveAmbient[0],
				(unsigned int)effectiveAmbient[1],
				(unsigned int)effectiveAmbient[2],
				(unsigned int)baked[0],
				(unsigned int)baked[1],
				(unsigned int)baked[2],
				(unsigned int)original[0],
				(unsigned int)original[1],
				(unsigned int)original[2],
				(unsigned int)result[0],
				(unsigned int)result[1],
				(unsigned int)result[2]);
			--s_xboxBmodelColorLogBudget;
		}
	} 
#endif

	return *(ulong *)result;
}

#ifdef _XBOX
//16 bits in, 32 bits out
inline ulong ComputeFinalVertexColor16(const byte *colors)
{ 
	int			k;
	byte		result[4];
	byte		color32[4];
	byte		original[2];
	ulong		r, g, b;
#ifdef _XBOX
	qboolean	xboxBlendBmodelLight;
	int			xboxAmbient;
#endif

	result[0] = colors[0] & 0xF0;
	result[1] = colors[0] << 4;
	result[2] = colors[1] & 0xF0;
	result[3] = colors[1] << 4;
	original[0] = colors[0];
	original[1] = colors[1];
	xboxBlendBmodelLight = ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity &&
		backEnd.currentEntity->e.hModel && tess.shader->lightmapIndex[0] == LIGHTMAP_BY_VERTEX );
	xboxAmbient = xboxBlendBmodelLight ? backEnd.currentEntity->ambientLightInt : 0;
	if (tess.shader->lightmapIndex[0] != LIGHTMAP_BY_VERTEX || r_fullbright->integer )
	{
		result[0] = 255;
		result[1] = 255;
		result[2] = 255;
		return *(ulong *)result;
	} 
	// an optimization could be added here to compute the style[0] (which is always the world normal light)
	r = g = b = 0;
	for(k = 0; k < MAXLIGHTMAPS; k++)
	{
		if (tess.shader->styles[k] < LS_UNUSED)
		{
			byte	*styleColor = styleColors[tess.shader->styles[k]];

			color32[0] = colors[k * 2] & 0xF0;
			color32[1] = colors[k * 2] << 4;
			color32[2] = colors[k * 2 + 1] & 0xF0;

			r += (ulong)(color32[0]) * (ulong)(*styleColor++);
			g += (ulong)(color32[1]) * (ulong)(*styleColor++);
			b += (ulong)(color32[2]) * (ulong)(*styleColor);
		}
		else
		{
			break;
		}
	} 
	result[0] = Com_Clamp(0, 255, r >> 8);
	result[1] = Com_Clamp(0, 255, g >> 8);
	result[2] = Com_Clamp(0, 255, b >> 8);
#ifdef _XBOX
	if ( xboxBlendBmodelLight )
	{
		static int s_xboxBmodelColor16LogBudget = 0;
		const byte xboxBmodelLightFloor = 64;
		byte *ambient = (byte *)&xboxAmbient;
		byte effectiveAmbient[3];
		byte baked[4];

		*(int *)baked = *(int *)result;
		effectiveAmbient[0] = max( ambient[0], xboxBmodelLightFloor );
		effectiveAmbient[1] = max( ambient[1], xboxBmodelLightFloor );
		effectiveAmbient[2] = max( ambient[2], xboxBmodelLightFloor );
		result[0] = max( result[0], effectiveAmbient[0] );
		result[1] = max( result[1], effectiveAmbient[1] );
		result[2] = max( result[2], effectiveAmbient[2] );
		result[3] = original[1] << 4;
		if ( s_xboxBmodelColor16LogBudget > 0 )
		{
			XBLF("JA: BMODEL_VERTEX_LIGHT16_BLEND ent=%d hModel=%d shader='%s' ambient=%u,%u,%u floor=%u,%u,%u baked=%u,%u,%u src16=%u,%u out=%u,%u,%u",
				backEnd.currentEntity->e.number,
				backEnd.currentEntity->e.hModel,
				tess.shader ? tess.shader->name : "<null>",
				(unsigned int)ambient[0],
				(unsigned int)ambient[1],
				(unsigned int)ambient[2],
				(unsigned int)effectiveAmbient[0],
				(unsigned int)effectiveAmbient[1],
				(unsigned int)effectiveAmbient[2],
				(unsigned int)baked[0],
				(unsigned int)baked[1],
				(unsigned int)baked[2],
				(unsigned int)original[0],
				(unsigned int)original[1],
				(unsigned int)result[0],
				(unsigned int)result[1],
				(unsigned int)result[2]);
			--s_xboxBmodelColor16LogBudget;
		}
	} 
#endif

	return *(ulong *)result;
}
#endif

/*
=============
RB_SurfaceTriangles
=============
*/
void RB_SurfaceTriangles( srfTriangles_t *srf ) {
	int			i, k;
	drawVert_t	*dv;
	float		*xyz, *normal, *texCoords;
#ifdef _XBOX
	float		*tangent;
#endif
	byte		*color;
	int			dlightBits;

	dlightBits = srf->dlightBits;
	tess.dlightBits |= dlightBits;

	RB_CHECKOVERFLOW( srf->numVerts, srf->numIndexes );

	for ( i = 0 ; i < srf->numIndexes ; i += 3 ) {
		tess.indexes[ tess.numIndexes + i + 0 ] = tess.numVertexes + srf->indexes[ i + 0 ];
		tess.indexes[ tess.numIndexes + i + 1 ] = tess.numVertexes + srf->indexes[ i + 1 ];
		tess.indexes[ tess.numIndexes + i + 2 ] = tess.numVertexes + srf->indexes[ i + 2 ];
	} 
	tess.numIndexes += srf->numIndexes;

	dv = srf->verts;
	xyz = tess.xyz[ tess.numVertexes ];
	normal = tess.normal[ tess.numVertexes ];
	texCoords = tess.texCoords[ tess.numVertexes ][0];
	color = tess.vertexColors[ tess.numVertexes ];
#ifdef _XBOX
	tangent = tess.tangent[ tess.numVertexes ];
#endif

	for ( i = 0 ; i < srf->numVerts ; i++, dv++) 
	{
#ifdef _XBOX
		xyz[0] = (float)dv->xyz[0];
		xyz[1] = (float)dv->xyz[1];
		xyz[2] = (float)dv->xyz[2];
		xyz += 4;

		if ( tess.shader->needsNormal || tess.dlightBits )
		{
			normal[0] = (float)dv->normal[0] / 32767.f;
			normal[1] = (float)dv->normal[1] / 32767.f;
			normal[2] = (float)dv->normal[2] / 32767.f;
			normal += 4;
		}

		if( tess.shader->needsTangent || tess.dlightBits )
		{
			tangent[0] = dv->tangent[0];
			tangent[1] = dv->tangent[1];
			tangent[2] = dv->tangent[2];
			tangent += 4;

			tess.setTangents = true;
		}

		Q_CastShort2FloatScale(&texCoords[0], &dv->dvst[0], 1.f / DRAWVERT_ST_SCALE);
		Q_CastShort2FloatScale(&texCoords[1], &dv->dvst[1], 1.f / DRAWVERT_ST_SCALE);

		for(k=0;k<MAXLIGHTMAPS;k++)
		{
			if (tess.shader->lightmapIndex[k] >= 0)
			{
				Q_CastShort2FloatScale(&texCoords[2+(k*2)+0], 
					&dv->dvlightmap[k][0], 1.f / DRAWVERT_LIGHTMAP_SCALE);
				Q_CastShort2FloatScale(&texCoords[2+(k*2)+1], 
					&dv->dvlightmap[k][1], 1.f / DRAWVERT_LIGHTMAP_SCALE);
			}
			else
			{	// can't have an empty slot in the middle, so we are done
				break;
			}
		}
		texCoords += NUM_TEX_COORDS*2;

#ifdef COMPRESS_VERTEX_COLORS
		*(unsigned int*)color = ComputeFinalVertexColor16((byte *)dv->dvcolor);
#else
		*(unsigned int*)color = ComputeFinalVertexColor((byte *)dv->dvcolor);
#endif
		color += 4;
#else // _XBOX
		xyz[0] = dv->xyz[0];
		xyz[1] = dv->xyz[1];
		xyz[2] = dv->xyz[2];
		xyz += 4;

		//if ( needsNormal ) 
		{
			normal[0] = dv->normal[0];
			normal[1] = dv->normal[1];
			normal[2] = dv->normal[2];
		}
		normal += 4;

		texCoords[0] = dv->st[0];
		texCoords[1] = dv->st[1];

		for(k=0;k<MAXLIGHTMAPS;k++)
		{
			if (tess.shader->lightmapIndex[k] >= 0)
			{
				texCoords[2+(k*2)] = dv->lightmap[k][0];
				texCoords[2+(k*2)+1] = dv->lightmap[k][1];
			}
			else
			{	// can't have an empty slot in the middle, so we are done
				break;
			}
		}
		texCoords += NUM_TEX_COORDS*2;

		*(unsigned int*)color = ComputeFinalVertexColor((byte *)dv->color);
		color += 4;
#endif // _XBOX
	} 

	for ( i = 0 ; i < srf->numVerts ; i++ ) {
		tess.vertexDlightBits[ tess.numVertexes + i] = dlightBits;
	} 

	tess.numVertexes += srf->numVerts;
}



/*
==============
RB_SurfaceBeam
==============
*/
static void RB_SurfaceBeam( void ) 
{ 
#define NUM_BEAM_SEGS 6
	refEntity_t *e;
	int	i;
	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t	start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	e = &backEnd.currentEntity->e;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if ( VectorNormalize( normalized_direction ) == 0 )
		return;

	PerpendicularVector( perpvec, normalized_direction );

	VectorScale( perpvec, 4, perpvec );

	for ( i = 0; i < NUM_BEAM_SEGS ; i++ )
	{
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i );
//		VectorAdd( start_points[i], origin, start_points[i] );
		VectorAdd( start_points[i], direction, end_points[i] );
	} 

	GL_Bind( tr.whiteImage );

	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );

	switch(e->skinNum)
	{
	case 1://Green
		glColor3f( 0, 1, 0 );
		break; 
	case 2://Blue
		glColor3f( 0.5, 0.5, 1 );
		break; 
	case 0://red
	default:
		glColor3f( 1, 0, 0 );
		break; 
	} 

#ifdef _XBOX
	glBeginEXT( GL_TRIANGLE_STRIP, ( NUM_BEAM_SEGS + 1 ) * 2, 0, 0, 0, 0 );
#else
	glBegin( GL_TRIANGLE_STRIP );
#endif
	for ( i = 0; i <= NUM_BEAM_SEGS; i++ ) {
		glVertex3fv( start_points[ i % NUM_BEAM_SEGS] );
		glVertex3fv( end_points[ i % NUM_BEAM_SEGS] );
	} 
	glEnd();
}


//------------------
// DoSprite
//------------------
static void DoSprite( vec3_t origin, float radius, float rotation ) 
{ 
	float	s, c;
	float	ang;
	vec3_t	left, up;

	ang = M_PI * rotation / 180.0f;
	s = sin( ang );
	c = cos( ang );

	VectorScale( backEnd.viewParms.or.axis[1], c * radius, left );
	VectorMA( left, -s * radius, backEnd.viewParms.or.axis[2], left );

	VectorScale( backEnd.viewParms.or.axis[2], c * radius, up );
	VectorMA( up, s * radius, backEnd.viewParms.or.axis[1], up );

	if ( backEnd.viewParms.isMirror ) 
	{
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( origin, left, up, backEnd.currentEntity->e.shaderRGBA );
}

//------------------
// RB_SurfaceSaber
//------------------
static void RB_SurfaceSaberGlow()
{ 
	vec3_t		end;
	refEntity_t *e;

	e = &backEnd.currentEntity->e;

	// Render the glow part of the blade
	for ( float i = e->saberLength; i > 0; i -= e->radius * 0.65f )
	{
		VectorMA( e->origin, i, e->axis[0], end );

		DoSprite( end, e->radius, 0.0f );//random() * 360.0f );
		e->radius += 0.017f;
	} 

	// Big hilt sprite
	// Please don't kill me Pat...I liked the hilt glow blob, but wanted a subtle pulse.:)  Feel free to ditch it if you don't like it.  --Jeff
	// Please don't kill me Jeff...  The pulse is good, but now I want the halo bigger if the saber is shorter...  --Pat
	DoSprite( e->origin, 5.5f + random() * 0.25f, 0.0f );//random() * 360.0f );
}

/*
** LerpMeshVertexes
*/
static void LerpMeshVertexes (md3Surface_t *surf, float backlerp) 
{ 
	short	*oldXyz, *newXyz, *oldNormals, *newNormals;
	float	*outXyz, *outNormal;
	float	oldXyzScale, newXyzScale;
	float	oldNormalScale, newNormalScale;
	int		vertNum;
	unsigned lat, lng;
	int		numVerts;

	outXyz = tess.xyz[tess.numVertexes];
	outNormal = tess.normal[tess.numVertexes];

	newXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
		+ (backEnd.currentEntity->e.frame * surf->numVerts * 4);
	newNormals = newXyz + 3;

	newXyzScale = MD3_XYZ_SCALE * (1.0 - backlerp);
	newNormalScale = 1.0 - backlerp;

	numVerts = surf->numVerts;

	if ( backlerp == 0 ) {
		//
		// just copy the vertexes
		//
		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			newXyz += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{

			outXyz[0] = newXyz[0] * newXyzScale;
			outXyz[1] = newXyz[1] * newXyzScale;
			outXyz[2] = newXyz[2] * newXyzScale;

			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= (FUNCTABLE_SIZE/256);
			lng *= (FUNCTABLE_SIZE/256);

			// decode X as cos( lat ) * sin( long )
			// decode Y as sin( lat ) * sin( long )
			// decode Z as cos( long )
#ifdef _XBOX
			if( tess.shader->needsNormal || tess.dlightBits )
			{
				outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
				outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
				outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
			}
#else
			outNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			outNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			outNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];
#endif
		}
	} else {
		//
		// interpolate and copy the vertex and normal
		//
		oldXyz = (short *)((byte *)surf + surf->ofsXyzNormals)
			+ (backEnd.currentEntity->e.oldframe * surf->numVerts * 4);
		oldNormals = oldXyz + 3;

		oldXyzScale = MD3_XYZ_SCALE * backlerp;
		oldNormalScale = backlerp;

		for (vertNum=0 ; vertNum < numVerts ; vertNum++,
			oldXyz += 4, newXyz += 4, oldNormals += 4, newNormals += 4,
			outXyz += 4, outNormal += 4) 
		{
			vec3_t uncompressedOldNormal, uncompressedNewNormal;

			// interpolate the xyz
			outXyz[0] = oldXyz[0] * oldXyzScale + newXyz[0] * newXyzScale;
			outXyz[1] = oldXyz[1] * oldXyzScale + newXyz[1] * newXyzScale;
			outXyz[2] = oldXyz[2] * oldXyzScale + newXyz[2] * newXyzScale;

#ifdef _XBOX
			if(tess.shader->needsNormal || tess.dlightBits )
			{
#endif
			// FIXME: interpolate lat/long instead?
			lat = ( newNormals[0] >> 8 ) & 0xff;
			lng = ( newNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;
			uncompressedNewNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedNewNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedNewNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			lat = ( oldNormals[0] >> 8 ) & 0xff;
			lng = ( oldNormals[0] & 0xff );
			lat *= 4;
			lng *= 4;

			uncompressedOldNormal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
			uncompressedOldNormal[1] = tr.sinTable[lat] * tr.sinTable[lng];
			uncompressedOldNormal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

			outNormal[0] = uncompressedOldNormal[0] * oldNormalScale + uncompressedNewNormal[0] * newNormalScale;
			outNormal[1] = uncompressedOldNormal[1] * oldNormalScale + uncompressedNewNormal[1] * newNormalScale;
			outNormal[2] = uncompressedOldNormal[2] * oldNormalScale + uncompressedNewNormal[2] * newNormalScale;

			VectorNormalize (outNormal);
#ifdef _XBOX
			}
#endif
		}
	} 
}

/*
=============
RB_SurfaceMesh
=============
*/
#ifdef _XBOX
void RE_GetModelBounds(refEntity_t *refEnt, vec3_t bounds1, vec3_t bounds2);
#endif
void RB_SurfaceMesh(md3Surface_t *surface) {
	int				j;
	float			backlerp;
	int				*triangles;
	float			*texCoords;
	int				indexes;
	int				Bob, Doug;
	int				numVerts;

#ifdef _XBOX
	UINT result;
	HRESULT hr;
	vec3_t		bounds[2], v;
	trRefEntity_t *ent = backEnd.currentEntity;

	if (ent)
	{
		ent->visible = 1;
		if (ent->e.number >= 0 && ent->e.number < (MAX_GENTITIES + 1000 + 256))
		{
			entityVisList[ent->e.number] = 1;
		}
	} 

	// Cxbx-R does not reliably complete the Xbox D3D visibility query path here.
	// Treat MD3 entities as visible so the renderer does not hang inside HLE.
#if 0
	if(ent->e.number == 0)
	{
		entityVisList[0] = 1;
		ent->visible = 1;
	} 

	extern bool		in_camera;
	if(in_camera)
	{
		// Going into an in-game cinematic essentially breaks the vis testing
		// So, just send 'em all through
		entityVisList[ent->e.number] = 1;
		ent->visible = 1;
	} 

	// If this is a portal surface, send it on through
	if(strstr(tr.models[backEnd.currentEntity->e.hModel]->name, "portal"))
	{
		entityVisList[ent->e.number] = 1;
		ent->visible = 1;
	} 

	// Get the visibility test from the last frame
	if(ent->visible == -1)
	{
		if(entityVisList[ent->e.number] == -1)
			entityVisList[ent->e.number] = 0;
		else {
			hr = glw_state->device->GetVisibilityTestResult( ent->e.number, &result, NULL );
			if( hr == D3D_OK)
				entityVisList[ent->e.number] = ((int)result) ? 1 : 0;
			else
				entityVisList[ent->e.number] = 1;
		}

		ent->visible = entityVisList[ent->e.number];

		// Run a visibility test to determine if this model should be rendered
		RE_GetModelBounds( &ent->e, bounds[0], bounds[1] );

		RB_RunVisTest(ent->e.number, bounds);
	} 

	if(ent->visible == 0)
	{
		// It's possible for the camera to be inside a bounding volume and falsely
		// report the object has failed it's vis test, test for that
		vec3_t cameraOrigin;
		VectorCopy(backEnd.ori.viewOrigin, cameraOrigin);

		RE_GetModelBounds( &ent->e, bounds[0], bounds[1] );

		if(cameraOrigin[0] >= bounds[0][0] &&
			cameraOrigin[0] <= bounds[1][0] &&
			cameraOrigin[1] >= bounds[0][1] &&
			cameraOrigin[1] <= bounds[1][1])
			ent->visible = 1;
		else
		{
			// Only do the camera/bounding check once per frame
			ent->visible = -2;
			return;
		}
	} 

	if(ent->visible == -2)
		return;
#endif
#endif

	if (  backEnd.currentEntity->e.oldframe == backEnd.currentEntity->e.frame ) {
		backlerp = 0;
	} else  {
		backlerp = backEnd.currentEntity->e.backlerp;
	} 

#ifdef VV_LIGHTING
	if(backEnd.currentEntity->dlightBits)
		tess.dlightBits = backEnd.currentEntity->dlightBits;
#endif

	RB_CHECKOVERFLOW( surface->numVerts, surface->numTriangles*3 );

	LerpMeshVertexes (surface, backlerp);

	triangles = (int *) ((byte *)surface + surface->ofsTriangles);
	indexes = surface->numTriangles * 3;
	Bob = tess.numIndexes;
	Doug = tess.numVertexes;
	for (j = 0 ; j < indexes ; j++) {
		tess.indexes[Bob + j] = Doug + triangles[j];
	} 
	tess.numIndexes += indexes;

	texCoords = (float *) ((byte *)surface + surface->ofsSt);

	numVerts = surface->numVerts;
	for ( j = 0; j < numVerts; j++ ) {
		tess.texCoords[Doug + j][0][0] = texCoords[j*2+0];
		tess.texCoords[Doug + j][0][1] = texCoords[j*2+1];
		// FIXME: fill in lightmapST for completeness?
	} 

	tess.numVertexes += surface->numVerts;
}

//#endif // _XBOX


/*
==============
RB_SurfaceFace
==============
*/
void RB_SurfaceFace( srfSurfaceFace_t *surf ) {
	int			i, k;
	float		*normal;
	int			ndx;
	int			Bob;
	int			numPoints;
	int			dlightBits;
	unsigned char	*indices;
	unsigned short	*tessIndexes;
	unsigned short	*v;

	RB_CHECKOVERFLOW( surf->numPoints, surf->numIndices );

	dlightBits = surf->dlightBits;
	tess.dlightBits |= dlightBits;

	indices = ( unsigned char * ) ( ( ( char  * ) surf ) + surf->ofsIndices );

	Bob = tess.numVertexes;
	tessIndexes = tess.indexes + tess.numIndexes;
	for ( i = surf->numIndices-1 ; i >= 0  ; i-- ) {
		tessIndexes[i] = indices[i] + Bob;
	} 

	tess.numIndexes += surf->numIndices;

	ndx = tess.numVertexes;

	numPoints = surf->numPoints;

	if ( tess.shader->needsNormal || tess.dlightBits) {
		normal = surf->plane.normal;
		for ( i = 0, ndx = tess.numVertexes; i < numPoints; i++, ndx++ ) {
			VectorCopy( normal, tess.normal[ndx] );
		}
	} 

	int nextSurfPoint = NEXT_SURFPOINT(surf->flags);
	int numLightMaps = surf->flags & 0x7F;

	for ( i = 0, v = surf->srfPoints, ndx = tess.numVertexes; i < numPoints; i++, v += nextSurfPoint, ndx++ ) {

		Q_CastShort2Float(&tess.xyz[ndx][0], (short*)&v[0]);
		Q_CastShort2Float(&tess.xyz[ndx][1], (short*)&v[1]);
		Q_CastShort2Float(&tess.xyz[ndx][2], (short*)&v[2]);

		if(tess.shader->needsTangent || tess.dlightBits)
		{
			Q_CastShort2FloatScale(&tess.tangent[ndx][0], (short*)&v[3], 1.f / 32767.0f);
			Q_CastShort2FloatScale(&tess.tangent[ndx][1], (short*)&v[4], 1.f / 32767.0f);
			Q_CastShort2FloatScale(&tess.tangent[ndx][2], (short*)&v[5], 1.f / 32767.0f);

			tess.setTangents = true;
		}

		Q_CastShort2FloatScale(&tess.texCoords[ndx][0][0], (short*)&v[6], 1.f / POINTS_ST_SCALE);
		Q_CastShort2FloatScale(&tess.texCoords[ndx][0][1], (short*)&v[7], 1.f / POINTS_ST_SCALE);

		for(k=0;k<numLightMaps;k++)
		{
			Q_CastUShort2FloatScale(&tess.texCoords[ndx][k+1][0], 
				&v[VERTEX_LM+(k*2)+0], 1.f / POINTS_LIGHT_SCALE);
			Q_CastUShort2FloatScale(&tess.texCoords[ndx][k+1][1], 
				&v[VERTEX_LM+(k*2)+1], 1.f / POINTS_LIGHT_SCALE);

		}
		if((surf->flags & 0x80) >> 7) {
#ifdef COMPRESS_VERTEX_COLORS
			*(unsigned int*) &tess.vertexColors[ndx] = ComputeFinalVertexColor16((byte *)&v[VERTEX_COLOR(surf->flags)]);
#else
			*(unsigned int*) &tess.vertexColors[ndx] = ComputeFinalVertexColor((byte *)&v[VERTEX_COLOR(surf->flags)]);
#endif
		}
	} 

	tess.numVertexes += surf->numPoints;
}


static float LodErrorForVolume( vec3_t local, float radius ) {
	vec3_t		world;
	float		d;

	world[0] = local[0] * backEnd.ori.axis[0][0] + local[1] * backEnd.ori.axis[1][0] + 
		local[2] * backEnd.ori.axis[2][0] + backEnd.ori.origin[0];
	world[1] = local[0] * backEnd.ori.axis[0][1] + local[1] * backEnd.ori.axis[1][1] + 
		local[2] * backEnd.ori.axis[2][1] + backEnd.ori.origin[1];
	world[2] = local[0] * backEnd.ori.axis[0][2] + local[1] * backEnd.ori.axis[1][2] + 
		local[2] * backEnd.ori.axis[2][2] + backEnd.ori.origin[2];

	VectorSubtract( world, backEnd.viewParms.or.origin, world );
	d = DotProduct( world, backEnd.viewParms.or.axis[0] );

	if ( d < 0 ) {
		d = -d;
	} 
	d -= radius;
	if ( d < 1 ) {
		d = 1;
	} 

	return r_lodCurveError->value / d;
}

/*
=============
RB_SurfaceGrid

Just copy the grid of points and triangulate
=============
*/
void RB_SurfaceGrid( srfGridMesh_t *cv ) {
	int		i, j, k;
	float	*xyz;
	float	*texCoords;
	float	*normal;
	unsigned char *color;
	drawVert_t	*dv;
	int		rows, irows, vrows;
	int		used;
	int		widthTable[MAX_GRID_SIZE];
	int		heightTable[MAX_GRID_SIZE];
	float	lodError;
	int		lodWidth, lodHeight;
	int		numVertexes;
	int		dlightBits;
	int		*vDlightBits;

	dlightBits = cv->dlightBits;
	tess.dlightBits |= dlightBits;

	// determine the allowable discrepance
	lodError = LodErrorForVolume( cv->lodOrigin, cv->lodRadius );

	// determine which rows and columns of the subdivision
	// we are actually going to use
	widthTable[0] = 0;
	lodWidth = 1;
	for ( i = 1 ; i < cv->width-1 ; i++ ) {
		if ( cv->widthLodError[i] <= lodError ) {
			widthTable[lodWidth] = i;
			lodWidth++;
		}
	} 
	widthTable[lodWidth] = cv->width-1;
	lodWidth++;

	heightTable[0] = 0;
	lodHeight = 1;
	for ( i = 1 ; i < cv->height-1 ; i++ ) {
		if ( cv->heightLodError[i] <= lodError ) {
			heightTable[lodHeight] = i;
			lodHeight++;
		}
	} 
	heightTable[lodHeight] = cv->height-1;
	lodHeight++;


	// very large grids may have more points or indexes than can be fit
	// in the tess structure, so we may have to issue it in multiple passes

	used = 0;
	rows = 0;
	while ( used < lodHeight - 1 ) {
		// see how many rows of both verts and indexes we can add without overflowing
		do {
			vrows = ( SHADER_MAX_VERTEXES - tess.numVertexes ) / lodWidth;
			irows = ( SHADER_MAX_INDEXES - tess.numIndexes ) / ( lodWidth * 6 );

			// if we don't have enough space for at least one strip, flush the buffer
			if ( vrows < 2 || irows < 1 ) {
				RB_EndSurface();
				RB_BeginSurface(tess.shader, tess.fogNum );
			} else {
				break;
			}
		} while ( 1 );

		rows = irows;
		if ( vrows < irows + 1 ) {
			rows = vrows - 1;
		}
		if ( used + rows > lodHeight ) {
			rows = lodHeight - used;
		}

		numVertexes = tess.numVertexes;

		xyz = tess.xyz[numVertexes];
		normal = tess.normal[numVertexes];
		texCoords = tess.texCoords[numVertexes][0];
		color = ( unsigned char * ) &tess.vertexColors[numVertexes];
		vDlightBits = &tess.vertexDlightBits[numVertexes];

#ifdef _XBOX
		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = (float)dv->xyz[0];
				xyz[1] = (float)dv->xyz[1];
				xyz[2] = (float)dv->xyz[2];
				xyz += 4;

				Q_CastShort2FloatScale(&texCoords[0], &dv->dvst[0], 1.f / GRID_DRAWVERT_ST_SCALE);
				Q_CastShort2FloatScale(&texCoords[1], &dv->dvst[1], 1.f / GRID_DRAWVERT_ST_SCALE);

				for(k=0;k<MAXLIGHTMAPS;k++)
				{
					Q_CastShort2FloatScale(&texCoords[2+(k*2)+0], 
						&dv->dvlightmap[k][0], 1.f / DRAWVERT_LIGHTMAP_SCALE);
					Q_CastShort2FloatScale(&texCoords[2+(k*2)+1], 
						&dv->dvlightmap[k][1], 1.f / DRAWVERT_LIGHTMAP_SCALE);
				}
				texCoords += NUM_TEX_COORDS*2;

				if ( tess.shader->needsNormal || tess.dlightBits) 
				{
					normal[0] = (float)dv->normal[0] / 32767.f;
					normal[1] = (float)dv->normal[1] / 32767.f;
					normal[2] = (float)dv->normal[2] / 32767.f;
					normal += 4;
				}
#ifdef COMPRESS_VERTEX_COLORS
				*(unsigned *)color = ComputeFinalVertexColor16((byte *)dv->dvcolor);
#else
				*(unsigned *)color = ComputeFinalVertexColor((byte *)dv->dvcolor);
#endif
				color += 4;
				*vDlightBits++ = dlightBits;
			}
		}
#else
		for ( i = 0 ; i < rows ; i++ ) {
			for ( j = 0 ; j < lodWidth ; j++ ) {
				dv = cv->verts + heightTable[ used + i ] * cv->width
					+ widthTable[ j ];

				xyz[0] = dv->xyz[0];
				xyz[1] = dv->xyz[1];
				xyz[2] = dv->xyz[2];
				xyz += 4;

				texCoords[0] = dv->st[0];
				texCoords[1] = dv->st[1];

				for(k=0;k<MAXLIGHTMAPS;k++)
				{
					texCoords[2+(k*2)]= dv->lightmap[k][0];
					texCoords[2+(k*2)+1]= dv->lightmap[k][1];
				}
				texCoords += NUM_TEX_COORDS*2;

//				if ( needsNormal ) 
				{
					normal[0] = dv->normal[0];
					normal[1] = dv->normal[1];
					normal[2] = dv->normal[2];
				}
				normal += 4;
				*(unsigned *)color = ComputeFinalVertexColor((byte *)dv->color);
				color += 4;
				*vDlightBits++ = dlightBits;
			}
		}
#endif

		// add the indexes
		{
			int		numIndexes;
			int		w, h;

			h = rows - 1;
			w = lodWidth - 1;
			numIndexes = tess.numIndexes;
			for ( int i = 0 ; i < h ; i++) {
				for (j = 0 ; j < w ; j++) {
					int		v1, v2, v3, v4;

					// vertex order to be reckognized as tristrips
					v1 = numVertexes + i*lodWidth + j + 1;
					v2 = v1 - 1;
					v3 = v2 + lodWidth;
					v4 = v3 + 1;

					tess.indexes[numIndexes] = v2;
					tess.indexes[numIndexes+1] = v3;
					tess.indexes[numIndexes+2] = v1;

					tess.indexes[numIndexes+3] = v1;
					tess.indexes[numIndexes+4] = v3;
					tess.indexes[numIndexes+5] = v4;
					numIndexes += 6;
				}
			}

			tess.numIndexes = numIndexes;
		}

		tess.numVertexes += rows * lodWidth;

		used += rows - 1;
	} 
}

static inline void Vector2Set(vec2_t a,float b,float c)
{ 
	a[0] = b;
	a[1] = c;
}

static inline void Vector2Copy(vec2_t src,vec2_t dst)
{ 
	dst[0] = src[0];
	dst[1] = src[1];
}

#define LATHE_SEG_STEP	10
#define BEZIER_STEP		0.05f	// must be in the range of 0 to 1

// FIXME: This function is horribly expensive
static void RB_SurfaceLathe()
{ 
	refEntity_t *e;
	vec2_t		pt, oldpt, l_oldpt;
	vec2_t		pt2, oldpt2, l_oldpt2; 
	float		bezierStep, latheStep;
    float		temp, mu, mum1;
	float		mum13, mu3, group1, group2;
	float		s, c, d = 1.0f, pain = 0.0f;
	int			i, t, vbase;

	e = &backEnd.currentEntity->e;

	if ( e->endTime && e->endTime > backEnd.refdef.time )
	{
		d = 1.0f - ( e->endTime - backEnd.refdef.time ) / 1000.0f;
	} 

	if ( e->frame && e->frame + 1000 > backEnd.refdef.time )
	{
		pain = ( backEnd.refdef.time - e->frame ) / 1000.0f;
//		pain *= pain;
		pain = ( 1.0f - pain ) * 0.08f;
	} 

	Vector2Set( l_oldpt, e->axis[0][0], e->axis[0][1] );

	// do scalability stuff...r_lodbias 0-3
	int lod = r_lodbias->integer + 1;
	if ( lod > 4 )
	{
		lod = 4;
	} 
	bezierStep = BEZIER_STEP * lod;
	latheStep = LATHE_SEG_STEP * lod;

	// Do bezier profile strip, then lathe this around to make a 3d model
	for ( mu = 0.0f; mu <= 1.01f * d; mu += bezierStep )
	{
		// Four point curve
		mum1	= 1 - mu;
		mum13	= mum1 * mum1 * mum1;
		mu3		= mu * mu * mu;
		group1	= 3 * mu * mum1 * mum1;
		group2	= 3 * mu * mu *mum1;

		// Calc the current point on the curve
		for ( i = 0; i < 2; i++ )
		{
			l_oldpt2[i] = mum13 * e->axis[0][i] + group1 * e->axis[1][i] + group2 * e->axis[2][i] + mu3 * e->oldorigin[i];
		}

		Vector2Set( oldpt, l_oldpt[0], 0 );
		Vector2Set( oldpt2, l_oldpt2[0], 0 );

		// lathe patch section around in a complete circle
		for ( t = latheStep; t <= 360; t += latheStep )
		{
			Vector2Set( pt, l_oldpt[0], 0 );
			Vector2Set( pt2, l_oldpt2[0], 0 );

			s = sin( DEG2RAD( t ));
			c = cos( DEG2RAD( t ));

			// rotate lathe points
//c -s 0
//s  c 0
//0  0 1
			temp = c * pt[0] - s * pt[1];
			pt[1] = s * pt[0] + c * pt[1];
			pt[0] = temp;
			temp = c * pt2[0] - s * pt2[1];
			pt2[1] = s * pt2[0] + c * pt2[1];
			pt2[0] = temp;

			RB_CHECKOVERFLOW( 4, 6 );  

			vbase = tess.numVertexes;

			// Actually generate the necessary verts
			VectorSet( tess.normal[tess.numVertexes], oldpt[0], oldpt[1], l_oldpt[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = oldpt[0] * 0.1f + oldpt[1] * 0.1f;
			tess.texCoords[tess.numVertexes][0][0] = (t-latheStep)/360.0f;
			tess.texCoords[tess.numVertexes][0][1] = mu-bezierStep + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], oldpt2[0], oldpt2[1], l_oldpt2[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = oldpt2[0] * 0.1f + oldpt2[1] * 0.1f;
			tess.texCoords[tess.numVertexes][0][0] = (t-latheStep) / 360.0f;
			tess.texCoords[tess.numVertexes][0][1] = mu + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], pt[0], pt[1], l_oldpt[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = pt[0] * 0.1f + pt[1] * 0.1f;
			tess.texCoords[tess.numVertexes][0][0] = t/360.0f;
			tess.texCoords[tess.numVertexes][0][1] = mu-bezierStep + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorSet( tess.normal[tess.numVertexes], pt2[0], pt2[1], l_oldpt2[1] );
			VectorAdd( e->origin, tess.normal[tess.numVertexes], tess.xyz[tess.numVertexes] );
			VectorNormalize( tess.normal[tess.numVertexes] );
			i = pt2[0] * 0.1f + pt2[1] * 0.1f;
			tess.texCoords[tess.numVertexes][0][0] = t/360.0f;
			tess.texCoords[tess.numVertexes][0][1] = mu + cos( i + backEnd.refdef.floatTime ) * pain;
			tess.vertexColors[tess.numVertexes][0] = e->shaderRGBA[0];
			tess.vertexColors[tess.numVertexes][1] = e->shaderRGBA[1];
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[2];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			tess.indexes[tess.numIndexes++] = vbase;
			tess.indexes[tess.numIndexes++] = vbase + 1;
			tess.indexes[tess.numIndexes++] = vbase + 3;

			tess.indexes[tess.numIndexes++] = vbase + 3;
			tess.indexes[tess.numIndexes++] = vbase + 2;
			tess.indexes[tess.numIndexes++] = vbase;

			// Shuffle new points to old
			Vector2Copy( pt, oldpt );
			Vector2Copy( pt2, oldpt2 );
		}

		// shuffle lathe points
		Vector2Copy( l_oldpt2, l_oldpt );
	} 
}

#define DISK_DEF	4
#define TUBE_DEF	6

static void RB_SurfaceClouds()
{ 
	// Disk definition
	float diskStripDef[DISK_DEF] = { 
				0.0f,
				0.4f,
				0.7f,
				1.0f };

	float diskAlphaDef[DISK_DEF] = { 
				1.0f,
				1.0f,
				0.4f,
				0.0f };

	float diskCurveDef[DISK_DEF] = {
				0.0f,
				0.0f,
				0.008f,
				0.02f };

	// tube definition
	float tubeStripDef[TUBE_DEF] = { 
				0.0f,
				0.05f,
				0.1f,
				0.5f,
				0.7f,
				1.0f };

	float tubeAlphaDef[TUBE_DEF] = { 
				0.0f,
				0.45f,
				1.0f,
				1.0f,
				0.45f,
				0.0f };

	float tubeCurveDef[TUBE_DEF] = { 
				0.0f,
				0.004f,
				0.006f,
				0.01f,
				0.006f,
				0.0f };

	refEntity_t *e;
	vec3_t		pt, oldpt;
	vec3_t		pt2, oldpt2;
	float		latheStep = 30.0f;
	float		s, c, temp;
	float		*stripDef, *alphaDef, *curveDef, ct;
	int			i, t, vbase;

	e = &backEnd.currentEntity->e;

	// select which type we shall be doing
	if ( e->renderfx & RF_GROW ) // doing tube type
	{
		ct = TUBE_DEF;
		stripDef = tubeStripDef;
		alphaDef = tubeAlphaDef;
		curveDef = tubeCurveDef;
		e->backlerp *= -1; // needs to be reversed
	} 
	else 
	{
		ct = DISK_DEF;
		stripDef = diskStripDef;
		alphaDef = diskAlphaDef;
		curveDef = diskCurveDef;
	} 

	// do the strip def, then lathe this around to make a 3d model
	for ( i = 0; i < ct - 1; i++ )
	{
		VectorSet( oldpt,	(stripDef[i]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i] * e->radius * e->backlerp );
		VectorSet( oldpt2,	(stripDef[i+1]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i+1] * e->radius * e->backlerp );

		// lathe section around in a complete circle
		for ( t = latheStep; t <= 360; t += latheStep )
		{
			// rotate every time except last seg
			if ( t < 360.0f )
			{
				VectorCopy( oldpt, pt );
				VectorCopy( oldpt2, pt2 );

				s = sin( DEG2RAD( latheStep ));
				c = cos( DEG2RAD( latheStep ));

				// rotate lathe points
				temp = c * pt[0] - s * pt[1];	// c -s 0
				pt[1] = s * pt[0] + c * pt[1];	// s  c 0
				pt[0] = temp;					// 0  0 1

				temp = c * pt2[0] - s * pt2[1];	 // c -s 0
				pt2[1] = s * pt2[0] + c * pt2[1];// s  c 0
				pt2[0] = temp;					 // 0  0 1
			}
			else
			{
				// just glue directly to the def points.
				VectorSet( pt,	(stripDef[i]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i] * e->radius * e->backlerp );
				VectorSet( pt2,	(stripDef[i+1]	* (e->radius - e->rotation)) + e->rotation,	0, curveDef[i+1] * e->radius * e->backlerp );
			}

			RB_CHECKOVERFLOW( 4, 6 );  

			vbase = tess.numVertexes;

			// Actually generate the necessary verts
			VectorAdd( e->origin, oldpt, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[tess.numVertexes][0][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] = 
			tess.vertexColors[tess.numVertexes][1] = 
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, oldpt2, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[tess.numVertexes][0][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] = 
			tess.vertexColors[tess.numVertexes][1] = 
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i+1];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, pt, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[tess.numVertexes][0][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] = 
			tess.vertexColors[tess.numVertexes][1] = 
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			VectorAdd( e->origin, pt2, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = tess.xyz[tess.numVertexes][0] * 0.1f;
			tess.texCoords[tess.numVertexes][0][1] = tess.xyz[tess.numVertexes][1] * 0.1f;
			tess.vertexColors[tess.numVertexes][0] = 
			tess.vertexColors[tess.numVertexes][1] = 
			tess.vertexColors[tess.numVertexes][2] = e->shaderRGBA[0] * alphaDef[i+1];
			tess.vertexColors[tess.numVertexes][3] = e->shaderRGBA[3];
			tess.numVertexes++;

			tess.indexes[tess.numIndexes++] = vbase;
			tess.indexes[tess.numIndexes++] = vbase + 1;
			tess.indexes[tess.numIndexes++] = vbase + 3;

			tess.indexes[tess.numIndexes++] = vbase + 3;
			tess.indexes[tess.numIndexes++] = vbase + 2;
			tess.indexes[tess.numIndexes++] = vbase;

			// Shuffle new points to old
			Vector2Copy( pt, oldpt );
			Vector2Copy( pt2, oldpt2 );
		}
	} 
}


/*
===========================================================================

NULL MODEL

===========================================================================
*/

/*
===================
RB_SurfaceAxis

Draws x/y/z lines from the origin for orientation debugging
===================
*/
static void RB_SurfaceAxis( void ) {
	GL_Bind( tr.whiteImage );
	glLineWidth( 3 );
#ifdef _XBOX
	glBeginEXT( GL_LINES, 6, 3, 0, 0, 0);
#else
	glBegin( GL_LINES );
#endif
	glColor3f( 1,0,0 );
	glVertex3f( 0,0,0 );
	glVertex3f( 16,0,0 );
	glColor3f( 0,1,0 );
	glVertex3f( 0,0,0 );
	glVertex3f( 0,16,0 );
	glColor3f( 0,0,1 );
	glVertex3f( 0,0,0 );
	glVertex3f( 0,0,16 );
	glEnd();
	glLineWidth( 1 );
}

//===========================================================================

#ifdef STEFX_RETAIL_SURFACE_ACTIVE
qboolean STEFX_EF_SurfaceEntity( void ) {
	switch ( backEnd.currentEntity->e.reType ) {
#if defined(STEFX_SP_HOSTED_MP)
	case RT_TEXTURED_LINE:
		RB_SurfaceTexturedLine();
		return qtrue;
	case RT_TAPERED_LINE:
		RB_SurfaceTaperedLine();
		return qtrue;
	case RT_BEZIER:
		RB_SurfaceBezier();
		return qtrue;
	case RT_EF_ORIENTED_SPRITE:
		RB_SurfaceEFOrientedSprite();
		return qtrue;
	case RT_EF_ALPHA_VERT_POLY:
		RB_SurfaceEFAlphaVertPoly();
		return qtrue;
	case RT_EF_LIGHTNING:
		RB_SurfaceEFLightning();
		return qtrue;
	case RT_EF_CYLINDER:
		RB_SurfaceEFCylinder();
		return qtrue;
	case RT_EF_ELECTRICITY:
		RB_SurfaceEFElectricity();
		return qtrue;
#endif
	case RT_LATHE:
		RB_SurfaceLathe();
		return qtrue;
	case RT_CLOUDS:
		RB_SurfaceClouds();
		return qtrue;
	default:
		return qfalse;
	}
}
#endif

/*
====================
RB_SurfaceEntity

Entities that have a single procedurally generated surface
====================
*/
void RB_SurfaceEntity( surfaceType_t *surfType ) {
	switch( backEnd.currentEntity->e.reType ) {
	case RT_SPRITE:
		RB_SurfaceSprite();
		break; 
	case RT_ORIENTED_QUAD:
		RB_SurfaceOrientedQuad();
		break; 
	case RT_LINE:
		RB_SurfaceLine();
		break; 
#if defined(STEFX_SP_HOSTED_MP)
	case RT_TEXTURED_LINE:
		RB_SurfaceTexturedLine();
		break; 
	case RT_ORIENTED_LINE:
		RB_SurfaceOrientedLine();
		break; 
	case RT_TAPERED_LINE:
		RB_SurfaceTaperedLine();
		break; 
	case RT_BEZIER:
		RB_SurfaceBezier();
		break; 
	case RT_EF_ORIENTED_SPRITE:
		RB_SurfaceEFOrientedSprite();
		break; 
	case RT_EF_ALPHA_VERT_POLY:
		RB_SurfaceEFAlphaVertPoly();
		break; 
	case RT_EF_LIGHTNING:
		RB_SurfaceEFLightning();
		break; 
	case RT_EF_CYLINDER:
		RB_SurfaceEFCylinder();
		break; 
	case RT_EF_ELECTRICITY:
		RB_SurfaceEFElectricity();
		break; 
#endif
	case RT_ELECTRICITY:
		RB_SurfaceElectricity();
		break; 
	case RT_BEAM:
		RB_SurfaceBeam();
		break; 
	case RT_SABER_GLOW:
		RB_SurfaceSaberGlow();
		break; 
	case RT_CYLINDER:
		RB_SurfaceCylinder();
		break; 
	case RT_LATHE:
		RB_SurfaceLathe();
		break; 
	case RT_CLOUDS:
		RB_SurfaceClouds();
		break; 
	default:
		RB_SurfaceAxis();
		break; 
	} 
	return;
}

void RB_SurfaceBad( surfaceType_t *surfType ) {
	VID_Printf( PRINT_ALL, "Bad surface tesselated.\n" );
}


/*
==================
RB_TestZFlare

This is called at surface tesselation time
==================
*/
#ifdef _XBOX
static bool RB_TestZFlare( vec3_t point, srfFlare_t *surf ) {
#else
static bool RB_TestZFlare( vec3_t point) {
#endif
	int				i;
	vec4_t			eye, clip, normalized, window;

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip( point, backEnd.ori.modelMatrix, 
		backEnd.viewParms.projectionMatrix, eye, clip );

	// check to see if the point is completely off screen
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( clip[i] >= clip[3] || clip[i] <= -clip[3] ) {
			return qfalse;
		}
	} 

	R_TransformClipToWindow( clip, &backEnd.viewParms, normalized, window );

	if ( window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth
		|| window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight ) {
		return qfalse;	// shouldn't happen, since we check the clip[] above, except for FP rounding
	} 

//do test
	// read back the z buffer contents
#ifdef _XBOX
	UINT result;
	HRESULT hr;
	DWORD zwrite, colorwrite;

	// Get the visibility test from the last frame
	if(surf->visible == -1)
		surf->visible = 0;
	else {
		hr = glw_state->device->GetVisibilityTestResult( (int)surf->number + 2024, &result, NULL );
		if( hr == D3D_OK)
			surf->visible = (int)result;
	} 

	glw_state->device->GetRenderState(D3DRS_ZWRITEENABLE, &zwrite);

	glw_state->device->SetRenderState(D3DRS_ZWRITEENABLE, false);
	glw_state->device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);

	glw_state->device->SetTransform(D3DTS_VIEW, 
			glw_state->matrixStack[glwstate_t::MatrixMode_Model]->GetTop());
	STEFX_D3D8_SetVertexShaderTracked(D3DFVF_XYZ);

	glw_state->device->BeginVisibilityTest();
	glw_state->device->Begin(D3DPT_POINTLIST);
	glw_state->device->SetVertexData4f(D3DVSDE_VERTEX, point[0], point[1], point[2], 1.0f);
	glw_state->device->End();
	glw_state->device->EndVisibilityTest((int)surf->number + 2024);

	glw_state->device->SetRenderState(D3DRS_ZWRITEENABLE, zwrite);
	glw_state->device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL);

	return (surf->visible > 0);

#else
	float			depth = 0.0f;
	bool			visible;
	float			screenZ;

	if ( r_flares->integer !=1 ) {	//skipping the the z-test
		return true;
	} 
	// Xbox qglReadPixels is a no-op. Preserve the finish state so this probe
	// cannot force RB_SwapBuffers to drain the GPU at the end of every frame.
#ifndef _XBOX
	glState.finishCalled = qfalse;
#endif
	glReadPixels( backEnd.viewParms.viewportX + window[0],backEnd.viewParms.viewportY + window[1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );

	screenZ = backEnd.viewParms.projectionMatrix[14] / 
		( ( 2*depth - 1 ) * backEnd.viewParms.projectionMatrix[11] - backEnd.viewParms.projectionMatrix[10] );

	visible = ( -eye[2] - -screenZ ) < 24;
	return visible;
#endif
}

void RB_SurfaceFlare( srfFlare_t *surf ) {
	vec3_t		left, up;
	float		radius;
	byte		color[4];
	vec3_t		dir;
	vec3_t		origin;
	float		d, dist;

	if ( !r_flares->integer ) {
		return;
	} 

#ifdef _XBOX
	static int s_xboxSkipFlareTraceCount = 64;
	if ( s_xboxSkipFlareTraceCount < 64 ) {
		XBLF("JA: RB_SurfaceFlare Xbox skip number=%d r_flares=%d visible=%d",
			(int)surf->number, r_flares->integer, (int)surf->visible);
		s_xboxSkipFlareTraceCount++;
	} 
	return;

	vec3_t     sorigin, snormal;

	Q_CastShort2Float(&sorigin[0], (short*)&surf->origin[0]);
	Q_CastShort2Float(&sorigin[1], (short*)&surf->origin[1]);
	Q_CastShort2Float(&sorigin[2], (short*)&surf->origin[2]);

	/*if (!RB_TestZFlare( sorigin) ) {
		return;
	}*/

	Q_CastShort2Float(&snormal[0], (short*)&surf->normal[0]);
	Q_CastShort2Float(&snormal[1], (short*)&surf->normal[1]);
	Q_CastShort2Float(&snormal[2], (short*)&surf->normal[2]);
	snormal[0] /= 32767.0f;
	snormal[1] /= 32767.0f;
	snormal[2] /= 32767.0f;

	// Pull the vertex toward the viewer so we don't get any z-fighting on the vis test
	VectorSubtract( backEnd.viewParms.or.origin, sorigin, dir );
	dist = VectorNormalize( dir );
	VectorMA( sorigin, 20, dir, origin );

	if (!RB_TestZFlare( origin, surf ) ) {
		return;
	} 

	// calculate the xyz locations for the four corners
	VectorMA( sorigin, 3, snormal, origin );
#else
	if (!RB_TestZFlare( surf->origin ) ) {
		return;
	} 

	// calculate the xyz locations for the four corners
	VectorMA( surf->origin, 3, surf->normal, origin );
	float* snormal = surf->normal;
#endif // _XBOX

	VectorSubtract( origin, backEnd.viewParms.or.origin, dir );
	dist = VectorNormalize( dir );

	d = -DotProduct( dir, snormal );
	if ( d < 0 ) {
		d = -d;
	} 

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	color[0] = d * 255;
	color[1] = d * 255;
	color[2] = d * 255;
	color[3] = 255;	//only gets used if the shader has cgen exact_vertex!

	radius = tess.shader->portalRange ? tess.shader->portalRange: 30;
	if (dist < 512.0f)
	{
		radius = radius * dist / 512.0f;
	} 
	if (radius<5.0f)
	{
		radius = 5.0f;
	} 
	VectorScale( backEnd.viewParms.or.axis[1], radius, left );
	VectorScale( backEnd.viewParms.or.axis[2], radius, up );
	if ( backEnd.viewParms.isMirror ) {
		VectorSubtract( vec3_origin, left, left );
	} 

	RB_AddQuadStamp( origin, left, up, color );
}


void RB_SurfaceDisplayList( srfDisplayList_t *surf ) {
	// all apropriate state must be set in RB_BeginSurface
	// this isn't implemented yet...
	glCallList( surf->listNum );
}

void RB_SurfaceSkip( void *surf ) {
}

void RB_SurfaceTerrain( surfaceInfo_t *surf );

void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( void *) = {
	(void(*)(void*))RB_SurfaceBad,			// SF_BAD, 
	(void(*)(void*))RB_SurfaceSkip,			// SF_SKIP, 
	(void(*)(void*))RB_SurfaceFace,			// SF_FACE,
	(void(*)(void*))RB_SurfaceGrid,			// SF_GRID,
	(void(*)(void*))RB_SurfaceTriangles,	// SF_TRIANGLES,
	(void(*)(void*))RB_SurfacePolychain,	// SF_POLY,
#ifdef _XBOX
	NULL,
#else
	(void(*)(void*))RB_SurfaceTerrain,		// SF_TERRAIN,
#endif
	(void(*)(void*))RB_SurfaceMesh,			// SF_MD3,
/*
Ghoul2 Insert Start
*/

	(void(*)(void*))RB_SurfaceGhoul,		// SF_MDX,
/*
Ghoul2 Insert End
*/
	(void(*)(void*))RB_SurfaceFlare,		// SF_FLARE,
	(void(*)(void*))RB_SurfaceEntity,		// SF_ENTITY
	(void(*)(void*))RB_SurfaceDisplayList,	// SF_DISPLAY_LIST
#ifdef STEFX_ELITE_FORCE_SP
	(void(*)(void*))RB_SurfaceAnim			// SF_MDR,
#endif
};
