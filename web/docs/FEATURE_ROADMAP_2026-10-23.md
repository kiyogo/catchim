# Lộ trình mở rộng Catchim Web Editor

Ngày lập: 2026-10-23
Phạm vi: `web/` — React/TypeScript, Rust/WASM và renderer trình duyệt.
Trạng thái: kế hoạch triển khai; chưa phải cam kết rằng các mục bên dưới đã có trong ứng dụng.

## Mục tiêu

Tăng nhanh số việc người dùng có thể làm trực tiếp trong editor, đồng thời đưa từng tính năng qua đủ giao diện, command/history, lưu/mở lại, preview và export. Bảng hiện tại ghi nhiều nền móng ở trạng thái `PARTIAL`; kế hoạch không tính một panel, shader hoặc module riêng lẻ là một workflow hoàn chỉnh.

## Thứ tự triển khai

### Gói 0 — Làm cho các luồng hiện có đáng tin cậy

Hoàn tất các phần đang có code nhưng chưa được xác minh đầu cuối. Đây là nền để các tính năng mới không chỉ hiện ra trên màn hình mà còn hoạt động sau khi mở lại dự án.

- Kiểm tra trên browser thật: media import/replace, rename, màu, captions, TTS, shortcuts, track lock, timeline drop/drag và settings.
- Tạo workflow kiểm tra preview → save → reload → undo/redo → export cho từng nhóm.
- Bổ sung relink cho media bị mất; giữ ID của asset và chỉ xác nhận khi nguồn mới đọc được.
- Hoàn tất chẩn đoán export: lỗi codec/dung lượng/bộ nhớ, trạng thái hủy và tiến trình rõ ràng.

### Gói 1 — Timeline và dựng cơ bản dễ dùng hơn

Ưu tiên cao nhất vì có nhiều thao tác người dùng thấy ngay và hiện có thể đi trên command/history model.

- Nâng tab Chuyển cảnh từ placeholder thành transition model thật: dissolve, fade, wipe và slide; xử lý clip thiếu source handles.
- Fade in/fade out cho video, ảnh, text và âm thanh; hiển thị handles trên clip.
- Ripple delete, ripple trim, roll trim và slip edit; mỗi thao tác là một giao dịch undo/redo.
- Marker trên timeline: thêm, đổi tên, màu, xóa, nhảy tới marker và lưu theo project.
- Điều khiển track: solo, đổi thứ tự, thêm track, đổi tên và grouping/link nhiều clip.
- Trường nhập chính xác cho thời gian, vị trí, scale, crop và âm lượng; snap bật/tắt và chỉnh khoảng snap.
- Phím tắt cho split, ripple delete, marker, zoom timeline, frame-step và nhảy giữa edit points.

### Gói 2 — Chuyển động, keyframe và tốc độ

- Preset chuyển động vào/ra cho clip và text; easing presets có preview.
- Thêm keyframe trực tiếp trên preview/timeline, chọn nhiều keyframe, copy/paste và điều chỉnh easing.
- Speed ramp dạng curve với preset Montage, Hero và Bullet; curve phải dùng chung cho preview/export/audio mapping.
- Reverse clip và freeze frame; lưu nguồn/thời điểm rõ ràng để không phá trim và undo.
- Điều khiển transform mở rộng: anchor point, crop, rotate, flip, fit/fill, safe-area guides.
- Preset canvas social: 9:16, 1:1, 4:5, 16:9, 3:4 và kích thước tùy chỉnh.

### Gói 3 — Visual, effects và compositing

- Thư viện filter/effect có phân loại, tìm kiếm, favorite, strength và apply cho nhiều clip.
- Bộ effect nền tảng: vignette, sharpen, grain, glow, chroma key, pixelate và blur có control thực.
- Color wheels, LUT import `.cube`, compare before/after và scopes cơ bản (waveform, RGB parade, vectorscope).
- Mask presets: rectangle, circle, linear/radial gradient; invert, feather và animate bằng keyframe.
- Layer blend modes và opacity controls nhất quán giữa preview và export.
- Text style presets, sticker packs nội bộ và title templates có thể sửa nội dung.

### Gói 4 — Audio và captions

- Thu voiceover từ microphone, chọn input, monitor mức âm lượng, đếm ngược và lưu thành media asset.
- Audio fades, keyframe volume, pan, normalize, compressor, EQ, reverb và noise gate.
- Ducking nhạc nền theo voiceover với tham số threshold/attack/release có thể chỉnh.
- Beat grid và marker theo nhịp; snap clip vào beat.
- Caption editor: sửa từng câu theo transcript, tìm/thay, gộp/tách cue, đổi timing hàng loạt.
- Style caption theo track: font, viền, nền, vị trí, animation và preset dọc/ngang; giữ rõ giới hạn của WebVTT khi nhập/xuất.
- Tạo captions nhiều ngôn ngữ và dịch transcript như một workflow riêng, có xác nhận trước khi thay nội dung.

### Gói 5 — Quản lý media, project và export

- Bins/folders, smart collections, tags, sort/filter lưu được và chọn nhiều asset.
- Relink media, cảnh báo duplicate, tạo thumbnail/waveform theo hàng đợi và trạng thái lỗi từng asset.
- Project archive portable gồm project + media; kiểm tra dung lượng, đường dẫn, font thiếu và khả năng mở lại.
- Autosave có trạng thái, recovery sau crash và lịch sử phiên bản cục bộ để phục hồi mốc.
- Export presets cho YouTube, TikTok, Reels và custom; chọn resolution/FPS/bitrate/codec theo khả năng browser.
- Export queue, cancel/retry, ước lượng dung lượng, vùng export và audio-only export.
- Kiểm tra file đầu ra bằng media probe và quy trình sample project để xác nhận duration, frame cuối, audio sync.

### Gói 6 — Tính năng AI và nâng cao

Chỉ bắt đầu sau khi các workflow media/export ổn định và đã chọn model/provider thực sự. Không tạo nút giả bằng heuristic rồi đặt tên như AI.

- Scene detection tạo marker/suggestions để người dùng duyệt trước khi cắt.
- Xóa khoảng lặng và đề xuất cut theo transcript; cho phép preview/hoàn tác trước khi áp dụng.
- Smart reframe có subject tracking cho 16:9 → 9:16/1:1.
- Background removal, motion tracking và stabilization.
- Voice isolation/noise reduction bằng model chuyên dụng.
- Proxy media cho project lớn và multicam sync bằng waveform/timecode.
- Tách lời/nhạc và nhận diện beat khi có engine/model phù hợp.

## Ranh giới TypeScript và Rust

| Lớp              | Phạm vi                                                                                                                                                                                              |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| TypeScript/React | UI, file picker, quyền microphone, drag/drop, browser codecs, WebAudio/WebGPU lifecycle, project/storage, async jobs, MCP và command/history.                                                        |
| Rust/WASM        | Planner thuần và xác định: timeline interval/ripple/roll/slip, transition timing, speed-curve mapping, marker/snap calculations, timecode, audio sample analysis/processing khi có ABI rõ ràng.      |
| Renderer         | Một scene/effect contract dùng chung cho preview và export; WGSL/WebGPU xử lý pixel khi phù hợp, fallback có kiểm tra. Rust không thay thế API giải mã/encode/GPU của browser.                       |
| AI               | Model/provider và license được chọn rõ; TypeScript điều phối worker/provider, Rust chỉ tham gia khi cần thuật toán CPU thuần. Chỉ báo khả dụng khi model thực sự chạy được trên môi trường mục tiêu. |

Không thêm project schema mới nếu mô hình hiện tại có thể mở rộng tương thích. Mọi command mới cần xử lý track lock, ID ổn định, no-op, lỗi lưu và undo/redo.

## Tiêu chí hoàn thành cho từng tính năng

1. Có điểm vào rõ ràng trong giao diện và thao tác dùng được với chuột/phím.
2. Tạo đúng command; undo/redo khôi phục dữ liệu và identity.
3. Lưu, đóng/mở lại vẫn giữ kết quả; migration chỉ thêm khi cần và có test tương thích.
4. Preview và export dùng cùng quy tắc thời gian/render; không báo thành công nếu browser từ chối thao tác.
5. Có test Rust cho planner/algorithm, test TypeScript cho adapter/command, và smoke workflow trên browser cho luồng người dùng.
6. Cập nhật `VIDEO_EDITOR_FEATURE_MATRIX.md` bằng bằng chứng thật; ghi rõ phần chưa được browser hoặc output QA.

## Cắt lát triển khai đề xuất tiếp theo

Bắt đầu với **Gói 1, lát đầu**: transition dissolve/fade + fade in/out, timeline markers, ripple delete/trim và track solo/reorder. Sau đó làm grouping/link và các thao tác roll/slip. Lát này bổ sung nhiều điều khiển nhìn thấy ngay, thay tab placeholder bằng chức năng thật, và kiểm tra được trên preview/export mà không cần chờ model AI.

### Tiến độ lát đầu — 2026-10-23

- Đã thêm panel Chuyển cảnh với fade in/out cho visual/audio clip và cross dissolve cho clip incoming nằm ở lớp trên, có vùng overlap hợp lệ.
- Đã thêm ripple delete, ripple trim, solo track, sắp thứ tự/đổi tên/thêm track và lệnh link/unlink; linked clip đi cùng khi kéo.
- Các bookmark/marker, snapping on/off, ripple mode và shortcut editor có sẵn nên được giữ nguyên.
- Đã chạy TypeScript check và 10 test mục tiêu. Cần browser QA để xác nhận thao tác chuột, preview, lưu/mở lại và file xuất.
- Phần còn lại của Gói 1: wipe/slide và transition handles, roll/slip, chỉnh ngưỡng snap, trường số chính xác xuyên suốt inspector, và hoàn thiện UX marker.

### Gói 2 — lát Motion preset đầu tiên — 2026-10-23

- Thêm tab Motion cho text, video, ảnh, sticker và graphic; có fade, zoom, trượt trái/phải/lên/xuống, hướng vào/ra, thời lượng và easing.
- Preset sinh keyframe opacity/transform trong animation data hiện có, áp dụng qua command để dùng history undo/redo; không đổi project schema và dùng renderer chung.
- TypeScript check đã chạy qua. Chưa xác minh thao tác trên browser, save/reopen hay file export thực tế.
- Còn lại của Gói 2: keyframe trực tiếp trên preview, copy/paste keyframe, speed ramp có Rust/TS mapping dùng chung, reverse/freeze, anchor/crop/flip/safe-area và thêm canvas presets.

### Gói 3 — lát effects library đầu tiên — 2026-10-23

- Nâng tab Effects thành thư viện có tìm kiếm, lọc Blur/Color/Keying/Stylize và favorite lưu trong trình duyệt.
- Thêm thao tác áp dụng effect vào visual clip đang chọn; vẫn giữ kéo/thêm effect layer như trước. Command add effect chặn track khóa.
- Đăng ký Vignette và Chroma key với điều khiển amount/softness/roundness hoặc key color/similarity/smoothness/spill; có pass WebGPU và fallback Rust/WASM trong effect contract.
- TypeScript check, production build và HTTP smoke các route editor/WASM/preview asset đều PASS. Chưa xác minh pixel trên browser hoặc file export đầu cuối.
- Còn lại của Gói 3: sharpen/grain/glow/pixelate, LUT `.cube`, color wheels/scopes, mask presets có invert/feather workflow, layer blend modes kiểm chứng đầy đủ và text/sticker templates.

### Gói 4 — lát thu voiceover đầu tiên — 2026-10-23

- Thêm tab Thu âm trong mục Âm thanh, cho phép chọn input micro, xin quyền sau thao tác chủ động, xem thời lượng đang thu và dừng để lưu.
- Bản ghi được lưu thành audio MediaAsset trước khi tạo upload-backed audio clip tại playhead; luồng dùng project storage và audio timeline hiện có, không thêm schema.
- Đã qua TypeScript và production build; đã khởi động lại localhost:3300. Chưa kiểm tra quyền micro, thiết bị vật lý, nghe preview hoặc file xuất trên browser thật.
- Phần còn lại của Gói 4: mức monitor và countdown, fade/keyframe volume/pan, normalize/EQ/compressor/reverb/noise gate, music ducking, beat grid/snap, caption editor và style presets.

### Gói 5 — lát preset Export đầu tiên — 2026-10-23

- Bổ sung chọn FPS cho một lần export: theo project hoặc 24/25/30/50/60 fps; giá trị truyền vào renderer dưới dạng `FrameRate` hữu tỉ hiện có.
- Cho lưu, nạp và xóa tối đa 20 preset export cục bộ gồm định dạng, chất lượng, độ phân giải, audio và FPS; dữ liệu nằm trong localStorage của browser, không sửa project settings.
- TypeScript, production build và HTTP smoke `/`, `/projects`, editor hiện tại, WASM đều PASS; thao tác lưu/nạp preset trong browser và chất lượng file đầu ra chưa QA.
- Chưa có preset platform dựng sẵn hoặc thay đổi aspect ratio, export queue/range, project archive hay browser/output QA.

### Gói 6 — lát dò điểm chuyển cảnh đầu tiên — 2026-10-23

- Thêm thao tác trên timeline khi chọn đúng một video clip; lấy mẫu tối đa 180 khung hình đã thu nhỏ, chấm độ khác nhau theo độ sáng và màu để đề xuất điểm cắt cảnh.
- Chuyển các điểm phát hiện thành bookmark qua BatchCommand, để có thể xem, di chuyển, xóa và undo bằng hệ thống marker/history hiện có.
- Đây là phát hiện cảnh bằng so sánh khung hình xác định, không gọi hoặc quảng bá thành mô hình AI. TypeScript, production build và HTTP smoke localhost PASS; chưa kiểm tra codec/file clip thực tế trên browser.
- Phần còn lại của Gói 6: scene suggestions có màn review/threshold, xóa khoảng lặng theo transcript, smart reframe/subject tracking, background removal, voice isolation, stabilization và proxy/multicam workflows.

### Gói 7 — phục hồi liên kết media bị mất — 2026-10-23

- Khi metadata còn trong IndexedDB nhưng source OPFS bị mất, giữ asset trong thư viện dưới trạng thái “Thiếu tệp nguồn” thay vì âm thầm bỏ asset khỏi project.
- Cho chọn lại source cùng loại và không ngắn hơn nguồn gốc; ghi file mới dưới media ID cũ để các clip trên timeline tự khôi phục.
- Dùng command riêng có undo/redo: undo đưa asset về trạng thái thiếu file, redo liên kết lại; asset offline không kéo/thêm clip và không tham gia audio playback/preview.
- Bảo toàn metadata offline khi xóa/undo và khi duplicate project. Không thay schema project; không cần thay đổi Rust/WASM.
- TypeScript + production build và HTTP smoke `/`, `/projects`, editor, WASM PASS; chưa mô phỏng OPFS file loss/relink hoặc GUI browser workflow.

### Gói 8 — thư viện Media có tổ chức — 2026-10-23

- Thêm thư mục/bin theo project: tạo, đổi tên, xóa; xóa bin giữ nguyên tệp và chuyển chúng về “Chưa phân loại”.
- Thêm smart views Tất cả, Yêu thích, Chưa phân loại, Thiếu tệp; lọc theo bin/tag và vẫn kết hợp được với search/type/sort hiện có.
- Gắn/bỏ tag, đánh dấu/bỏ yêu thích và chuyển nhiều asset cùng lúc; từng thay đổi là một command undo/redo.
- Lưu bin trong IndexedDB riêng theo project và lưu bin/tag/favorite cùng metadata asset hiện có; asset cũ được đọc với giá trị mặc định, không cần migration project.
- Import, replace, relink và undo xóa giữ taxonomy; không đổi timeline, preview hay export contract.
- TypeScript + production build PASS; production routes `/`, `/projects`, editor và WASM trả 200. Browser persistence và undo/redo workflow chưa QA.

### Gói 9 — Caption Workbench — 2026-10-23

- Captions → Sửa phụ đề: chọn rõ text/caption track, danh sách cue theo thời gian, tìm, sửa nội dung/start/end và seek tới cue.
- Tách cue theo playhead và vị trí con trỏ chữ; gộp với cue kế tiếp theo thời gian; giữ ID, trim provenance và animation slicing/clamping.
- Find/replace literal có phân biệt hoa thường, dịch toàn bộ cue theo offset (reject nguyên thao tác nếu cue đi trước 0), bốn preset style và xuất SRT/VTT ngay trong panel.
- Một thao tác là một command undo/redo; track khóa và no-op không thêm history. Không tạo transcript/project model thứ hai.
- Core timing Rust `caption_edit.rs`, typed bridge và compatibility path cho artifact cũ; real-WASM differential tests.

### Gói 10 — Xuất vùng chọn và WAV — 2026-10-23

- Chọn full timeline, span từ clip đang chọn hoặc In/Out tùy chỉnh; nút lấy mốc In/Out từ playhead. Span gồm toàn bộ nội dung timeline ở giữa hai mốc.
- Rust xác minh range nằm trong timeline; frame scheduler dùng độ dài range, render bằng thời gian scene gốc, encode timestamp bắt đầu từ 0 và giữ partial frame cuối.
- Audio mixer chỉ fetch/decode clip giao range, giữ source trim/retime/keyframe gain; cửa sổ output dùng cùng In/Out của video.
- Xuất audio-only WAV PCM16 stereo; Rust interleave/quantize theo chunk, TypeScript ghi RIFF và quản lý cancellation/browser buffer.
- Năm preset encoding YouTube/TikTok/Reels; preset giữ aspect ratio project, format và chế độ audio/video. Saved presets cũ vẫn đọc được.
- Preflight chặn nguồn media thiếu đang tham gia vùng xuất, có xét hidden/mute/solo/source-audio; nguồn ngoài vùng không chặn export.
- Gói 8 được sửa thêm: bulk toolbar không làm rơi selection; tạo thư mục khi thư viện rỗng; context menu bin nhận đầy đủ event/ref.

Kiểm tra mốc Gói 9/10: `npx vitest run --maxWorkers=4` **80 file / 401 test PASS**; `cargo test --manifest-path rust/crates/opencut_engine/Cargo.toml --lib` **51 test PASS**; TypeScript và production build PASS. HTTP `/`, `/projects`, editor và WASM PASS; artifact mới 120.448 byte có đủ ABI caption/range/PCM. Browser automation vẫn bị Work settings chặn; chưa coi đây là GUI, microphone hoặc file video đầu ra QA.

Đã phát hiện và sửa lỗi đóng gói production standalone: HTML 200 nhưng client chunks 404 vì thiếu `.next/static`. Build hiện copy đủ JS/CSS/public; `start:standalone` kiểm tra bộ file và mặc định cổng 3300. `check:web` đã xác nhận **3 route + 22 JS/CSS assets + WASM hợp lệ**. Đây là kiểm tra tải dependency thực tế, vẫn chưa thay browser interaction QA.

### Các lát tiếp theo

- LUT `.cube` với parser giới hạn/kiểm lỗi, pass renderer dùng chung và before/after compare.
- Keyframe copy/paste, chọn nhiều và nhập giá trị chính xác trong inspector.
- Hàng đợi export, portable project archive, saved smart collections, audio ducking/EQ và scene suggestions có màn review.
- Các tính năng học sâu như xóa nền, theo dõi chủ thể, smart reframe, ổn định và voice isolation chỉ mở sau khi chọn model/provider có thể chạy được.

## Tài liệu nền

- Trạng thái theo từng luồng: [VIDEO_EDITOR_FEATURE_MATRIX.md](./VIDEO_EDITOR_FEATURE_MATRIX.md)
- Nghiên cứu CapCut và audit web: [CAPCUT_WEB_PARITY_2026-10-23.md](./CAPCUT_WEB_PARITY_2026-10-23.md)
- Ranh giới Rust/WASM và TypeScript: [RUST_ARCHITECTURE.md](./RUST_ARCHITECTURE.md)
