# Virtual Voyager runtime checks

All twenty source maps passed the packed-data audit. This table tracks runtime checks separately. A load pass does not establish complete routes or interaction coverage.

| Map | Runtime load | Native visual review | Save/load |
| --- | --- | --- | --- |
| _brig | Passed, live/error-clear through authored failure | Early room/forcefield/NPC captures and authored mission-analysis screen reviewed; no missing models | Authored cinematic/death retains normal save restrictions; no ordinary roundtrip claimed |
| _holodeck_camelot | Passed, including reload | Three native captures checked; intact scenery/weapon/HUD, zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_firingrange | Passed, including reload | Three native captures checked; source supports dark fog/sky, zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_garden | Passed, including reload | Three garden courtyard captures checked; zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_highnoon | Passed, including reload | Three alive courtyard/stairs/passage views checked; weapon/HUD intact | Two writes/reload passed, restored VV mode |
| _holodeck_minigame | Passed, including reload | Three arena/overhead captures checked; zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_proton | Passed, including reload | Three rocky-passage/doorway views checked; weapon/HUD intact, local coverage | Two writes/reload passed, restored VV mode |
| _holodeck_proton2 | Passed, including reload | Three stairwell/panel captures checked; zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_temple | Passed, including reload | Three corrected-harness captures checked; god verified, zero missing-model submissions | Two writes/reload passed, restored VV mode |
| _holodeck_warlord | Passed, including reload | Three room/props/panel captures checked; weapon/HUD intact | Two writes/reload passed, restored VV mode |
| tour/deck01 | Passed, including reload | Three post-movement/reload bridge captures checked; no obvious visible fault | Two writes/reload passed, restored VV mode |
| tour/deck02 | Passed, including reload | Initial room and three post-movement/reload quarters captures checked | Passed named overwrite/reload and cold UI slot load, VV mode restored |
| tour/deck03 | Passed, including reload | Revised walk/turn: doorway and corridor inspected; third capture during reload | Two writes/reload passed, restored VV mode |
| tour/deck04 | Passed, including reload | Three corridor captures after movement/reload checked; no obvious visible fault | Two writes/reload passed, restored VV mode |
| tour/deck05 | Passed, including reload | Three wider captures checked: corridor beyond lift, intact panels/lights/HUD; local coverage | Two writes/reload passed, restored VV mode |
| tour/deck08 | Passed, including reload | Wider corridor checked; blue effect confirmed authored forcefield by shader and exact surface coordinates | Two writes/reload passed, restored VV mode |
| tour/deck09 | Passed, including reload | FIX VERIFIED: NPC restored after reload; three captures checked, zero missing-model submissions | Two writes/reload passed, restored VV mode |
| tour/deck10 | Passed, including reload | Corridor/door junction before and after reload checked; no obvious visible fault | Two writes/reload passed, restored VV mode |
| tour/deck11 | Passed, including reload | Three wider corridor/doorway captures inspected; no obvious visible fault | Two writes/reload passed, restored VV mode |
| tour/deck15 | Passed, including reload | Three maintenance corridor/equipment captures checked; no obvious visible fault | Two writes/reload passed, restored VV mode |
