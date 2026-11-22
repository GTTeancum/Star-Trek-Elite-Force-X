# Minimum geometry LOD in two-player Forge 4

Three boot pairs on the 733 MHz, 64 MB retail Xbox averaged 18.13 FPS at normal detail and 19.56 FPS with all available geometry LODs forced to their minimum. The difference was +1.43 FPS. The slowest observed window was 16.16 and 16.83 FPS respectively. This test measures the open Forge 4 fight. Live enemy behavior and visibility varied between boots.

| Forge 4 pair | Normal FPS | Lowest FPS | Difference | Normal slow window | Lowest slow window |
|---|---:|---:|---:|---:|---:|
| 1 | 18.36 | 19.27 | +0.91 | 16.96 | 16.83 |
| 2 | 17.88 | 19.54 | +1.66 | 16.16 | 17.39 |
| 3 | 18.14 | 19.86 | +1.72 | 16.22 | 17.95 |

These timings came from the local tester with the earlier split-screen optimizations enabled. The same instrumented Xbox Release binary and game data ran both arms in 480p, 4:3. Each boot used the same starting room and input replay, and the order alternated. Both players moved and fired, both were alive in the retained interval, and opponents fired in each run. Timing used complete gameplay publications in server seconds 6–26. Pooled FPS uses the total frame count divided by total elapsed time. Slow windows use contiguous whole publications covering at least ten seconds. Native captures were taken after timing. The separate comparison-camera boot contributes no FPS samples.

| Submitted triangles per main-loop iteration, both views combined | Normal | Lowest |
|---|---:|---:|
| Skeletal meshes (MDR) | 8,424 | 3,769 |
| MD3 models with lower meshes | 1,415 | 550 |
| Curved map patches | 956 | 418 |
| Fixed-detail MD3 models | 3,446 | 3,278 |

The back end submitted 105,239 skeletal surfaces from LOD2 and 34,400 eligible MD3 surfaces from their actual lower-file allocations. Both cameras recorded zero eligible full-detail submissions in each forced gameplay interval. All 192,853 patch submissions used the minimum legal row and column selections, which retain shared borders and mandatory authored rows. All 326 observed game-side cylinders used seven segments. Retail renderer cylinders and lathes also select their minimum tessellation in code. Their observed counts in these intervals were 0 and 0 respectively.

The Xbox model loader normally pins MD3 files to slot zero. The study loaded the actual coarsest numbered sidecar alongside the base, with base-model tag interpolation intact, in both timed configurations. Across the installed game data, 60 base MD3 paths have lower meshes. Forge 4 submitted lower heads for Alexandria, Avatar, Munro and Reaver and lower world models for the phaser, compression rifle and scavenger rifle. The first-person phaser and compression rifle each have one authored MD3 mesh. Their world models each have a lower sidecar. Planar BSP surfaces and triangle soups have fixed topology. The previous skeletal-only study measured 17.58 and 18.31 FPS in its separate six boots.

The ordinary 720p single-player run set the test switch explicitly, advanced 527 main-loop frames and submitted 27,516 LOD0 skeletal surfaces. The co-op geometry counters stayed zero. Both hardware stages restored all 16 original files, including all nine saves, by readback. These measurements used the opt-in experiment in the local build directory.

The local test archive under `build/coop-all-lod/` contains the native same-pose comparison, mesh wireframes, per-run counters, and experimental build script.

Timed executable SHA-256 `59dbf1d8f8a9aa6df36284e7b798241cdbd7dca7966ecb33c78c7f4945b41556`. Comparison-camera executable SHA-256 `9b33c33814509efcadbb4577358d00da50b12404c65bb45aa9037056705e1432`. The second executable changes camera placement and player visibility only for the dedicated visual comparison.

## Co-op release default

The local SP/co-op Release executable now selects the lowest authored geometry in both co-op views at every distance. The MD3 loader discovers numbered meshes directly from the installed game data and keeps base-model tags for attachments. The Forge 4 hardware smoke used the stock PAKs for the lower meshes and ordinary co-op settings. The log recorded LOD2 skeletal selection in each camera and loaded all seven expected lower heads and world weapons. Both players moved and fired. Complete gameplay timing records averaged 19.55 FPS. A separate 720p single-player smoke showed the opening fight with normal detail. The Xbox returned to the dashboard, with all nine saves verified by hash.

The tested local release executable had SHA-256 `9f69528a835f74bab5e8e8a974811d7a951246505f0c50ad5412ebac88450ef8`. The smoke captures, timing records, and restoration manifest are archived locally under `build/coop-lod-default/`.
