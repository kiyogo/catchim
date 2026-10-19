# TÀI LIỆU QUY CHUẨN HÀNH VI TƯƠNG TÁC (BEHAVIOR SPECIFICATION)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Hành vi Tương tác Dòng thời gian (Timeline Interaction Behaviors)

### 1.1. Thao tác Kéo & Thả (Drag & Drop)
- **Kéo Clip**:
  - Khi người dùng nhấn giữ chuột trái trên thân clip và di chuyển, clip sẽ trượt dọc theo track hoặc chuyển giữa các track tương thích.
  - Vị trí thời gian được tính toán dựa trên mức zoom hiện tại (`pixelsPerSecond`).
- **Cắt đầu / Đuôi clip (Trimming)**:
  - Khi di chuột vào 8px cạnh trái hoặc cạnh phải của clip, con trỏ đổi thành dạng `Qt::SizeHorCursor`.
  - Kéo cạnh trái điều chỉnh `trimStart` và `startTime`. Kéo cạnh phải điều chỉnh `duration`.
  - Khi chế độ Ripple được bật, các clip kế tiếp sẽ tự động dồn lại hoặc dịch sang phải tương ứng.
- **Hít nam châm thông minh (Smart Snapping)**:
  - Ngưỡng hít: `12px` trên màn hình.
  - Các điểm hít (Snap Points): Vị trí Playhead hiện tại, điểm bắt đầu và kết thúc của các clip lân cận trên tất cả các track, các điểm mốc Marker.
  - Khi đạt ngưỡng hít, vị trí clip sẽ tự động nhảy vào điểm căn gióng và hiển thị đường chỉ báo căn gióng (Snap Indicator Line).

### 1.2. Thao tác với Đầu đọc (Playhead Scrubbing & Seeking)
- Nhấn hoặc kéo chuột trên thanh Thước đo (Ruler) sẽ định vị ngay lập tức Playhead tới thời điểm đó.
- Cả Ruler và Track đều tôn trọng độ lệch nhãn track `Metrics::trackLabelsWidth` (112px).
- Bấm phím mũi tên `◄` hoặc `►` để dịch chuyển chính xác 1 khung hình (1 frame).
- Giữ phím `Shift` + `◄` / `►` để nhảy nhanh 5 giây.
- Bấm phím `Home` để về thời điểm 0s, bấm `End` để tới điểm cuối cùng của dòng thời gian.

### 1.3. Phím tắt Cắt gọt & Biên tập (Timeline Editing Hotkeys)
- Phím `S`: Cắt clip đang chọn (hoặc tất cả các clip nằm dưới Playhead) thành hai nửa độc lập tại vị trí Playhead.
- Phím `Q`: Cắt bỏ toàn bộ phần clip bên trái tính từ điểm bắt đầu clip đến Playhead và tự động dồn clip về vị trí Playhead nếu Ripple bật.
- Phím `W`: Cắt bỏ toàn bộ phần clip bên phải tính từ Playhead đến điểm kết thúc clip.
- Phím `Delete` / `Backspace`: Xóa các clip đang chọn.
- Phím `Ctrl + D`: Nhân bản các clip đang chọn và đặt nối tiếp ngay sau.
- Phím `M`: Đặt hoặc xóa mốc đánh dấu (Marker) tại Playhead.
- Phím `N`: Bật hoặc tắt chế độ hít nam châm (Snapping).
- Phím `R`: Bật hoặc tắt chế độ Ripple Editing.

---

## 2. Hành vi Phát lại & Xem trước (Playback & Preview Behaviors)

### 2.1. Phát lại Thời gian Thực (Real-Time Playback)
- Phím `Space`: Chuyển đổi giữa Phát và Tạm dừng.
- Khi đang phát, bộ hẹn giờ `QTimer` (60 Hz) sẽ kích hoạt đồng bộ hình ảnh và đẩy mẫu âm thanh qua WASAPI.
- Bộ giải mã âm thanh chạy trên luồng ngầm độc lập, không bao giờ làm đơ hoặc khựng luồng giao diện người dùng (UI Thread).
- Các phím J / K / L:
  - `J`: Tua lùi (-1x, -2x).
  - `K`: Tạm dừng.
  - `L`: Tua tiến (1x, 2x, 4x).

### 2.2. Khung xem trước & Hiển thị
- Phím `F11` hoặc phím `F`: Chuyển đổi toàn màn hình (Fullscreen preview).
- Phím `Z` (hoặc phím `T`): Bật/tắt hiển thị lưới an toàn (Safe Zones).
- Kéo thả tỷ lệ chia đôi màn hình qua các thanh QSplitter diễn ra mượt mà, kích thước các panel tự co giãn tỉ lệ phù hợp.

---

## 3. Hành vi Quản lý Dự án & Tệp (Project & File Behaviors)

### 3.1. Hộp thoại Quản lý Dự án (Projects Hub)
- Phím tắt `Ctrl + Shift + O`: Mở ngay Hộp thoại Quản lý Dự án.
- Nhấp đúp vào bất kỳ thẻ dự án nào sẽ mở và nạp dự án đó vào không gian làm việc.
- Khi gõ từ khóa vào ô tìm kiếm, danh sách dự án được lọc tức thời không cần bấm Enter.
- Chuyển đổi qua lại giữa chế độ Lưới (Grid) và Danh sách (List) giữ nguyên trạng thái chọn của người dùng.
- Thao tác xóa dự án bắt buộc hiển thị hộp thoại cảnh báo xác nhận, ngăn ngừa việc vô tình xóa mất tệp quan trọng.

### 3.2. Lưu tệp & Tự động sao lưu
- Phím `Ctrl + S`: Lưu tức thì trạng thái dự án hiện tại vào tệp JSON nguyên tử.
- Phím `Ctrl + O`: Mở hộp thoại chọn tệp hệ thống Windows (`.json`, `.catchim`).
- Cơ chế ghi đệm qua `.tmp` file đảm bảo không bao giờ làm hỏng tệp gốc ngay cả khi có sự cố hệ thống.
