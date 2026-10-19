import { afterEach, expect, test, vi } from "vitest";
import { createCanvas } from "@napi-rs/canvas";
vi.mock("./webgpu-engine", () => ({ webGpuEngine: { isAvailable: false, device: null } }));
import { initCompositor, uploadTexture, renderFrame, getCompositorCanvas, releaseTexture } from "./opencut-wasm-compat";

afterEach(() => { releaseTexture("red"); releaseTexture("blue"); releaseTexture("mask"); vi.unstubAllGlobals(); });
function setup() {
  vi.stubGlobal("document", { createElement: () => Object.assign(createCanvas(8, 4), { setAttribute() {}, style: {} }) });
  vi.stubGlobal("OffscreenCanvas", function(width: number, height: number) { return createCanvas(width, height); });
  initCompositor(8, 4);
}
function source(color: string) {
  const canvas = createCanvas(8, 4); const ctx = canvas.getContext("2d"); ctx.fillStyle = color; ctx.fillRect(0, 0, 8, 4); return canvas;
}
function layer(id: string, blendMode = "normal", mask: any = null) {
  return { type: "layer", textureId: id, opacity: 1, blendMode, mask, effectPassGroups: [], transform: { centerX: 4, centerY: 2, width: 8, height: 4, rotationDegrees: 0 } };
}
test("fallback preserves blend modes instead of replacing them with normal alpha", () => {
  setup();
  uploadTexture({ id: "red", source: source("red") as any, width: 8, height: 4 });
  uploadTexture({ id: "blue", source: source("blue") as any, width: 8, height: 4 });
  renderFrame({ width: 8, height: 4, clear: { color: [0, 0, 0, 1] }, items: [layer("red"), layer("blue", "multiply")] });
  expect([...getCompositorCanvas().getContext("2d")!.getImageData(2, 2, 1, 1).data]).toEqual([0, 0, 0, 255]);
});
test("fallback applies full-frame masks at the transformed layer position", () => {
  setup();
  const mask = createCanvas(8, 4); const ctx = mask.getContext("2d"); ctx.fillStyle = "white"; ctx.fillRect(0, 0, 4, 4);
  uploadTexture({ id: "red", source: source("red") as any, width: 8, height: 4 });
  uploadTexture({ id: "mask", source: mask as any, width: 8, height: 4 });
  renderFrame({ width: 8, height: 4, clear: { color: [0, 0, 0, 1] }, items: [layer("red", "normal", { textureId: "mask", feather: 0, inverted: false })] });
  const output = getCompositorCanvas().getContext("2d")!;
  expect([...output.getImageData(1, 2, 1, 1).data]).toEqual([255, 0, 0, 255]);
  expect([...output.getImageData(6, 2, 1, 1).data]).toEqual([0, 0, 0, 255]);
});
