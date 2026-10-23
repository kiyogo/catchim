import { describe, expect, it, vi } from "vitest";
import { runAudioTask, yieldAudioWork } from "./audio-cancellation";

describe("audio cancellation boundaries", () => {
	it("does not begin an operation after cancellation", () => {
		const controller = new AbortController();
		controller.abort();
		const operation = vi.fn(async () => 1);
		expect(() => runAudioTask(operation, controller.signal)).toThrow(expect.objectContaining({ name: "AbortError" }));
		expect(operation).not.toHaveBeenCalled();
	});

	it("removes its listener after success", async () => {
		const signal = new AbortController().signal;
		const add = vi.spyOn(signal, "addEventListener");
		const remove = vi.spyOn(signal, "removeEventListener");
		expect(await runAudioTask(async () => 42, signal)).toBe(42);
		expect(remove).toHaveBeenCalledWith("abort", add.mock.calls[0][1]);
	});

	it("rejects promptly, cleans its listener and handles a late decoder rejection", async () => {
		const controller = new AbortController();
		const remove = vi.spyOn(controller.signal, "removeEventListener");
		let rejectLate!: (error: Error) => void;
		const stop = vi.fn();
		const result = runAudioTask(() => new Promise<number>((_resolve, reject) => { rejectLate = reject; }), controller.signal, stop);
		await Promise.resolve();
		const rejected = expect(result).rejects.toMatchObject({ name: "AbortError" });
		controller.abort();
		await rejected;
		expect(stop).toHaveBeenCalledTimes(1);
		expect(remove).toHaveBeenCalled();
		rejectLate(new Error("decoder finished after cancellation"));
		await Promise.resolve();
	});

	it("yields to the event loop so a queued cancel can interrupt work", async () => {
		const controller = new AbortController();
		setTimeout(() => controller.abort(), 0);
		await expect(yieldAudioWork(controller.signal)).rejects.toMatchObject({ name: "AbortError" });
	});
});
