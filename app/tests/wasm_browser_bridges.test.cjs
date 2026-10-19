const { test } = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const vm = require('node:vm');

function runtime(script, extra = {}) {
    const memory = new SharedArrayBuffer(4096);
    const scope = {
        Module: {}, HEAP32: new Int32Array(memory), HEAPU32: new Uint32Array(memory),
        HEAPU8: new Uint8Array(memory), HEAPF32: new Float32Array(memory),
        Atomics, setTimeout, clearTimeout, console, _malloc: () => 512, _free() {},
        FS: { readFile: () => new Uint8Array([1, 2, 3]) }, ...extra
    };
    vm.runInNewContext(readFileSync(join(__dirname, '../src/wasm', script), 'utf8'), scope);
    return scope;
}

class FakeAudioContext {
    constructor() { this.currentTime = 0; this.state = 'running'; }
    createGain() { return { connect() {}, gain: { setTargetAtTime() {} } }; }
    createBuffer(channels, frames) { return { getChannelData: () => new Float32Array(frames) }; }
    createBufferSource() { return { connect() {}, start() {}, stop() {} }; }
    async decodeAudioData() {
        return { duration: 4 / 48000, sampleRate: 48000, numberOfChannels: 2,
            getChannelData: () => Float32Array.of(.1, .2, .3, .4) };
    }
}

test('audio decode completes and notifies the worker using signed atomic memory', async () => {
    const r = runtime('wasm-audio.js', {
        window: { AudioContext: FakeAudioContext, addEventListener() {} }
    });
    await r.Module.catchimDecodeAudio('/clip.mp4', 48000, 2, 4, 8, 12, 16);
    assert.equal(r.HEAPU32[3], 1);
    assert.equal(r.HEAPU32[4], 1);
    assert.equal(r.HEAPU32[2], 4);
    assert.equal(r.HEAPU32[1], 512);
});

test('scheduled audio acknowledges its copy without throwing in Atomics.notify', () => {
    const r = runtime('wasm-audio.js', {
        window: { AudioContext: FakeAudioContext, addEventListener() {} }
    });
    r.Module.catchimScheduleAudio(512, 4, 2, 48000, 1, 4, 8, 0, 12);
    assert.equal(r.HEAPU32[3], 1);
    assert.equal(r.HEAPU32[1], 4);
});

test('reserved packets are counted once and survive their queued reset', () => {
    const r = runtime('wasm-audio.js', {
        window: { AudioContext: FakeAudioContext, addEventListener() {} }
    });
    r.HEAPU32[1] = 4;
    r.HEAPU32[2] = 1;
    r.Module.catchimResetAudio(4, 8, 1);
    assert.equal(r.HEAPU32[1], 4);
    r.Module.catchimScheduleAudio(512, 4, 2, 48000, 1, 4, 8, 1, 0, true);
    assert.equal(r.HEAPU32[1], 4);
    // An obsolete callback cannot reset or queue audio for the new generation.
    r.Module.catchimResetAudio(4, 8, 0);
    r.Module.catchimScheduleAudio(512, 4, 2, 48000, 1, 4, 8, 0, 0, true);
    assert.equal(r.HEAPU32[1], 4);
    assert.equal(r.HEAPU32[2], 1);
});

test('filmstrip seeks use a different video element from live preview', async () => {
    class Video {
        constructor() {
            this.listeners = new Map(); this.paused = true; this.readyState = 4;
            this.videoWidth = 2; this.videoHeight = 2; this.duration = 60; this.time = 0;
            this.style = {};
        }
        addEventListener(name, callback) { this.listeners.set(name, callback); }
        removeEventListener(name) { this.listeners.delete(name); }
        load() { queueMicrotask(() => this.listeners.get('loadedmetadata')?.()); }
        get currentTime() { return this.time; }
        set currentTime(value) { this.time = value; queueMicrotask(() => this.listeners.get('seeked')?.()); }
        async play() { this.paused = false; }
        pause() { this.paused = true; }
        requestVideoFrameCallback(callback) { this.frameCallback = callback; return 1; }
        cancelVideoFrameCallback() { this.frameCallback = null; }
    }
    const timers = [];
    const r = runtime('wasm-media.js', {
        setTimeout(callback) { timers.push(callback); return timers.length; }, clearTimeout() {},
        performance: { now: () => 0 }, Blob,
        URL: { createObjectURL: () => 'blob:test', revokeObjectURL() {} },
        HTMLMediaElement: { HAVE_CURRENT_DATA: 2 },
        document: { body: { appendChild() {} }, createElement(type) {
            return type === 'video' ? new Video() : { getContext: () => ({
                drawImage() {}, getImageData: () => ({ data: new Uint8Array(16) })
            }) };
        } }
    });
    const decode = (time, stream, playing = stream === 'preview') => r.Module.catchimDecodeVideoFrame('/clip.mp4', time,
        2, 2, 512, 4, 8, 12, 16, stream, playing);
    await decode(0, 'preview');
    await decode(1 / 30, 'preview');
    assert.equal(r.Module.__catchimVideos.get('preview').video.paused, false);
    await decode(18, 'filmstrip');
    assert.equal(r.Module.__catchimVideos.size, 2);
    assert.equal(r.Module.__catchimVideos.get('preview').video.paused, false);
    assert.equal(r.Module.__catchimVideos.get('preview').video.currentTime, 0);
    assert.equal(r.Module.__catchimVideos.get('filmstrip').video.currentTime, 18);
    // A delayed frame request is still playback, rather than a scrub/seek.
    await decode(.2, 'preview');
    assert.equal(r.Module.__catchimVideos.get('preview').video.paused, false);
    assert.equal(r.Module.__catchimVideos.get('preview').video.currentTime, 0);
    await decode(.2, 'preview', false);
    assert.equal(r.Module.__catchimVideos.get('preview').video.paused, true);
    assert.equal(r.Module.__catchimVideos.get('preview').video.currentTime, .2);
    await r.Module.catchimDecodeVideoFrame('/clip.mp4', .2, 2, 2, 1024, 4, 8, 12, 16, 'preview', true, 20, 42);
    const pointer = r.HEAPU32[5], head = pointer >> 2;
    assert.equal(pointer, 512);
    assert.equal(r.HEAPU32[head + 1], 42);
    assert.equal(r.HEAPU32[head + 2], 2);
    assert.equal(r.HEAPU32[head + 7], 1);
    const video = r.Module.__catchimVideos.get('preview').video;
    video.time = .4;
    timers.at(-1)();
    assert.equal(r.HEAPU32[head + 6], 400);
    const sequence = r.HEAPU32[head];
    // The browser skips publication while the worker owns the frame copy.
    Atomics.store(r.HEAPU32, head, sequence + 1);
    video.time = .5;
    timers.at(-1)();
    assert.equal(r.HEAPU32[head + 6], 400);
    Atomics.store(r.HEAPU32, head, sequence);
    await decode(.5, 'preview', false);
    assert.equal(r.HEAPU32[head + 7], 0);
});
