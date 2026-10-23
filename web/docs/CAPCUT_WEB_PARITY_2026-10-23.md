# CapCut study → Catchim Web implementation

Ngày: 2026-10-23. Phạm vi đích triển khai: `web/`, Rust/WASM + TypeScript/React. Đây là nghiên cứu tính năng và triển khai độc lập; không sử dụng code, model, nhạc, template hoặc asset của CapCut trong sản phẩm.

## Bằng chứng REA

Đã đọc skill `C:/Users/sonng/.agents/skills/reverse-engineer-anything/SKILL.md` v43 và các hướng dẫn connection-recovery, JavaScript applications. Phiên không có REA MCP tools; `rea-agents@6.7.0 doctor --client codex --json` xác nhận thiếu đăng ký MCP. Dùng CLI trực tiếp, không sửa cấu hình Codex hay cài native disassembler. Skill v43 mới hơn catalog CLI (skill expected v42); chỉ dùng tham số được CLI `--help` quảng bá.

Target xác định từ cài đặt trên máy: `C:/Users/sonng/AppData/Local/CapCut/Apps/ProductInfo.xml` khai báo **9.7.0.4137, beta3**. Bản 9.6 trong nghiên cứu native trước đây không được dùng để suy ra hành vi web.

| Evidence | Phạm vi và nhận định được hỗ trợ | Giới hạn |
| --- | --- | --- |
| `ev_eed17c0f80459df49c5bc29ddf2ba6dde6c5faa25bacf05bf0486c3b7a14eb5e` | Inventory Resources SHA-256 `971783348f42b2648d2ecd057e2e2a4510f412ecbe0867ddd4658b319a09500e`; 1.108 file liên quan, chọn đúng 6 JSON, đọc 3.196 bytes. | Không phân tích native binary, không có JS được parse, 1.102 file ngoài scope. Đây là inventory/config evidence, không phải bằng chứng thuật toán hoặc runtime. |
| `ev_a4e6df35ed03096d4c238bf2fc8b0315d094fd03da8d5eddf15e61988922fc1b` | Inventory plugin SmartCrop SHA-256 `899c97f205e60a85a599a5ef4748467c9c320aad334458ef50d69500481e780a`, manifest và JS 1.182.448 bytes. | Worker 512 MiB hết heap, JS bị skipped; không có semantic findings. |
| `ev_2fc4f3b2e956dda8b811caab0e0cbdabc86856edd5059692315accad94895807` | Thử lại đúng artifact SmartCrop với heap 1.536 MiB và budget semantic nhỏ hơn. | Timeout 90 giây; partial observation giữ inventory/identity. Không dùng lần thử này làm chứng cứ thuật toán crop. |
| `ev_7d90eab398cda3ef255ec8ff9ffac18274ff8722dc090dc34f3b9e1537a98fbf` | REA CLI `inspect-pe-resources` trên launcher CapCut 9.7.0.4137: PE x86_64, SHA-256 `bf375fd0a9c609016d8351178b0d1790d865e920c0ee3183abda475865e37b28`, 7 resource entries. | Chỉ là PE resource/icon inventory, không chứng minh workflow hoặc hành vi editor. |
| `ev_32c06bdb52f49592e95ef3a90a2114400f4a43da1f27b5de48f7b8bafe9b469e` | CapCut `Resources/icon` SHA-256 `e80b60f6640f0a67dd174c85d4bda2486c19eda2f51465af05519d282a1232ca`: inventory 34 file liên quan, đọc đúng 2 JSON caption mẫu (9.876 byte); có paragraph và token timing/cờ `is_key`. | 32 file ngoài scope; không parse JavaScript, không semantic findings. Metadata mẫu không xác lập đơn vị thời gian, thuật toán hay hành vi runtime. |

Các JSON được chọn: `Curves/config.json`, `Curves/algorithmConfig.json`, `SceneEditDetection/config.json`, `Chroma2/config.json`, `matting_config/config.json`, `lvop_intelligent_crop/manifest.json`. Đọc trực tiếp nội dung JSON sau inventory cho thấy:

- Scene detection cấu hình node `compress_shot_detect`, sliding window 7, feature dimension 128, threshold 0.35 và hai model được đặt tên. Điều này chỉ chứng minh có cấu hình learned scene detection; không cung cấp model hoặc độ chính xác.
- Matting khai báo nhóm model; không thể thay thế bằng một nút xóa nền khi Catchim chưa có model/pipeline tương ứng.
- Chroma có effect bundle; Curves có algorithm graph chứa blit node. Tên bundle không đủ chứng minh toàn bộ control màu hoặc công thức nội bộ.
- Manifest đặt tên plugin SmartCrop và UI dạng dialog. Không có bằng chứng runtime về cách chọn subject/reframe.

Evidence CLI gốc được giữ ngoài repo ở `%LOCALAPPDATA%/Temp/catchim-capcut-rea-20261022.json`, `catchim-capcut-smartcrop-rea-20261022.json`, `catchim-capcut-smartcrop-rea-20261022-1536.json`. Không đưa source bundle/asset CapCut vào repo.

Ngày 2026-10-23 đã thử `rea observe-native-ui` với tiến trình/cửa sổ CapCut đang chạy và chỉ yêu cầu đọc accessibility/screenshot, không thao tác lên project. CLI 6.7.0 trả `capability_unavailable`: provider `composite:native-macos+rea-artifact-graph+rea-dotnet-static` yêu cầu macOS. Vì vậy lượt này không thu được bằng chứng runtime trên Windows; không suy ra tính năng từ tên resource hoặc trạng thái process.

Mô tả sản phẩm công khai được đối chiếu từ nguồn chính thức: [basic/advanced editing](https://www.capcut.com/resource/how-to-use-capcut), [desktop feature families](https://www.capcut.com/tools/desktop-ai-power), [keyframes](https://www.capcut.com/tools/keyframe-animation), [speed curves](https://www.capcut.com/tools/speed-ramp). Các trang này mô tả sản phẩm; không chứng minh feature đó có trong mọi phiên bản, tài khoản hoặc browser.

Lượt nghiên cứu bổ sung dùng [xuất một đoạn có In/Out](https://www.capcut.com/help/export-clip-of-video), [caption editor/batch edit](https://www.capcut.com/help/batch-edit-to-recognize-subtitle), [split subtitle](https://www.capcut.com/help/split-the-subtiltes), [subtitle import/edit](https://www.capcut.com/help/how-to-import-subtitles) và [audio export](https://www.capcut.com/resource/extract-audio-from-video) để chọn Gói 9/10. Tài liệu CapCut nêu Find/Replace phụ thuộc phiên bản và phân biệt [batch text thường](https://www.capcut.com/help/batch-edit-text) với caption. Catchim áp dụng trên text/caption track được người dùng chọn rõ, không suy ra word alignment tự động từ JSON mẫu.

Evidence mới giữ ở `%LOCALAPPDATA%/Temp/catchim-capcut-icon-caption-rea-20261023.json`. Hai file nguồn mẫu có SHA-256 `dc70113fa4bb8eccd43574df1cc10566066dfe0516e8fa0b1c5a195f2cb2e106` và `2c849031d0b4e72217258b3ea9426581d59dc47da87def37729477a3d006073d`; nội dung/asset CapCut không được đưa vào sản phẩm.

## Audit web và thứ tự tích hợp

| Nhóm | Bằng chứng source web | Trạng thái cần xử lý |
| --- | --- | --- |
| Media import/organizer | `components/editor/panels/assets/views/assets.tsx`, `core/managers/media-manager.ts`, storage service | Có search/type/sort, rename, source download, replacement/relink và Gói 8 bins/smart views/tags/favorites/bulk organize. Browser QA các luồng mới chưa chạy. |
| Multitrack, trim/split, move, clipboard | `commands/timeline/`, `timeline/placement/`, `timeline/group-move/` | Có model/command; lock và redo identity đã được regression test. GUI chưa xác minh. |
| Gap/ripple editing | `ripple/`, Rust `gap_edit.rs`, close-track-gaps command | Đợt này bổ sung đóng gap từng track, giữ leading offset/overlap/source mapping, một undo transaction; explicit command opt-out khỏi auto ripple lần hai. |
| Track controls | `timeline/types.ts`, track commands, shared scene/audio builders | Có mute/hide/lock; không suy ra solo, group/link identity hoặc reorder từ việc có nhiều track. |
| Canvas, transform, keyframes | `preview/controllers/`, `animation/`, keyframe commands, renderer resolve | Có transform/opacity/color/volume keyframes; cần QA toàn luồng và parity render. |
| Masks/compositing | `masks/`, `services/renderer/`, Rust `masks.rs`, `compositor.rs` | Có shape/freeform/feather paths; không suy ra AI cutout hay motion tracking. |
| Effects/color/transitions | `effects/definitions/index.ts`, `components/editor/panels/assets/index.tsx`, `native/webgpu-engine.ts` | Adjustment có 8 control và effect pass color-grading, với WebGPU cùng Rust/WASM fallback path; browser pixel/export parity chưa xác minh. Transitions vẫn chưa có model/command/render path. |
| Speed/time remap | `retime/resolve.ts`, `presets.ts`, `audio-stretch.ts` | Retime hiện là constant rate + tùy chọn giữ pitch; chưa có speed-curve model/reverse/optical-flow pipeline đầy đủ. |
| Text/graphics/stickers | `text/`, `graphics/`, `stickers/`, shared scene builder | Có element/renderer paths; cần audit font missing, line break, animation và export thực tế. |
| Captions | `subtitles/`, transcription service/worker | SRT/ASS import + auto-caption service đã có code. Đợt này thêm VTT import và SRT/VTT sidecar export text track; style/position VTT được báo import thành plain text. |
| TTS/audio | TTS service, generated-speech helper, audio mixer | Đợt này lưu WAV generated thành MediaAsset trước khi insert; tránh blob URL chết sau reload. Cần QA giọng/provider thực tế; không gọi dịch vụ tổng hợp để chạy test. |
| Voiceover/recording | Tìm trong web cho MediaRecorder/getUserMedia và audio UI | Chưa thấy workflow ghi mic/webcam/screen trong phạm vi tìm kiếm. Cần permission/lifecycle/asset storage trước khi mở UI khả dụng. |
| Audio analysis/effects | beat detection, waveform cache, mastering, Rust audio | Có beat/waveform/mix/limiter paths. Vocal isolation/ML denoise không được coi đã có chỉ vì có limiter. |
| Export | RendererManager → SceneExporter → CanvasRenderer | Đợt này Rust lập count/timestamp/duration frame, giữ partial frame cuối; TS điều phối GPU/browser encoder. Hủy audio có signal + checkpoints + cleanup. Chưa có golden MP4/WebM. |
| Project/history/storage | project manager, commands, migrations, OPFS/IndexedDB | Giữ schema hiện có. Chưa có portable project archive/relink/collaboration chỉ từ việc local save/load tồn tại. |
| Nâng cao/AI | Cần model/provider, renderer contract và benchmark riêng | Scene suggestions, auto reframe, stabilization, tracking, optical flow, compound clips, multicam, transcript edits, brand kit và cloud collaboration còn backlog; không tạo nút giả. |

## Ranh giới Rust / TypeScript đã áp dụng

- **Rust:** `ExportTiming` dùng số nguyên i128 tính ceil count và thời điểm từng frame; thời lượng frame cuối không vượt timeline. `compact_track_gaps` giải bài toán interval, giữ overlap/identity. Subtitle timecode kiểm tra dữ liệu và carry millisecond đúng qua giây/phút/giờ.
- **WASM ABI:** `ocw_export_frame_count`, `ocw_export_frame_sample`, `ocw_timeline_compact_gaps`; dùng ABI subtitle timestamp hiện có. Bridge kiểm tra safe integer/u32 và trả kết quả typed; giải phóng scratch memory ở mọi nhánh.
- **TypeScript:** UI/context menu, command/history, track/project references, WebVTT/SRT file adapter, file download, browser decode/encode, AbortController, event-loop scheduling, AudioContext ownership và OPFS/IndexedDB.
- **Tương thích:** giữ project format, API caller không truyền signal, Emscripten fallback. TS planner chỉ là đường tương thích khi Rust capability chưa tải hoặc artifact cũ; không đổi framework hay dựng engine song song.

## Phần thực tế đã triển khai trong đợt này

1. Đóng khoảng trống trên một track từ context menu; không dịch clip đầu, không phá overlap/trim/retime; track khóa và tác vụ no-op không thêm history; undo/redo qua CommandManager.
2. WAV TTS được lưu thành media upload, URL synthesis được revoke; chỉ insert sau khi lưu thành công và còn đúng scene/project. Test storage roundtrip vẫn đọc được bytes sau khi URL cũ bị revoke.
3. Hủy trong fetch/decode/mix/retime/mastering: trả cancelled, không tạo scene/exporter khi bị hủy trong prep; ignore late decode; đóng AudioContext do job sở hữu, giữ context do caller cấp. Browser decodeAudioData/OfflineAudioContext không có API abort hoàn toàn; không tuyên bố ngắt native compute tức thời.
4. Rust export timing + test WASM thật; timestamp không bị tích lũy sai số làm tròn, partial frame lấy duration còn lại. Exporter không phát complete khi hủy trong finalization.
5. VTT import (BOM, CRLF, cue id, shorthand timestamps, NOTE, plain text/entities) qua luồng caption command sẵn có. Xuất SRT/WebVTT từ đúng text track được chọn, lấy timing/content sau edit, sắp theo thời gian; không trộn title từ track khác. Bên trong menu track text: **Xuất phụ đề → SubRip/WebVTT**.
6. Gói 8 media organizer: thư mục theo project, smart views và bulk taxonomy trong TypeScript/IndexedDB/CommandManager; không đưa metadata giao diện vào Rust hoặc project timeline schema.

## Giới hạn kiểm chứng

Local browser automation bị Work settings tắt. Không dùng HTTP 200 hoặc test scene graph để thay cho GUI/âm thanh/MP4 QA. Ma trận tính năng ghi `PARTIAL` cho các workflow chưa có xác minh đầu cuối. Toàn bộ các nhóm CapCut và backlog V2 01–108 chưa hoàn tất; tài liệu này không đặt phần trăm hoàn thành giả.
