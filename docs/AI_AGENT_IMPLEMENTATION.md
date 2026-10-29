# Catchim editable AI agent

## Source audit (2026-10-30)

Scope: existing Next web editor in `web/`. Native `app/` has unrelated work in progress and is not an implementation target. This is an implementation ledger, not a claim that the requested six phases are complete.

| Current module | Evidence in source | Missing / implementation direction | Compatibility and risks | Required checks |
| --- | --- | --- | --- | --- |
| `core`, `project`, `timeline/types.ts` | Project → scenes → main/overlay/audio tracks → elements → imported media; effects and animation channels on elements | Add revision-bound edit plans, reuse scenes as sequences | Keep project schema and media references | stale plans, missing targets, save/reopen |
| `commands`, `core/managers/commands.ts` | Shared command history, ripple and selection reactors, browser history bridge | Atomic AI snapshot command after detached validation | Generic BatchCommand currently does not roll back failures; do not use it for AI transactions | rollback, stable undo/redo, manual edit conflicts |
| `mcp` and `app/api/mcp` | 17 tools, 3 resources, bearer token isolation, local Origin checks, bounded pending queue, cancellation before delivery, browser relay | Extend catalog with plans and structured errors, add client and stdio adapter | Browser must remain open; broker is process-local; delivered legacy operations cannot be recalled | official SDK client, auth, revocation, cancellation, concurrency |
| installed MCP SDK `1.32.1` | `SUPPORTED_PROTOCOL_VERSIONS` ends at `2025-11-25` | Explicit compatibility adapter; no claim of 2026-07-28 support | Requested spec changes initialization/capability negotiation; do not silently label old transport as new protocol | negotiation and version rejection |
| `effects`, `params`, effect commands | Four registered effect types, declarative parameter inspectors, effect animation channels | Editable generated definitions must reuse these contracts and persist source definitions | Current renderer dispatches known shader names; arbitrary WGSL is not executed | shader rejection, parameter bounds, preview/export parity |
| `graphics`, `motion`, `animation` | Shapes, motion presets, scalar/discrete/Bezier channels | Compose separate editable elements; reuse animation resolution | Do not flatten generated content or introduce a second timebase | keyframe bounds, refinement, serialization |
| `rust/crates/opencut_engine` | Modules for time, timeline, gap planning, animation, effects, audio, compositor, export, masks; C ABI bridge | Reuse native algorithms; extend one processing behavior at a time | C++ browser fallback remains; no new engine | cargo tests, WASM build, differential tests |
| `services/renderer`, `preview`, `native/webgpu-engine.ts` | Shared render nodes, media decode/cache, known GPU effects and CPU paths | Generated effects need real backend registration and identical preview/export evaluation | Existing GPU loop skips unknown shader names; this is not generic WGSL support | actual frames, device lost, fallback, output parity |
| `core/managers/renderer-manager.ts`, `export` | Mediabunny export, range/cancellation/preflight support | Agent export must call existing renderer and verify streams | Browser codec/device capabilities vary | encoded output duration, audio and frame content |
| `services/storage`, `save-manager`, migrations | Local persistence, autosave, additive migration policy | Preserve project data; generated definitions need additive persistence | Unknown fields must survive reopening; storage failure must propagate | quota failure, migration, reopen |
| `services/transcription`, `subtitles`, `services/tts` | Local transcription worker/models, editable caption elements, subtitle import/export, speech insertion | Explicit model download/network consent and real workflow integration | Installed library does not prove a model is present; no invented transcript | offline failure, timestamps, editability |
| `core/managers/audio-manager.ts`, `native/audio-export.ts`, Rust audio | Gain/fades, source scheduling and audio export | Extend measured DSP rather than simulated semantic analysis | No evidence yet of general tracking/depth/semantic understanding | audio sync, clipping and DSP tests |
| AI/providers/plugins/tracking | No in-editor orchestrator/provider adapter or general tracking engine found in web source | Bounded planning, explicit permissions; advanced models and plugin sandbox remain separate phases | No arbitrary code execution or automatic cloud media upload | provider timeout, scope enforcement, unsupported tasks |
| `*.test.ts`, Rust unit tests, scripts | Vitest suites, Rust tests, HTTP asset and synthetic MCP smoke scripts | Real editor integration tests and browser evidence required | Existing synthetic smoke only proves transport; it does not prove editing/rendering | phase-specific tests, browser use, production assets |

Source presence above is audit evidence only. Test status is recorded after commands are run. No benchmark or pixel parity is claimed.

## Phase status

| Phase | Status | Evidence / tests | Remaining |
| --- | --- | --- | --- |
| 1 Core integration | Implemented and tested within the scope below | Official SDK → broker → real core integration, atomic plans, AI panel, HTTP and stdio | Requested 2026 protocol blocked by installed SDK; live model inference not exercised |
| 2 Effect generation | Not implemented | Existing effect primitives identified | Generated schema/graph, shader validation, registration, persistence, parity |
| 3 Autonomous editing | Not implemented | Existing media and caption APIs identified | Analysis, planning, pacing, music and subtitle workflow |
| 4 Advanced visuals | Not implemented | Existing shapes/masks identified | Tracking, particles, compositing graphs, supported 3D |
| 5 Advanced agents | Not implemented | None | Sandboxed tools, bounded self-repair; multi-agent stays opt-in |
| 6 Production hardening | Not implemented | None | Full security, resource, compatibility and export QA |

## Core integration contract

- AI panel appears in the existing assets sidebar and mobile toolbar. Offline planning currently recognizes `Thêm chữ "Nội dung" 5 giây` / `Add text "Content" 5s` at the playhead. Other natural-language requests need a configured model. Unsupported work fails explicitly.
- OpenAI-compatible/custom and Ollama adapters make one bounded request (45-second planning deadline, 256-KiB response cap). Model responses must match the operation schema. The user must approve context transfer to the endpoint; only metadata and text are sent, not media bytes. Keys stay in panel memory. Actual model availability, CORS and inference are not claimed as tested.
- `AgentManager` lives on `EditorCore`; editor state remains authoritative. SHA-256 revisions cover scenes, settings and imported media identity. Host permission is separate from untrusted plan arguments and is rechecked after asynchronous hashing. Scope checks cover both old and new clip extents. Locked tracks, missing media/targets, overlap, invalid parameters and unordered dependencies are rejected before publication.
- Supported operations: add text, insert a bounded range of imported media, update/delete clip, add/update/remove existing effect, add/update a keyframe. Times use integer frames converted through the existing time adapter. Placement and animation use existing code, not separate algorithm implementations.
- Preview is a detached **timeline data diff**, not rendered frames. Apply publishes one snapshot command with automatic ripple suppressed and one shared Undo/Redo step. Atomic command execution restores the snapshot and selection if a reactor fails.
- Plan replay receipts and audit logs are bounded to 100 entries in memory. Idempotency does not survive reload or eviction. Undo history retains the editor's existing persistence behavior. No automatic retry of a delivered mutation.
- MCP adds `catchim_agent_context`, `catchim_preview_plan`, `catchim_apply_plan`, `catchim_agent_history`, `catchim_list_effects` (22 implemented tools total). HTTP remains loopback-only with bearer/Origin checks. The broker permits 120 calls/minute/connection and at most 16 pending calls. A browser must stay connected; state is not shared across server processes.
- stdio adapter: run `node scripts/mcp-stdio.mjs` from `web`, set `CATCHIM_MCP_TOKEN` to the access token from Settings → MCP, optionally set `CATCHIM_MCP_URL` (default `http://localhost:3300/api/mcp`). The adapter is intentionally restricted to loopback. Tokens must not be committed. It forwards actual tools/resources/cancellation via the installed SDK's 2025-11-25 compatibility path; no 2026-07-28 conformance is claimed.
- Levels 4/5 do not unlock extra capabilities. Autonomous multi-agent editing, arbitrary code/shaders, filesystem access, publishing and media uploads are not exposed.

## Evidence collected for core integration

| Item | Status | Evidence | Remaining limitation |
| --- | --- | --- | --- |
| TypeScript/Vitest | Passed | Full run: 81 files / 410 tests; subsequent focused run: 7 files / 43 tests; AI group: 14 tests including real MCP/core transaction and live revocation check | Full suite must be repeated after further implementation changes |
| Production build | Passed | `npm run build`, including Rust WASM and standalone public/static packaging | Final build required after subsequent changes |
| MCP HTTP and stdio | Passed | `node scripts/check-mcp.mjs`: official clients, discovery, relay, permission, token isolation/revocation | Script's relay is explicitly synthetic; separate Vitest integration executes real editor operations |
| Browser editing | Passed for text insertion | New empty local project, offline plan → apply; `CATCHIM` visible in preview | Broader autonomous editing and effect generation scenarios not yet supported |
| Save/reopen | Passed for text insertion | Reloaded project `85abd6e3-cc68-498f-b34c-12811c478a55`; title and 5-second timeline retained | Generated definitions and tracking do not exist yet |
| MP4 export | Passed for text insertion | Browser output `Dự án mới (2).mp4`; ffprobe: H.264, 1920×1080, 30/1 fps, 150 frames, 5.000000 s; full ffmpeg decode exited 0 | No audio in this title-only fixture; not a preview/export pixel parity test |

Full requested test scenarios A–J are **not all passing**: text insertion/core transactions cover only a subset. General WGSL generation, model-backed media understanding, tracking, generative particles and advanced autonomous edits still require implementation and real output QA.
