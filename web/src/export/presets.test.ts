import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { deleteExportPreset, loadSavedExportPresets, saveExportPreset, type SavedExportPreset } from "./presets";

const storageKey = "catchim-export-presets-v1";
const oldPreset: SavedExportPreset = { id: "old", name: "Existing preset", format: "webm", quality: "high", resolution: "source", includeAudio: false, fps: "project" };
let data: Map<string, string>;

beforeEach(() => {
  data = new Map();
  vi.stubGlobal("window", { localStorage: {
    getItem: (key: string) => data.get(key) ?? null,
    setItem: (key: string, value: string) => data.set(key, value),
  } });
});
afterEach(() => vi.unstubAllGlobals());

describe("export preset persistence compatibility", () => {
  it("retains existing video presets without the optional audio-only flag", () => {
    data.set(storageKey, JSON.stringify([oldPreset]));
    expect(loadSavedExportPresets()).toEqual([oldPreset]);
    expect(loadSavedExportPresets()[0].audioOnly ?? false).toBe(false);
  });

  it("round-trips audio-only settings and keeps existing presets intact", () => {
    data.set(storageKey, JSON.stringify([oldPreset]));
    const audio = { ...oldPreset, id: "audio", name: "Podcast", audioOnly: true, includeAudio: true };
    saveExportPreset({ preset: audio });
    expect(loadSavedExportPresets()).toEqual([audio, oldPreset]);
    expect(deleteExportPreset({ id: "audio" })).toEqual([oldPreset]);
  });

  it("skips corrupt entries without hiding valid existing presets", () => {
    data.set(storageKey, JSON.stringify([null, { ...oldPreset, audioOnly: "true" }, { ...oldPreset, fps: "144" }, oldPreset]));
    expect(loadSavedExportPresets()).toEqual([oldPreset]);
    data.set(storageKey, "{invalid JSON");
    expect(loadSavedExportPresets()).toEqual([]);
  });

  it("replaces an existing preset id even at capacity and rejects a new entry", () => {
    data.set(storageKey, JSON.stringify(Array.from({ length: 20 }, (_, id) => ({ ...oldPreset, id: String(id) }))));
    const updated = { ...oldPreset, id: "0", audioOnly: true };
    expect(saveExportPreset({ preset: updated })).toHaveLength(20);
    expect(loadSavedExportPresets()[0]).toEqual(updated);
    expect(() => saveExportPreset({ preset: { ...oldPreset, id: "new" } })).toThrow("up to 20");
  });

  it("does not report a successful save when browser storage rejects the write", () => {
    vi.stubGlobal("window", { localStorage: { getItem: () => null, setItem: () => { throw new Error("Quota exceeded"); } } });
    expect(() => saveExportPreset({ preset: oldPreset })).toThrow("Quota exceeded");
  });
});
