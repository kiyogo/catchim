# Browser transcription path

The WASM editor uses the browser for local Whisper inference while C++ owns
timeline mixing, chunk scheduling, caption timing, and project insertion.

## Runtime flow

1. The Generate Transcript action snapshots the active timeline and media
   library, then runs outside the UI thread.
2. `AudioPlaybackEngine` renders 30-second, 16 kHz slices as mono audio. Slices
   advance by 25 seconds, so adjacent requests overlap by five seconds without
   materializing the full timeline as PCM.
3. `TranscriptionService` sends each slice through the Emscripten bridge in
   `src/wasm/wasm-transcription.js`. The bridge transfers a copy of the samples
   to `catchim-transcription-worker.js` and returns progress/results through
   shared WASM memory.
4. The worker runs Transformers.js with ONNX Runtime Web. The current UI uses
   `onnx-community/whisper-small` in q4 mode and lets Transformers.js select the
   available device. Model weights are downloaded from Hugging Face on first
   use and cached by the browser runtime.
5. Recognized word segments become subtitle clips on a new text track. Track
   creation and clip insertion share one undoable batch command.

## Build and serving requirements

- Build from the configured Qt for WebAssembly build directory with
  `cmake --build <build-dir> --target catchim_app -j2`.
- The build requires `web/node_modules` to contain `esbuild`,
  `@huggingface/transformers`, and `onnxruntime-web`. CMake bundles the worker
  and copies the ONNX Runtime Web WASM assets next to the app output.
- Serve the output with cross-origin isolation headers (`COOP: same-origin`
  and `COEP: require-corp`) so the multithreaded WASM runtime can create the
  worker and shared memory.
- The native desktop build reports that automatic transcription requires the
  browser WASM app; it does not return fabricated transcript text.
