// Browser audio decoder/output bridge used by the existing C++ timeline mixer.
// Audio starts only after a user gesture, as required by browser autoplay policy.
(() => {
    let context = null;
    let masterGain = null;
    let nextStart = 0;
    const activeSources = new Map();

    function getContext() {
        if (!context) {
            const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
            if (!AudioContextCtor) throw new Error("Web Audio is unavailable.");
            context = new AudioContextCtor({ latencyHint: "interactive", sampleRate: 48000 });
            masterGain = context.createGain();
            masterGain.connect(context.destination);
            nextStart = context.currentTime;
        }
        return context;
    }

    async function unlock() {
        try {
            const audio = getContext();
            if (audio.state !== "running") await audio.resume();
        } catch (error) {
            console.warn("[Catchim] Browser audio could not start:", error);
        }
    }

    if (typeof window !== "undefined") {
        window.addEventListener("pointerdown", unlock, { capture: true, passive: true });
        window.addEventListener("keydown", unlock, { capture: true });
    }

    Module.catchimDecodeAudio = async function (path, sampleRate, channels, resultPtr, framesPtr, donePtr, okPtr) {
        let allocated = 0;
        try {
            const audio = getContext();
            const bytes = FS.readFile(path);
            const decoded = await audio.decodeAudioData(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
            const frames = Math.max(1, Math.ceil(decoded.duration * sampleRate));
            allocated = _malloc(frames * channels * Float32Array.BYTES_PER_ELEMENT);
            if (!allocated) throw new Error("Not enough browser memory for decoded audio.");

            const left = decoded.getChannelData(0);
            const right = decoded.numberOfChannels > 1 ? decoded.getChannelData(1) : left;
            const sourceRate = decoded.sampleRate;
            let cursor = 0;
            const copyChunk = () => {
                const end = Math.min(frames, cursor + 32768);
                for (; cursor < end; cursor++) {
                    const sourcePosition = cursor * sourceRate / sampleRate;
                    const a = Math.min(left.length - 1, Math.floor(sourcePosition));
                    const b = Math.min(left.length - 1, a + 1);
                    const t = sourcePosition - a;
                    const l = left[a] + (left[b] - left[a]) * t;
                    const r = right[a] + (right[b] - right[a]) * t;
                    const out = (allocated >> 2) + cursor * channels;
                    HEAPF32[out] = l + (r - l) * 0.5;
                    if (channels > 1) HEAPF32[out + 1] = r;
                }
                if (cursor < frames) setTimeout(copyChunk, 0);
                else finish(true);
            };
            const finish = (ok) => {
                HEAPU32[resultPtr >> 2] = allocated;
                HEAPU32[framesPtr >> 2] = frames;
                HEAPU32[okPtr >> 2] = ok ? 1 : 0;
                Atomics.store(HEAPU32, donePtr >> 2, 1);
                Atomics.notify(HEAP32, donePtr >> 2);
            };
            copyChunk();
        } catch (error) {
            if (allocated) _free(allocated);
            HEAPU32[resultPtr >> 2] = 0;
            HEAPU32[framesPtr >> 2] = 0;
            HEAPU32[okPtr >> 2] = 0;
            Atomics.store(HEAPU32, donePtr >> 2, 1);
            Atomics.notify(HEAP32, donePtr >> 2);
            console.warn("[Catchim] Audio decode failed:", path, error);
        }
    };

    Module.catchimScheduleAudio = function (samplesPtr, frames, channels, sampleRate, volume, queuePtr, generationPtr, generation, copiedPtr, reserved = false) {
        try {
            if (Atomics.load(HEAPU32, generationPtr >> 2) !== generation) return;
            const audio = getContext();
            if (!masterGain || channels < 1 || frames < 1) return;
            masterGain.gain.setTargetAtTime(Math.max(0, volume), audio.currentTime, 0.008);
            const buffer = audio.createBuffer(channels, frames, sampleRate);
            const offset = samplesPtr >> 2;
            for (let channel = 0; channel < channels; channel++) {
                const data = buffer.getChannelData(channel);
                for (let frame = 0; frame < frames; frame++) data[frame] = HEAPF32[offset + frame * channels + channel];
            }

            const source = audio.createBufferSource();
            source.buffer = buffer;
            source.connect(masterGain);
            const when = Math.max(audio.currentTime + 0.012, nextStart);
            nextStart = when + frames / sampleRate;
            if (!reserved) Atomics.add(HEAPU32, queuePtr >> 2, frames);
            let sources = activeSources.get(queuePtr);
            if (!sources) activeSources.set(queuePtr, sources = new Set());
            sources.add(source);
            source.onended = () => {
                sources.delete(source);
                if (Atomics.load(HEAPU32, generationPtr >> 2) === generation)
                    Atomics.sub(HEAPU32, queuePtr >> 2, frames);
            };
            source.start(when);
        } catch (error) {
            if (reserved && Atomics.load(HEAPU32, generationPtr >> 2) === generation)
                Atomics.sub(HEAPU32, queuePtr >> 2, frames);
            console.warn("[Catchim] Audio output failed:", error);
        } finally {
            if (copiedPtr) {
                Atomics.store(HEAPU32, copiedPtr >> 2, 1);
                Atomics.notify(HEAP32, copiedPtr >> 2);
            }
        }
    };

    Module.catchimResetAudio = function (queuePtr, generationPtr, generation) {
        if (Atomics.load(HEAPU32, generationPtr >> 2) !== generation) return;
        const sources = activeSources.get(queuePtr);
        if (sources) {
            for (const source of sources) {
                source.onended = null;
                try { source.stop(); } catch (_) {}
            }
            sources.clear();
        }
        if (context) nextStart = context.currentTime + 0.012;
        // The worker already cleared the queue when incrementing generation.
        // Preserve any reservations made after that reset was requested.
    };
})();
