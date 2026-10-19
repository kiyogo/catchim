import React from "react";
import { createRoot } from "react-dom/client";
import { ThemeProvider } from "next-themes";
import { I18nProvider } from "@/i18n";
import { TooltipProvider } from "@/components/ui/tooltip";
import { Toaster } from "@/components/ui/sonner";
import Editor from "@/app/editor/[project_id]/page";
import Projects from "@/app/projects/page";
import { usePathname } from "./navigation";
import "@/app/globals.css";
import { initializeBrowserEditorCore } from "@/native/browser-editor-core";
import { installBrowserPerformanceMonitor } from "@/diagnostics/browser-performance";

function App() {
  const path = usePathname();
  return <ThemeProvider attribute="class" defaultTheme="system" disableTransitionOnChange>
    <TooltipProvider><I18nProvider><Toaster />
      {path.startsWith("/editor/") ? <Editor key={path} /> : <Projects />}
    </I18nProvider></TooltipProvider>
  </ThemeProvider>;
}
(globalThis as any).__CATCHIM_BROWSER__ = true;
initializeBrowserEditorCore().then(() => {
  createRoot(document.getElementById("root")!).render(<App />);
  installBrowserPerformanceMonitor();
}).catch(error => {
  document.getElementById("root")!.textContent = `Không thể tải lõi Catchim: ${error.message}`;
  console.error(error);
});
