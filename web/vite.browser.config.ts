import { defineConfig } from "vite";
import path from "node:path";

export default defineConfig({
  root: path.resolve(__dirname, "browser"),
  publicDir: path.resolve(__dirname, "public"),
  resolve: { alias: {
    "@": path.resolve(__dirname, "src"),
    "opencut-wasm": path.resolve(__dirname, "src/native/opencut-wasm-compat.ts"),
    "next/navigation": path.resolve(__dirname, "browser/navigation.ts"),
    "next/link": path.resolve(__dirname, "browser/link.tsx"),
    "next/image": path.resolve(__dirname, "browser/image.tsx"),
  } },
  define: {
    "process.env": JSON.stringify({ NODE_ENV: "production", NEXT_PUBLIC_DEPLOYMENT_ID: "catchim-browser" }),
  },
  esbuild: { jsx: "automatic" },
  build: {
    outDir: path.resolve(__dirname, "dist-browser"), emptyOutDir: true,
    sourcemap: false, target: "es2022", chunkSizeWarningLimit: 2000,
  },
});
