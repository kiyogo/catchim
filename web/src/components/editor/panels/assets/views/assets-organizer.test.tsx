import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { beforeEach, describe, expect, it, vi } from "vitest";
import type { MediaAsset, MediaBin } from "@/media/types";

const state = vi.hoisted(() => ({
  assets: [] as MediaAsset[],
  bins: [] as MediaBin[],
  isDragOver: false,
}));

vi.mock("@/i18n", () => ({ useTranslation: () => ({ t: (key: string) => key, language: "en" }) }));
vi.mock("@/editor/use-editor", () => {
  const editor = {
    media: { getAssets: () => state.assets, getBins: () => state.bins },
    project: { getActive: () => ({ metadata: { id: "project" } }) },
  };
  return { useEditor: (selector?: (value: typeof editor) => unknown) => selector ? selector(editor) : editor };
});
vi.mock("@/media/use-file-upload", () => ({ useFileUpload: () => ({ isDragOver: state.isDragOver, dragProps: {}, openFilePicker: () => {}, fileInputProps: { type: "file" } }) }));
vi.mock("@/components/editor/panels/assets/assets-panel-store", () => ({ useAssetsPanelStore: () => ({ mediaViewMode: "grid", setMediaViewMode: () => {}, highlightMediaId: null, clearHighlight: () => {}, mediaSortBy: "name", mediaSortOrder: "asc", setMediaSort: () => {} }) }));
vi.mock("@/media/processing", () => ({ processMediaAssets: vi.fn() }));
vi.mock("@/media/upload-toast", () => ({ showMediaUploadToast: vi.fn() }));
vi.mock("@/actions", () => ({ invokeAction: vi.fn() }));
vi.mock("@/components/ui/use-overlay-open-change", () => ({ useOverlayOpenChange: ({ onOpenChange }: { onOpenChange?: (open: boolean) => void }) => onOpenChange }));
vi.mock("@/timeline/element-utils", () => ({ buildElementFromMedia: vi.fn() }));
vi.mock("@/components/editor/panels/assets/draggable-item", () => ({ DraggableItem: () => null }));

import { MediaView } from "./assets";

beforeEach(() => {
  state.assets = [];
  state.bins = [];
  state.isDragOver = false;
});

describe("media library organization in an empty project", () => {
  it("keeps folder creation and library filters accessible beside the upload prompt", () => {
    const html = renderToStaticMarkup(createElement(MediaView));
    expect(html).toContain("assets.addBin");
    expect(html).toContain("assets.allMedia");
    expect(html).toContain("assets.favorites");
    expect(html).toContain("assets.dragDropMediaHere");
    expect(html).not.toContain("assets.noMediaMatches");
  });

  it("keeps an existing empty folder visible when only temporary audio assets exist", () => {
    state.bins = [{ id: "bin", name: "Empty folder" }];
    state.assets = [{ id: "temporary", ephemeral: true, tags: [] } as unknown as MediaAsset];
    const html = renderToStaticMarkup(createElement(MediaView));
    expect(html).toContain("Empty folder");
    expect(html).toContain("assets.addBin");
    expect(html).toContain("assets.dragDropMediaHere");
  });

  it("keeps the full drop target during an active external file drag", () => {
    state.isDragOver = true;
    const html = renderToStaticMarkup(createElement(MediaView));
    expect(html).toContain("assets.dragDropMediaHere");
    expect(html).not.toContain("assets.addBin");
  });
});
