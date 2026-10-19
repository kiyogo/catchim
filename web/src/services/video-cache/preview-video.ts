// Live preview samples native decoded video surfaces directly. Export continues
// to use timestamp-exact WebCodecs frames in VideoCache.
type Entry = { video: HTMLVideoElement; file: File; url: string; ready: Promise<void>; used: number; playPending: boolean };
export class PreviewVideoPool {
  private entries = new Map<string, Entry>();
  private failedFiles = new WeakSet<File>();
  private generation = 0;
  beginFrame() { this.generation++; }
  endFrame() {
    for (const [key, entry] of this.entries) {
      if (entry.used !== this.generation) entry.video.pause();
      if (this.generation - entry.used > 300 || (this.entries.size > 8 && entry.used !== this.generation)) this.remove(key);
    }
  }
  pause() { for (const entry of this.entries.values()) entry.video.pause(); }
  private remove(key: string) {
    const entry = this.entries.get(key);
    if (!entry) return;
    entry.video.pause(); entry.video.removeAttribute("src"); entry.video.load(); entry.video.remove();
    URL.revokeObjectURL(entry.url); this.entries.delete(key);
  }
  clear() { for (const key of this.entries.keys()) this.remove(key); }
  isKnownUnsupported(file: File) { return this.failedFiles.has(file); }
  markUnsupported(file: File) { this.failedFiles.add(file); }
  async get({ key, file, time, playing, rate }: { key: string; file: File; time: number; playing: boolean; rate: number }): Promise<HTMLVideoElement> {
    let entry = this.entries.get(key);
    if (entry && entry.file !== file) { this.remove(key); entry = undefined; }
    if (!entry) {
      const video = document.createElement("video");
      video.muted = true; video.playsInline = true; video.preload = "auto";
      video.dataset.catchimPreviewVideo = key;
      video.style.cssText = "position:fixed;left:-2px;top:-2px;width:1px;height:1px;opacity:0;pointer-events:none";
      const url = URL.createObjectURL(file);
      const ready = new Promise<void>((resolve, reject) => {
        const finish = (error?: Error) => {
          clearTimeout(timeout); video.removeEventListener("loadeddata", loaded); video.removeEventListener("error", failed);
          error ? reject(error) : resolve();
        };
        const loaded = () => finish();
        const failed = () => finish(new Error(video.error?.message || "Không thể giải mã video"));
        const timeout = setTimeout(() => finish(new Error("Video decode timed out")), 15000);
        video.addEventListener("loadeddata", loaded); video.addEventListener("error", failed);
      });
      entry = { video, file, url, ready, used: this.generation, playPending: false };
      this.entries.set(key, entry); document.body.appendChild(video); video.src = url;
    }
    entry.used = this.generation;
    await entry.ready;
    const video = entry.video;
    const target = Math.max(0, Math.min(time, Math.max(0, video.duration - .001)));
    if (!playing) video.pause();
    // A continuous clock, rather than currentTime assignment every RAF.
    const tolerance = playing && !video.paused ? .15 : 1 / 240;
    if (Math.abs(video.currentTime - target) > tolerance) {
      await new Promise<void>((resolve, reject) => {
        const finish = (error?: Error) => {
          clearTimeout(timeout); video.removeEventListener("seeked", seeked); video.removeEventListener("error", failed);
          error ? reject(error) : resolve();
        };
        const seeked = () => finish();
        const failed = () => finish(new Error("Video seek failed"));
        const timeout = setTimeout(() => finish(new Error("Video seek timed out")), 10000);
        video.addEventListener("seeked", seeked); video.addEventListener("error", failed);
        video.currentTime = target;
      });
    }
    video.playbackRate = Math.max(.0625, Math.min(16, rate));
    if (playing && video.paused && !entry.playPending) {
      entry.playPending = true;
      video.play().catch(error => console.warn("Video playback failed", error)).finally(() => { entry!.playPending = false; });
    }
    return video;
  }
}
export const previewVideoPool = new PreviewVideoPool();
