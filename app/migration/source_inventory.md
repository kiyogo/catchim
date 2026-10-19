# SOURCE INVENTORY — CATCHIM WEB TO NATIVE C++

| Original Web Path | Type | Purpose | Dependencies | Target C++ Implementation | Status |
|---|---|---|---|---|---|
| `src/app/editor/[project_id]/page.tsx` | Next.js Page | Core Editor Layout & Resizable Panels | Lucide/Hugeicons, Zustand, OpenCut core | `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h` | VERIFIED |
| `src/components/editor/editor-header.tsx` | Component | Top navigation, project name, theme toggle, export trigger | Hugeicons, Radix Dropdown | `src/ui/shell/EditorHeader.cpp`, `EditorHeader.h` | VERIFIED |
| `src/components/editor/export-button.tsx` | Component | Export dialog button & modal trigger | Radix Dialog, Hugeicons | `src/ui/export/ExportDialog.cpp`, `ExportWorker.cpp` | VERIFIED |
| `src/components/editor/panels/assets/index.tsx` | Component | Left Assets TabBar and stacked panels | Radix, Hugeicons | `src/ui/panels/assets/AssetsPanel.cpp`, `AssetsPanel.h` | VERIFIED |
| `src/components/editor/panels/assets/tabbar.tsx` | Component | 9-item vertical sidebar tabs (Media, Audio, Text, Stickers, Effects...) | Hugeicons, Tooltip | `src/ui/panels/assets/AssetsPanel.cpp` (QListWidget #tabList) | VERIFIED |
| `src/components/editor/panels/assets/views/assets.tsx` | Component | Media upload dropzone & project media library | Media library, drag & drop | `src/ui/panels/assets/AssetsPanel.cpp` (MediaListWidget) | VERIFIED |
| `src/components/editor/panels/assets/views/settings/index.tsx` | Component | Canvas Settings (16:9, 9:16, 1:1, FPS, Canvas BG) | Editor Store, Project Settings | `src/ui/panels/properties/PropertiesPanel.cpp` & `AssetsPanel.cpp` | VERIFIED |
| `src/components/editor/panels/properties/index.tsx` | Component | Property Inspector for selected clip & canvas | Radix, Forms | `src/ui/panels/properties/PropertiesPanel.cpp` | VERIFIED |
| `src/components/editor/panels/properties/empty-view.tsx` | Component | Empty inspector / Project Canvas Settings | Hugeicons Settings05 | `src/ui/panels/properties/PropertiesPanel.cpp` (emptyView_) | VERIFIED |
| `src/preview/components/index.tsx` | Component | Viewport preview canvas & transport bar | OpenGL/WebGL, Canvas | `src/ui/preview/PreviewPanel.cpp`, `PreviewWidget.cpp` | VERIFIED |
| `src/preview/components/preview-controls.tsx` | Component | Timecode, Play/Pause, Step frame, Zoom, Safe Zones | Lucide, Timecode utils | `src/ui/preview/PreviewPanel.cpp` | VERIFIED |
| `src/timeline/components/index.tsx` | Component | Main Timeline container & tracks viewport | Timeline Store, Interaction | `src/ui/timeline/TimelinePanel.cpp`, `TimelineTracksWidget.cpp` | VERIFIED |
| `src/timeline/components/timeline-toolbar.tsx` | Component | Split Q/S/W, Unlink Audio, Duplicate, Delete, Snapping, Zoom | Toolbar buttons, Slider | `src/ui/timeline/TimelineToolbar.cpp`, `TimelineToolbar.h` | VERIFIED |
| `src/timeline/components/timeline-ruler.tsx` | Component | Time ticks, Playhead indicator, bookmarks row | Timecode, Scale | `src/ui/timeline/TimelineRulerWidget.cpp`, `TimelineRulerWidget.h` | VERIFIED |
| `src/timeline/components/timeline-track.tsx` | Component | Individual video/audio/text tracks & clips | Wavesurfer, Canvas | `src/ui/timeline/TimelineTracksWidget.cpp` | VERIFIED |
| `src/actions/components/shortcuts-dialog.tsx` | Component | Keyboard shortcuts cheatsheet | Dialog | `src/ui/shell/ShortcutsDialog.cpp`, `ShortcutsDialog.h` | VERIFIED |
| `src/app/projects/page.tsx` | Next.js Page | Projects Hub (list, search, sort, create, delete) | Project store, Drizzle/Storage | `src/ui/projects/ProjectsDialog.cpp`, `ProjectsDialog.h` | IN_PROGRESS |
| `src/core/` | Core Engine | Video/Audio/Timeline Core Models & Math | C++20 standard | `src/core/`, `src/editor/`, `src/media/`, `src/render/` | VERIFIED |
