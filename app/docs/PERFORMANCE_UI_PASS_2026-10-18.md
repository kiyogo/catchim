# Performance/UI pass — 2026-10-18

- Timeline filmstrip now obtains one cover frame first and uses it as a temporary fallback for missing tiles. It avoids queueing all long-clip seeks before the first paint and stops requesting filmstrips while playback is running.
- Media clip placeholders are neutral instead of the selected blue fill, so an unready filmstrip does not look like a solid blue clip.
- Removed editor tooltips that were covering controls in the native preview, ruler and transform tab rail.
- Added English to the settings language list.

Verification after the pass: native workflow regression completed successfully; the existing preview-selection and 241-suite core regressions remain green. Export still uses the existing FFmpeg hardware encoder selection; renderer throughput depends on project resolution, effects and source codec, so it is not reported as fully optimized by this pass.
