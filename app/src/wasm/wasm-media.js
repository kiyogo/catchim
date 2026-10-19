// Browser-native video decoding bridge for the Qt/C++ preview workers.
// Calls are proxied to the main runtime because HTMLMediaElement belongs to
// the browser main thread; decoded RGBA bytes are copied into shared WASM RAM.
Module.catchimDecodeVideoFrame = async function (
    path, timestampSeconds, maxWidth, maxHeight,
    pixelPointer, widthPointer, heightPointer, sourceWidthPointer, sourceHeightPointer,
    streamKey = path, playing = false, cachePointer = 0, streamId = 0
) {
    const videos = Module.__catchimVideos || (Module.__catchimVideos = new Map());
    // Filmstrip workers and live preview must never seek the same element.
    // A thumbnail seek otherwise pauses/moves the playing preview decoder.
    let record = videos.get(streamKey);
    if (!record) {
        const bytes = FS.readFile(path);
        const extension = path.slice(path.lastIndexOf(".") + 1).toLowerCase();
        const mimeTypes = {
            mp4: "video/mp4", m4v: "video/mp4", mov: "video/quicktime",
            webm: "video/webm", ogv: "video/ogg"
        };
        const url = URL.createObjectURL(new Blob([bytes], { type: mimeTypes[extension] || "video/mp4" }));
        const video = document.createElement("video");
        video.muted = true;
        video.playsInline = true;
        video.preload = "auto";
        video.style.cssText = "position:fixed;left:-10000px;top:0;width:2px;height:2px;";
        document.body.appendChild(video);
        const canvas = document.createElement("canvas");
        const context = canvas.getContext("2d", { alpha: false, willReadFrequently: true });
        record = { video, url, ready: null, canvas, context };
        record.ready = new Promise((resolve, reject) => {
            video.addEventListener("loadedmetadata", resolve, { once: true });
            video.addEventListener("error", () => reject(new Error("Browser could not decode this video.")), { once: true });
        });
        videos.set(streamKey, record);
        video.src = url;
        video.load();

        // Bound decoder memory for projects containing many source clips.
        if (videos.size > 12) {
            const oldestPath = videos.keys().next().value;
            if (oldestPath !== streamKey) {
                const oldest = videos.get(oldestPath);
                const live = Module.__catchimLiveFrame;
                if (live && live.record === oldest) {
                    clearTimeout(live.callback);
                    live.record = null;
                    Atomics.store(HEAPU32, (live.pointer >> 2) + 7, 0);
                }
                oldest.video.removeAttribute("src");
                oldest.video.load();
                oldest.video.remove();
                URL.revokeObjectURL(oldest.url);
                videos.delete(oldestPath);
            }
        }
    }

    const video = record.video;
    const live = Module.__catchimLiveFrame;
    if (!playing && live && live.record === record) {
        if (live.callback !== undefined) clearTimeout(live.callback);
        live.record = null;
        Atomics.store(HEAPU32, (live.pointer >> 2) + 7, 0);
    }
    await record.ready;

    if (video.readyState < HTMLMediaElement.HAVE_CURRENT_DATA) {
        await new Promise((resolve, reject) => {
            const timeout = setTimeout(() => reject(new Error("Timed out loading the first video frame.")), 15000);
            const finish = (callback) => (event) => {
                clearTimeout(timeout);
                video.removeEventListener("loadeddata", onLoaded);
                video.removeEventListener("error", onError);
                callback(event);
            };
            const onLoaded = finish(resolve);
            const onError = finish(() => reject(new Error("Video frame could not be decoded.")));
            video.addEventListener("loadeddata", onLoaded, { once: true });
            video.addEventListener("error", onError, { once: true });
            video.load();
        });
    }

    if (!video.videoWidth || !video.videoHeight) throw new Error("Video has no decoded dimensions.");
    const time = Math.max(0, Math.min(timestampSeconds, Math.max(0, video.duration - 0.001)));
    const previousRequest = record.lastRequestedTime;
    const timelineDelta = previousRequest === undefined ? Infinity : time - previousRequest;
    if (playing) {
        // Let the browser's native decoder advance continuously during play.
        // Seeking every preview frame forces a demux/decode stall on each tick.
        const discontinuity = previousRequest === undefined || timelineDelta < -0.1 || timelineDelta > 0.75;
        if (Math.abs(video.currentTime - time) > (discontinuity ? 0.001 : 0.35)) {
            video.pause();
            await new Promise((resolve, reject) => {
                const timeout = setTimeout(() => reject(new Error("Timed out seeking the video frame.")), 15000);
                const finish = (callback) => (event) => {
                    clearTimeout(timeout);
                    video.removeEventListener("seeked", onSeeked);
                    video.removeEventListener("error", onError);
                    callback(event);
                };
                const onSeeked = finish(resolve);
                const onError = finish(() => reject(new Error("Video seek failed.")));
                video.addEventListener("seeked", onSeeked, { once: true });
                video.addEventListener("error", onError, { once: true });
                video.currentTime = time;
            });
        }
        if (video.paused) await video.play();
    } else {
        if (!video.paused) video.pause();
        if (Math.abs(video.currentTime - time) > 0.001) {
        await new Promise((resolve, reject) => {
            const timeout = setTimeout(() => reject(new Error("Timed out seeking the video frame.")), 15000);
            const finish = (callback) => (event) => {
                clearTimeout(timeout);
                video.removeEventListener("seeked", onSeeked);
                video.removeEventListener("error", onError);
                callback(event);
            };
            const onSeeked = finish(resolve);
            const onError = finish(() => reject(new Error("Video seek failed.")));
            video.addEventListener("seeked", onSeeked, { once: true });
            video.addEventListener("error", onError, { once: true });
            video.currentTime = time;
        });
        }
    }
    record.lastRequestedTime = time;

    const scale = Math.min(1, maxWidth / video.videoWidth, maxHeight / video.videoHeight);
    const width = Math.max(1, Math.round(video.videoWidth * scale));
    const height = Math.max(1, Math.round(video.videoHeight * scale));
    const canvas = record.canvas;
    const context = record.context;
    if (!context) throw new Error("Could not create a browser video frame canvas.");
    if (canvas.width !== width || canvas.height !== height) {
        canvas.width = width;
        canvas.height = height;
    }
    context.drawImage(video, 0, 0, width, height);
    const rgba = context.getImageData(0, 0, width, height).data;

    HEAPU8.set(rgba, pixelPointer);
    HEAP32[widthPointer >> 2] = width;
    HEAP32[heightPointer >> 2] = height;
    HEAP32[sourceWidthPointer >> 2] = video.videoWidth;
    HEAP32[sourceHeightPointer >> 2] = video.videoHeight;

    // One bounded, module-owned mailbox lives until the WASM instance exits.
    // The browser publishes frames directly; the preview worker can read them
    // without a main-thread round trip on every frame. Thumbnail/paused reads
    // never subscribe. Sequence numbers prevent partially copied RGBA reads.
    if (playing && cachePointer && width <= 480 && height <= 270) {
        let mailbox = Module.__catchimLiveFrame;
        if (!mailbox) {
            const pointer = _malloc(32 + 480 * 270 * 4);
            if (!pointer) return;
            mailbox = Module.__catchimLiveFrame = { pointer, record: null, callback: undefined };
            HEAPU8.fill(0, pointer, pointer + 32);
        }
        HEAPU32[cachePointer >> 2] = mailbox.pointer;
        if (mailbox.record && mailbox.callback !== undefined)
            clearTimeout(mailbox.callback);
        if (mailbox.record && mailbox.record !== record) mailbox.record.video.pause();
        mailbox.record = record;
        mailbox.streamId = streamId;
        const publish = (data) => {
            const head = mailbox.pointer >> 2;
            const sequence = Atomics.load(HEAPU32, head);
            if ((sequence & 1) || Atomics.compareExchange(HEAPU32, head, sequence, sequence + 1) !== sequence) return;
            HEAPU32[head + 1] = streamId;
            HEAPU32[head + 2] = width;
            HEAPU32[head + 3] = height;
            HEAPU32[head + 4] = video.videoWidth;
            HEAPU32[head + 5] = video.videoHeight;
            HEAPU32[head + 6] = Math.round(video.currentTime * 1000);
            HEAPU32[head + 7] = 1;
            HEAPU8.set(data, mailbox.pointer + 32);
            Atomics.store(HEAPU32, head, sequence + 2);
        };
        publish(rgba);
        const tick = () => {
            if (mailbox.record !== record || mailbox.streamId !== streamId || video.paused) return;
            const started = performance.now();
            context.drawImage(video, 0, 0, width, height);
            publish(context.getImageData(0, 0, width, height).data);
            mailbox.callback = setTimeout(tick, Math.max(0, 1000 / 30 - (performance.now() - started)));
        };
        // Offscreen HTMLVideoElement frame callbacks may be throttled by the
        // compositor. A bounded timer keeps canvas preview sampling independent
        // of whether that hidden media element itself is being painted.
        mailbox.callback = setTimeout(tick, 1000 / 30);
    }
};
