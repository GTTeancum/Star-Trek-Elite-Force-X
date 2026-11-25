#ifndef STEFX_SAVE_MENU_H
#define STEFX_SAVE_MENU_H

#if defined(_XBOX) && defined(STEFX_ELITE_FORCE_SP) && !defined(STEFX_SP_HOSTED_MP)
// Internal queued command for a submission from the native EF Save Game UI.
// Keep the token four characters, like "save", for the same command capacity.
#define STEFX_EF_MENU_SAVE_COMMAND "efsv"
void SV_EFMenuSaveGame_f(void);
bool UI_EFSave_HasLegacyWait(void);
#endif

#endif
