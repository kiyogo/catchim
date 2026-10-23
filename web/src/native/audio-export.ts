type Exports = {
	oc_malloc(size: number): number;
	oc_free(pointer: number): void;
	ocw_audio_encode_pcm16?: (
		input: number,
		channels: number,
		frames: number,
		output: number,
	) => number;
};
let bindings: ReturnType<typeof createRustAudioExportBindings> = null;

export function createRustAudioExportBindings(
	exp: Exports,
	memory: WebAssembly.Memory,
) {
	const encode = exp.ocw_audio_encode_pcm16;
	if (!encode) return null;
	return {
		encodePcm16(samples: Float32Array, channels: number): Uint8Array | null {
			if (
				!Number.isInteger(channels) ||
				channels < 1 ||
				channels > 32 ||
				samples.length % channels !== 0 ||
				samples.length / channels > 65_536
			)
				return null;
			if (samples.length === 0) return new Uint8Array();
			let input = 0;
			let output = 0;
			try {
				input = exp.oc_malloc(samples.byteLength);
				output = exp.oc_malloc(samples.length * 2);
				if (!input || !output) return null;
				new Float32Array(memory.buffer, input, samples.length).set(samples);
				if (encode(input, channels, samples.length / channels, output) !== 1)
					return null;
				return new Uint8Array(
					memory.buffer,
					output,
					samples.length * 2,
				).slice();
			} finally {
				if (output) exp.oc_free(output);
				if (input) exp.oc_free(input);
			}
		},
	};
}
export function initializeAudioExportBindings(
	exp: Exports,
	memory: WebAssembly.Memory,
) {
	bindings = createRustAudioExportBindings(exp, memory);
}
export function getRustAudioExportBindings() {
	return bindings;
}
