// WebCodecs export bridge for the Qt/WASM editor. Mediabunny is bundled into
// catchim-export.js by build-browser.ps1 and provides MP4/WebM muxing.
(() => {
    let output, target, videoSource, audioSource, canvas, context, formatName, gpuCompositorEnabled = false;
    const report = (address, ok) => {
        const view = new Int32Array(Module.HEAPU8.buffer, address, 1);
        Atomics.store(view, 0, ok ? 1 : -1);
        Atomics.notify(view, 0);
    };
    Module.catchimExportBegin = async (width, height, fps, totalFrames, format, filename, includeAudio, address, gpuStatusAddress = 0, allowGpu = true, errorAddress = 0, targetVideoBitrate = 0) => {
        try {
            const api = window.CatchimMedia;
            if (!api) throw new Error('Thiếu thư viện xuất Mediabunny (catchim-export.js chưa tải được).');
            if (!window.isSecureContext) throw new Error('WebCodecs cần ngữ cảnh bảo mật HTTPS hoặc localhost. Hãy mở liên kết HTTPS do máy chủ Catchim cung cấp.');
            if (!('VideoEncoder' in window)) throw new Error('Trình duyệt này không hỗ trợ WebCodecs VideoEncoder. Hãy thử Chrome hoặc Edge mới.');
            if (!('AudioEncoder' in window) && includeAudio) throw new Error('Trình duyệt này không hỗ trợ WebCodecs AudioEncoder. Hãy tắt âm thanh hoặc dùng Chrome/Edge mới.');
            const codec = format === 'webm' ? 'vp9' : 'avc';
            // Mediabunny validates bitrate as an integer before it reaches
            // WebCodecs; fractional project frame rates (e.g. 29.97) otherwise
            // make valid dimensions fail during encoder startup.
            const requestedBitrate = Number(targetVideoBitrate);
            const automaticBitrate = Math.round(width * height * fps * .12);
            const bitrate = Number.isFinite(requestedBitrate) && requestedBitrate > 0
                ? Math.min(100_000_000, Math.max(1_000_000, Math.round(requestedBitrate)))
                : Math.min(100_000_000, Math.max(1_000_000, automaticBitrate));
            // Do not force a hardware encoder. Some browsers report the codec
            // as available, then reject the first frame when hardware is
            // explicitly preferred (common with headless/SwiftShader setups).
            // Probe the exact WebCodecs configuration and keep the first
            // supported acceleration mode, starting with the browser default.
            let hardwareAcceleration = 'no-preference';
            if (typeof api.canEncodeVideo === 'function') {
                hardwareAcceleration = null;
                for (const acceleration of ['no-preference', 'prefer-software', 'prefer-hardware']) {
                    try {
                        if (await api.canEncodeVideo(codec, {
                            width, height, bitrate, frameRate: fps, hardwareAcceleration: acceleration
                        })) {
                            hardwareAcceleration = acceleration;
                            break;
                        }
                    } catch (_) {
                        // Try the next supported browser encoder mode.
                    }
                }
                if (!hardwareAcceleration) {
                    throw new Error(`${codec === 'avc' ? 'H.264/MP4' : 'VP9/WebM'} không được hỗ trợ ở ${width}×${height} ${fps} fps trên trình duyệt/thiết bị này. Hãy chọn độ phân giải thấp hơn hoặc định dạng WebM.`);
                }
            }
            if (includeAudio && typeof api.canEncodeAudio === 'function' && !await api.canEncodeAudio(format === 'webm' ? 'opus' : 'aac', { numberOfChannels: 2, sampleRate: 48000, bitrate: 192_000 })) {
                throw new Error(`Trình duyệt không hỗ trợ mã hóa âm thanh ${format === 'webm' ? 'Opus' : 'AAC'} cho định dạng này. Hãy tắt âm thanh hoặc chọn WebM.`);
            }
            formatName = format;
            gpuCompositorEnabled = !!(allowGpu && Module.catchimGpuExportBegin && Module.catchimGpuExportBegin(width, height));
            if (gpuCompositorEnabled) {
                canvas = Module.catchimGpuExportCanvas();
                context = null;
            } else {
                canvas = document.createElement('canvas'); canvas.width = width; canvas.height = height;
                context = canvas.getContext('2d', { alpha: false, willReadFrequently: false });
            }
            target = new api.BufferTarget();
            output = new api.Output({ format: format === 'webm' ? new api.WebMOutputFormat() : new api.Mp4OutputFormat(), target });
            videoSource = new api.CanvasSource(canvas, {
                codec: format === 'webm' ? 'vp9' : 'avc', bitrate,
                frameRate: fps, hardwareAcceleration, latencyMode: 'quality'
            });
            output.addVideoTrack(videoSource);
            if (includeAudio) {
                audioSource = new api.AudioBufferSource({ codec: format === 'webm' ? 'opus' : 'aac', bitrate: 192_000 });
                output.addAudioTrack(audioSource);
            }
            await output.start();
            Module.catchimExportGpuEnabled = gpuCompositorEnabled;
            Module.catchimExportEncoderPowerEfficient = null;
            if (typeof navigator !== 'undefined' && navigator.mediaCapabilities?.encodingInfo) {
                try {
                    const info = await navigator.mediaCapabilities.encodingInfo({ type: 'webrtc', video: {
                        contentType: format === 'webm' ? 'video/VP9' : 'video/H264', width, height,
                        bitrate, framerate: fps
                    } });
                    Module.catchimExportEncoderPowerEfficient = !!info.powerEfficient;
                } catch (_) {}
            }
            if (gpuStatusAddress) Atomics.store(new Int32Array(Module.HEAPU8.buffer), gpuStatusAddress >> 2, gpuCompositorEnabled ? 1 : 0);
            console.info(`[Catchim] Export render backend: ${gpuCompositorEnabled ? 'WebGPU' : 'CPU fallback'}; browser encoder hardware efficient: ${Module.catchimExportEncoderPowerEfficient === null ? 'unknown' : Module.catchimExportEncoderPowerEfficient}`);
            Module.catchimExportMeta = { totalFrames, width, height, fps, format: formatName, filename };
            report(address, true);
        } catch (error) {
            Module.catchimExportError = error?.message || String(error);
            console.error('[Catchim] WebCodecs export initialization failed:', error);
            if (errorAddress && typeof stringToUTF8 === 'function') stringToUTF8(Module.catchimExportError, errorAddress, 1024);
            report(address, false);
        }
    };
    Module.catchimExportFrame = async (pointer, length, frameIndex, address, errorAddress = 0) => {
        try {
            const rgba = new Uint8ClampedArray(Module.HEAPU8.buffer, pointer, length).slice();
            context.putImageData(new ImageData(rgba, canvas.width, canvas.height), 0, 0);
            await videoSource.add(frameIndex / Module.catchimExportMeta.fps, 1 / Module.catchimExportMeta.fps);
            report(address, true);
        } catch (error) {
            Module.catchimExportError = error?.message || String(error);
            if (errorAddress && typeof stringToUTF8 === 'function') stringToUTF8(Module.catchimExportError, errorAddress, 1024);
            report(address, false);
        }
    };
    Module.catchimExportGpuFrame = async (descriptorPointer, layerCount, frameIndex, red, green, blue, address, errorAddress = 0) => {
        try {
            if (!gpuCompositorEnabled || !Module.catchimGpuExportRender) throw new Error('WebGPU export compositor is unavailable.');
            await Module.catchimGpuExportRender(descriptorPointer, layerCount, red, green, blue);
            await videoSource.add(frameIndex / Module.catchimExportMeta.fps, 1 / Module.catchimExportMeta.fps);
            report(address, true);
        } catch (error) {
            Module.catchimExportError = error?.message || String(error);
            console.error("[Catchim] WebGPU export frame failed:", error);
            if (errorAddress && typeof stringToUTF8 === 'function') stringToUTF8(Module.catchimExportError, errorAddress, 1024);
            report(address, false);
        }
    };
    Module.catchimExportAudio = async (pointer, samples, address, errorAddress = 0) => {
        try {
            if (!audioSource) throw new Error('Audio track chưa được khởi tạo.');
            const frames = samples / 2;
            const buffer = new AudioBuffer({ numberOfChannels: 2, length: frames, sampleRate: 48000 });
            const source = new Float32Array(Module.HEAPU8.buffer, pointer, samples);
            const left = buffer.getChannelData(0), right = buffer.getChannelData(1);
            for (let i = 0; i < frames; i++) { left[i] = source[i * 2]; right[i] = source[i * 2 + 1]; }
            await audioSource.add(buffer);
            report(address, true);
        } catch (error) {
            Module.catchimExportError = error?.message || String(error);
            if (errorAddress && typeof stringToUTF8 === 'function') stringToUTF8(Module.catchimExportError, errorAddress, 1024);
            report(address, false);
        }
    };
    Module.catchimExportFinish = async (address, errorAddress = 0) => {
        try {
            await output.finalize();
            if (!target.buffer) throw new Error('Không tạo được dữ liệu video.');
            const meta = Module.catchimExportMeta;
            const mime = formatName === 'webm' ? 'video/webm' : 'video/mp4';
            if (Module.catchimExportUrl) URL.revokeObjectURL(Module.catchimExportUrl);
            const url = URL.createObjectURL(new Blob([target.buffer], { type: mime }));
            Module.catchimExportUrl = url;
            Module.catchimExportFilename = meta.filename.split(/[\\/]/).pop() || `catchim.${formatName}`;
            videoSource.close(); if (audioSource) audioSource.close();
            report(address, true);
        } catch (error) {
            Module.catchimExportError = error?.message || String(error);
            if (errorAddress && typeof stringToUTF8 === 'function') stringToUTF8(Module.catchimExportError, errorAddress, 1024);
            report(address, false);
        }
    };
    // Browsers require a user gesture for a reliable download. Qt invokes this
    // from the completed export button, after the asynchronous encode finishes.
    Module.catchimExportDownload = () => {
        if (!Module.catchimExportUrl) return false;
        try {
            const link = document.createElement('a');
            link.href = Module.catchimExportUrl; link.download = Module.catchimExportFilename;
            document.body.appendChild(link); link.click(); link.remove();
            const url = Module.catchimExportUrl;
            Module.catchimExportUrl = null;
            setTimeout(() => URL.revokeObjectURL(url), 60_000);
            return true;
        } catch (error) { Module.catchimExportError = String(error); return false; }
    };
    Module.catchimExportCancel = async () => {
        try { if (output && output.state === 'started') await output.cancel(); } catch (_) {}
        if (videoSource) videoSource.close(); if (audioSource) audioSource.close();
        Module.catchimGpuExportEnabled = false;
    };
})();
