import { describe, expect, it } from "vitest";
import { resolveExportCanvasSize } from "@/export";
import { findPlatformExportPreset, PLATFORM_EXPORT_PRESETS } from "./platform-presets";

describe("shipped export settings presets", () => {
  it("keeps platform settings separate from saved user presets and output modes", () => {
    expect(new Set(PLATFORM_EXPORT_PRESETS.map((preset) => preset.id)).size).toBe(PLATFORM_EXPORT_PRESETS.length);
    for (const preset of PLATFORM_EXPORT_PRESETS) {
      expect(Object.keys(preset).sort()).toEqual(["fps", "id", "name", "quality", "resolution"]);
      expect(preset.id.startsWith("platform-")).toBe(true);
      expect(findPlatformExportPreset(preset.id)).toBe(preset);
    }
    expect(findPlatformExportPreset("custom")).toBeUndefined();
  });
  it("preserves landscape and portrait project orientation when setting resolution", () => {
    for (const preset of PLATFORM_EXPORT_PRESETS) {
      for (const sourceSize of [{ width: 1920, height: 1080 }, { width: 1080, height: 1920 }]) {
        const output = resolveExportCanvasSize({ sourceSize, resolution: preset.resolution });
        expect(Math.sign(output.width - output.height)).toBe(Math.sign(sourceSize.width - sourceSize.height));
        expect(Math.abs(output.width - output.height * sourceSize.width / sourceSize.height)).toBeLessThanOrEqual(1);
        expect(sourceSize).toEqual(sourceSize.width > sourceSize.height ? { width: 1920, height: 1080 } : { width: 1080, height: 1920 });
      }
    }
  });
});
