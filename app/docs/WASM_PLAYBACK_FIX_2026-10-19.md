# Qt WebAssembly playback repair

The affected deliverable is the compiled C++/Qt app served from
`C:/catchim-wasm-mt/bin` on port 8766, rather than the React editor in `web/`.

## Causes and changes

- `PlaybackAudioSession` held its UI state mutex across `writeSamples()`. In
  WASM that call waits for a browser-main-thread copy. The UI timer could block
  on the same mutex, preventing the copy callback from running. Release the
  mutex before writing and recheck the session serial afterward, so stale
  writes cannot publish their position after a pause or seek.
- Send owned audio packets asynchronously to the browser instead of waiting
  on a futex for every 20 ms packet. Reserve queued frames before dispatch,
  free each packet after its browser copy, and discard obsolete generations.
  A browser-only 200 ms queue target tolerates longer Qt paints without
  starving output. The native backend retains its existing latency target.
- `Atomics.notify()` was given `HEAPU32`. JavaScript only permits an Int32Array
  or BigInt64Array for this operation. The resulting exceptions prevented
  immediate wakeups and introduced approximately one second per audio packet.
  Notify with `HEAP32`, viewing the same shared bytes as signed integers.
- Browser video records were keyed only by file path. Thumbnail seeks and
  live playback therefore shared one HTMLVideoElement. Pass the C++ reader key
  into the bridge and give each stream its own element. Wait for decoded data
  before validating dimensions.
- Pass explicit playback state to the video decoder instead of guessing from
  the spacing of frame requests. Browser paint delays previously converted
  normal playback into repeated exact-frame seeks. Use native continuous
  playback with bounded drift correction, retaining exact seeks when paused.
  Do not invalidate in-flight preview frames for small audio-clock corrections;
  actual seek and play-state callbacks still invalidate them. Limit the WASM
  GUI tick to 30 Hz so media callbacks have time to run.
- Bound the browser's live preview readback/compositor to 480x270. Pausing
  restores the normal preview resolution automatically. This only changes
  preview processing; exports continue to use project resolution.
- Publish continuously decoded frames into a single bounded shared mailbox
  (32-byte header plus 480x270 RGBA capacity, owned for the WASM module's
  lifetime). A nonblocking atomic ownership word prevents simultaneous reads
  and writes; browser callbacks skip a busy slot. Stream IDs and timestamps
  prevent stale sources from being used after a seek or clip switch. This
  removes the per-frame worker/main-thread/futex round trip. Reverse playback,
  speed changes and paused inspection retain exact asynchronous decode.
- Extrapolate the browser playback clock between WebAudio packet notifications;
  only resynchronize when drift exceeds 100 ms. Repeatedly seeking to a coarse
  packet position otherwise repeats video frames between notification batches.
- An unspecified CMake build type compiled the pixel compositor at `-O0`.
  Default an unspecified single-configuration WASM build to Release; explicit
  build configurations remain supported. C++ uses Release optimization; the
  final WASM link uses `-O1` to avoid an expensive second full optimization pass.

## Repeatable checks

Run browser bridge regressions with:

```powershell
node --test app/tests/wasm_browser_bridges.test.cjs
```

To build the optional browser integration test, configure the Qt multithreaded
WASM build with `-DCATCHIM_WASM_PLAYBACK_TEST_MEDIA=<absolute MP4 path>` and build
`catchim_wasm_playback`. This embeds the fixture only in the separate test
executable. Serve its generated HTML with the same COOP/COEP headers as the app.
Click Play to satisfy the browser's audio gesture requirement.

After a two-second warmup following audio/video readiness, the test measures
actual preview frames, distinct mailbox publications, clock advancement and GUI timer
heartbeats, then verifies pause, seek to 18 seconds and resume. The fixture must
have at least 23 seconds of video and audio. `WASM_*` console messages report
each result. The playback threshold is over 90 frames and 4.5 seconds of
timeline advancement during a six-second measurement.

## Final browser result

Chrome, original `E:/1004(1).mp4` (2030x1080 HEVC, 30 fps), 2026-10-19:

- Play: PASS; 6.03142 seconds advanced during six seconds.
- 122 preview presentations, 117 distinct decoded/publication frames (about
  19.5 new preview frames/second), 133 GUI heartbeats.
- Last worker render: 8.4 ms; live preview canvas: 480x270.
- Pause, exact seek to 18 seconds, and resume: PASS.
- Resume: position 21.9268 seconds with 77 preview presentations in four seconds.
- Browser bridges: 4 tests passed.

Evidence: `app/qa/wasm-playback-2026-10-19/results.json` and `result.jpg`.
The production app JS/WASM in `C:/catchim-wasm-mt/bin` was rebuilt and is served
by the existing port-8766 server. Reload the old page to use the new runtime.

Bridge unit checks: 4 passed, including signed atomic wakeups, packet
reservation/reset generations and isolation between filmstrip and live
playback. An intermediate unoptimized browser build
remained responsive but presented only 29 frames in six seconds, motivating
the Release build and stricter integration threshold.
