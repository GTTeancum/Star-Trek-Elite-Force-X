// client_ui.h -- header for client access to ui funcs
#ifndef __CLIENTUI_H__
#define __CLIENTUI_H__

#include "../ui/ui_public.h"

void _UI_KeyEvent( int key, qboolean down );
void UI_SetActiveMenu( const char* menuname,const char *menuID );
void UI_UpdateConnectionMessageString( char *string );
qboolean UI_ConsoleCommand( void ) ;
qboolean _UI_IsFullscreen( void );
#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP)
void SCR_DrawVirtualStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t shader);
qboolean UI_STEFX_ShouldDispatchGameplayMenuBind( void );
void CL_STEFX_SetObjectivesOverlay( qboolean active, const char *source );
qboolean CL_STEFX_ObjectivesOverlayActive( void );
void CL_STEFX_DrawObjectivesOverlay( void );
qboolean CL_STEFX_MissionFailedOverlayActive( void );
void CL_STEFX_DrawMissionFailedOverlay( void );
void CL_STEFX_RequestPauseMenu( const char *source );
void CL_STEFX_ServiceMenuRequests( void );
void CL_STEFX_ObjectivesOverlay_f( void );
void CL_STEFX_MissionFailedOverlay_f( void );
#endif

#endif //__CLIENTUI_H__
