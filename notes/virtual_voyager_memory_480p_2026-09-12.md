# Virtual Voyager memory review and 480p — 2026-09-12

User priority: 480p before anti-aliasing. AA stays off. User manually played the second run and requested closure; no emulator remains owned by this run.

## Findings from the captured run

Source: build/research/virtual_voyager/manual_runs/stefx_vv_memory_20260912_115517/. 59 distinct memory reports, final64MiB read-only RAM snapshot, continuous flight records and ring history retained. This is sampled local gameplay evidence, not exhaustive campaign peak coverage.

| Memory | Observed | Interpretation |
|---|---|---|
| Physical free | 0 to2,220,032bytes | Real pressure at the worst samples. |
| Zone free | 220,571 to9,721,433bytes | Do not shrink the zone based on the earlier5.3MiB free snapshot. |
| Largest zone block at peak use | 211,800bytes | Little room for a new large allocation. |
| Static texture pool | 10MiB reserved; sampled maximum7,828KiB used | About2.36MiB slack at sampled peak; not a safe global reduction yet. |
| Model texture pool | 4MiB reserved; sampled maximum417KiB used | Strongest budget candidate. A2MiB pool could recover2MiB, pending validation. |
| Audio allocation | sampled maximum962,188bytes | The6MiB sound cvar is a ceiling, not a reserved6MiB allocation. Reducing it does not directly reclaim5MiB. |
| Save/Bink scratch | static2.5MiB array | Potential lifetime improvement, but moving it to on-demand allocation can make save/load fail under pressure; defer until measured. |

The texture pools really allocate their complete capacity with D3D_AllocContiguousMemory before zone sizing. Freed startup pool capacity would currently enlarge the zone automatically; it would NOT automatically increase physical free memory for later D3D allocations. A rebalance must deliberately decide how much goes to the zone versus physical reserve. No pool, scratch, sound, mip or zone budgets changed in this pass.

Model pool Size() is a bump pointer that resets on a swap. Consequently417KiB sampled usage alone does not establish the total working set or exclude missed swaps. The long frame log lines truncate before the skin-swap tail, so there are no complete frame-profile pool records to substantiate zero swaps. Add bounded high-water and swap reporting before qualifying a2MiB candidate across populated decks, map transitions, save/load and existing modes. Preserve visual detail and avoid additional disk churn.

Final RAM exported counters independently decode to clientstate7, advancing mainloop23557, physicalfree471040 and audio417904bytes. No Com_Error appeared in captured ring history. User-requested termination accounts for exitcode15; this is not a crash report.

## 480p source correction

code/win32/win_qgl_dx8.cpp incorrectly nested the480p capability check and progressive presentation flag inside widescreen selection. Both are now independent. Presentation interval resets retain scan/aspect flags instead of clearing them. Rendering stays640x480 and D3DMULTISAMPLE_NONE. 480p changes the scan mode, not the pixel count relative to640x480 interlaced output.

Both SP and SP-hosted Holomatch builds passed using5558 with FrameDiagnostics, SkipAssets, SkipStage and ReuseObjects. Evidence: build/research/virtual_voyager/480p_build.log. Separate candidate: StarTrekEliteForceX_virtual_voyager_480p_candidate.iso; previous testing ISO is unchanged. Matching maps retained in480p_candidate_symbols. Package verification is in the candidate manifest.

Runtime4:3/widescreen progressive selection and presentation resets still require testing. No runtime480p or image-quality pass is claimed. Current release executable/maps now belong to this candidate: DO NOT attach them to the older testing ISO. The old manual launch script hardcodes the previous ISO and a mutable release-map path; do not reuse it unchanged.

Silence, over-downsampled textures and the prior portal-reference error remain unresolved. This change does not fix them or resume paused muddy-voice tuning.
