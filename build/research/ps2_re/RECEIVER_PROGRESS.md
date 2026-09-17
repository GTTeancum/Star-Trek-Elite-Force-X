# Receiver progress — 2026-09-12

Receiving host is TARDIS. The transferred goal is active; memory reduction remains the gate before station/UI completion. Original projects, generic decompilation and logs are preserved. No production edits, builds or emulator launches have occurred in this receiver pass.

## Tool readiness

Old Python, Java, clean XDK5558 and VS8 MASM paths are absent. Bundled Python 3.12.14 is usable. Shared Ghidra 11.0.1 and the compiled R5900 extension exist. Exact Temurin 21.0.10+7 is being provisioned under tooling with its published SHA256 checked before extraction. The interrupted project has stale lock files; no Java/XEMU process was found in receiver preflight. Recovery uses a distinct project and output, without deleting those locks.

## Allocation investigation

`xbox_bsp_byte_inventory.json` records 103 raw BSP entries from source packages, with all 17 lump bounds validated. Examples: borg1 8,142,016 bytes; tour/deck04 10,369,668; tour/deck09 9,549,480; tour/deck02 7,250,464. These are file lengths, not live RAM savings. Source versions and package precedence must be resolved against the selected candidate ISO before using them in a runtime prediction.

`EFBSP_LoadFile` reads a whole selected raw BSP; `CM_LoadMap` frees it after collision and renderer construction. However, xbox0.pk3 also contains packed .mle sidecars, so raw loading cannot be assumed for every image. Existing STEFX SP raw paths already stream per-surface conversion and collision patches; the whole-array conversions under the alternate preprocessor branch are not new optimization opportunities for active SP. Next: confirm correct R5900 export, resolve PMP representation and selected Xbox load path, then account live allocations and lifetimes.

No PS2-derived memory saving is implemented or qualified yet. Full Virtual Voyager verification remains pending.

## Matching C: setup requested by user

Target paths remain the old host paths for machine interchangeability. The attempt to create C:\Program Files\Eclipse Adoptium was denied by Windows ACLs; full Codex access does not provide Windows administrator privileges. Python 3.12.10 signed-installer download/install command was rejected by execution policy before launch. Neither matching-path installation is complete. Located VS2005 Professional Disc 1/2 ISOs under Z:\Programming\VS2005. Located only XDKSetup5849.exe and its expanded XDK under Z:\Programming\Xbox XDK 5849; clean 5558 was not found in the inspected top two levels. Asked user for 5558 source location. Do not substitute 5849.

## PS2 representation found during receiver analysis

BORG1.PMP entry 709 (hash 75be6d3d) is a hash-indexed shader token table. `decode_shader_token_table.py` validates all 2,051 record ranges and token string offsets. Its 373,908 bytes comprise 16,412 table bytes, 273,580 token-offset bytes and an 83,916-byte string pool. There are 68,394 token references to 2,818 unique string offsets. The first definition resolves to valid shader syntax including surfaceparm fog/nodrop/nolightmap/nonsolid/trans, cull back and fogparms. Outputs: ps2_shader_token_table.json and ps2_shader_tokens.json. Executable lookup and lifetime remain unconfirmed.

Xbox ScanAndLoadShaderFiles currently holds every individually allocated shader file (compressed in-place without reducing original allocations), then allocates aggregate s_shaderText and copies/frees source buffers in reverse order. This is a confirmed source-level overlap to quantify. PS2 token precompilation is evidence of a representation strategy, not proof it is smaller than Xbox's COM_Compress output. A transferable change must preserve duplicate-definition search order, shader syntax semantics and runtime asset lookup.

Packed index feasibility audit validates record widths/ranges across 66 packaged maps. Within each triangle/face pass all VV ranges are forward-ordered. For deck09, maximum per-surface input is 288 bytes versus 545,646 whole-file bytes; deck04 540 versus 429,618. This is a theoretical input-buffer reduction, before stream/allocator overhead and actual runtime peak phase; no implementation or savings qualification yet.

Repository Java extraction completed with SHA256 08cae782814f027f8b159d6b68823f0f87422eb475c1a0ea1abc7a4357aaf11f; java -version confirms Temurin 21.0.10+7. Receiver Ghidra started in exec session 84153, Java PID 3816, with confirmed r5900:LE:32:default:default. Do not restart until terminal status is confirmed. Generic output and interrupted old project remain untouched.

## Common archive format correction

CD.PMP is an uncompressed archive with count followed by 12-byte [hash,offset,size] records. All 187 records validate; 90 entries start IBSP/version46 and have individually validated lump bounds. See cd_pmp_uncompressed_index.json and probe_cd_archive.py. The initial cd.pmp.index.json and common_archive_headers.json attempted the compressed 16-byte layout and explicitly failed bounds; they are superseded for CD.PMP and must not be used as an index. BIG.PMP and PMODELS.PMP do validate the 16-byte compressed entry layout (15,035 and 2,347 entries respectively). Source ISO has only been opened read-only.

This resolves where the map BSPs are stored, but their filenames remain hash-only until PMP lookup is decoded. Do not equate archive position to PC map order. PS2 world loader uses a streamed file handle and checks a 144-byte header/version46; inspect individual loader functions to determine which lumps are read, expanded or retained.

## Filename hash and matched map comparison

PMP lookup lowercases names (0x281a78), hashes them through 0x209968, then combines four words at 0x209a38 as product XOR sum modulo 2^32. Interpreting those words as a little-endian MD5 digest matches all 59 BSPs independently identified by exact entity content. See pmp_hash_validation.json and resolve_pmp_names.py. Direct MD5 core analysis is pending. This resolves 85/187 CD entries and 351/1541 BORG1 entries against canonical PAK filenames; unresolved names are not missing assets and hash collisions retain candidate lists.

maps/borg1.bsp resolves to CD hash 7cbaec44, offset in cd_pmp_named_index.json. Exact-length extraction and PC reference are under map_comparison, with per-lump SHA256/length comparisons in borg1_comparison.json. PS2 BSP 8,137,984 bytes versus PC 8,142,016: only 4,032 bytes smaller overall, with identical entity bytes but other lump differences. This is not proof of identical visuals/geometry or a PS2 RAM saving. PS2 surface loader checks 104-byte surfaces, 44-byte vertices and 4-byte indices, matching PC on-disk widths. Runtime surface descriptors appear to be 16 bytes each; detailed parser allocation and assembly validation pending.

Archive compressed-cache allocation at 0x20b2c0 is ((requested >> 11) + 2) * 2048 for positive requested lengths, freeing prior buffer before replacement. Opening a compressed package allocates count*16 for its index. A map package can coexist with shared package objects; total cache residency must account each owner, not assume a single cache for all archives.

PMP virtual export completed and saved 21 functions with assembly. Geometry/MD5 follow-up runs on the saved receiver project with a 900-second analysis limit in session 62770; do not stack another project writer.
