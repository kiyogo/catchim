const [base = "http://localhost:3300", ...requestedPaths] = process.argv.slice(2);
const routes = requestedPaths.length ? requestedPaths : ["/", "/projects"];
const assets = new Set();

async function get(url) {
  const response = await fetch(url, { signal: AbortSignal.timeout(20_000) });
  if (!response.ok) throw new Error(`${response.status} ${url}`);
  return response;
}

for (const route of routes) {
  const response = await get(new URL(route, base));
  if (!(response.headers.get("content-type") ?? "").includes("text/html")) throw new Error(`Unexpected page type: ${route}`);
  const html = await response.text();
  for (const match of html.matchAll(/(?:src|href)="([^"]+)"/g)) {
    const asset = new URL(match[1].replaceAll("&amp;", "&"), base);
    if (asset.pathname.startsWith("/_next/static/") && /\.(js|css)$/.test(asset.pathname)) assets.add(asset.href);
  }
}
if (assets.size === 0) throw new Error("No Next.js client assets found in page responses");

for (const url of assets) {
  const response = await get(url);
  const type = response.headers.get("content-type") ?? "";
  if (new URL(url).pathname.endsWith(".css") ? !type.includes("text/css") : !/(java|ecma)script/.test(type)) throw new Error(`Unexpected client asset type: ${type} ${url}`);
  await response.arrayBuffer();
}
const wasm = await get(new URL("/wasm/opencut_engine.wasm", base));
if (!(wasm.headers.get("content-type") ?? "").includes("application/wasm")) throw new Error("Unexpected WASM content type");
await WebAssembly.compile(await wasm.arrayBuffer());
process.stdout.write(`PASS: ${routes.length} page routes, ${assets.size} JS/CSS assets and valid Rust WASM.\n`);
