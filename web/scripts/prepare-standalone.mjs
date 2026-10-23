import { access, cp, mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const webRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const standaloneRoot = path.join(webRoot, ".next", "standalone");
await access(path.join(standaloneRoot, "server.js"));

for (const [source, target] of [
  [path.join(webRoot, ".next", "static"), path.join(standaloneRoot, ".next", "static")],
  [path.join(webRoot, "public"), path.join(standaloneRoot, "public")],
]) {
  await access(source);
  await mkdir(path.dirname(target), { recursive: true });
  await cp(source, target, { recursive: true, force: true });
}

process.stdout.write("Standalone client JS/CSS and public assets are ready.\n");
