# COMPONENT MAPPING — WEB TO NATIVE C++ QT6

### 1. Editor Workspace Components

| Web Component (React/Next.js) | Native C++ Component (Qt6 Widgets) | File Location | Key Responsibilities |
|---|---|---|---|
| `<EditorHeader />` | `EditorHeader` (`QWidget`) | `src/ui/shell/EditorHeader.cpp` | Logo menu, inline project name edit, Undo/Redo, Export button, Theme toggle |
| `<AssetsPanel />` | `AssetsPanel` (`QWidget`) | `src/ui/panels/assets/AssetsPanel.cpp` | Left TabBar (44px, 9 tabs), stacked subviews: Media, Audio, Text, Stickers, Effects, Transitions, Captions, Adjustment, Settings |
| `<PreviewPanel />` | `PreviewPanel` (`QWidget`) | `src/ui/preview/PreviewPanel.cpp` | OpenGL video canvas (`PreviewWidget`), Timecode display, frame stepping, Play/Pause, Zoom combo, Safe Zones, Fullscreen |
| `<PropertiesPanel />` | `PropertiesPanel` (`QWidget`) | `src/ui/panels/properties/PropertiesPanel.cpp` | When clip selected: Transform, Speed, Audio, Text, Effects. When empty: Project & Canvas Settings (Aspect Ratio buttons, FPS, Canvas BG) |
| `<Timeline />` | `TimelinePanel` (`QWidget`) | `src/ui/timeline/TimelinePanel.cpp` | Contains Toolbar, Ruler widget, Tracks scroll area |
| `<TimelineToolbar />` | `TimelineToolbar` (`QWidget`) | `src/ui/timeline/TimelineToolbar.cpp` | Split tools (Q/S/W), Extract audio, Duplicate, Delete, Bookmark, Add Track, Snapping, Ripple, Zoom slider |
| `<TimelineRuler />` | `TimelineRulerWidget` (`QWidget`) | `src/ui/timeline/TimelineRulerWidget.cpp` | Synchronized ticks, time labels, bookmarks, Playhead cursor aligned with 112px track labels |
| `<TimelineTracks />` | `TimelineTracksWidget` (`QWidget`) | `src/ui/timeline/TimelineTracksWidget.cpp` | Multi-track lanes, clip rendering, audio waveform bars, drag-to-move, trim handles, cross-track placement |
| `<ExportDialog />` | `ExportDialog` (`QDialog`) | `src/ui/export/ExportDialog.cpp` | Video format (MP4/WebM), resolution presets (480p..4K), quality presets, real-time render progress bar |
| `<ShortcutsDialog />` | `ShortcutsDialog` (`QDialog`) | `src/ui/shell/ShortcutsDialog.cpp` | Interactive shortcut list categorized by Playback, Timeline, Edit, Navigation |
| `<ProjectsPage />` | `ProjectsDialog` (`QDialog`) | `src/ui/projects/ProjectsDialog.cpp` | Projects manager: list/grid, search, new project, rename, delete, duplicate, open project |

### 2. Design System Tokens & QSS Styling

- Theme tokens defined in `src/ui/theme/Colors.h`:
  - Background: `#09090b` (zinc-950)
  - Surface/Panel: `#121215` (zinc-900 surface)
  - Text Primary: `#f4f4f5` (zinc-100)
  - Text Secondary: `#a1a1aa` (zinc-400)
  - Border: `#27272a` (zinc-800)
  - Primary Accent: `#38bdf8` (sky-400)
  - Export Gradient: `#2567ec` -> `#38bdf8`
  - Timeline clips: Video `#1e293b`, Audio `#8F5DBA`, Text `#5DBAA0`, Graphic `#BA5D7A`, Effect `#5D93BA`.
