import type { FrameRate } from "opencut-wasm";
import { TICKS_PER_SECOND } from "@/wasm";
import { getRustExportTimingBindings, type ExportFrameSample } from "@/native/rust-engine";

function validInputs(duration: number, fps: FrameRate): boolean {
	return Number.isSafeInteger(duration) && duration >= 0 &&
		[fps.numerator, fps.denominator].every((value) => Number.isInteger(value) && value > 0 && value <= 0xffff_ffff);
}

/** Number of samples needed to cover the complete timeline, including a partial final frame. */
export function getExportFrameCount(durationTicks: number, fps: FrameRate): number {
	if (!validInputs(durationTicks, fps)) return 0;
	const native = getRustExportTimingBindings();
	if (native) return native.frameCount(durationTicks, fps.numerator, fps.denominator) ?? 0;
	// Compatibility for older/unavailable WASM; integer arithmetic mirrors export.rs.
	const numerator = BigInt(durationTicks) * BigInt(fps.numerator);
	const denominator = BigInt(TICKS_PER_SECOND) * BigInt(fps.denominator);
	const count = Number((numerator + denominator - BigInt(1)) / denominator);
	return Number.isSafeInteger(count) ? count : 0;
}

export function getExportFrameSample(durationTicks: number, fps: FrameRate, index: number): ExportFrameSample | null {
	if (!validInputs(durationTicks, fps) || !Number.isSafeInteger(index) || index < 0) return null;
	const native = getRustExportTimingBindings();
	if (native) return native.frameSample(durationTicks, fps.numerator, fps.denominator, index);
	if (index >= getExportFrameCount(durationTicks, fps)) return null;
	const numerator = BigInt(fps.numerator);
	const denominator = BigInt(fps.denominator);
	const clock = BigInt(TICKS_PER_SECOND);
	const frameNumerator = BigInt(index) * denominator;
	const tickNumerator = frameNumerator * clock;
	const roundedTicks = Number((tickNumerator + numerator / BigInt(2)) / numerator);
	const remaining = BigInt(durationTicks) * numerator - tickNumerator;
	const durationNumerator = remaining < clock * denominator ? remaining : clock * denominator;
	return {
		timeTicks: Math.min(roundedTicks, durationTicks - 1),
		timestampSeconds: Number(frameNumerator) / Number(numerator),
		durationSeconds: Number(durationNumerator) / Number(clock * numerator),
	};
}
