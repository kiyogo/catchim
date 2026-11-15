(() => {
  let worker = null;
  let nextRequestId = 1;
  const pending = new Map();

  function finish(request, status, payload) {
    if (!pending.has(request.requestId)) return;
    pending.delete(request.requestId);

    let pointer = 0;
    let length = 0;
    try {
      const bytes = new TextEncoder().encode(payload || "");
      pointer = _malloc(bytes.length + 1);
      if (!pointer) throw new Error("Không đủ bộ nhớ để nhận kết quả chép lời.");
      Module.HEAPU8.set(bytes, pointer);
      Module.HEAPU8[pointer + bytes.length] = 0;
      length = bytes.length;
    } catch (error) {
      status = 2;
      const bytes = new TextEncoder().encode(JSON.stringify({
        error: error instanceof Error ? error.message : "Không thể nhận kết quả chép lời.",
      }));
      pointer = _malloc(bytes.length + 1);
      if (pointer) {
        Module.HEAPU8.set(bytes, pointer);
        Module.HEAPU8[pointer + bytes.length] = 0;
        length = bytes.length;
      }
    }

    if (pointer) Atomics.store(Module.HEAPU32, request.resultPointerAddress >> 2, pointer);
    Atomics.store(Module.HEAPU32, request.resultLengthAddress >> 2, length);
    Atomics.store(Module.HEAPU32, request.statusAddress >> 2, status);
    Atomics.store(Module.HEAPU32, request.doneAddress >> 2, 1);
    Atomics.notify(Module.HEAP32, request.doneAddress >> 2);
  }

  function reportProgress(request, phase, percent) {
    const value = ((phase & 0xff) << 24) | (Math.max(0, Math.min(100, percent)) & 0xffffff);
    Atomics.store(Module.HEAPU32, request.progressAddress >> 2, value);
    Atomics.notify(Module.HEAP32, request.progressAddress >> 2);
  }

  function createWorker() {
    if (worker) return worker;
    const workerUrl = new URL("catchim-transcription-worker.js", document.baseURI);
    worker = new Worker(workerUrl, { type: "module", name: "catchim-transcription" });
    worker.onmessage = ({ data }) => {
      const request = pending.get(data?.requestId);
      if (!request) return;
      switch (data.type) {
        case "init-progress":
          reportProgress(request, 1, data.progress ?? 0);
          break;
        case "init-complete":
          reportProgress(request, 1, 100);
          break;
        case "transcribe-progress":
          reportProgress(request, 2, data.progress ?? 0);
          break;
        case "transcribe-complete":
          finish(request, 1, data.payload ?? "{}");
          break;
        case "transcribe-error":
          finish(request, 2, JSON.stringify({ error: data.error ?? "Không thể chép lời âm thanh." }));
          break;
      }
    };
    worker.onerror = (event) => {
      const message = event.message || "Không thể khởi chạy worker chép lời.";
      for (const request of pending.values()) {
        finish(request, 2, JSON.stringify({ error: message }));
      }
      worker?.terminate();
      worker = null;
    };
    worker.onmessageerror = () => {
      for (const request of pending.values()) {
        finish(request, 2, JSON.stringify({ error: "Worker không đọc được dữ liệu âm thanh." }));
      }
    };
    return worker;
  }

  Module.catchimTranscribeChunk = function (
    samplesPointer,
    sampleCount,
    language,
    modelId,
    doneAddress,
    resultPointerAddress,
    resultLengthAddress,
    statusAddress,
    progressAddress,
  ) {
    const requestId = nextRequestId++;
    const request = {
      requestId,
      doneAddress,
      resultPointerAddress,
      resultLengthAddress,
      statusAddress,
      progressAddress,
    };
    pending.set(requestId, request);
    try {
      const samples = Module.HEAPF32.slice(
        samplesPointer >> 2,
        (samplesPointer >> 2) + sampleCount,
      );
      createWorker().postMessage({
        type: "transcribe",
        requestId,
        audio: samples,
        language: UTF8ToString(language),
        modelId: UTF8ToString(modelId),
      }, [samples.buffer]);
    } catch (error) {
      finish(request, 2, JSON.stringify({
        error: error instanceof Error ? error.message : "Không thể gửi âm thanh đến mô hình.",
      }));
    }
  };

  Module.catchimCancelTranscription = function () {
    worker?.postMessage({ type: "cancel" });
  };
})();
