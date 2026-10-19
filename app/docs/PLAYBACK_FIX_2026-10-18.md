# Native playback / timeline repair — 2026-10-18

## Scope

Repair the reported timeline zoom, HEVC playback stalls and broken audio in the native Qt/C++ editor. No web UI was embedded, no existing server/port was changed, and the user's source video was not modified.

Exact input: `E:\1004(1).mp4`, SHA256 `A32D64A4A4DAA269ECEFB097846230FCE5E713A7CA76E3D6F41E49008E7C93DA`.
HEVC 30 fps, 2030×1080 visible / 2048×1088 coded, 59.420 s; AAC 44.1 kHz stereo.

## Repairs

- Timeline zoom spans 1–5000 pixels/second rather than starting at 441 pixels/second. Initial insertion fits the whole clip. A single manual scroll offset is used consistently for clips, ruler and hit-testing; labels stay fixed. Zoom anchors the visible playhead, wheel scrolling works, and explicit paused seeks reveal the playhead with an inset rather than rounding it outside the content edge.
- Preview decode and compositing run on a dedicated worker with its own Media Foundation/D3D decoder. Thumbnail seeks no longer contend for that decoder's mutex. Only one replaceable pending preview request is kept; generations reject obsolete results after seeks/edits. GUI painting only blits a completed image. Preview compositing uses bounded visible resolution; export resolution is unchanged.
- NV12 conversion uses SSE2 on x64, honors decoder matrix/range attributes, and crops the valid display aperture. Seeking no longer stops at an arbitrary 60 samples before reaching the target keyframe interval.
- Audio decoding, mixing and Windows device writes run off the GUI thread. WASAPI consumes the exact 48 kHz float stereo mixer format and performs endpoint format conversion. The write API returns accepted frames; unaccepted tails are retained and retried. Playback follows consumed hardware queue frames instead of advancing past discarded audio.
- AAC priming recorded in a simple MP4 audio edit list is removed when the MF reader retained it. Audio cache keys include sample rate and channel count. Pause/seek/edit invalidates old buffered work.
- Immutable project/media snapshots isolate background readers from GUI edits. Paused seeks refresh the image, and stopping at the end retains the final preview frame.

## Verification

`catchim_playback_regression` reads the exact user file and exercises real Qt widgets, a real Windows audio endpoint (test output volume zero), cold decoding, continuous playback, zoom extrema, end scrolling, ruler hit-testing, paused/rapid seeks and final-second frame accounting. It compares decoded PCM and video frames against independent local FFmpeg outputs. This is pipeline verification, not a claim of having subjectively listened to the sound.

Raw results and screenshots: `app/qa/playback-1004/`.
The final full-length measurement is recorded in `results-final.json`; the earlier 58-second run is retained in `results-endurance.json`.

Final run: 30/30 checks passed. The continuous measured window was 58.019 s at 29.990 fps, after cold startup and warm-up; the last second was also tested separately through automatic stop. Hardware queue underruns: 0. GUI 5 ms timer p95/max: 6.541/14.653 ms. Maximum preview age: 80.3 ms. Maximum playhead versus consumed-audio-queue clock difference: 10.3 ms. Cold audio preparation: 764 ms, off the GUI thread. Paused seek at 18 s reveals the playhead at viewport x=136.

48 kHz stereo audio/reference correlation: 0.998672, alignment lag 0 frames, duration difference 2 frames (0.0417 ms). Full-resolution frame RGB mean absolute error at 18/17.9/55 s: 0.9465/0.9486/0.9568 on the 0–255 scale. These frame comparisons also verify cropping to 2030×1080.

Additional regression: 241 core suites and the native workflow checks (drag/undo, context actions, mute, hidden clips, mixed/muted export, export geometry/duration and cancellation).

Reproduce from `app` after running `tools\build-ui-parity.cmd`:

```powershell
$env:QT_QPA_PLATFORM_PLUGIN_PATH='D:\Qt\6.8.0\msvc2022_64\plugins\platforms'
$env:QT_SCALE_FACTOR='1'
$env:CATCHIM_TEST_PLAYBACK_MS='58000'
.\build\bin\catchim_playback_regression.exe -platform offscreen
.\build\bin\catchim_native_workflow.exe -platform offscreen
.\build\bin\catchim_tests.exe
```

## Limits

Measured playback is 1× with this exact source on this machine. This does not certify every codec, very long/many-layer projects, all web features, or pixel-identical browser rendering. J/L shuttle audio is not enabled by this update's 1× audio session wiring. TTS/ASR and the other previously documented parity gaps remain separate work; this report does not relabel them complete.

Audio sources are still cached as full PCM buffers, so memory use and cold preparation time grow with source length. Preview can discard obsolete frames if a future source is slower than real time; it must not queue an unbounded lag behind audio.

## Delivery

`E:\DATA\source\catchim\Catchim-PlaybackFix-20261018-1804.exe`, 4,006,400 bytes.
SHA256: `C2F1DDB45ABE090D195062DF5C994C503B1D8F2EF2B9A1DC08D38229F71907FD`.
The packaged executable matches the tested Release build. Its `--version` smoke test exits successfully. Keep it beside the existing Qt 6.8 DLLs and `platforms` directory; earlier delivered executables were not overwritten or terminated.

## Implementation references

- [WASAPI automatic PCM/sample-rate conversion](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audclnt-streamflags-xxx-constants)
- [Media Foundation display aperture](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-mt-minimum-display-aperture-attribute)
- [Media Foundation audio source-reader configuration](https://learn.microsoft.com/en-us/windows/win32/medfound/tutorial--decoding-audio)
