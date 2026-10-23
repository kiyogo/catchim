"use client";
import { useEffect, useState } from "react";
import { webGpuEngine } from "@/native/webgpu-engine";
import { getCompositorBackend } from "opencut-wasm";
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover";
import { Button } from "@/components/ui/button";
export function EngineStatusBadge() {
  const [backend, setBackend] = useState<"webgpu" | "canvas2d" | "unavailable">("unavailable");
  useEffect(() => {
    const timer = setInterval(() => setBackend(getCompositorBackend()), 1000);
    setBackend(getCompositorBackend());
    return () => clearInterval(timer);
  }, []);
  const active = backend === "webgpu" && webGpuEngine.isHardwareAdapter;
  return <Popover><PopoverTrigger asChild>
    <Button variant="outline" size="sm" className="h-8 px-2 text-xs" title="Bộ dựng hình">
      {active ? "WebGPU · GPU" : backend === "webgpu" ? "WebGPU · GPU chưa xác minh" : "Canvas 2D"}
    </Button>
  </PopoverTrigger><PopoverContent align="end" className="w-64 text-xs space-y-2">
    <p>Preview: {backend === "webgpu" ? "WebGPU" : "Canvas 2D"}</p>
    <p>GPU: {webGpuEngine.isAvailable ? webGpuEngine.adapterName : "Không có adapter WebGPU"}</p>
    <p>Xuất: {active ? "WebGPU; encoder ưu tiên phần cứng" : "Cần GPU WebGPU phần cứng"}</p>
    <p>Timeline: Canvas 2D</p>
    <p>FPS phụ thuộc video nguồn, màn hình và tải xử lý. Bật ?perf để đo trên máy này.</p>
  </PopoverContent></Popover>;
}
