# Catchim MCP editor integration

## Architecture and scope

Catchim exposes editing capabilities through MCP. There is no in-editor AI agent, model provider, chat loop or internal natural-language planner. External MCP clients own language interpretation and planning. The editor workspace itself uses the official MCP client over authenticated Streamable HTTP; it does not bypass the protocol to mutate the core.

`MCP client → MCP server → bounded browser broker → editor bridge → McpEditorRuntime → command system → existing Rust/media/render services`.

The implementation targets `web/`. Native `app/` contains unrelated work and has not been changed. The framework, project format, source media, native fallback and existing editor layout remain in use. New MCP entries and generated-effect controls are additive.

The installed `@modelcontextprotocol/sdk` is `1.32.1`. Its supported protocol versions end at **2025-11-25**. The HTTP/stdio implementation is an explicit compatibility path; it does **not** claim MCP 2026-07-28 stateless-core conformance. This must be revisited when a compatible SDK is deliberately adopted.

## Source audit

Source presence is not a claim that every requested feature has passed end-to-end QA.

| Module | Existing behavior/evidence | Integration and missing functionality | Risks / checks |
| --- | --- | --- | --- |
| `core`, `project`, `timeline/types.ts` | Project → scenes → main/overlay/audio tracks → elements → imported assets; effects and animation channels live on elements | Scenes serve as sequences; revision-bound MCP plans reuse this model | Scope, references, stale plans, source preservation |
| `commands`, command manager | Shared Undo/Redo, selection/ripple reactors and browser history bridge | MCP plans publish one atomic snapshot command; failure restores state/selection | One Undo step, stable IDs, failure rollback, concurrent requests |
| `mcp`, API routes | Previously 17 direct tools, browser relay, bearer tokens and local Origin checks | Added context, history, plan/effect builders, validation, schemas, prompts and stdio | Live tab required; process-local broker; delivered calls may already have committed on cancellation |
| Rust crate / native bridge | Time, timeline, masks, audio, compositor, export and editing helpers via C ABI | New bounded effect-graph validator lives in Rust, including actual WASM export | Cargo tests, browser WASM build, ABI tests; no duplicate TypeScript graph traversal |
| `effects`, `params`, `animation` | Blur, color grade, vignette, chroma key; editable parameter schemas and animation channels | Self-contained generated recipes compose these renderers; exposed parameters support normal keyframes/inspector | Cycles, bindings, range/type validation, pass and pixel budgets |
| Renderer / WebGPU | Shared preview/export scene evaluation, known GPU pipelines and CPU/WASM implementations | Generated definitions resolve into the existing render passes. WebGPU export renders to a dedicated readable texture and feeds copied pixels to the encoder, avoiding swapchain-canvas frame lag | No arbitrary WGSL execution, particles, merge/feedback graphs or generic shader compiler yet; readback adds a CPU copy per frame |
| Media / decoding / export | Browser media APIs, Mediabunny, range/preflight/cancellation, audio export | Existing manual export is reused; unavailable effects now block export instead of silently disappearing. The current browser export was inspected at frame 0 and the MCP title boundary | Actual codec support varies; encoded stream/content checks required for each browser/GPU |
| Storage / save / migrations | Local persistence, autosave, additive migration policy | Recipes are optional additive data on effects; unknown definitions remain stored | Reopen, unknown-version preservation, quota errors; history/receipts remain in memory |
| Transcription / subtitles / TTS | Local transcription worker/model support, editable captions, SRT/VTT and speech insertion | Existing tools remain; model inference/semantic workflows not exposed as fake MCP tools | Model availability, download consent, timestamp/audio checks |
| Graphics / masks / motion | Shapes, masks, motion presets, scalar/discrete/Bezier keyframes | Existing implementations remain; MCP plans can animate supported property paths | Full tracking/depth/3D/segmentation and autonomous scene understanding not implemented |

## MCP catalog

25 implemented tools: 17 original tools plus:

- `catchim_get_context`: project/scene revision, FPS, selection, imported-media metadata, capabilities and existing effects/keyframes.
- `catchim_get_edit_history`: bounded plan audit log.
- `catchim_preview_plan`: detached timeline diff and validation; does not render frames.
- `catchim_apply_plan`: atomic, revision-checked apply through the shared command system.
- `catchim_list_effects`: implemented effect definitions/parameter schemas.
- `catchim_create_text_plan`: deterministic editable text plan using integer frame inputs.
- `catchim_create_effect_plan`: deterministic plan for a declarative generated effect and explicit target clips.
- `catchim_validate_effect`: real schema/binding/Rust topology/resource validation; not shader compilation.

Resources: current project, active timeline, imported media, `catchim://schemas/edit-plan`, `catchim://schemas/effect`.

Prompts: `catchim_edit_workflow`, `catchim_effect_workflow`. Both guide external clients through context → schema → plan → preview → approved apply. Prompts do not execute edits.

Tool discovery includes input/output schemas, read/write annotations, permission metadata, timeout, cancellation policy, idempotency behavior and installed protocol compatibility. The general output schema is a JSON object; result fields are tool-specific. Errors use `isError`, text and structured error metadata when available.

HTTP remains loopback-only with Host/Origin validation and distinct access/browser tokens. Connections default to read-only. Host editing permission is checked by the broker and bridge; scoped plan permissions/locks/revision are checked again in the core. No caller-supplied plan can grant permissions. Limits: 120 tool calls/minute/connection, 16 pending calls, 32 connections, bounded request/response bodies and deadlines. Revocation cancels queued edits; after delivery, inspect state before retrying a cancelled or timed-out request.

stdio: run `node scripts/mcp-stdio.mjs` from `web/`. Set `CATCHIM_MCP_TOKEN` to the access token from MCP settings, optionally `CATCHIM_MCP_URL` (default `http://localhost:3300/api/mcp`). Only loopback HTTP(S) endpoints are allowed. The adapter forwards tools, resources, prompts and SDK cancellation. Never commit tokens.

## Edit plans

Plans carry project/scene IDs, SHA-256 expected revision, stable plan/operation IDs, target scope, ordered dependencies and an atomic rollback contract. Supported operations: add text/media ranges, update/delete clips, add/create/update/remove effects, and upsert keyframes. Operation times use **integer project frames**, converted through the existing time bridge; the legacy direct tools retain seconds.

Projection runs on detached immutable tracks using existing placement, parameter and animation helpers. Missing media/targets, locked/out-of-scope tracks, invalid types/ranges/keyframes, introduced collisions and unordered dependencies fail before publication. Apply checks the project again after async work, rechecks live host permission, suppresses automatic ripple for the preplanned snapshot, and commits one shared Undo/Redo step. Reactor failures roll back the snapshot and selection.

Replay receipts and audit entries retain the last 100 items in editor memory. Idempotency does not survive reload/eviction. A replay after Undo reports a historical receipt and does not silently reapply the plan. Project persistence retains the existing editor's Undo-history limitations.

## Editable generated effects

`Effect.generated` stores a versioned, self-contained definition: name/ID/provenance, exposed typed parameters, nodes, bindings, source connections and output. Current nodes are single-input built-in renderers: blur, color-grading, vignette and chroma-key. Graph storage order need not equal render order; the Rust validator computes the order and rejects cycles, invalid references and unused nodes. Limits: 8 nodes, 32 passes, maximum dimension 8192 and at most 16,777,216 pixels per effect surface.

Parameters remain normal `Effect.params`, with animation channels at `effects.<effectId>.params.<key>`. Definitions survive save/load and duplication as data. Preview/export use the same recipe-to-pass resolver. Unknown versions/dependencies are preserved, shown unavailable and blocked by export preflight while enabled. Original source clips are not flattened/replaced.

This is a bounded compositional effect system. General WGSL generation/compilation, arbitrary compute, merge/feedback nodes, procedural particles, audio-reactive uniforms, object tracking, sandboxed plugin generation and preset libraries still need implementation. Those capabilities are not registered as working MCP tools.

## Evidence and remaining work

| Item | Status | Evidence | Limitation |
| --- | --- | --- | --- |
| MCP core and HTTP/stdio compatibility | Tested | Full Vitest suite: 82 files / 421 tests. `node scripts/check-mcp.mjs http://127.0.0.1:3301` passed against standalone production with the official SDK clients, 25 tools, 5 resources, 2 prompts, auth, revocation, origin checks and stdio. Editor runtime tests apply a plan through the real command system | Smoke relay is explicitly synthetic; real browser workflow was separately exercised |
| Rust graph validation | Tested | `cargo test`: 54 tests passed, including topology/resource cases; WASM artifact built | GPU compute/shader validation is not part of this validator |
| Effect processing | Tested | Real Rust/WASM ABI and CPU pixel test: generated recipe changes source pixels and equals explicit passes byte-for-byte | Not a general CPU/GPU or preview/encoded-video parity claim |
| Persistence and MCP workflow | Tested | Browser MCP workspace applied an editable title and generated effects; Undo/Redo changed and restored the timeline. Reopening the project retained generated effect parameters and the animated brightness keyframe; verification screenshot is saved outside the repository | MCP grants and commit receipts are per connected editor instance; reconnect after reload |
| Browser export | Tested | Exported a 10-second H.264 MP4 at 1920×1080, 30 fps, 300 frames. `ffprobe` metadata passed and full `ffmpeg` decode exited 0. Decoded frame 0 contains CATCHIM; decoded frame 150 contains MCP TITLE. The earlier swapchain CanvasSource path produced blank/green frames and was replaced with WebGPU texture readback + `VideoSampleSource` | Tested in this local browser/GPU only; no pixel-diff or cross-device performance claim |
| Production build and assets | Tested | Final `npm run build` passed with Rust WASM and standalone public/static packaging. `npm run check:web -- http://127.0.0.1:3301 / /projects /editor/85abd6e3-cc68-498f-b34c-12811c478a55` passed: 3 routes, 26 JS/CSS assets and valid WASM. Production MCP smoke passed after the exporter patch | Build output includes the existing BetterAuth warning that the configured secret is short/low-entropy; the local environment value was not changed |
| Tooling limitations | Recorded | `npm run lint` cannot start because this install has no `eslint`; crate-wide `cargo fmt -- --check` reports pre-existing formatting differences in untouched files; the new Rust effect graph file passes its scoped format check | Do not treat unavailable lint or pre-existing formatting drift as passing |

The broad original scenarios A–J are not all complete. In particular, unrestricted shader generation, tracking, semantic media analysis, automatic music/subtitle production, advanced 3D and sandboxed tool development are not implemented. No completeness percentage or acceleration benchmark is claimed.
