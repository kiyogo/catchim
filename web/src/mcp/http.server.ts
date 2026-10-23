/** The initial MCP feature is deliberately scoped to a local Node server. */
export function localRequestError(
	request: Request,
	requireOrigin = false,
): Response | null {
	const url = new URL(request.url);
	const allowed = new Set(["localhost", "127.0.0.1", "[::1]"]);
	const host = request.headers.get("host");
	let hostUrl: URL;
	try {
		hostUrl = new URL(`${url.protocol}//${host ?? url.host}`);
	} catch {
		return Response.json({ error: "Invalid host" }, { status: 403 });
	}
	if (
		!allowed.has(hostUrl.hostname) ||
		(!allowed.has(url.hostname) && url.hostname !== "0.0.0.0") ||
		hostUrl.port !== url.port ||
		(host !== null && host.toLowerCase() !== hostUrl.host)
	)
		return Response.json(
			{ error: "MCP is available on localhost only" },
			{ status: 403 },
		);
	const origin = request.headers.get("origin");
	// Next standalone may reconstruct request.url with its internal bind address.
	// The validated loopback Host remains the public authority for Origin checks.
	const expectedOrigin = `${url.protocol}//${hostUrl.host}`;
	if (
		(requireOrigin && !origin) ||
		(origin && origin !== expectedOrigin) ||
		request.headers.get("sec-fetch-site") === "cross-site"
	)
		return Response.json({ error: "Origin is not allowed" }, { status: 403 });
	return null;
}
export function bearerToken(request: Request): string {
	return (
		/^Bearer ([A-Za-z0-9_-]{43})$/.exec(
			request.headers.get("authorization") ?? "",
		)?.[1] ?? ""
	);
}
export function jsonNoStore(data: unknown, status = 200): Response {
	return Response.json(data, {
		status,
		headers: { "Cache-Control": "no-store" },
	});
}
export async function limitedJson(
	request: Request,
	limit = 1024 * 1024,
): Promise<unknown> {
	if (Number(request.headers.get("content-length") ?? 0) > limit)
		throw new Error("Request is too large");
	const reader = request.body?.getReader();
	if (!reader) throw new Error("Missing request body");
	const parts: Uint8Array[] = [];
	let size = 0;
	try {
		while (true) {
			const { value, done } = await reader.read();
			if (done) break;
			size += value.byteLength;
			if (size > limit) {
				await reader.cancel();
				throw new Error("Request is too large");
			}
			parts.push(value);
		}
	} finally {
		reader.releaseLock();
	}
	const bytes = new Uint8Array(size);
	let offset = 0;
	for (const part of parts) {
		bytes.set(part, offset);
		offset += part.length;
	}
	return JSON.parse(new TextDecoder().decode(bytes));
}
