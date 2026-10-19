import type { EditorCore } from "@/core";
import type { TProject } from "@/project/types";
import type { EditorSelectionSnapshot } from "@/selection/editor-selection";

type Snapshot = { project: TProject; selection: EditorSelectionSnapshot };
type Reply = { ok: boolean; error?: string; document: Snapshot; canUndo: boolean; canRedo: boolean };
let request: ((value: { op: string; document?: Snapshot }) => Reply) | null = null;
let projectId: string | null = null;
export const isBrowserHistoryActive = () => request !== null;

export async function initializeBrowserEditorCore() {
  if (!(globalThis as any).__CATCHIM_BROWSER__) return;
  const url = "/wasm/catchim_browser_core.mjs";
  const { default: factory } = await import(/* @vite-ignore */ /* webpackIgnore: true */ url);
  const module = await factory({ locateFile: (name: string) => `/wasm/${name}` });
  request = value => {
    // Project thumbnails can exceed Emscripten's stack. Marshal on the heap.
    const bytes = new TextEncoder().encode(JSON.stringify(value));
    const pointer = module._malloc(bytes.length + 1);
    if (!pointer) throw new Error("Không đủ bộ nhớ để lưu dự án");
    let reply: Reply;
    try {
      module.HEAPU8.set(bytes, pointer);
      module.HEAPU8[pointer + bytes.length] = 0;
      reply = JSON.parse(module.UTF8ToString(module._catchim_browser_request(pointer))) as Reply;
    } finally { module._free(pointer); }
    if (!reply.ok) throw new Error(reply.error || "C++ editor rejected edit");
    return reply;
  };
}

function capture(editor: EditorCore): Snapshot {
  return {
    project: { ...editor.project.getActive(), scenes: editor.scenes.getScenes(), currentSceneId: editor.scenes.getActiveScene().id },
    selection: editor.selection.getSnapshot(),
  };
}
export function resetBrowserHistory(editor: EditorCore) {
  if (!request) return;
  const document = capture(editor);
  request({ op: "load", document });
  projectId = document.project.metadata.id;
}
export function prepareBrowserHistory(editor: EditorCore) {
  if (request && projectId !== editor.project.getActiveOrNull()?.metadata.id) resetBrowserHistory(editor);
}
export function commitBrowserHistory(editor: EditorCore) {
  if (request) request({ op: "commit", document: capture(editor) });
}
export function restoreBrowserHistory(editor: EditorCore, op: "undo" | "redo") {
  if (!request) return;
  const selection = editor.selection.getSnapshot();
  const { document } = request({ op });
  const project = document.project;
  // Renaming is a project action outside the timeline's undo stack.
  project.metadata.name = editor.project.getActive().metadata.name;
  project.metadata.createdAt = new Date(project.metadata.createdAt);
  project.metadata.updatedAt = new Date(project.metadata.updatedAt);
  const buffers = new Map(editor.scenes.getScenes().flatMap(scene => scene.tracks.audio.flatMap(track => track.elements.map(element => [element.id, "buffer" in element ? element.buffer : undefined] as const))));
  for (const scene of project.scenes) {
    scene.createdAt = new Date(scene.createdAt);
    scene.updatedAt = new Date(scene.updatedAt);
    for (const track of scene.tracks.audio) for (const element of track.elements) {
      if ("buffer" in element) element.buffer = buffers.get(element.id);
    }
  }
  editor.project.setActiveProject({ project });
  editor.scenes.initializeScenes({ scenes: project.scenes, currentSceneId: project.currentSceneId });
  editor.selection.restoreSnapshot({ snapshot: selection });
  editor.timeline.discardPreview();
  editor.save.markDirty();
}
