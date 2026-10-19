# BÁO CÁO ÁNH XẠ TRẠNG THÁI ỨNG DỤNG (STATE MAPPING)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Tổng quan Kiến trúc Trạng thái (State Architecture)

Trong phiên bản Web, trạng thái được phân tán qua các Zustand Store:
- `useEditor`: Trạng thái dự án, media, tracks, clips, selection, active tool.
- `useTimelineStore`: Trạng thái zoom, scrollLeft, playhead position, snapping, ripple.
- `useProjectsStore`: Trạng thái danh sách dự án, search query, sort key, sort order, view mode, selected project IDs.
- `useTheme`: Trạng thái theme hệ thống (dark/light).

Trong phiên bản C++ Native, toàn bộ trạng thái được mô hình hóa hướng đối tượng chặt chẽ, an toàn luồng và đồng bộ qua cơ chế Callback / Observer và Qt Signals & Slots:

```text
+---------------------------------------------------------------------------------+
|                               EDITOR ENGINE CORE                                |
|  +-------------------+  +----------------------+  +--------------------------+  |
|  | Project & Scenes  |  | PlaybackController   |  | CommandHistory           |  |
|  | - metadata        |  | - isPlaying          |  | - undoStack              |  |
|  | - settings        |  | - currentTime        |  | - redoStack              |  |
|  | - tracks & clips  |  | - playbackRate       |  | - transactionDepth       |  |
|  +-------------------+  +----------------------+  +--------------------------+  |
|                                                                                 |
|  +-------------------+  +----------------------+  +--------------------------+  |
|  | Selection State   |  | Timeline View State  |  | App Settings             |  |
|  | - selectedClipIds |  | - zoomLevel          |  | - theme (dark/light)     |  |
|  | - activeTrackId   |  | - scrollLeft         |  | - language               |  |
|  | - snappingEnabled |  | - rulerInterval      |  | - autosaveInterval       |  |
|  +-------------------+  +----------------------+  +--------------------------+  |
+---------------------------------------------------------------------------------+
                                      |
                           NOTIFY OBSERVERS / SLOTS
                                      v
  [EditorHeader]      [AssetsPanel]     [PreviewPanel]     [PropertiesPanel]   [TimelinePanel]
```

---

## 2. Chi tiết Ánh xạ Zustand Stores sang C++ Native

| Web Zustand Store / Field | Kiểu dữ liệu Web | Thành phần C++ Native | Cơ chế đồng bộ / Cập nhật |
|---|---|---|---|
| `editor.project` | `TProject` | `editor::Project` | `engine_.project()`, nạp/lưu qua `ProjectStorage` |
| `editor.currentTime` | `number` (microseconds) | `core::TimelineTime` | `engine_.playback().currentTime()`, đồng bộ mỗi tick 60Hz |
| `editor.isPlaying` | `boolean` | `bool` | `engine_.playback().isPlaying()`, điều khiển bởi Space / nút Play |
| `editor.selectedClipIds` | `string[]` | `std::vector<core::ClipId>` | `engine_.selectedClips()`, kích hoạt `onSelectionChanged` |
| `editor.canUndo / canRedo` | `boolean` | `bool` | `engine_.history().canUndo() / canRedo()`, cập nhật nút Undo/Redo |
| `editor.snappingEnabled` | `boolean` | `bool` | `engine_.isSnappingEnabled()`, cập nhật nút nam châm và phím 'N' |
| `editor.rippleEnabled` | `boolean` | `bool` | `engine_.isRippleEnabled()`, cập nhật nút Ripple và phím 'R' |
| `projectsStore.searchQuery` | `string` | `QString searchQuery_` | `ProjectsHubDialog::onSearchChanged()`, lọc thời gian thực |
| `projectsStore.sortOption` | `key-order` | `ProjectSortField` + `bool` | `ProjectsHubDialog::onSortFieldChanged()`, sắp xếp danh sách |
| `projectsStore.viewMode` | `'grid' \| 'list'` | `ProjectViewMode` | `ProjectsHubDialog::setViewMode()`, hoán đổi QStackedWidget |
| `projectsStore.selectedIds` | `string[]` | `std::unordered_set<std::string>` | `ProjectsHubDialog::updateToolbarState()`, bật/tắt nút xóa/nhân bản |
| `theme.theme` | `'dark' \| 'light'` | `ThemeMode` | `Theme::instance().setMode()`, nạp lại QSS toàn ứng dụng |

---

## 3. Tính An Toàn Dữ Liệu và Đồng Bộ Hóa

1. **Không xảy ra Race Condition**: Mọi thao tác biên tập timeline và render preview đều được điều phối tuần tự hoặc khóa bảo vệ mutex khi nạp media đa luồng.
2. **Khôi phục lỗi tức thì**: Quá trình lưu trữ dự án atomic qua `.tmp` file đảm bảo không bao giờ có tình trạng tệp JSON bị ghi dở dang khi xảy ra sự cố đột ngột.
3. **Đồng bộ hóa 100% giữa Ruler và Track**: Cả Ruler và Track đều chia sẻ chung một giá trị `TimelineTime` và `Metrics::trackLabelsWidth` (112px), đảm bảo playhead trên thước và playhead trên track di chuyển thẳng hàng tuyệt đối.
