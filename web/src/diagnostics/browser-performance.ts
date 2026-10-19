let previewSubmissions = 0;
export const recordPreviewSubmission = () => { previewSubmissions++; };

// Opt-in diagnostics: actual RAF cadence, render submissions and browser video
// quality are separate counters. A 30fps video is never labelled as 60fps.
export function installBrowserPerformanceMonitor() {
  if (!new URLSearchParams(location.search).has("perf")) return;
  const panel = document.createElement("aside");
  panel.setAttribute("aria-label", "Đo hiệu năng");
  panel.style.cssText = "position:fixed;bottom:16px;right:16px;z-index:2147483647;background:#111e;color:#eee;border:1px solid #555;border-radius:8px;padding:10px;font:12px/1.5 monospace;min-width:260px";
  const button = document.createElement("button");
  button.textContent = "Đo 10 giây"; button.style.cssText = "border:1px solid #888;border-radius:4px;padding:3px 12px;cursor:pointer";
  const output = document.createElement("pre"); output.style.margin = "6px 0 0";
  panel.append(button, output); document.body.append(panel);
  let started = 0, previous = 0, frames: number[] = [], inputs: number[] = [], pendingInput = 0, submissions = 0, longTasks = 0;
  let startVideoFrames = 0, startDroppedFrames = 0;
  let playheadMoves = 0, lastPlayheadTransform = "";
  const quality = () => Array.from(document.querySelectorAll<HTMLVideoElement>("video[data-catchim-preview-video]")).reduce((total, video) => {
    const value = video.getVideoPlaybackQuality();
    return { total: total.total + value.totalVideoFrames, dropped: total.dropped + value.droppedVideoFrames };
  }, { total: 0, dropped: 0 });
  const percentile = (values: number[], fraction: number) => [...values].sort((a, b) => a - b)[Math.min(values.length - 1, Math.floor(values.length * fraction))] || 0;
  const onInput = () => { if (started && !pendingInput) pendingInput = performance.now(); };
  for (const event of ["pointermove", "wheel", "pointerdown"]) document.addEventListener(event, onInput, { passive: true, capture: true });
  let observer: PerformanceObserver | null = null;
  try { observer = new PerformanceObserver(list => { if (started) longTasks += list.getEntries().length; }); observer.observe({ type: "longtask", buffered: false }); } catch {}
  button.onclick = () => {
    frames = []; inputs = []; pendingInput = 0; longTasks = 0; started = performance.now(); previous = started; submissions = previewSubmissions;
    playheadMoves = 0; lastPlayheadTransform = "";
    const value = quality(); startVideoFrames = value.total; startDroppedFrames = value.dropped;
    button.disabled = true; panel.removeAttribute("data-result");
  };
  function tick(now: number) {
    if (started) {
      frames.push(now - previous); previous = now;
      const transform = document.querySelector<HTMLElement>("[data-timeline-playhead]")?.style.transform ?? "";
      if (transform && transform !== lastPlayheadTransform) { playheadMoves++; lastPlayheadTransform = transform; }
      // RAF's timestamp describes the frame boundary and can precede an input
      // delivered during that frame. Measure callback latency on one clock.
      if (pendingInput) { inputs.push(performance.now() - pendingInput); pendingInput = 0; }
      const seconds = (now - started) / 1000;
      if (seconds >= 10) {
        const value = quality();
        const result = {
          seconds: +seconds.toFixed(3), uiRafHz: +(frames.length / seconds).toFixed(1),
          rafP95Ms: +percentile(frames, .95).toFixed(2), inputToRafP95Ms: +percentile(inputs, .95).toFixed(2),
          inputSamples: inputs.length, previewSubmissionsHz: +((previewSubmissions - submissions) / seconds).toFixed(1),
          playheadMovesHz: +(playheadMoves / seconds).toFixed(1),
          videoDecodedHz: +((value.total - startVideoFrames) / seconds).toFixed(1),
          videoDropped: value.dropped - startDroppedFrames, longTasks,
          backend: document.querySelector("#opencut-webgpu-viewport")?.getAttribute("data-engine"),
        };
        panel.dataset.result = JSON.stringify(result); output.textContent = JSON.stringify(result, null, 2);
        console.info("CATCHIM_PERFORMANCE", JSON.stringify(result)); started = 0; button.disabled = false;
      } else output.textContent = `Đang đo: ${seconds.toFixed(1)} / 10 giây`;
    }
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
}
