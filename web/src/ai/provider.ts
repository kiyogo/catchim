import { AgentError } from "./contracts";

export interface PlanningMessage { role: "system" | "user"; content: string }
export interface PlanningProvider {
	complete(messages: PlanningMessage[], signal: AbortSignal): Promise<string>;
}
export interface ProviderConfig {
	kind: "openai-compatible" | "ollama";
	endpoint: string;
	model: string;
	apiKey?: string;
	allowContextTransfer: boolean;
}

/** Runs in the browser. Endpoint, model and credentials stay under the user's control. */
export class HttpPlanningProvider implements PlanningProvider {
	constructor(private config: ProviderConfig) {}
	async complete(messages: PlanningMessage[], signal: AbortSignal): Promise<string> {
		if (!this.config.allowContextTransfer) throw new AgentError("PERMISSION_DENIED", "Approve sending project metadata and text to the configured model first");
		const url = new URL(this.config.endpoint);
		if (!["http:", "https:"].includes(url.protocol) || url.username || url.password || url.hash || url.search) throw new AgentError("PROVIDER_ERROR", "Use an HTTP(S) endpoint without credentials, query or fragment");
		if (!this.config.model.trim()) throw new AgentError("PROVIDER_ERROR", "Choose a model");
		const ollama = this.config.kind === "ollama";
		const response = await fetch(url, {
			method: "POST", credentials: "omit", redirect: "error", signal,
			headers: { "Content-Type": "application/json", ...(this.config.apiKey ? { Authorization: `Bearer ${this.config.apiKey}` } : {}) },
			body: JSON.stringify({ model: this.config.model.trim(), messages, stream: false, ...(ollama ? { format: "json", options: { num_predict: 8192 } } : { response_format: { type: "json_object" }, max_tokens: 8192 }) }),
		});
		if (!response.ok) throw new AgentError("PROVIDER_ERROR", `Model returned HTTP ${response.status}`);
		const reader = response.body?.getReader();
		if (!reader) throw new AgentError("PROVIDER_ERROR", "Model returned no body");
		let size = 0;
		const chunks: Uint8Array[] = [];
		try {
			while (true) {
				const { done, value } = await reader.read();
				if (done) break;
				size += value.byteLength;
				if (size > 256 * 1024) { await reader.cancel(); throw new AgentError("RESOURCE_LIMIT", "Model response exceeds 256 KiB"); }
				chunks.push(value);
			}
		} finally { reader.releaseLock(); }
		const bytes = new Uint8Array(size);
		let offset = 0;
		for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
		const data = JSON.parse(new TextDecoder().decode(bytes));
		const content = ollama ? data.message?.content : data.choices?.[0]?.message?.content;
		if (typeof content !== "string") throw new AgentError("PROVIDER_ERROR", "Model did not return JSON text");
		return content;
	}
}
