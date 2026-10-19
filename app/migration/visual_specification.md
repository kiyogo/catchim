# TÀI LIỆU QUY CHUẨN THIẾT KẾ GIAO DIỆN (VISUAL SPECIFICATION)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Nguyên tắc Thiết kế Cốt lõi (Design Philosophy)

Giao diện của Catchim Desktop tuân thủ 100% ngôn ngữ thiết kế của bản Web gốc:
1. **Dark Zinc Elegance**: Nền chính là đen sâu Zinc-950 (`#09090b`), các panel chức năng nổi bật với tông `#121215`, các viền mảnh 1px (`#27272a`).
2. **Floating Panels Architecture**: Các panel không chạm sát mép cửa sổ mà được bọc trong container `editorBody` với margin 10px và spacing 4px, bo góc 8px.
3. **Pixel-Perfect Alignment**: Thước đo Ruler và các Track dòng thời gian đồng bộ hoàn toàn với độ lệch `trackLabelsWidth = 112px`. Playhead line trên Ruler và Track nằm thẳng tắp một hàng.
4. **Vibrant Accents**: Điểm nhấn thương hiệu sử dụng gradient xanh hiện đại (`#2567ec` -> `#38bdf8`), các clip trên timeline phân biệt màu sắc trực quan (Video xanh lam, Audio xanh lá, Text tím, Transition cam).

---

## 2. Thông số Kích thước & Khoảng cách (Metrics & Spacing)

```text
+-----------------------------------------------------------------------------------+
| Editor Header: Chiều cao 48px, Padding 12px 16px, Viền dưới 1px solid #27272a     |
+-----------------------------------------------------------------------------------+
| Margin: 10px                                                                      |
| +-------------------+  +--------------------------------+  +--------------------+ |
| | Assets Panel      |  | Preview Panel                  |  | Properties Panel   | |
| | Width: ~340px     |  | Width: ~780px (Tỉ lệ 50%)      |  | Width: ~320px      | |
| | Radius: 8px       |  | Radius: 8px                    |  | Radius: 8px        | |
| | Border: #27272a   |  | Border: #27272a                |  | Border: #27272a    | |
| +-------------------+  +--------------------------------+  +--------------------+ |
| Spacing: 4px                                                                      |
| +-------------------------------------------------------------------------------+ |
| | Timeline Panel: Chiều cao ~320px (Tỉ lệ 35%), Radius: 8px, Border: #27272a    | |
| | - Timeline Toolbar: Chiều cao 40px                                            | |
| | - Ruler Widget: Chiều cao 28px (Bắt đầu tại x = 112px)                        | |
| | - Track Labels: Chiều rộng 112px                                              | |
| | - Tracks Area: Bắt đầu tại x = 112px, Playhead Sky Blue #38bdf8               | |
| +-------------------------------------------------------------------------------+ |
| Margin: 10px                                                                      |
+-----------------------------------------------------------------------------------+
```

---

## 3. Quy chuẩn Chi tiết cho từng Thành phần

### 3.1. Editor Header
- Chiều cao cố định: `48px`.
- Logo & Project Title:
  - Nút Logo: `32x32px`, menu thả xuống với các lệnh Dự án (Tất cả dự án `Ctrl+Shift+O`, Tạo mới, Mở tệp, Lưu tệp, Phím tắt, Thoát).
  - Tên dự án: `QLineEdit` inline với viền trong suốt, khi focus viền sáng `#38bdf8`.
- Nút Hoàn tác / Làm lại (Undo / Redo): `30x30px`, icon 16px, tooltip phím tắt.
- Nút Xuất video:
  - Chiều cao: `32px`.
  - Gradient: `qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2567ec, stop:1 #38bdf8)`.
  - Chữ trắng đậm, bo góc `6px`.
- Nút Theme: `32x32px`, icon Mặt trời (Sun) ở Dark mode và Đĩa sáng tối (Contrast) ở Light mode.

### 3.2. Assets Panel
- TabBar dọc bên trái: `44px`, icon 20px. Tab đang chọn có vạch chỉ báo `border-left: 3px solid #38bdf8`.
- Khung tải tệp (Drop Zone): Bo góc `8px`, viền nét đứt `2px dashed #3f3f46`, nền `#18181b`. Khi kéo tệp qua đổi sang viền `#38bdf8`.

### 3.3. Preview Panel & Canvas
- Tỉ lệ khung hình linh hoạt: 16:9, 9:16 (TikTok), 1:1, 4:5, 21:9.
- Thước gióng & Safe Zones: Lưới an toàn hành động (90%) màu vàng mờ và an toàn tiêu đề (80%) màu xanh lá mờ.
- Thanh điều khiển phát lại dưới khung hình:
  - Timecode display: phông chữ monospace, nhãn hiển thị `00:00:00:00 / 00:01:23:00`.
  - Các nút: Step Back, Play/Pause (xanh lam), Step Forward, Safe Zones, Fullscreen.

### 3.4. Properties Panel
- Khi chọn clip: Hiển thị đầy đủ thông số Vị trí (X, Y), Tỉ lệ (Scale), Độ mờ (Opacity), Xoay (Rotation), Âm lượng (Volume dB).
- Khi không chọn clip: Hiển thị ngay bảng Cài đặt Khung hình Dự án (Canvas Settings) với các nút chọn nhanh tỉ lệ 16:9, 9:16, 1:1, 4:5, 21:9, bộ chọn FPS (24, 25, 30, 50, 60) và bộ chọn màu nền canvas.

### 3.5. Timeline Panel
- Đồng bộ Playhead:
  - Tay cầm thước đo (Playhead Handle): Đỉnh tam giác nhọn hướng xuống tại `x + 112px`.
  - Đường chỉ Playhead trên Track: Đường thẳng 2px màu xanh trời `#38bdf8` chạy xuyên suốt các track.
- Màu sắc các phân đoạn clip trên track:
  - Video clips: `#2563eb` (Xanh lam đậm) với viền `#3b82f6`.
  - Audio clips: `#059669` (Xanh lục ngọc) với biểu đồ sóng âm thanh RMS bên trong.
  - Text clips: `#7c3aed` (Tím) với nhãn văn bản.
  - Effect / Transition: `#d97706` (Hổ phách cam).
  - Trạng thái khi clip được chọn: Viền phát sáng 2px vàng trắng (`#ffffff`) hoặc xanh trời (`#38bdf8`).

### 3.6. Projects Hub Dialog
- Kích thước chuẩn: `1000 x 680px`.
- Breadcrumb: `Catchim / Tất cả dự án`.
- Bộ chuyển đổi chế độ xem: Icon Grid và Icon List với trạng thái kích hoạt nổi bật.
- Ô tìm kiếm: Tự động lọc danh sách khi người dùng gõ phím.
- Nút "+ Tạo dự án mới": Nút gradient xanh nổi bật.
- Thẻ dự án (Card): Tỉ lệ thumbnail 16:9, nhãn thời lượng góc dưới, nhãn ngày tạo góc dưới, menu thao tác 3 chấm `...`.
