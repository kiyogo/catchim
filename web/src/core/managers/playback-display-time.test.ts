import { afterEach, expect, test, vi } from "vitest";
import { PlaybackManager } from "./playback-manager";
import type { EditorCore } from "@/core";
afterEach(() => vi.unstubAllGlobals());
test("display clock advances between source frames without changing editing time", () => {
  let now = 0;
  vi.stubGlobal("performance", { now: () => now });
  vi.stubGlobal("requestAnimationFrame", () => 1); vi.stubGlobal("cancelAnimationFrame", () => {});
  const editor = {
    timeline: { getTotalDuration: () => 1200000 },
    project: { getActive: () => ({ settings: { fps: { numerator: 30, denominator: 1 } } }) },
  } as unknown as EditorCore;
  const playback = new PlaybackManager(editor); playback.play(); now = 1000 / 120;
  expect(playback.getCurrentTime()).toBe(0);
  expect(playback.getDisplayTime()).toBe(1000);
  now = 1000 / 60; expect(playback.getDisplayTime()).toBe(2000);
  playback.pause(); expect(playback.getDisplayTime()).toBe(playback.getCurrentTime());
});
