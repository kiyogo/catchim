export const AUDIO_WORK_CHUNK_SAMPLES = 65_536;

export function throwIfAudioAborted(signal?: AbortSignal): void {
	if (signal?.aborted) {
		throw new DOMException("Audio preparation was cancelled", "AbortError");
	}
}

export function isAudioAbortError(error: unknown): boolean {
	return error instanceof Error && error.name === "AbortError";
}

/** Ignore late results from browser operations such as decodeAudioData, which
 * cannot themselves be aborted. Attach handlers to their eventual rejection. */
export function runAudioTask<T>(
	operation: () => Promise<T>,
	signal?: AbortSignal,
	onAbort?: () => void,
): Promise<T> {
	throwIfAudioAborted(signal);
	if (!signal) return operation();
	return new Promise<T>((resolve, reject) => {
		const aborted = () => {
			try {
				onAbort?.();
			} catch {
				// A decoder/source can already have stopped while abort was queued.
			} finally {
				cleanup();
				reject(new DOMException("Audio preparation was cancelled", "AbortError"));
			}
		};
		const cleanup = () => signal.removeEventListener("abort", aborted);
		signal.addEventListener("abort", aborted, { once: true });
		Promise.resolve().then(() => {
			throwIfAudioAborted(signal);
			return operation();
		}).then(
			(value) => { cleanup(); resolve(value); },
			(error: unknown) => { cleanup(); reject(error); },
		);
	});
}

/** A macrotask lets UI events and the export cancellation poll run between chunks. */
export function yieldAudioWork(signal?: AbortSignal): Promise<void> {
	return runAudioTask(() => new Promise<void>((resolve) => setTimeout(resolve, 0)), signal);
}
