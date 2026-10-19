# BÁO CÁO ÁNH XẠ TÀI NGUYÊN VÀ ASSETS (ASSET MAPPING)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Tổng quan Tài nguyên & Assets

Ứng dụng Web gốc sử dụng các loại tài nguyên chính:
1. **Iconography**: Gói `@hugeicons/*` và `lucide-react` (SVG icons).
2. **Typography**: Phông chữ Inter / Cascadia Code / sans-serif qua Google Fonts và Tailwind CSS.
3. **Color Palette**: Bảng màu Zinc của Tailwind CSS kết hợp màu điểm nhấn Sky-400 và Blue-600.
4. **Media Asset Probing**: Các thumbnail video và icon biểu tượng dự án.

---

## 2. Bảng ánh xạ Icon SVG (Web sang C++ `UiIcons`)

Trong C++, lớp `UiIcons` (`src/ui/icons/UiIcons.h` & `UiIcons.cpp`) đảm nhiệm việc kết xuất vector SVG thời gian thực qua `QSvgRenderer` và `QPainter`, hỗ trợ đổi màu (tinting) theo trạng thái Normal, Active/Hover, và Disabled:

| Biểu tượng (Icon Name) | Gói Web (`@hugeicons` / Lucide) | Giá trị Enum C++ (`UiIcon`) | Đường dẫn Vector SVG chuẩn |
|---|---|---|---|
| Media / Video Files | `Video01Icon` / `OcVideoIcon` | `UiIcon::Media` | Folder & filmstrip SVG |
| Audio Files | `AudioWaveIcon` / `Headphones` | `UiIcon::Audio` | Headphones & waveform SVG |
| Text / Typography | `TextIcon` / `Type` | `UiIcon::Text` | Letter 'T' polyline SVG |
| Stickers & Graphics | `SmileIcon` / `Sticker` | `UiIcon::Stickers` | Smiley face SVG |
| Visual Effects | `MagicWandIcon` / `Wand2` | `UiIcon::Effects` | Wand with sparkle SVG |
| Transitions | `ArrowLeftRight` | `UiIcon::Transitions` | Double chevron SVG |
| Captions & Subtitles | `SubtitlesIcon` | `UiIcon::Captions` | Rect with caption bars SVG |
| Settings | `Settings02Icon` / `Sliders` | `UiIcon::Settings` | Multi-slider toggles SVG |
| Split Clip | `ScissorsIcon` / `Cut` | `UiIcon::Split` | Scissor blades SVG |
| Duplicate Clip | `Copy01Icon` / `Copy` | `UiIcon::Duplicate` | Overlapping rounded rects SVG |
| Delete Clip / Project | `Delete02Icon` / `Trash2` | `UiIcon::Delete` | Trash can SVG with lid |
| Magnet / Snapping | `MagnetIcon` | `UiIcon::Magnet` | Horseshoe magnet SVG |
| Playback Controls | `PlayIcon`, `PauseIcon`, `Step` | `UiIcon::Play`, `Pause`, `StepBack` | Rounded play polygon, pause bars SVG |
| Zoom Slider | `ZoomInIcon`, `ZoomOutIcon` | `UiIcon::ZoomIn`, `ZoomOut` | Magnifying glass with +/- SVG |
| Undo / Redo | `Undo01Icon`, `Redo01Icon` | `UiIcon::Undo`, `UiIcon::Redo` | Curved circular arrow SVG |
| Export Video | `Share01Icon` / `Download` | `UiIcon::Export` | Arrow leaving tray SVG |
| Theme Toggle | `Sun01Icon`, `Moon01Icon` | `UiIcon::Sun`, `UiIcon::Contrast` | Sun rays & half circle contrast SVG |
| Projects & Folders | `Folder01Icon` | `UiIcon::Folder` | Folder tab SVG |
| Grid & List View | `GridViewIcon`, `ListDash` | `UiIcon::Grid`, `UiIcon::List` | 4-grid squares, list dashes SVG |
| Clock / Calendar | `Calendar04Icon`, `Clock01` | `UiIcon::Clock` | Clock circle with hands SVG |
| Keyboard Shortcuts | `Keyboard` | `UiIcon::Keyboard` | Keyboard keys layout SVG |

---

## 3. Hệ thống Phông chữ & Typography (Typography System)

- **Web**: Định nghĩa trong `tailwind.config.ts`: `font-sans: ['Inter', ...]` và `font-mono: ['JetBrains Mono', ...]`.
- **C++**: Tự động nhận diện phông chữ hệ thống Windows qua `QFont`:
  - UI Font chính: `-apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif`.
  - Phím tắt & Timecode: `Cascadia Code`, `Consolas`, `monospace`.
  - Hỗ trợ đầy đủ tiếng Việt có dấu Unicode UTF-8 (`/utf-8` compiler flag).

---

## 4. Hệ màu chuẩn (Color Token Alignment)

| Mã màu thiết kế | Hex Code | Tên màu tương ứng Web | Sử dụng trong C++ (`Colors.h` / `Theme.h`) |
|---|---|---|---|
| Background Dark | `#09090b` | `zinc-950` | Nền cửa sổ chính, khoảng đệm giữa các panels |
| Panel Background | `#121215` | Custom Dark Zinc | Nền của 4 panel card chính, Projects Hub |
| Card / Container | `#18181b` | `zinc-900` | Nền thumbnail box, thanh tab, dropdown |
| Border Default | `#27272a` | `zinc-800` | Viền panels, viền card, đường phân chia |
| Border Muted | `#3f3f46` | `zinc-700` | Viền hover, đường phân cách ruler |
| Text Primary | `#f4f4f5` | `zinc-100` | Tiêu đề, tên dự án, nhãn nút chính |
| Text Secondary | `#a1a1aa` | `zinc-400` | Nhãn mô tả, thông số thuộc tính |
| Text Muted | `#71717a` | `zinc-500` | Chú thích thời gian, breadcrumbs |
| Brand Accent Primary| `#2567ec` | `blue-600` | Nút xuất video gradient stop 1, tab active |
| Brand Accent Sky | `#38bdf8` | `sky-400` | Nút xuất video gradient stop 2, playhead, active border |
| Destructive Red | `#ef4444` | `red-500` | Nút xóa dự án, xóa clip |
