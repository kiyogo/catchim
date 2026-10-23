import { defineConfig } from "vitest/config";
import path from "path";

export default defineConfig({
  resolve: {
    alias: {
      "bun:test": path.resolve(__dirname, "./src/test-shims/bun-test.ts"),
      "@": path.resolve(__dirname, "./src"),
      "opencut-wasm": path.resolve(__dirname, "./src/native/opencut-wasm-compat.ts"),
    },
  },
  test: {
    environment: "node",
    exclude: ["**/node_modules/**", "**/.next/**", "**/*.bench.ts"],
  },
});
