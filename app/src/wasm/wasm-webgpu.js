// Acquire a browser WebGPU device before the Qt/C++ application starts.
// Emscripten exposes it to C++ through emscripten_webgpu_get_device().
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    const dependency = "catchim-webgpu-device";
    addRunDependency(dependency);

    const gpu = typeof navigator !== "undefined" ? navigator.gpu : undefined;
    if (!gpu) {
        Module.catchimWebGpuAvailable = false;
        console.info("[Catchim] WebGPU unavailable; browser renderer must use its fallback.");
        removeRunDependency(dependency);
        return;
    }

    gpu.requestAdapter({ powerPreference: "high-performance" })
        .then((adapter) => adapter ? adapter.requestDevice() : null)
        .then((device) => {
            Module.preinitializedWebGPUDevice = device || undefined;
            Module.catchimWebGpuAvailable = !!device;
            if (device) {
                console.info("[Catchim] WebGPU device acquired for the C++ runtime.");
                device.lost.then((info) => {
                    Module.catchimWebGpuAvailable = false;
                    console.warn("[Catchim] WebGPU device lost:", info.message);
                });
            } else {
                console.info("[Catchim] No WebGPU adapter; continuing without GPU acceleration.");
            }
        })
        .catch((error) => {
            Module.catchimWebGpuAvailable = false;
            console.warn("[Catchim] WebGPU initialization failed; continuing without GPU acceleration:", error);
        })
        .finally(() => removeRunDependency(dependency));
});
