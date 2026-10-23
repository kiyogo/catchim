export interface ResolvedExportRange {
	start: number;
	end: number;
	duration: number;
}
type Exports = {
	oc_malloc(size: number): number;
	oc_free(pointer: number): void;
	ocw_export_resolve_range?: (
		duration: number,
		start: number,
		end: number,
		out: number,
	) => number;
};
let bindings: ReturnType<typeof createRustExportRangeBindings> = null;

export function createRustExportRangeBindings(
	exp: Exports,
	memory: WebAssembly.Memory,
) {
	const plan = exp.ocw_export_resolve_range;
	if (!plan) return null;
	return {
		resolve(
			duration: number,
			start: number,
			end: number,
		): ResolvedExportRange | null {
			if (
				![duration, start, end].every((v) => Number.isSafeInteger(v) && v >= 0)
			)
				return null;
			let pointer = 0;
			try {
				pointer = exp.oc_malloc(24);
				if (!pointer || plan(duration, start, end, pointer) !== 1) return null;
				const values = Array.from(new Float64Array(memory.buffer, pointer, 3));
				if (
					values[0] !== start ||
					values[1] !== end ||
					values[2] !== end - start ||
					end > duration ||
					end <= start
				)
					return null;
				return { start: values[0], end: values[1], duration: values[2] };
			} finally {
				if (pointer) exp.oc_free(pointer);
			}
		},
	};
}
export function initializeExportRangeBindings(
	exp: Exports,
	memory: WebAssembly.Memory,
) {
	bindings = createRustExportRangeBindings(exp, memory);
}
export function getRustExportRangeBindings() {
	return bindings;
}
