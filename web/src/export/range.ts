import { getRustExportRangeBindings } from "@/native/export-range";
import { mediaTime, type MediaTime } from "@/wasm";

export interface ExportRange {
	start: MediaTime;
	end: MediaTime;
}
export function resolveExportRange({
	duration,
	range,
}: {
	duration: number;
	range?: ExportRange;
}): ExportRange & { duration: MediaTime } {
	const start = range?.start ?? 0;
	const end = range?.end ?? duration;
	if (
		![duration, start, end].every(
			(value) => Number.isSafeInteger(value) && value >= 0,
		)
	)
		throw new Error("Export range must use valid nonnegative timeline ticks");
	const native = getRustExportRangeBindings();
	const resolved = native
		? native.resolve(duration, start, end)
		: end > start && end <= duration
			? { start, end, duration: end - start }
			: null;
	if (!resolved)
		throw new Error(
			"Export range must end after its start and stay inside the timeline",
		);
	return {
		start: mediaTime({ ticks: resolved.start }),
		end: mediaTime({ ticks: resolved.end }),
		duration: mediaTime({ ticks: resolved.duration }),
	};
}
