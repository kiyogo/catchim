# ROUTE MAPPING — WEB TO NATIVE C++

| Web Route | URL | Purpose | C++ Native Implementation | Interaction Pattern |
|---|---|---|---|---|
| `/` | `src/app/page.tsx` | Landing / Direct App Access | Launch directly into Catchim Editor with Projects management access | Desktop Main Window |
| `/projects` | `src/app/projects/page.tsx` | Projects Hub & Manager | `ProjectsHubDialog` / `ProjectsView` via Editor Header logo menu & File menu | Native Dialog / Modal View with Grid/List & Search |
| `/editor/[project_id]` | `src/app/editor/[project_id]/page.tsx` | Core Video Editor Workspace | `MainWindow` (`catchim::ui::MainWindow`) | Primary Native Qt QMainWindow with 4 dynamic floating panels |
| Shortcuts Modal | `src/actions/components/shortcuts-dialog.tsx` | Phím tắt bàn phím | `ShortcutsDialog` (`catchim::ui::ShortcutsDialog`) | Native Modal Dialog (accessible via Header Logo Menu) |
| Export Modal | `src/components/editor/export-button.tsx` | Xuất video MP4/WebM | `ExportDialog` (`catchim::ui::ExportDialog`) | Native Modal Dialog with resolution, quality, format presets & progress |
