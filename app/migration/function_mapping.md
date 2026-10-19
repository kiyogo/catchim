# BÁO CÁO ÁNH XẠ CHỨC NĂNG (FUNCTION MAPPING)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Bảng ánh xạ chức năng cốt lõi (Core Functions Mapping)

| Phân hệ chức năng | Chức năng Web gốc | Triển khai C++ Native | Tệp nguồn C++ tương ứng | Trạng thái Parity |
|---|---|---|---|---|
| **Dự án (Project)** | Tạo mới dự án | `EditorEngine::newProject()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Đổi tên dự án | `EditorEngine::renameProject()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Tải dự án từ JSON | `EditorEngine::loadProjectFromJson()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Tuần tự hóa JSON | `EditorEngine::saveProjectToJson()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Lưu tệp nguyên tử | `ProjectStorage::saveAtomic()` | `src/storage/ProjectStorage.cpp` | **Đạt 100%** |
| | Quản lý dự án Hub | `ProjectsHubDialog` (Grid/List/Search/Sort/Duplicate/Delete) | `src/ui/projects/ProjectsHubDialog.cpp` | **Đạt 100%** |
| **Dòng thời gian (Timeline)** | Thêm clip vào track | `EditorEngine::addClip()` / `insertElement()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Di chuyển clip | `EditorEngine::moveClip()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Cắt đầu clip (Trim Start) | `EditorEngine::trimClipStart()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Cắt đuôi clip (Trim End) | `EditorEngine::trimClipEnd()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Tách clip tại playhead (Split 'S') | `EditorEngine::splitAtPlayhead()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Cắt bỏ bên trái (Split Left 'Q') | `EditorEngine::splitLeftAtPlayhead()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Cắt bỏ bên phải (Split Right 'W') | `EditorEngine::splitRightAtPlayhead()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Xóa clip đang chọn | `EditorEngine::deleteSelectedClips()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Xóa dồn (Ripple Delete) | `EditorEngine::rippleDelete()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Nhân bản clip | `EditorEngine::duplicateSelectedClips()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Tách/Ghép âm thanh gốc | `EditorEngine::toggleSourceAudioSeparation()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Bật/tắt hít nam châm (Snap 'N') | `EditorEngine::toggleSnapping()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Bật/tắt Ripple Mode ('R') | `EditorEngine::toggleRipple()` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Đặt mốc đánh dấu (Marker 'M') | `EditorEngine::toggleBookmarkAtPlayhead()`| `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| **Phát lại (Playback)** | Phát / Tạm dừng (Space) | `PlaybackController::toggle()` | `src/editor/playback/PlaybackController.cpp` | **Đạt 100%** |
| | Nhảy khung hình (◄ / ►) | `PlaybackController::stepFrame()` | `src/editor/playback/PlaybackController.cpp` | **Đạt 100%** |
| | Nhảy đến đầu / cuối (Home/End) | `EditorEngine::seek(0)` / `seek(duration)` | `src/editor/EditorEngine.cpp` | **Đạt 100%** |
| | Tua nhanh J / K / L | `PlaybackController::setRate()` | `src/editor/playback/PlaybackController.cpp` | **Đạt 100%** |
| **Lịch sử (History)** | Hoàn tác (Undo Ctrl+Z) | `CommandHistory::undo()` | `src/editor/history/CommandHistory.cpp` | **Đạt 100%** |
| | Làm lại (Redo Ctrl+Y) | `CommandHistory::redo()` | `src/editor/history/CommandHistory.cpp` | **Đạt 100%** |
| **Hiển thị & Canvas** | Tỉ lệ khung hình (16:9, 9:16, 1:1, 4:5, 21:9) | `PropertiesPanel` + `ProjectSettings` | `src/ui/panels/properties/PropertiesPanel.cpp` | **Đạt 100%** |
| | Tốc độ khung hình (24, 25, 30, 50, 60 fps) | `PropertiesPanel` + `RationalFrameRate` | `src/ui/panels/properties/PropertiesPanel.cpp` | **Đạt 100%** |
| | Màu nền canvas | `PropertiesPanel` QColorPicker | `src/ui/panels/properties/PropertiesPanel.cpp` | **Đạt 100%** |
| | Toàn màn hình (F11 / F) | `PreviewPanel::toggleFullscreen()` | `src/ui/preview/PreviewPanel.cpp` | **Đạt 100%** |
| | Khung an toàn (Safe Zones 'Z') | `PreviewPanel::toggleSafeZones()` | `src/ui/preview/PreviewPanel.cpp` | **Đạt 100%** |
| **Xuất video (Export)** | Hộp thoại xuất video (Ctrl+E) | `ExportDialog` | `src/ui/export/ExportDialog.cpp` | **Đạt 100%** |
| | Tiến trình mã hóa ngầm | `ExportWorker` (Đa luồng không khóa UI) | `src/ui/export/ExportWorker.cpp` | **Đạt 100%** |
| **Giao diện & Theme** | Chuyển đổi Dark / Light Theme | `EditorHeader::themeToggleClicked` | `src/ui/theme/Theme.cpp` | **Đạt 100%** |
| | Phím tắt trợ giúp (?) | `ShortcutsDialog` | `src/ui/shell/ShortcutsDialog.cpp` | **Đạt 100%** |
