# Campaign level-entry timing (#6)

Keep the loading screen, authored movies, first rendered world frame, first
controllable frame and first 20 seconds of movement separate. The source
changes below remove avoidable work; none measures those intervals on an
Xbox or proves why the player's copy felt slow.

* **First scene:** Six routine interior first-frame checkpoints still write to
  the 32 KiB RAM log mirror and to the same file when a disk sink is active.
  Instead of synchronously flushing each one, retail single-player uses the
  existing 32-write / 64 KiB limit. Begin/end and first-render-complete remain
  durable, as do errors, unknown stages and save checkpoints. On the full
  nine-marker path with no other intervening records or expired budget, forced
  file flushes drop from nine to three; the nine writes remain. An early return
  or a different render path need not emit all nine. Verbose and qualification
  modes keep the old flush behavior; hardware-frame diagnostics can use RAM only.
* **Saves:** The Xbox native campaign UI and engine-initiated saves no longer
  spin to fill one second **after a successful fast save** when there is no
  legacy Saving popup or pending UI script. Real writes/validation, published
  failure behavior and deferred UI completion remain. An ordinary console
  slot save still uses its old timing. For an eligible new map
  `SV_ClientEnterWorld` writes both the rolling checkpoint and its distinct
  named entry save after `ClientBegin`, before it returns to process normal
  play. The normal New Game menu queues a *direct map*, which skips that
  entry-save branch while the direct-map flag is set. The authored
  `target_autosave` path can separately issue `save auto*` during play.
  Do not assume that it fires at entry, or on every map. The wait was a
  minimum including the I/O, not one extra second on top of the I/O.
* **Model registration:** The non-Borg player-model streaming path reuses the
  already measured compression choice when writing each frame block. There is
  no extra cache allocation or disk read; the original bytes and tie-breaking
  are preserved. Direct comparison of 14 Raven demo upper/lower MDRs exercised
  the actual loader, with host file and heap adapters. That is registration CPU
  work, not whole-map time, live FPS, Borg-only coverage or a report of when
  those particular models appear.

## How to test the symptom

Use the same console, drive, game data, difficulty, resolution, pre-transition
save and inputs on both builds. Leave the trace switches, verbose mode and
`stefx_console_qualification` **off** for the performance comparison. Take a
cold run from a fresh launch and a warm repeat of the same transition in the
same process on each build. Also run New Game through the ordinary menu; do not
substitute a developer map shortcut. Align external video on the first
controllable frame, mark any movie and any checkpoint separately, and follow the
same movement for 20 seconds. Compare the opening separately from later motion.

On a lab console with the matching binary's symbols and an authorized read-only
monitor, use the sampling logic in `scripts/console_split_perf.py::sample`
to capture the live 32 KiB log mirror **and** the separate FPS publication ring.
This sampling logic is reusable; its full split-screen deployment command is
not a single-player route. Preserve both rings before wrap; record the dropped
counts and do not treat missing records as good frames. Match first-active /
first-render / save boundaries with the recording. Compare complete
`ft=count/elapsed_ms/max_ms/p95_ms/p99_ms/over50` windows labeled
`gameplay=1 excludedChecks=0`. Keep windows crossing the load separate; these
are windowed summaries, not per-frame events. The capped `STEFX_LONG_FRAME`
records (first ten completed frames >=250 ms per process) cannot rule out
continuous sub-250 ms stutter. Without a matching monitor, use synchronized
video and the ordinary game log rather than guessing about frame time.

Host-only regressions: `python3 scripts/tests/verify_issue6_save_menu.py`
(dispatch and save/UI lifecycle); `python3 scripts/tests/verify_campaign_entry_log_io.py`
(and `--baseline` for the v0.9.4 release-manifest source). They do not
substitute for the paired console runs.
