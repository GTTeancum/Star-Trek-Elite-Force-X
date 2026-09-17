# R5900 loader findings — receiver first export

Evidence: receiver_loader_decompilation.json, 94 functions, language r5900:LE:32:default. Initial auto-analysis timed out at 300 seconds; post-analysis and project save succeeded. Treat inferred types and incomplete call discovery cautiously; assembly cross-check and completion pass remain required. Generic MIPS3 exports are superseded only for functions actually validated.

- 0x202510 switches the map package through an object constructed by 0x20b868. It closes/reuses the existing object before opening the new map package and clears the global pointer/state if opening fails. Confirmed object allocation request is 0x204 bytes.
- 0x20b868 initializes twenty 24-byte file-handle records within that object, with pointer/state fields and a vtable per handle.
- 0x20b398 is an archive read-cache fetch, not itself decompression. It checks an existing byte range, otherwise aligns the requested disk offset down to 2048 bytes and reads into the object-owned cache. Cache allocation/resizing is an indirect vtable call and still needs resolution. This corrects the tentative compressed-fetch label.
- 0x20b658 has two modes. In one, it fetches stored entry bytes from the archive cache and invokes 0x20adc8 into the caller destination, requiring the resulting length to match the raw-size table field. On mismatch it invalidates the cache and retries. In the other, 0x20b5b8 advances a pointer inside an already resident decoded entry and bytes are copied to the caller.
- 0x20b5b8 bounds the cursor against resident entry base plus raw length. This supports a per-file full-residency mode as well as direct whole-entry decode; PS2 is not simply streaming every arbitrary partial read from compressed disk data.
- 0x2009f0 tries ordinary filesystem sources and then up to three package objects via virtual open, registering package-backed handles distinctly. The PMP filename hash is therefore in the unresolved virtual open implementation, not established by the generic 0x1ffef8 filesystem hash call.
- 0x1fd208 aligns requested permanent hunk bytes to four, invokes 0x2834e8 on pressure, checks again, updates permanent/temp marks and zeroes the returned region. The pressure handler remains unresolved; do not assume it frees unused model slots.

Next: resolve PMP vtables from ELF segments; export virtual open/cache-resize plus their exact callees, and complete analysis on the saved receiver project. No Xbox saving can be attributed to these mechanisms until the Xbox allocation/lifetime comparison and runtime measurement are complete.
