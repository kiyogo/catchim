import { afterEach, expect, it, vi } from "vitest";
import { HttpPlanningProvider } from "./provider";
afterEach(() => vi.unstubAllGlobals());
const config = { kind: "ollama" as const, endpoint: "http://localhost:11434/api/chat", model: "local", allowContextTransfer: true };
it("does not contact a model without explicit context-transfer consent", async () => {
	const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher);
	await expect(new HttpPlanningProvider({ ...config, allowContextTransfer: false }).complete([], new AbortController().signal)).rejects.toMatchObject({ code: "PERMISSION_DENIED" });
	expect(fetcher).not.toHaveBeenCalled();
});
it("rejects credential-bearing and non-HTTP endpoints", async () => {
	const fetcher = vi.fn(); vi.stubGlobal("fetch", fetcher);
	for (const endpoint of ["file:///private", "https://secret@provider.test/chat", "https://provider.test/chat?token=secret"]) await expect(new HttpPlanningProvider({ ...config, endpoint }).complete([], new AbortController().signal)).rejects.toThrow();
	expect(fetcher).not.toHaveBeenCalled();
});
it("bounds model responses and reports HTTP failures", async () => {
	vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response("secret failure", { status: 429 })));
	await expect(new HttpPlanningProvider(config).complete([], new AbortController().signal)).rejects.toThrow("HTTP 429");
	vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response("x".repeat(256 * 1024 + 1))));
	await expect(new HttpPlanningProvider(config).complete([], new AbortController().signal)).rejects.toMatchObject({ code: "RESOURCE_LIMIT" });
});
