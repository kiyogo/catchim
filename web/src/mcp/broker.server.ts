import { randomBytes, randomUUID } from "node:crypto";
import {
	toolDetails,
	type BridgeJob,
	type BridgeResult,
	type EditorToolName,
	type McpCredentials,
} from "./contracts";

interface PendingJob {
	job: BridgeJob;
	delivered: boolean;
	resolve: (result: BridgeResult) => void;
	finish: () => void;
}
export interface EditorConnection extends McpCredentials {
	projectName: string;
	lastSeen: number;
	pending: Map<string, PendingJob>;
	rateWindowStart: number;
	rateCount: number;
}

/** Browser connections are separate from MCP protocol/transport sessions. */
export class McpBroker {
	private connections = new Map<string, EditorConnection>();
	constructor(
		private options: {
			now?: () => number;
			ttlMs?: number;
			timeoutMs?: number;
		} = {},
	) {}
	private now() {
		return (this.options.now ?? Date.now)();
	}
	private prune() {
		for (const connection of this.connections.values()) {
			if (this.now() - connection.lastSeen > (this.options.ttlMs ?? 90000))
				this.disconnect(connection);
		}
	}
	create(input: {
		projectId: string;
		projectName: string;
		allowEdits: boolean;
	}): EditorConnection {
		this.prune();
		if (this.connections.size >= 32)
			throw new Error(
				"Too many editor connections. Disable an existing connection first.",
			);
		const connection: EditorConnection = {
			...input,
			id: randomUUID(),
			accessToken: randomBytes(32).toString("base64url"),
			bridgeToken: randomBytes(32).toString("base64url"),
			lastSeen: this.now(),
			pending: new Map(),
			rateWindowStart: this.now(),
			rateCount: 0,
		};
		this.connections.set(connection.id, connection);
		return connection;
	}
	authorize(token: string, role: "access" | "bridge"): EditorConnection | null {
		this.prune();
		if (!token) return null;
		for (const connection of this.connections.values()) {
			if (
				token ===
				(role === "access" ? connection.accessToken : connection.bridgeToken)
			)
				return connection;
		}
		return null;
	}
	disconnect(connection: EditorConnection): void {
		this.connections.delete(connection.id);
		for (const pending of [...connection.pending.values()]) {
			pending.resolve({
				ok: false,
				error:
					"The editor connection was closed or expired. Reconnect from Settings → MCP.",
			});
			pending.finish();
		}
	}
	setAllowEdits(connection: EditorConnection, allowEdits: boolean): void {
		connection.allowEdits = allowEdits;
		connection.lastSeen = this.now();
		if (!allowEdits) {
			for (const pending of [...connection.pending.values()]) {
				if (!toolDetails[pending.job.name].readOnly) {
					pending.resolve({
						ok: false,
						error: "Editing permission was revoked in the editor.",
					});
					pending.finish();
				}
			}
		}
	}
	takeJob(connection: EditorConnection): BridgeJob | null {
		connection.lastSeen = this.now();
		for (const pending of connection.pending.values()) {
			if (!pending.delivered && pending.job.expiresAt > this.now()) {
				pending.delivered = true;
				return pending.job;
			}
		}
		return null;
	}
	complete(
		connection: EditorConnection,
		id: string,
		result: BridgeResult,
	): boolean {
		const pending = connection.pending.get(id);
		if (!pending?.delivered || pending.job.expiresAt <= this.now())
			return false;
		pending.resolve(result);
		pending.finish();
		return true;
	}
	dispatch(
		connection: EditorConnection,
		name: EditorToolName,
		input: unknown,
		signal?: AbortSignal,
	): Promise<BridgeResult> {
		if (!this.connections.has(connection.id))
			return Promise.resolve({ ok: false, error: "Editor disconnected" });
		if (!toolDetails[name].readOnly && !connection.allowEdits)
			return Promise.resolve({
				ok: false,
				error:
					"This connection is read-only. Enable editing in Settings → MCP.",
			});
		if (signal?.aborted)
			return Promise.resolve({
				ok: false,
				error: "Request cancelled before dispatch",
			});
		if (this.now() - connection.rateWindowStart >= 60000) {
			connection.rateWindowStart = this.now();
			connection.rateCount = 0;
		}
		if (++connection.rateCount > 120) return Promise.resolve({ ok: false, code: "RATE_LIMIT", error: "Maximum 120 tool calls per minute per connection" });
		if (connection.pending.size >= 16)
			return Promise.resolve({
				ok: false,
				error: "Editor queue is full. Wait for pending calls to finish.",
			});
		return new Promise((resolve) => {
			const job: BridgeJob = {
				id: randomUUID(),
				name,
				input,
				expiresAt: this.now() + (this.options.timeoutMs ?? 20000),
			};
			const finish = () => {
				clearTimeout(timer);
				signal?.removeEventListener("abort", cancel);
				connection.pending.delete(job.id);
			};
			const cancel = () => {
				resolve({
					ok: false,
					error:
						"Request cancelled. Inspect editor state before retrying a delivered operation.",
				});
				finish();
			};
			const timer = setTimeout(() => {
				resolve({
					ok: false,
					error:
						"Editor did not respond in time. Keep its tab active and inspect state before retrying.",
				});
				finish();
			}, this.options.timeoutMs ?? 20000);
			timer.unref?.();
			connection.pending.set(job.id, {
				job,
				delivered: false,
				resolve,
				finish,
			});
			signal?.addEventListener("abort", cancel, { once: true });
		});
	}
}

const globals = globalThis as typeof globalThis & {
	__catchimMcpBroker?: McpBroker;
};
export const mcpBroker = (globals.__catchimMcpBroker ??= new McpBroker());
