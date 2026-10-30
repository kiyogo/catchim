type GraphExports = {
	oc_malloc?: (size: number) => number;
	oc_free?: (pointer: number) => void;
	ocw_effect_graph_order?: (inputs: number, count: number, output: number, order: number) => number;
};
let binding: { exp: Required<GraphExports>; memory: WebAssembly.Memory } | null = null;
export function initializeEffectGraphBindings(exp: GraphExports, memory: WebAssembly.Memory) {
	binding = typeof exp.ocw_effect_graph_order === "function" && typeof exp.oc_malloc === "function" && typeof exp.oc_free === "function" ? { exp: exp as Required<GraphExports>, memory } : null;
}
export function effectGraphOrder(inputs: number[], output: number): number[] {
	if (!binding) throw new Error("Effect graph validation requires the updated Rust WASM core");
	if (!inputs.length || inputs.length > 8 || !Number.isInteger(output) || inputs.some((v) => !Number.isInteger(v))) throw new Error("Invalid effect graph dimensions");
	const { exp, memory } = binding;
	let inputPtr = 0, outputPtr = 0;
	try {
		inputPtr = exp.oc_malloc(inputs.length * 4); outputPtr = exp.oc_malloc(inputs.length * 4);
		if (!inputPtr || !outputPtr) throw new Error("Insufficient memory to validate effect graph");
		new Int32Array(memory.buffer, inputPtr, inputs.length).set(inputs);
		const status = exp.ocw_effect_graph_order(inputPtr, inputs.length, output, outputPtr);
		const errors: Record<number, string> = { [-1]: "Effect graph exceeds node budget", [-2]: "Invalid effect graph reference", [-3]: "Effect graph contains a cycle", [-4]: "Effect graph contains unreachable nodes" };
		if (status !== inputs.length) throw new Error(errors[status] ?? "Effect graph validation failed");
		return Array.from(new Int32Array(memory.buffer, outputPtr, inputs.length));
	} finally { if (outputPtr) exp.oc_free(outputPtr); if (inputPtr) exp.oc_free(inputPtr); }
}
