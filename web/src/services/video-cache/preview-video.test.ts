import { afterEach, expect, test, vi } from "vitest";
import { PreviewVideoPool } from "./preview-video";
class Video extends EventTarget {
  muted = false; playsInline = false; preload = ""; dataset = {}; style = {}; duration = 60; paused = true; playbackRate = 1;
  error = null; videoWidth = 1920; videoHeight = 1080; seeks = 0; time = 0; plays = 0;
  set src(value: string) { queueMicrotask(() => this.dispatchEvent(new Event("loadeddata"))); }
  get currentTime() { return this.time; }
  set currentTime(value: number) { this.time = value; this.seeks++; queueMicrotask(() => this.dispatchEvent(new Event("seeked"))); }
  pause() { this.paused = true; }
  play() { this.paused = false; this.plays++; return Promise.resolve(); }
  load() {} remove() {} removeAttribute() {}
}
afterEach(() => vi.unstubAllGlobals());
test("live playback never seeks each frame, pauses inactive clips and isolates duplicate sources", async () => {
  const videos: Video[] = [];
  vi.stubGlobal("document", { createElement: () => { const video = new Video(); videos.push(video); return video; }, body: { appendChild() {} } });
  const pool = new PreviewVideoPool(); const file = new File(["video"], "test.mp4");
  pool.beginFrame(); await pool.get({ key: "clip-a", file, time: 0, playing: true, rate: 1 }); pool.endFrame();
  for (let i = 1; i < 120; i++) {
    videos[0].time = i / 60; pool.beginFrame();
    await pool.get({ key: "clip-a", file, time: i / 60, playing: true, rate: 1 }); pool.endFrame();
  }
  expect(videos[0].seeks).toBe(0); expect(videos[0].plays).toBe(1);
  pool.beginFrame(); await pool.get({ key: "clip-b", file, time: 12, playing: false, rate: 1 }); pool.endFrame();
  expect(videos[0].paused).toBe(true); expect(videos[1].currentTime).toBe(12);
  expect(videos[0].currentTime).toBeCloseTo(119 / 60); pool.clear();
});
