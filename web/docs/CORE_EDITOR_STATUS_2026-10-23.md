# Catchim Web — triển khai tiếp ngày 2026-10-23

Phạm vi: `web/`. Không thay project schema, không commit/push. Đây là đợt tiếp theo của yêu cầu V2 và nghiên cứu CapCut bằng REA; chưa hoàn thành toàn bộ backlog CapCut/V2.

## Đã triển khai

| Workflow | Code và hành vi |
| --- | --- |
| Đóng gap từng track | Rust `gap_edit.rs` → C ABI → typed bridge → `CloseTrackGapsCommand` → context menu. Giữ clip đầu, overlap, source mapping và IDs. Khóa track/no-op được chặn. CommandManager opt-out auto ripple cho edit đã tự lập lịch. Có history, preview/export scene-plan và storage roundtrip tests. |
| TTS save/reopen | `services/tts/insert-generated-speech.ts` lưu file WAV thành MediaAsset trước khi insert upload-audio clip, dọn URL synthesis và báo lỗi lưu. Không còn phụ thuộc blob URL trong project. Test revoke URL cũ rồi load media bytes lại qua StorageService. |
| Export audio cancellation | AbortController từ đầu job; signal xuyên fetch/decode/retime/mix/mastering. Yield giữa chunk, bỏ kết quả muộn, dọn listeners/WASM allocation; chỉ đóng AudioContext owned. DecodeAudioData/offline rendering không có native abort API; kết quả được bỏ thay vì tiếp tục pipeline. |
| Export frame schedule | Rust `export.rs` dùng i128, ceil count, rational timestamps và duration cuối bằng phần còn lại. Render sample không ở exclusive end. SceneExporter truyền đúng arguments vào encoder, dọn compositor khi lỗi/hủy, không phát complete nếu hủy lúc finalize. |
| WebVTT import | File input nhận .vtt. Parser hỗ trợ BOM/CRLF/cue identifier/short timestamps/NOTE/plain text entities. Reuse caption BatchCommand, text tracks và storage hiện có; CSS/position không bảo toàn, có warning cho metadata/settings bị bỏ. |
| SRT/WebVTT export | Context menu của text track → Xuất phụ đề → SubRip/WebVTT. Lấy timing/content đã chỉnh, sort theo start, multiline/Unicode/entity escaping, tối thiểu 1ms cho cue ngắn. Chỉ xuất track đã chọn. |
| Subtitle time | Rust parser reject timestamp lỗi thay vì 0 giả; formatter round total millisecond trước khi chia field, sửa carry qua giây/phút/giờ; bảo vệ null/zero/truncated output buffer. |
| Thư viện media | Thêm tìm kiếm theo tên, lọc video/audio/image, đổi tên có lưu và undo/redo, tải file nguồn, thay source giữ nguyên media ID; thay source yêu cầu cùng loại và không ngắn hơn file trước, đồng thời cập nhật cache và rollback best-effort khi ghi thất bại. |
| Điều chỉnh màu | Nối Adjustment tab với effect color-grading gồm brightness, contrast, saturation, exposure, temperature, tint, hue và gamma; dùng effect data/commands hiện có, thêm pass WebGPU và gọi Rust/WASM ở đường canvas fallback. UI/pixel parity chưa được kiểm chứng bằng browser. |

## Kiến trúc

Rust sở hữu các planner/time/sample algorithms. TypeScript giữ UI, command history, project references, browser decode/encode/GPU/audio lifecycle, cancellation và lưu trữ/download. Giữ typed bridge, Emscripten compatibility và fallback cho artifact Rust cũ/chưa load. Không dùng CapCut source/model/asset trong Catchim.

Đã cập nhật [RUST_ARCHITECTURE.md](./RUST_ARCHITECTURE.md), [VIDEO_EDITOR_FEATURE_MATRIX.md](./VIDEO_EDITOR_FEATURE_MATRIX.md), [CAPCUT_WEB_PARITY_2026-10-23.md](./CAPCUT_WEB_PARITY_2026-10-23.md). Tài liệu REA giữ Evidence ID, artifact hash, phạm vi chọn file và giới hạn heap/timeout; không khẳng định đã decompile thành công SmartCrop.

## Kiểm chứng đã chạy

- Full `npx vitest run`: **61 files, 329 tests PASS**. Sau guard tránh history no-op và UI type fixes, targeted gap/subtitle/export integration: **3 files, 18 tests PASS**.
- `cargo test --manifest-path rust/crates/opencut_engine/Cargo.toml`: **44 tests PASS**.
- `npx tsc --noEmit`: PASS; Next production build cũng hoàn tất typecheck.
- `npm run build`: PASS, Rust/WASM và Next production.
- `npm run build:browser`: PASS, Vite client artifact. Có cảnh báo bundler hiện hữu về `use client`, Node modules externalized và bundle lớn; không coi là bằng chứng runtime browser đã hoạt động.
- WASM: **115802 bytes**, SHA-256 `fc9597424ed26d5797fce4e4833e366f379cf3dd49b3fc48d88b712bcdab9d0a`. `public/wasm/` và `.next/standalone/public/wasm/` có hash giống nhau.
- Production server trên port 3300; GET editor route và HEAD WASM trả **HTTP 200**.

Local browser tool trả `Local browser use is disabled in Work settings`. Vì vậy chưa có GUI click/drag/file-download QA, nghe playback, GPU benchmark hoặc golden MP4/WebM. Test scene graph/mock encoder không thay thế các kiểm chứng này. Đã chạy `npm run lint`: thất bại vì executable `eslint` chưa có trong install hiện tại. `rustfmt --check` trên hai module mới `export.rs` và `gap_edit.rs` pass.

### Kiểm chứng phần tiếp nối media/color

- `npx tsc --noEmit`: PASS sau khi nối media organizer, source replacement và Adjustment tab.
- `npm run build`: PASS; Rust/WASM và Next.js production build hoàn tất.
- Production server đã khởi động lại ở `http://localhost:3300/`; root redirect `/projects` (307), `/projects`, editor hiện tại và `/wasm/opencut_engine.wasm` đều trả HTTP thành công.
- Chưa chạy test suite hoặc browser GUI/pixel QA trong phần tiếp nối này; build không xác nhận hình ảnh màu thực tế hoặc file MP4 xuất cuối.

## Các nhóm chưa hoàn tất

Transitions tab vẫn là placeholder; chưa có transition model/command/render contract. Thay source hoạt động từ context menu trong thư viện media, nhưng chưa có relink cho file bị mất hoặc thay asset bằng file ngắn hơn. Color grading đã có giao diện/pass renderer nhưng chưa có browser pixel QA. Speed curves/reverse, mic/webcam recording, persistent groups/link, solo/reorder, nested/compound, proxy, multicam, motion tracking/stabilization/optical flow, brand kit/template, transcript edits và cloud collaboration chưa có workflow hoàn chỉnh được chứng minh trong audit này. AI features cần model/provider và QA riêng; không biến panel hoặc shader thành trạng thái DONE.

Kế hoạch tiếp theo: (1) kiểm tra trực quan các control màu và roundtrip source replacement/rename trong browser, gồm export mẫu; (2) thiết kế transition model dựa trên overlap/source handles rồi nối vào preview và export chung; (3) tiếp tục các workflow media và timeline có thể hoàn thiện bằng model hiện tại; (4) sau khi core ổn định mới chọn các tính năng cần model/provider như tracking, isolation hoặc smart reframe. Không đánh dấu hoàn tất chỉ vì thêm panel.
