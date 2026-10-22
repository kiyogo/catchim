# Catchim Web Video Editor — Agent Guide

## Scope and project identity

These instructions govern work on the web editor in `web/`. The repository is a monorepo; do not modify the native Qt/C++ application in `app/` unless the task explicitly requires it.

- Frontend: Next.js 16, React 19, TypeScript, Tailwind CSS 4, Radix UI.
- Package metadata declares Bun 1.2.18, while the checked-in web lockfile is `web/package-lock.json`. Preserve the existing dependency state and use the lockfile-consistent workflow unless the repository standard is deliberately changed.
- Web Rust core: `web/rust/crates/opencut_engine/` (Rust 2021, `cdylib` + `rlib`, currently dependency-free).
- Browser Rust artifact: `web/public/wasm/opencut_engine.wasm`; `web/src/native/rust-engine.ts` loads its C ABI exports. `web/src/native/opencut-core.ts` tries this Rust engine first and then falls back to the Emscripten C++ browser core when Rust WASM is unavailable.
- Rendering integration: Canvas renderer and compositor under `web/src/services/renderer/`; preview integration under `web/src/preview/`; WebGPU adapter and shaders under `web/src/native/webgpu-engine.ts` and `web/rust/crates/opencut_engine/src/gpu.rs`.
- The web editor uses the browser's media APIs. Keep UI/browser lifecycle and DOM integration in TypeScript; move suitable business and performance-sensitive algorithms into Rust/WASM incrementally.

## Verified commands

Run commands from `web/` unless noted.

- Development server: `npm run dev`
- Next.js production build: `npm run build`
- Browser/Vite build: `npm run build:browser`
- Lint: `npm run lint`
- Type check: `npx tsc --noEmit`
- TypeScript tests (Vitest is installed; there is no `test` package script): `npx vitest run`
- Rust unit tests: `cargo test --manifest-path rust/crates/opencut_engine/Cargo.toml`
- Rust formatting check: `cargo fmt --manifest-path rust/crates/opencut_engine/Cargo.toml -- --check`
- Rust browser target build: `cargo build --manifest-path rust/crates/opencut_engine/Cargo.toml --target wasm32-unknown-unknown --release`

Do not report a command as passing unless it was run. There is no checked-in dedicated pixel-diff or benchmark command at present; use an explicit browser comparison or a repeatable benchmark before making those claims.

## UI preservation is mandatory

The current UI is the required visual reference. Preserve its rendered appearance, layout, and behavior before and after internal refactors. Do not change pixels, CSS, typography, spacing, colors, icons, or responsive behavior unless the user clearly authorizes a UI change.

> “Giao diện hiện tại là chuẩn tham chiếu bắt buộc. Mọi công việc phải giữ nguyên hình ảnh hiển thị, bố cục và hành vi giao diện trước và sau chuyển đổi. Không được thay đổi pixel, CSS, typography, spacing, màu sắc, icon hoặc hành vi responsive nếu không có yêu cầu rõ ràng cho phép thay đổi giao diện.”

Read this rule before touching UI, markup, styles, design tokens, or layout. Prefer changing an internal adapter while retaining existing component APIs, DOM, classes, and CSS. Capture and compare equivalent UI states when a change could affect rendering; do not claim pixel parity without evidence.

## Rust-first architecture and migration

- Rust is the preferred owner of core editing rules, timeline/time calculations, media algorithms, and other processing-heavy business logic where the browser boundary permits it.
- TypeScript remains responsible for UI, DOM events, browser APIs, lifecycle coordination, and a thin typed bridge to Rust/WASM.
- Keep a single source of truth for each domain. Avoid adding a second TypeScript implementation of core behavior when Rust can own it.
- Use the existing Rust crate and browser binding architecture unless evidence shows a concrete limitation. Do not replace the frontend framework, C++ fallback, renderer, or project model as part of a narrow migration.
- Migrate one independently testable behavior at a time. Establish current behavior, add regression/differential coverage, implement and connect the Rust path, retain compatibility/fallback behavior, then validate the relevant build, tests, and UI state.
- Do not remove the previous implementation, alter persisted project formats, or change public APIs until compatibility is demonstrated. Handle errors explicitly; a failed WASM call must not be reported as success.
- Canvas/WebGPU should do real rendering or compute work when used. Keep GPU resource lifetime, cancellation, fallback, and output equivalence testable. Do not claim acceleration without measurements.

## Quality and data compatibility

- Build, test, and review each completed feature. Fix regressions introduced by the change before calling it done; distinguish pre-existing failures from new ones.
- Add Rust unit tests for core algorithms and TypeScript/browser tests for the WASM boundary, error handling, and compatibility when applicable. Compare the old and new behavior on the same inputs when practical.
- Preserve saved project data and public APIs. Storage migrations under `web/src/services/storage/migrations/` must also follow that directory's `AGENTS.md` additive-only data policy.
- Update focused documentation when architecture, commands, or stable integration contracts change. Keep this file to durable guidance rather than a running work log.

## Git and GitHub workflow

- Inspect the Git root, branch, remotes, ahead/behind state, and working tree before editing. This monorepo may contain unrelated user changes; preserve them and stage only files for the completed feature.
- Commit and push each independently completed feature on the repository's established branch/workflow. Verify the push and remote commit before reporting success.
- Never use `git reset --hard`, `git clean -fd`, force push, or broad staging to resolve unrelated changes. Never commit credentials, secrets, personal media, or generated artifacts unless explicitly part of the feature.
- If the remote has advanced, integrate it safely before pushing. If permissions or branch policy prevent a push, retain the local commit and report the exact status; do not claim it was pushed.
