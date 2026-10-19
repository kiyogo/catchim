# Timeline interaction fixes — 2026-10-18

- Project-name editing now commits and loses its focus border when the user clicks elsewhere in the application.
- Ruler and track hit-testing share the same duration-clamped time conversion, so seeking cannot continue after the project ends.
- Horizontal scrolling no longer adds a 64px empty tail after the project duration.
- Clip end trimming rejects durations beyond `sourceDuration - trimStart`; clip start trimming also rejects an invalid source span. This prevents a video from being stretched after its media has ended.

The native UI build was verified with the existing preview-selection regression (58/58), core suite (241 suites), and native workflow regression (all checks passed).
