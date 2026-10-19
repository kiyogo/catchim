"use client";
import { useEffect, useState } from "react";
import { webGpuEngine } from "@/native/webgpu-engine";
import { Popover, PopoverContent, PopoverTrigger } from "@/components/ui/popover";
import { Button } from "@/components/ui/button";
export function EngineStatusBadge() {
  const [active, setActive] = useState(webGpuEngine.isAvailable);
  useEffect(() => {
    const timer = setInterval(() => setActive(webGpuEngine.isAvailable), 1000);
    return () => clearInterval(timer);
  }, []);
  return <Popover><PopoverTrigger asChild>
    <Button variant="outline" size="sm" className="h-8 px-2 text-xs" title="Bộ dựng hình">
      {active ? "WebGPU" : "Canvas 2D"}
    </Button>
  </PopoverTrigger><PopoverContent align="end" className="w-64 text-xs space-y-2">
    <p>Preview: {active ? "WebGPU" : "Canvas 2D"}</p><p>Timeline: Canvas 2D</p>
    <p>FPS phụ thuộc video nguồn, màn hình và tải xử lý. Bật ?perf để đo trên máy này.</p>
  </PopoverContent></Popover>;
}
