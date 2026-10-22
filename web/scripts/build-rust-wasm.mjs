import { spawnSync } from "node:child_process";
import { copyFile, mkdir, readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const webRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
);
const manifestPath = path.join(
  webRoot,
  "rust",
  "crates",
  "opencut_engine",
  "Cargo.toml",
);

function runCargo(args, { captureOutput = false } = {}) {
  const result = spawnSync("cargo", args, {
    cwd: webRoot,
    encoding: "utf8",
    stdio: captureOutput ? ["ignore", "pipe", "inherit"] : "inherit",
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(
      `cargo ${args.join(" ")} failed with exit code ${result.status}`,
    );
  }
  return result.stdout;
}

const metadata = JSON.parse(
  runCargo(
    [
      "metadata",
      "--manifest-path",
      manifestPath,
      "--format-version",
      "1",
      "--no-deps",
    ],
    { captureOutput: true },
  ),
);
const targetDirectory = metadata.target_directory;
if (!targetDirectory) {
  throw new Error("Cargo metadata did not report the Rust target directory");
}

runCargo([
  "build",
  "--manifest-path",
  manifestPath,
  "--target",
  "wasm32-unknown-unknown",
  "--release",
]);

const wasmPath = path.join(
  targetDirectory,
  "wasm32-unknown-unknown",
  "release",
  "opencut_engine.wasm",
);
const wasmBytes = await readFile(wasmPath);
const wasmModule = await WebAssembly.compile(wasmBytes);
const exportNames = new Set(
  WebAssembly.Module.exports(wasmModule).map(({ name }) => name),
);
for (const requiredExport of [
  "memory",
  "oc_malloc",
  "oc_free",
  "ocw_audio_detect_beats",
  "ocw_audio_sample_peak_buckets",
]) {
  if (!exportNames.has(requiredExport)) {
    throw new Error(
      `Rust WASM artifact is missing required export: ${requiredExport}`,
    );
  }
}

const outputDirectory = path.join(webRoot, "public", "wasm");
await mkdir(outputDirectory, { recursive: true });
const outputPath = path.join(outputDirectory, "opencut_engine.wasm");
await copyFile(wasmPath, outputPath);
process.stdout.write(
  `Built Rust browser core: ${outputPath} (${wasmBytes.byteLength} bytes)\n`,
);
