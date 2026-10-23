# Catchim Web: tiến độ core editor ngày 2026-10-22

Đây là bản ghi lịch sử. Tiến độ mới hơn: [CORE_EDITOR_STATUS_2026-10-23.md](./CORE_EDITOR_STATUS_2026-10-23.md).

Bản ghi này mô tả phạm vi đã sửa và bằng chứng của lần triển khai tiếp theo. Các thay đổi đang ở working tree, chưa commit/push. Không thay đổi schema dự án hoặc bố cục UI.

## Stack và ranh giới thực tế

- Frontend: Next.js 16.1.3, React 19, TypeScript, Tailwind 4, Radix; có bản build browser riêng qua Vite.
- Engine: crate Rust 2021 `rust/crates/opencut_engine`, build sang `public/wasm/opencut_engine.wasm` qua C ABI. `src/native/rust-engine.ts` là bridge; core C++/Emscripten cũ vẫn là fallback hiện hữu.
- Media: Mediabunny/browser codecs; preview dùng Canvas/WebGPU và Web Audio. Dữ liệu media lưu bằng OPFS, metadata/project bằng IndexedDB.
- Rust xử lý thuật toán phù hợp; TypeScript giữ lifecycle browser, state/commands và Web Audio nodes. Không có Rust backend encode mới trong lần sửa này; export tiếp tục dùng pipeline browser hiện có.

## Phạm vi đã hoàn thành

| Nhóm | Trạng thái | Hành vi và bằng chứng |
| --- | --- | --- |
| Metadata import/save/load | DONE | Giữ FPS đo được thay vì làm tròn 29.97 thành 30. Lưu và nạp lại `fps`, `hasAudio`, kể cả `false`. Dữ liệu cũ thiếu trường vẫn nạp được, không gán metadata giả. Test roundtrip thất bại trước sửa và pass sau sửa. |
| Identity khi undo/redo | DONE trong 6 command được kiểm tra | Split, duplicate, paste, insert tự tạo track, paste keyframes, tách/khôi phục audio nguồn giữ snapshot kết quả lần đầu. Redo không sinh ID clip/track/keyframe khác. Test dùng CommandManager và move/keyframe commands thật. Chuỗi 30 chỉnh sửa rồi undo/redo toàn bộ giữ đúng từng snapshot, selection và shared media reference. |
| Audio streaming sau trim/split/seek | DONE ở scheduler và bridge | Rust giao các khoảng decoded buffer, clip, vị trí seek và audio clock; TypeScript chỉ tạo node khi có đoạn phát hợp lệ. Offset và duration được quy đổi theo playback rate. Có test Rust, ABI WASM thật, fallback và AudioManager với context/decoder mô phỏng. |
| Gap trên main track | DONE ở quy tắc update và group move | Clip đầu/duy nhất không còn bị kéo về 0; thời gian âm vẫn clamp tại 0. Kiểm tra collision với clip đứng yên vẫn từ chối move. Hai test update và hai test resolver pass. |
| Nhiều node preview dùng chung media | DONE ở hàng đợi decoder | Request cùng asset được resolve tuần tự trên sink dùng chung; hai time target khác nhau không còn hủy lẫn nhau. Có integration test với hai request song song. Chưa benchmark nhiều stream. |
| Track lock | PARTIAL | Thêm `locked?` tương thích project cũ, command toggle/undo, header và context menu. Placement/move bị chặn; update/delete/split bỏ qua; manager bảo toàn element list của track khóa trước mọi command khác. Serialize/deserialize giữ field lock. Chưa QA browser. |
| Export frame boundary/cancel | PARTIAL | Frame count dùng ceil theo FPS rational; timestamp từng frame tính trực tiếp từ phân số. Poll hủy chạy xuyên audio preparation và exporter. Test frame-count pass; audio decode/mix đang chạy chưa hỗ trợ AbortSignal, nên thao tác hủy có thể chờ tới khi bước đó xong. Chưa tạo MP4/WebM golden output. |

Các đường đã nối: metadata được probe khi import → asset được lưu → clip vẫn tham chiếu cùng asset → command chỉnh sửa/undo/redo cập nhật cùng timeline model → AudioManager lấy clip/source mapping từ timeline và giới hạn đoạn phát. Export vẫn dùng timeline audio mixer hiện hữu; lần này không thay thuật toán encode/export.

## File thay đổi

- `src/media/processing.ts`, `processing.test.ts`, `src/fps/__tests__/fps.test.ts`: giữ FPS lẻ và kiểm tra ánh xạ NTSC rational đã có.
- `src/services/storage/service.ts`, `service.test.ts`: roundtrip hai trường metadata optional; không migration.
- `src/commands/timeline/element/{split-elements,duplicate-elements,insert-element,toggle-source-audio-separation}.ts`, `clipboard/{paste,paste-keyframes}.ts`: redo giữ identity. Test tại `src/commands/timeline/__tests__/stable-redo.test.ts`.
- `rust/crates/opencut_engine/src/{audio,ffi,lib}.rs`: planner và C ABI `ocw_audio_plan_buffer_schedule` (`-1` invalid, `0` empty, `1` ready).
- `src/native/rust-engine.ts`, `src/media/audio-scheduling.ts`, `src/core/managers/audio-manager.ts`: capability optional, fallback tương thích, cleanup allocation và tích hợp node scheduling.
- `src/timeline/update-pipeline.ts`, `src/timeline/group-move/resolve-move.ts`: giữ gap khi thay đổi start time hoặc kéo clip trên main track; không đổi collision policy.
- `src/timeline/__tests__/update-pipeline.test.ts`, `src/timeline/group-move/__tests__/resolve-move.test.ts`: regression giữ gap và chặn overlap.
- `src/services/video-cache/service.ts`, `service.test.ts`: giữ riêng kết quả frame cho các request song song cùng media.
- `src/timeline/types.ts`, `track-capabilities.ts`, `src/commands/timeline/track/toggle-track-lock.ts`, `src/core/managers/timeline-manager.ts`: model lock, toggle command và guard nội dung chung.
- `src/timeline/placement/resolve.ts`, `src/timeline/group-move/resolve-move.ts`, commands element `move/update/delete/split/duplicate`, `src/timeline/components/index.tsx`, `src/timeline/controllers/drag-drop-controller.ts`: enforcement và affordance lock trên UI/drop.
- `src/i18n/{types.ts,locales/en.ts,locales/vi.ts}` và test lock, project persistence, placement, resolver.
- `src/{native,media}/audio-scheduling.test.ts`, `src/core/managers/audio-manager.test.ts`: regression, differential WASM và integration scheduling.
- `scripts/build-rust-wasm.mjs`: kiểm tra export ABI mới trong artifact trước khi đồng bộ. `docs/AUDIO_ENGINE.md`: mô tả đúng contract Web Audio đang dùng.

## Kiểm chứng

Chạy từ `web/`:

| Lệnh | Kết quả |
| --- | --- |
| `npx vitest run` | PASS: 51 file, 269 test. |
| `npx tsc --noEmit` | PASS sau track lock, storage roundtrip và drag/drop integration. |
| `cargo test --manifest-path rust/crates/opencut_engine/Cargo.toml` | PASS: 31 test. |
| `npm run build:rust:wasm` | PASS; artifact 120680 byte, có ABI mới. |
| `npm run build:browser` | PASS; còn cảnh báo bundle lớn, `use client`, externalized Node modules hiện hữu. |
| `npm run build` | PASS: Rust/WASM, Next production compile, TypeScript và tạo route hoàn tất sau track lock. |
| `npm run lint` | BLOCKED: executable `eslint` chưa có trong install hiện tại. |
| `cargo clippy --manifest-path rust/crates/opencut_engine/Cargo.toml --all-targets` | FAIL baseline: 75 lỗi FFI raw-pointer signatures, 16 warnings. Đã chạy cùng lệnh trên bản HEAD `101bdcb6` trích riêng vào thư mục tạm: cũng 75 lỗi, 16 warnings. Export mới khai báo `unsafe` với safety contract. |
| `cargo fmt --manifest-path rust/crates/opencut_engine/Cargo.toml -- --check` | FAIL ở định dạng crate hiện hữu; `rustfmt --check` riêng `audio.rs` mới PASS. Không format lại toàn bộ crate. |

Server production đã được khởi động lại tại `http://localhost:3300` sau khi build mới. Route editor và HEAD WASM trả HTTP 200. Đợt trước đã compile artifact tải trực tiếp bằng WebAssembly và xác nhận export `ocw_audio_plan_buffer_schedule`; build hiện tại giữ artifact 120680 byte. Artifact đồng bộ ở `public/wasm/opencut_engine.wasm` và `.next/standalone/public/wasm/opencut_engine.wasm`.

## Acceptance cases và giới hạn

- CASE 03/19: PARTIAL ở cấp workflow model. Split/undo/redo và chuỗi 30 thao tác có regression; chưa xác minh phím tắt/GUI hoặc bridge history C++ khi bật bản browser.
- CASE 04/07: PARTIAL. Đã xác minh biên audio trim/split/seek, late offset 0.5x/2x và ABI thật; chưa nghe playback thực trong browser hoặc xuất MP4 mới để đối chiếu.
- CASE 13: PARTIAL. Roundtrip metadata qua adapter tests; chưa refresh/reopen với OPFS thật trong browser trong lần sửa này.
- CASE 14/15: chưa xác minh lại encode/output/cancel. Không có output MP4 mới được dùng làm bằng chứng.
- CASE 20: có thêm lock control vào hàng track và context menu theo yêu cầu chức năng; chưa có pixel-diff hoặc GUI QA vì browser automation bị tắt trong Work settings.

Các lỗi còn lại từ code audit, chưa sửa trong phạm vi này:

1. CASE 05: gap policy cho main track đã sửa ở group move và update pipeline; cần QA browser drag/inspector và rà các đường commit khác.
2. CASE 18: track lock đã được nối model/command/placement/guards/persistence; còn QA browser và disabled-state/feedback nhất quán cho mọi action.
3. Request video dùng chung sink nay được xếp hàng để giữ đúng source time cho nhiều instance; tải nhiều instance vẫn cần benchmark hiệu năng.
4. Export đã sửa frame count và đưa poll hủy lên trước audio preparation; cần truyền AbortSignal sâu vào decode/mix và xác minh output MP4/WebM ở duration partial-frame.
5. Native preview kẹp tốc độ rất chậm; cần chọn frame-cache fallback khi không phát đúng rate.
6. Chưa chứng minh một transport master clock dùng chung cho audio/video; lần sửa scheduler không phải phép đo A/V drift.

Ưu tiên tiếp theo: kiểm chứng source-time preview qua trim/split trong browser, đo hiệu năng hàng đợi nhiều instance, rồi truyền AbortSignal vào audio decode/mix và tạo golden output export.

## Tiếp nối ngày 2026-10-22

Đã sửa gap policy, xử lý request frame song song cùng media, thêm track lock, và tạo ma trận theo yêu cầu V2 tại [`VIDEO_EDITOR_FEATURE_MATRIX.md`](./VIDEO_EDITOR_FEATURE_MATRIX.md). Tiếp tục sửa frame boundary cho export và mở rộng polling hủy tới audio preparation. Kiểm chứng: `npx vitest run` — 52 files, 273 tests pass; `npx tsc --noEmit` pass; `npm run build` pass; editor route và WASM trả HTTP 200. Browser GUI và golden MP4/WebM chưa được xác minh; hủy chưa ngắt được decode/mix audio đang chạy. Backlog V2 rộng chưa được audit đầy đủ và không được xem là hoàn tất.

## Cách chạy

`npm run dev` mở Next dev tại cổng 3300. `npm run build` build Rust/WASM rồi Next production. Khi chạy standalone, đồng bộ cả `public/` và `.next/static/` vào `.next/standalone/` trước khi chạy server; nếu chỉ cập nhật JavaScript, server có thể tiếp tục phục vụ WASM cũ.

```powershell
npm run build
Copy-Item -Path public -Destination .next/standalone -Recurse -Force
Copy-Item -Path .next/static -Destination .next/standalone/.next -Recurse -Force
$env:PORT = '3300'
node .next/standalone/server.js
```

`npm run build:browser` tạo artifact browser trong `dist-browser/`. Rust ở đây chạy qua WASM trong browser; không yêu cầu người dùng chạy native Rust binary để edit.
