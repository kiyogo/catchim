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

test('full audio decode preserves left and right channels', async () => {
    class StereoAudioContext extends FakeAudioContext {
        async decodeAudioData() {
            return { duration: 2 / 48000, sampleRate: 48000, numberOfChannels: 2,
                getChannelData: channel => channel === 0 ? Float32Array.of(.25, .5) : Float32Array.of(-.25, -.5) };
        }
    }
    const r = runtime('wasm-audio.js', { window: { AudioContext: StereoAudioContext, addEventListener() {} } });
    await r.Module.catchimDecodeAudio('/clip.mp4', 48000, 2, 4, 8, 12, 16);
    assert.deepEqual(Array.from(r.HEAPF32.slice(128, 132)), [.25, -.25, .5, -.5]);
    assert.equal(r.HEAPU32[4], 1);
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
    let fileReads = 0, blobUrls = 0;
    const revoked = [];
    class Video {
        constructor() {
            this.listeners = new Map(); this.paused = true; this.readyState = 4;
            this.videoWidth = 2; this.videoHeight = 2; this.duration = 60; this.time = 0;
            this.style = {}; this.dataset = {};
        }
        addEventListener(name, callback) { this.listeners.set(name, callback); }
        removeEventListener(name) { this.listeners.delete(name); }
        load() { queueMicrotask(() => this.listeners.get('loadedmetadata')?.()); }
        get currentTime() { return this.time; }
        set currentTime(value) { this.time = value; queueMicrotask(() => this.listeners.get('seeked')?.()); }
        async play() { this.paused = false; }
        pause() { this.paused = true; }
        removeAttribute() {}
        remove() {}
        requestVideoFrameCallback(callback) { this.frameCallback = callback; return 1; }
        cancelVideoFrameCallback() { this.frameCallback = null; }
    }
    const timers = [];
    const r = runtime('wasm-media.js', {
        setTimeout(callback) { timers.push(callback); return timers.length; }, clearTimeout() {},
        performance: { now: () => 0 }, Blob,
        FS: { readFile: () => { fileReads++; return new Uint8Array([1, 2, 3]); } },
        URL: { createObjectURL: () => `blob:test-${++blobUrls}`, revokeObjectURL(url) { revoked.push(url); } },
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
    assert.equal(fileReads, 1);
    assert.equal(blobUrls, 1);
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
    for (let i = 0; i < 11; ++i) await decode(0, `shared-${i}`, false);
    assert.equal(r.Module.__catchimVideos.size, 12);
    assert.deepEqual(revoked, []);
    assert.equal(fileReads, 1);
    for (let i = 0; i < 12; ++i)
        await r.Module.catchimDecodeVideoFrame(`/other-${i}.mp4`, 0, 2, 2, 1024, 4, 8, 12, 16, `other-${i}`, false);
    assert.equal(revoked.filter(url => url === 'blob:test-1').length, 1);
});

test('GPU playback keeps the browser video surface out of the WASM pixel buffer', async () => {
    class Video {
        constructor() { this.listeners = new Map(); this.paused = true; this.readyState = 4; this.videoWidth = 1920; this.videoHeight = 1080; this.duration = 60; this.time = 0; this.style = {}; this.dataset = {}; }
        addEventListener(name, callback) { this.listeners.set(name, callback); }
        removeEventListener(name) { this.listeners.delete(name); }
        load() { queueMicrotask(() => this.listeners.get('loadedmetadata')?.()); }
        get currentTime() { return this.time; }
        set currentTime(value) { this.time = value; queueMicrotask(() => this.listeners.get('seeked')?.()); }
        async play() { this.paused = false; }
        pause() { this.paused = true; }
    }
    let readbacks = 0;
    const r = runtime('wasm-media.js', {
        Module: { catchimGpuPreviewAvailable: true, catchimExportGpuEnabled: true }, Blob,
        URL: { createObjectURL: () => 'blob:test', revokeObjectURL() {} },
        HTMLMediaElement: { HAVE_CURRENT_DATA: 2 },
        document: { body: { appendChild() {} }, createElement(type) {
            return type === 'video' ? new Video() : { getContext: () => ({ drawImage() { readbacks++; }, getImageData() { readbacks++; return { data: new Uint8Array(16) }; } }) };
        } }
    });
    await r.Module.catchimDecodeVideoFrame('/clip.mp4', 0, 480, 270, 512, 4, 8, 12, 16, 'clip@/clip.mp4', true, 0, 17, true, .25);
    assert.equal(r.HEAP32[1], 1);
    assert.equal(r.HEAP32[2], 1);
    assert.equal(r.HEAP32[3], 1920);
    assert.equal(r.HEAP32[4], 1080);
    assert.equal(readbacks, 0);
    assert.equal(r.Module.__catchimVideos.get('clip@/clip.mp4').video.paused, false);
    assert.equal(r.Module.__catchimVideos.get('clip@/clip.mp4').video.playbackRate, .25);
    r.Module.catchimGpuPreviewAvailable = false;
    r.HEAPU8.fill(173,512,544);
    await assert.rejects(r.Module.catchimDecodeVideoFrame('/clip.mp4', .2, 480, 270, 512, 4, 8, 12, 16, 'clip@/clip.mp4', false, 0, 17, true), /GPU video surface/);
    assert.equal(readbacks,0);
    assert.ok(r.HEAPU8.subarray(512,544).every(value=>value===173), 'GPU descriptor may not overflow into the WASM heap');
    r.Module.catchimGpuPreviewAvailable = true;
    await r.Module.catchimDecodeVideoFrame('/clip.mp4', .5, 480, 270, 512, 4, 8, 12, 16, 'clip@/clip.mp4', false, 0, 17, false);
    assert.equal(r.HEAP32[1], 480);
    assert.equal(r.HEAP32[2], 270);
    assert.equal(readbacks, 2);
});

test('Qt browser export encodes frames through WebCodecs and triggers a download', async () => {
    const calls = [];
    const memory = new SharedArrayBuffer(8192);
    const heap = new Uint8Array(memory);
    class FakeSource {
        constructor(canvas, config) { calls.push(['video-config', config]); }
        async add(timestamp, duration) { calls.push(['frame', timestamp, duration]); }
        close() {}
    }
    class FakeAudioSource {
        constructor(config) { calls.push(['audio-config', config]); }
        async add(buffer) { calls.push(['audio', buffer.length]); }
        close() {}
    }
    class FakeTarget { constructor() { this.buffer = null; } }
    class FakeOutput {
        constructor(options) { this.options = options; this.target = options.target; calls.push(['output', options.format.name]); }
        addVideoTrack() {}
        addAudioTrack() {}
        async start() { this.state = 'started'; }
        async finalize() { this.target.buffer = Uint8Array.of(1, 2, 3).buffer; }
        async cancel() { this.state = 'canceled'; }
    }
    class FakeImageData {
        constructor(data, width, height) { this.data = data; this.width = width; this.height = height; }
    }
    class FakeAudioBuffer {
        constructor({ numberOfChannels, length }) {
            this.length = length; this.channels = Array.from({ length: numberOfChannels }, () => new Float32Array(length));
        }
        getChannelData(index) { return this.channels[index]; }
    }
    const link = { click() { calls.push(['download', this.download]); }, remove() {} };
    const scope = {
        Module: { HEAPU8: heap },
        isSecureContext: true,
        window: { isSecureContext: true, VideoEncoder: class {}, AudioEncoder: class {}, CatchimMedia: {
            BufferTarget: FakeTarget, Output: FakeOutput, CanvasSource: FakeSource, AudioBufferSource: FakeAudioSource,
            Mp4OutputFormat: class { constructor() { this.name = 'mp4'; } }, WebMOutputFormat: class { constructor() { this.name = 'webm'; } }
        } },
        document: {
            createElement(name) {
                if (name === 'canvas') return { width: 0, height: 0, getContext: () => ({ putImageData(image) { calls.push(['pixels', image.width, image.height]); } }) };
                return link;
            },
            body: { appendChild() {} }
        },
        ImageData: FakeImageData, AudioBuffer: FakeAudioBuffer, Blob: class { constructor(parts, options) { this.parts = parts; this.type = options.type; } },
        URL: { createObjectURL: () => 'blob:test', revokeObjectURL() {} }, setTimeout: () => 0,
        Atomics, console
    };
    vm.runInNewContext(readFileSync(join(__dirname, '../src/wasm/wasm-export.js'), 'utf8'), scope);
    const result = new Int32Array(memory);
    await scope.Module.catchimExportBegin(2, 2, 29.97, 1, 'mp4', 'project.mp4', true, 64, 0, true, 0, 8_000_000);
    assert.equal(result[16], 1);
    assert.equal(calls.find(([type]) => type === 'video-config')[1].hardwareAcceleration, 'prefer-hardware');
    assert.equal(calls.find(([type]) => type === 'video-config')[1].bitrate, 8_000_000);
    heap.set(new Uint8Array(16).fill(127), 256);
    await scope.Module.catchimExportFrame(256, 16, 0, 68);
    assert.equal(result[17], 1);
    await scope.Module.catchimExportAudio(512, 4, 72);
    assert.equal(result[18], 1);
    await scope.Module.catchimExportFinish(76);
    assert.equal(result[19], 1);
    assert.equal(calls.some(([type]) => type === 'download'), false);
    assert.equal(scope.Module.catchimExportDownload(), true);
    assert.ok(calls.some(([type, name]) => type === 'download' && name === 'project.mp4'));
});

test('Qt browser export reports actionable secure-context and codec failures', async () => {
    const memory = new SharedArrayBuffer(4096);
    const heap = new Uint8Array(memory);
    const r = runtime('wasm-export.js', {
        Module: { HEAPU8: heap },
        stringToUTF8(text, pointer, maxBytes) {
            const bytes = new TextEncoder().encode(text).subarray(0, maxBytes - 1);
            heap.set(bytes, pointer); heap[pointer + bytes.length] = 0;
        },
        window: { isSecureContext: true, VideoEncoder: class {}, CatchimMedia: {
            canEncodeVideo: async () => false
        } },
        Atomics, console
    });
    const status = new Int32Array(memory);
    await r.Module.catchimExportBegin(3840, 2160, 30, 5602, 'mp4', 'project.mp4', false, 64, 0, false, 128);
    assert.equal(status[16], -1);
    const error = new TextDecoder().decode(heap.subarray(128, 512)).split('\0')[0];
    assert.match(error, /H.264\/MP4.*3840×2160/);
});

test('Qt WebGPU retries default and low-power adapters when high-performance fails', async () => {
    const requests = [];
    const device = { lost: new Promise(() => {}) };
    const adapter = { info: { vendor: 'gpu' }, async requestDevice() { return device; } };
    let finishInitialization;
    const initialized = new Promise(resolve => { finishInitialization = resolve; });
    const r = runtime('wasm-webgpu.js', {
        isSecureContext: true,
        navigator: { gpu: { async requestAdapter(options) {
            requests.push(options);
            if (requests.length === 1) throw new Error('high-performance adapter unavailable');
            return adapter;
        } } },
        addRunDependency() {}, removeRunDependency() { finishInitialization(); }
    });
    r.Module.catchimInitGpuPreview = async () => { r.Module.catchimGpuPreviewAvailable = true; return true; };
    r.Module.preRun[0]();
    await initialized;
    assert.equal(requests.length, 2);
    assert.equal(requests[0].powerPreference, 'high-performance');
    assert.equal(Object.keys(requests[1]).length, 0);
    assert.equal(r.Module.catchimGpuPreviewAvailable, true);
    assert.equal(r.Module.catchimWebGpuStatusCode, 0);
});

test('Qt browser export uses the GPU canvas when WebGPU export is available', async () => {
    const calls = [];
    const memory = new SharedArrayBuffer(8192);
    const heap = new Uint8Array(memory);
    const canvas = { width: 1920, height: 1080 };
    class FakeSource {
        constructor(source, config) { calls.push(['source', source, config]); }
        async add(timestamp, duration) { calls.push(['frame', timestamp, duration]); }
        close() {}
    }
    class FakeAudioSource { constructor() {} async add() {} close() {} }
    class FakeTarget { constructor() { this.buffer = null; } }
    class FakeOutput { constructor() { this.state = 'started'; this.target = null; } addVideoTrack() {} addAudioTrack() {} async start() {} async finalize() { this.target.buffer = new ArrayBuffer(8); } async cancel() {} }
    FakeOutput.prototype.target = null;
    const r = runtime('wasm-export.js', {
        isSecureContext: true,
        Module: { HEAPU8: heap, catchimGpuExportBegin: (w, h) => w === 1920 && h === 1080,
            catchimGpuExportCanvas: () => canvas, async catchimGpuExportRender(pointer, count, red, green, blue) { calls.push(['gpu-render', pointer, count, red, green, blue]); } },
        window: { isSecureContext: true, VideoEncoder: class {}, AudioEncoder: class {}, CatchimMedia: { BufferTarget: FakeTarget, Output: FakeOutput,
            CanvasSource: FakeSource, AudioBufferSource: FakeAudioSource, Mp4OutputFormat: class {}, WebMOutputFormat: class {} } },
        navigator: { mediaCapabilities: { async encodingInfo() { return { powerEfficient: false }; } } },
        URL: { createObjectURL: () => 'blob:test', revokeObjectURL() {} }, Blob, Atomics, console
    });
    const result = new Int32Array(memory);
    await r.Module.catchimExportBegin(1920, 1080, 30, 1, 'mp4', 'project.mp4', false, 64, 80, true);
    assert.equal(result[16], 1);
    assert.equal(result[20], 1);
    assert.equal(r.Module.catchimExportGpuEnabled, true);
    assert.equal(r.Module.catchimExportEncoderPowerEfficient, false);
    await r.Module.catchimExportGpuFrame(256, 1, 0, 0, 0, 0, 68);
    assert.equal(result[17], 1);
    assert.ok(calls.some(([type, source]) => type === 'source' && source === canvas));
    assert.ok(calls.some(([type]) => type === 'gpu-render'));
    assert.ok(calls.some(([type]) => type === 'frame'));
});

test('Qt GPU preview rejects a software adapter and safely falls back', async () => {
    const r = runtime('wasm-preview.js', { document: { createElement() { throw new Error('should not create canvases for software adapter'); } } });
    const available = await r.Module.catchimInitGpuPreview({}, { isFallbackAdapter: true, info: { description: 'SwiftShader' } });
    assert.equal(available, false);
    assert.equal(r.Module.catchimGpuPreviewAvailable, false);
});

test('Qt GPU preview uploads RGBA layers and submits a WebGPU composition', async () => {
    const calls=[];
    const canvases=[];
    const pass={setPipeline(){},setBindGroup(){},draw(){calls.push('draw');},end(){}};
    const device={
        createSampler(){return {};}, createShaderModule(){return {};},
        createRenderPipeline(){return {getBindGroupLayout:()=>({})};},
        createTexture(){return {createView:()=>({}),destroy(){}};}, createBuffer(){return {destroy(){}};},
        createBindGroup(){return {};}, queue:{writeTexture(){calls.push('upload');},writeBuffer(){},submit(){calls.push('submit');}},
        createCommandEncoder(){return {beginRenderPass(){return pass;},finish(){return {};}};},
        lost:new Promise(()=>{})
    };
    class Canvas {
        constructor(){this.style={};this.width=0;this.height=0;}
        getContext(type){
            if(type==='webgpu')return {configure(){},getCurrentTexture(){return {createView:()=>({})}}};
            return {setTransform(x,b,c,y){calls.push(['ui-scale',x,y]);},clearRect(){},setLineDash(){},strokeRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},arc(){},fill(){},fillRect(){},strokeRect(){}};
        }
    }
    const r=runtime('wasm-preview.js',{
        navigator:{gpu:{getPreferredCanvasFormat:()=> 'bgra8unorm'}},
        GPUTextureUsage:{TEXTURE_BINDING:1,COPY_DST:2},GPUBufferUsage:{UNIFORM:1,COPY_DST:2},
        document:{createElement:()=>{const canvas=new Canvas();canvases.push(canvas);return canvas;},getElementById:()=>({appendChild(){}})}
    });
    r.Module.HEAPU8=r.HEAPU8; r.Module.HEAPF32=r.HEAPF32;
    assert.equal(await r.Module.catchimInitGpuPreview(device,{isFallbackAdapter:false,info:{vendor:'nvidia'}}),true);
    assert.equal(r.Module.catchimGpuPreviewBegin(10,20,200,100,1,0,0,0,false,false,false),true);
    r.HEAPU8.fill(255,128,128+16);
    assert.equal(r.Module.catchimGpuPreviewLayer(0,128,2,2,0,0,1,1,0,1,'normal'),true);
    r.Module.catchimGpuPreviewEnd(0,0,false);
    assert.ok(calls.includes('upload'));
    assert.ok(calls.includes('draw'));
    assert.ok(calls.includes('submit'));
    assert.equal(canvases[0].width,200);
    r.Module.catchimGpuPreviewBegin(10,20,220,100,1,0,0,0,false,false,false);
    assert.equal(canvases[0].width,200); // keep current GPU surface during the drag
    r.Module.catchimGpuPreviewEnd(0,0,false);
    assert.equal(calls.at(-1)[0],'ui-scale');
    assert.equal(calls.at(-1)[1],200/220); // selection remains inside the old backing store
    await new Promise(resolve=>setTimeout(resolve,100));
    assert.equal(canvases[0].width,200); // timer cannot erase the displayed frame
    assert.equal(r.Module.catchimGpuPreviewNeedsRedraw,true);
    r.Module.catchimGpuPreviewBegin(10,20,220,100,1,0,0,0,false,false,false);
    assert.equal(canvases[0].width,220); // resize synchronously with the next draw
});

test('Qt GPU preview caches paused video textures across transforms with an external-texture fallback', async () => {
    const calls=[];
    const pass={setPipeline(){},setBindGroup(){},draw(){calls.push('draw');},end(){}};
    const device={
        createSampler(){return {};}, createShaderModule(){return {};},
        createRenderPipeline(){return {getBindGroupLayout:()=>({})};},
        createTexture(){return {createView:()=>({}),destroy(){}};}, createBuffer(){return {destroy(){}};},
        createBindGroup({entries}){calls.push(entries[0].resource.external ? 'video-bind' : 'rgba-bind');return {};},
        importExternalTexture({source}){assert.equal(source.tag,'VIDEO');calls.push('import-video');return {external:true};},
        queue:{writeTexture(){calls.push('upload');},writeBuffer(){},submit(){calls.push('submit');}},
        createCommandEncoder(){return {beginRenderPass(){return pass;},finish(){return {};}};},
        lost:new Promise(()=>{})
    };
    class Canvas {
        constructor(){this.style={};this.width=0;this.height=0;}
        getContext(type){
            if(type==='webgpu')return {configure(){},getCurrentTexture(){return {createView:()=>({})}}};
            return {setTransform(){},clearRect(){},setLineDash(){},strokeRect(){},beginPath(){},moveTo(){},lineTo(){},stroke(){},arc(){},fill(){},fillRect(){}};
        }
    }
    const r=runtime('wasm-preview.js',{
        navigator:{gpu:{getPreferredCanvasFormat:()=> 'bgra8unorm'}}, HTMLMediaElement:{HAVE_CURRENT_DATA:2},
        GPUTextureUsage:{TEXTURE_BINDING:1,COPY_DST:2,RENDER_ATTACHMENT:4},GPUBufferUsage:{UNIFORM:1,COPY_DST:2},
        document:{createElement:()=>new Canvas(),getElementById:()=>({appendChild(){}})}
    });
    r.Module.HEAPU8=r.HEAPU8; r.Module.HEAPF32=r.HEAPF32;
    r.Module.__catchimVideos=new Map([['clip@/clip.mp4',{video:{tag:'VIDEO',readyState:4}}]]);
    assert.equal(await r.Module.catchimInitGpuPreview(device,{isFallbackAdapter:false,info:{vendor:'amd'}}),true);
    r.Module.catchimGpuPreviewBegin(0,0,1280,720,1,0,0,0,false,false,false);
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    r.Module.catchimGpuPreviewEnd(0,0,false);
    assert.equal(r.Module.catchimGpuPreviewHasFrame,true);
    assert.ok(calls.includes('import-video'));
    assert.ok(calls.includes('video-bind'));
    assert.ok(calls.includes('draw'));
    assert.ok(calls.includes('submit'));
    let copies=0;
    device.queue.copyExternalImageToTexture=()=>{copies++;};
    const video=r.Module.__catchimVideos.get('clip@/clip.mp4').video;
    video.currentTime=0;video.videoWidth=1920;video.videoHeight=1080;
    for(let move=0;move<50;move++) {
        r.Module.catchimGpuPreviewBegin(0,0,1280,720,1,0,0,0,false,false,false);
        assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,move,0,1+move*.01,1,0,1,'clip@/clip.mp4'),true);
        r.Module.catchimGpuPreviewEnd(0,0,false);
    }
    assert.equal(copies,1,'pointer moves reuse the GPU video texture');
    video.currentTime=.1;
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(copies,2,'a new video frame updates the GPU texture');
    video.readyState=1;
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(copies,2,'a seek preserves the complete GPU frame');
    video.readyState=4;video.currentTime=.2;video.seeking=true;
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(copies,2,'readyState can still expose the old frame during a seek');
    video.seeking=false;
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(copies,3,'the completed seek must replace the old GPU image at the same timestamp');
    video.currentTime=.3;
    let failedCopies=0;
    device.queue.copyExternalImageToTexture=()=>{failedCopies++;throw new Error('driver copy unsupported');};
    const previousImports=calls.filter(call=>call==='import-video').length;
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(calls.filter(call=>call==='import-video').length,previousImports+1,'driver copy failure falls back to another GPU path');
    assert.equal(r.Module.catchimGpuPreviewVideoLayer(0,1920,1080,0,0,1,1,0,1,'clip@/clip.mp4'),true);
    assert.equal(failedCopies,1,'do not repeatedly stall the driver on an unsupported copy');
});
