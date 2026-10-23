import { afterEach, describe, expect, it, vi } from "vitest";
import {
	disableMcp,
	enableMcp,
	setMcpEditing,
	useMcpConnection,
} from "./connection-store";
import type { McpCredentials } from "./contracts";

const credentials: McpCredentials = {
	id: "first",
	projectId: "project",
	accessToken: "a".repeat(43),
	bridgeToken: "b".repeat(43),
	allowEdits: false,
};
afterEach(async () => {
	await disableMcp();
	vi.unstubAllGlobals();
});

describe("MCP browser credential lifecycle", () => {
	it("starts connections read-only and revokes both local credentials on disable", async () => {
		const requests = vi.fn(async (_url, options) =>
			options?.method === "POST"
				? Response.json(credentials)
				: new Response(null, { status: 204 }),
		);
		vi.stubGlobal("fetch", requests);
		await enableMcp({ id: "project", name: "Project" });
		expect(useMcpConnection.getState()).toMatchObject({
			status: "connected",
			allowEdits: false,
			connection: credentials,
		});
		await disableMcp();
		expect(useMcpConnection.getState().connection).toBeNull();
		expect(
			requests.mock.calls.some(([, options]) => options?.method === "DELETE"),
		).toBe(true);
	});
	it("revokes a late creation response instead of reconnecting after disable", async () => {
		let resolveCreate!: (response: Response) => void;
		const created = new Promise<Response>((resolve) => {
			resolveCreate = resolve;
		});
		vi.stubGlobal(
			"fetch",
			vi.fn(async (_url, options) =>
				options?.method === "POST"
					? created
					: new Response(null, { status: 204 }),
			),
		);
		const enabling = enableMcp({ id: "project", name: "Project" });
		await vi.waitFor(() =>
			expect(useMcpConnection.getState().status).toBe("connecting"),
		);
		await disableMcp();
		resolveCreate(Response.json(credentials));
		await enabling;
		expect(useMcpConnection.getState()).toMatchObject({
			connection: null,
			status: "off",
		});
	});
	it("an old permission failure cannot disable a newer project connection", async () => {
		let rejectPolicy!: (error: Error) => void;
		vi.stubGlobal(
			"fetch",
			vi.fn((_url, options) =>
				options?.method === "PATCH"
					? new Promise((_resolve, reject) => {
							rejectPolicy = reject;
						})
					: Promise.resolve(new Response(null, { status: 204 })),
			),
		);
		useMcpConnection.setState({
			connection: credentials,
			status: "connected",
			allowEdits: true,
		});
		const policy = setMcpEditing(false);
		expect(useMcpConnection.getState().allowEdits).toBe(false);
		const replacement = {
			...credentials,
			id: "second",
			projectId: "second-project",
		};
		useMcpConnection.setState({ connection: replacement, status: "connected" });
		rejectPolicy(new Error("Old request failed"));
		await policy;
		expect(useMcpConnection.getState().connection?.id).toBe("second");
	});
});
