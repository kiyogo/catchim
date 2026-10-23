import type { ExportQuality, ExportResolution } from "@/export";
import type { ExportFpsPreset } from "./presets";

/** Encoding suggestions only. The project canvas and aspect ratio are retained. */
export interface PlatformExportPreset {
  id: string;
  name: string;
  quality: ExportQuality;
  resolution: ExportResolution;
  fps: ExportFpsPreset;
}

export const PLATFORM_EXPORT_PRESETS: readonly PlatformExportPreset[] = [
  { id: "platform-youtube-hd", name: "YouTube · 1080p · 30 fps", quality: "high", resolution: "1080p", fps: "30" },
  { id: "platform-youtube-4k", name: "YouTube · 4K · 30 fps", quality: "very_high", resolution: "2160p", fps: "30" },
  { id: "platform-shorts", name: "YouTube Shorts · 1080p · 60 fps", quality: "high", resolution: "1080p", fps: "60" },
  { id: "platform-tiktok", name: "TikTok · 1080p · 30 fps", quality: "high", resolution: "1080p", fps: "30" },
  { id: "platform-reels", name: "Instagram Reels · 1080p · 30 fps", quality: "high", resolution: "1080p", fps: "30" },
];

export function findPlatformExportPreset(id: string): PlatformExportPreset | undefined {
  return PLATFORM_EXPORT_PRESETS.find((preset) => preset.id === id);
}
