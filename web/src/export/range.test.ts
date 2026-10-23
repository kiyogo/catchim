import { describe, expect, it, vi } from "vitest";
import { resolveExportRange } from "./range";
import { mediaTime } from "@/wasm";
import { initRustEngine } from "@/native/rust-engine";
import {
	createRustExportRangeBindings,
	getRustExportRangeBindings,
} from "@/native/export-range";

describe("export range Rust boundary", () => {
	it("keeps fallback and actual WASM range validation equivalent", async () => {
		const range = {
			start: mediaTime({ ticks: 120_000 }),
			end: mediaTime({ ticks: 240_001 }),
		};
		const fallback = resolveExportRange({ duration: 360_000, range });
		await initRustEngine();
		expect(getRustExportRangeBindings()).not.toBeNull();
		expect(resolveExportRange({ duration: 360_000, range })).toEqual(fallback);
		expect(fallback).toEqual({
			start: 120_000,
			end: 240_001,
			duration: 120_001,
		});
		for (const [start, end] of [
			[-1, 30],
			[0, 0],
			[10, 5],
			[0, 360_001],
			[0, NaN],
			[0, 1.5],
		]) {
			expect(() =>
				resolveExportRange({
					duration: 360_000,
					range: {
						start: mediaTime({ ticks: Number.isInteger(start) ? start : 0 }),
						end: end as never,
					},
				}),
			).toThrow();
		}
	});
	it("frees output scratch on native rejection and rejects unsafe inputs", () => {
		const free = vi.fn();
		const plan = vi.fn(() => 0);
		const bindings = createRustExportRangeBindings(
			{ oc_malloc: () => 16, oc_free: free, ocw_export_resolve_range: plan },
			new WebAssembly.Memory({ initial: 1 }),
		);
		expect(bindings!.resolve(100, 20, 50)).toBeNull();
		expect(free).toHaveBeenCalledWith(16);
		expect(bindings!.resolve(Number.MAX_SAFE_INTEGER + 1, 0, 1)).toBeNull();
		expect(plan).toHaveBeenCalledTimes(1);
	});
});
