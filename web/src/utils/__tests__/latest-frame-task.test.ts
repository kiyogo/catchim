import { afterEach, expect, test, vi } from "vitest";
import { LatestFrameTask } from "../latest-frame-task";
afterEach(() => vi.unstubAllGlobals());
test("coalesces fast pointer input and flushes the final position before committing", () => {
  const callbacks = new Map<number, FrameRequestCallback>(); let id = 0;
  vi.stubGlobal("requestAnimationFrame", (callback: FrameRequestCallback) => { callbacks.set(++id, callback); return id; });
  vi.stubGlobal("cancelAnimationFrame", (frame: number) => callbacks.delete(frame));
  const applied: number[] = [];
  const task = new LatestFrameTask<number>(value => applied.push(value));
  for (let position = 0; position < 100; position++) task.schedule(position);
  expect(callbacks.size).toBe(1); expect(applied).toEqual([]);
  task.flush(); expect(applied).toEqual([99]); expect(callbacks.size).toBe(0);
  task.schedule(123); task.cancel(); expect(callbacks.size).toBe(0); expect(applied).toEqual([99]);
});
