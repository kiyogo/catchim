import type { ExportFormat, ExportQuality, ExportResolution } from "@/export";

export type ExportFpsPreset = "project" | "24" | "25" | "30" | "50" | "60";

export interface SavedExportPreset {
  id: string;
  name: string;
  format: ExportFormat;
  quality: ExportQuality;
  resolution: ExportResolution;
  includeAudio: boolean;
  fps: ExportFpsPreset;
  /** Omitted by presets saved before audio-only export was introduced. */
  audioOnly?: boolean;
}

const STORAGE_KEY = "catchim-export-presets-v1";
const MAX_EXPORT_PRESETS = 20;

function isPreset(value: unknown): value is SavedExportPreset {
  if (!value || typeof value !== "object") return false;
  const preset = value as Partial<SavedExportPreset>;
  return (
    typeof preset.id === "string" &&
    typeof preset.name === "string" &&
    preset.name.trim().length > 0 &&
    (preset.format === "mp4" || preset.format === "webm") &&
    (preset.quality === "low" ||
      preset.quality === "medium" ||
      preset.quality === "high" ||
      preset.quality === "very_high") &&
    ["source", "480p", "720p", "1080p", "1440p", "2160p", "4320p"].includes(
      preset.resolution ?? "",
    ) &&
    typeof preset.includeAudio === "boolean" &&
    (preset.audioOnly === undefined || typeof preset.audioOnly === "boolean") &&
    ["project", "24", "25", "30", "50", "60"].includes(preset.fps ?? "")
  );
}

export function loadSavedExportPresets(): SavedExportPreset[] {
  if (typeof window === "undefined") return [];
  try {
    const stored = window.localStorage.getItem(STORAGE_KEY);
    if (!stored) return [];
    const parsed: unknown = JSON.parse(stored);
    return Array.isArray(parsed)
      ? parsed.filter(isPreset).slice(0, MAX_EXPORT_PRESETS)
      : [];
  } catch {
    return [];
  }
}

export function saveExportPreset({
  preset,
}: {
  preset: SavedExportPreset;
}): SavedExportPreset[] {
  if (typeof window === "undefined") {
    throw new Error("Export presets are only available in a browser.");
  }
  if (!isPreset(preset)) {
    throw new Error("Invalid export preset.");
  }
  const current = loadSavedExportPresets().filter((item) => item.id !== preset.id);
  if (current.length >= MAX_EXPORT_PRESETS) {
    throw new Error(`You can save up to ${MAX_EXPORT_PRESETS} export presets.`);
  }
  const next = [preset, ...current];
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(next));
  return next;
}

export function deleteExportPreset({
  id,
}: {
  id: string;
}): SavedExportPreset[] {
  if (typeof window === "undefined") return [];
  const next = loadSavedExportPresets().filter((preset) => preset.id !== id);
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(next));
  return next;
}
