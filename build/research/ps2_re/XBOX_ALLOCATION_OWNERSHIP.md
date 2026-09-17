# Xbox allocation ownership — initial receiver audit

Status update 2026-09-13: completed R5900 exports and RedRocket control snapshots now refine this table. No paired runtime saving claimed here yet; see notes/receiver_redrocket_2026-09-13.md. The original source/package audit below predates the applied index-streaming candidate.

| Allocation | Owner / lifetime | Byte accounting | Implication |
|---|---|---|---|
| Raw BSP file | EFBSP_LoadFile / EFBSP_FreeFile, spanning raw collision + renderer construction | Exact per-file lengths in xbox_bsp_byte_inventory.json | Only used when packed misc.mle probe fails; do not attribute this whole-file overlap to packed VV maps. |
| Packed vertices | LumpStream in CM_LoadMap, open through patch/triangle/face construction | deck04 file 6,157,196; deck09 5,916,612; deck02 4,200,972 | Already streamed. File length is not live allocation. |
| Packed faces | LumpStream, only face construction | Per-map lengths in xbox_packed_lump_inventory.json | Already streamed; per-surface scratch still requires accounting. |
| Packed indices (original path) | Lump indexes in CM_LoadMap, triangle construction through faces and flares until scope exit | deck04 429,618; deck09 545,646; deck02 278,154, plus allocator overhead | Whole FS_ReadFile allocation. These exceed its 256KiB external heap threshold; they are not necessarily zone allocations. Deck09 control measured 548,864 physical bytes committed at index allocation, zone counters unchanged. Candidate replaces with an 8KiB zone scratch plus ZIP stream state; paired measurement pending. |
| Packed triangle descriptors | Lump trisurfs, triangle construction only; clear immediately afterward | Per-map inventory | Already lifetime-bounded. |
| Packed patches | Lump patches, collision + renderer patches then clear | Per-map inventory | Already lifetime-bounded; collision retained representation and scratch need separate counts. |
| Other collision lumps | Reused outputLump; each load clears previous buffer | Per-map inventory | Input/output coexist while each converter runs; no accumulation of all source lumps. |
| TempAlloc sandbox | Static s_TempAllocPool in executable | 2,621,440 bytes | Live static storage, but save/Bink lifetime and guaranteed availability prohibit assuming safe removal. |
| Ghoul2 bone pool | Static TheBonePool in linked tr_ghoul2.obj; also lends buffers to save compression and Bink | 100 pages of1024 compressed quaternions plus owner/touch fields and manager metadata, approximately1.4MiB; exact ABI size requires accounting | Not an unused EF skeletal reserve: sv_savegame CompressMem_AllocScratchBuffer and BinkVideo::Start call BonePoolTempAlloc. Save full-buffer storage and compressed output coexist; they cannot simply alias the2.5MiB TempAlloc sandbox. |
| MODEL_MEM reserve | Zone budget arithmetic | Sum of declared constants | Not proof of allocated slots: AllocateModelSlots is disabled. |

Evidence: code/qcommon/cm_load_xbox.cpp packed dispatch and construction; code/qcommon/qcommon.h Lump and LumpStream; code/qcommon/ef_bsp_xbox_shared.h EFBSP_XboxPackedLumpsExist; code/qcommon/z_memman_console.cpp. Current package has 66 maps with .mle entries. Candidate ISO identity still requires direct verification before runtime use.

Next measurements must separate load transients from final residency and show physical free, zone free/largest block and peaks. A reduction in input scratch may help peak contiguous allocation without increasing steady-state physical free; label results accordingly.
