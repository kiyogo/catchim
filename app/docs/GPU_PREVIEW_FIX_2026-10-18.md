# Native GPU preview and UI cleanup — 2026-10-18

## Requested changes

- Suppress the floating filename/duration popup over timeline clips and filename popups in the media grid. Button/track-control help remains available.
- Remove the media list's extra inner frame. Keep the outer Assets panel frame.
- Replace the full-width cyan splitter hover stripe with a small neutral grip; preserve native splitter dragging and the existing panel proportions.
- Move expensive video processing to the GPU, investigate remaining playback stutter, and run the reference web application without taking another project's port.

## GPU path

The previous native decoder already supplied a D3D11 device manager to Media Foundation, but copied full-resolution NV12 surfaces to CPU memory for color conversion and scaling. Creating a D3D device alone was not evidence that the whole preview path was GPU-processed.

The preview now obtains the decoder texture through `IMFDXGIBuffer`, including its array subresource, and uses `ID3D11VideoProcessor::VideoProcessorBlt` for display-aperture cropping, NV12-to-RGB conversion and preview-size scaling. Only the small RGB output is staged/read back for the native Qt compositor. The original source dimensions are returned separately so scale, rotation and position remain in project coordinates. Export/default full-resolution validation is not reduced to preview resolution.

Hardware-adapter selection checks video decoder profile support and prefers dedicated video memory. This avoids assuming that the first/default adapter is the real GPU when a virtual display adapter is also installed. Confirmed device on this machine: **AMD Radeon RX590 GME**.

Per-preview diagnostics count actual DXGI surfaces, successful GPU processing calls, CPU fallbacks, readback bytes and the last GPU error. Filmstrip/media thumbnails use a separate small GPU decoder. New filmstrip jobs are not scheduled while playback is active; cached covers are reused and missing thumbnails are filled when paused.

The production GUI tick now explicitly uses a precise 16 ms timer. Timeline playhead invalidation runs every tick instead of every third tick, and repaints narrow old/new playhead strips instead of invalidating the whole timeline for each movement.

## Verification

Exact input is still `E:\1004(1).mp4` (HEVC, 2030×1080 visible, 30 fps, AAC 44.1 kHz stereo, 59.420 s). The original file was read, not modified.

The GPU test checks real painted-frame cadence, not only decoder callback counts. It also checks filename popup suppression, rendered border pixels, neutral splitter hover, zoom/scroll/seek, hardware audio queue accounting, independent PCM/video references, original source metadata and transformed render geometry.

The first border assertion incorrectly treated Qt's `frameWidth()` as border-only. Qt includes the 4 px stylesheet padding in that value. The assertion was corrected to compare eight composed edge/background pixel pairs; it would still detect the original visible border. No visible border remains.

Before the final timer/playhead polish, a complete measured 58.015 s window produced 1772 GPU frames, zero CPU fallback frames, 29.992 painted fps, maximum preview age 65.0 ms and zero steady audio underruns. GPU/CPU image mean RGB differences at 18/17.9/55 s were 1.121/1.117/1.085 on the 0–255 scale; transformed geometry comparison error was 0.454.

Independent Windows `GPU Engine` performance counters, filtered to the regression process (PID 53488 during that run), recorded Video Decode utilization of 13.447% and 14.302%, and 3D utilization of 1.937% and 2.039%. These are process-specific observed samples, not global GPU utilization or an inferred capability flag.

The final production-like timing run uses a coarse 50 ms observation timer, so the test itself does not introduce a separate fast 5 ms timer. Results/screenshots are in `app/qa/gpu-playback-production/`. The preceding 5 ms timer run is in `app/qa/gpu-playback-1004/`.

The production-like run after the precise-timer/playhead change passed **44/44** checks over a 58.005 s measured window: 1773 GPU frames, 0 CPU fallbacks, 29.997 painted fps, maximum preview age 65.4 ms, and 0 steady audio underruns. After the final idle-repaint guard/unused-label cleanup, the packaged build passed the same 44 checks at DPI 150% over 8.025 s: 30.031 painted fps, 271 GPU frames, 0 CPU fallbacks and 0 underruns. That final-build report is `app/qa/gpu-final-dpi-1.5/results.json`.

The 241 core suites passed. The final native workflow regression passed 62/62 checks at both DPI 100% and 150%, including dragging/undo, context actions, hidden/muted clips, mixed and muted export, geometry/duration and cancellation. Targeted `git diff --check` passed.

## Web

The project at `E:\DATA\source\catchim\web` was started in a hidden background Node process with explicit host `127.0.0.1`, port **3311**, after checking that the port was unoccupied. The existing `dev` script's hard-coded port was not edited and no other server was stopped. Root HTTP response: 200. The Codex browser-open request points to `http://127.0.0.1:3311`.

## Limits

This is still native Qt/C++; the EXE does not load the reference web UI. GPU decoding/conversion/scaling is verified for this source and GPU. Qt still composites overlays and performs the final image blit; this is not a claim of an entirely zero-copy GPU renderer. Unsupported hardware/formats retain a CPU fallback rather than refusing all playback. This work does not certify every codec or all previously documented web-parity gaps.

## Delivery

`E:\DATA\source\catchim\Catchim-GPUFix-20261018-1833.exe`, 4,015,616 bytes.
SHA256: `A3DD91AB03F16014D57FEA39842FCB8D714AB06F966949E0B70BA0BEAAF1E58A`.
The delivery matches `app/build/bin/Catchim.exe`; its `--version` smoke test exits successfully. Keep the EXE beside the existing Qt DLLs/platform plugins. Older delivered EXEs were not overwritten or terminated. Save any open project before switching from an older running instance.

## API references

- [DXGI sample subresource](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfdxgibuffer-getsubresourceindex)
- [Video processor input views](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11videodevice-createvideoprocessorinputview)
- [GPU video processing](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11videocontext-videoprocessorblt)
- [Media Foundation hardware transforms and DXVA](https://learn.microsoft.com/en-us/windows/win32/medfound/mf-readwrite-enable-hardware-transforms)
