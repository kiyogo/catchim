import { access } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const webRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const standaloneRoot = path.join(webRoot, ".next", "standalone");
try {
  await Promise.all([
    access(path.join(standaloneRoot, "server.js")),
    access(path.join(standaloneRoot, ".next", "static", "chunks")),
    access(path.join(standaloneRoot, "public", "wasm", "opencut_engine.wasm")),
  ]);
} catch {
  throw new Error("Standalone files are incomplete. Run npm run build, or npm run prepare:standalone after an existing Next.js build.");
}
process.env.PORT ||= "3300";
await import(pathToFileURL(path.join(standaloneRoot, "server.js")).href);
