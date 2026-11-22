# Xbox video, campaign lighting and split-screen performance

The retail tester configuration uses `scripts/build_xbox.ps1 -Target spmp -SplitPerformanceDefaults 15 -HolomatchWorldMode 2 -SkipStage`. This builds the campaign/co-op and Holomatch executables. Rebuild the SP patch package to pick up the lightmap change. The installed game PAKs already contain the lower-detail model meshes.

## Video, lighting and sound

- Single player uses native 1280 × 720 when the dashboard and AV pack enable it. Multiplayer renders at 640 × 480, with 4:3 pillarboxing on widescreen signals. The display code also handles 480i and 480p, scales the safe-area margins, and maps menu and loading-screen coordinates to the active display.
- Campaign lightmaps now use the brightness authored in the BSP. The package regression compared 15,810,560 texels in 965 atlases across 66 maps, along with surface assignments and texture coordinates. Holomatch retains its previous lightmap gain.
- The soundbank path keeps the registered sound name for voice and effect routing when the optional file list is absent. Four-player Holomatch can reuse an idle DirectSound buffer with a compatible route.

## Retail performance

The retained changes include split-screen cvar lookup, per-view BSP visibility, reuse of actor materials and compressed attachment bones, server collision and NPC query work, and shared world geometry in Holomatch. GPU actor skinning remains an opt-in four-player Holomatch setting. Co-op forces the lowest authored geometry in both views. [Co-op geometry measurements](coop_minimum_geometry_2026-11-22.md)

| Retail workload | Before split-screen cvar indexing | With indexing | Slow window with indexing |
| --- | ---: | ---: | ---: |
| Forge 4, two co-op players | 15.35 FPS | 17.99 FPS | 16.00 FPS |
| High Noon, four local players and four bots | 20.51 FPS | 23.95 FPS | 20.62 FPS |
| Voyager, four local players and four bots | 21.78 FPS | 32.72 FPS | 29.02 FPS |
| Scavenger, four local players and four bots | 31.85 FPS | 47.70 FPS | 43.69 FPS |

Each row pools two cold boots per build on a 733 MHz, 64 MiB retail Xbox. Run order was reversed for the second pair. The averages use complete gameplay frame-time records. The slow window is the lowest rate across adjacent records spanning at least ten seconds. These comparisons preceded the co-op LOD change. A separate six-boot Forge 4 comparison measured 18.13 FPS at normal detail and 19.56 FPS at minimum detail. Live combat varied between boots.
