import { afterEach, describe, expect, it, vi } from "vitest";
import { McpBroker } from "./broker.server";

const input = {
	projectId: "project-a",
	projectName: "Project A",
	allowEdits: false,
};
afterEach(() => vi.useRealTimers());

describe("MCP browser connection lifecycle", () => {
	it("uses independent credentials for clients and the browser, and revokes both", () => {
		const broker = new McpBroker();
		const connection = broker.create(input);
		expect(broker.authorize(connection.accessToken, "access")).toBe(connection);
		expect(broker.authorize(connection.accessToken, "bridge")).toBeNull();
		expect(broker.authorize(connection.bridgeToken, "access")).toBeNull();
		broker.disconnect(connection);
		expect(broker.authorize(connection.accessToken, "access")).toBeNull();
		expect(broker.authorize(connection.bridgeToken, "bridge")).toBeNull();
	});
	it("blocks edits before they enter the browser queue in read-only mode", async () => {
		const broker = new McpBroker();
		const connection = broker.create(input);
		expect(
			await broker.dispatch(connection, "catchim_delete_clips", {
				elements: [],
			}),
		).toMatchObject({ ok: false });
		expect(broker.takeJob(connection)).toBeNull();
	});
	it("delivers a job once and accepts only that connection's first acknowledgement", async () => {
		const broker = new McpBroker();
		const first = broker.create(input);
		const second = broker.create({ ...input, projectId: "project-b" });
		const pending = broker.dispatch(first, "catchim_get_project", {});
		const job = broker.takeJob(first)!;
		expect(broker.takeJob(first)).toBeNull();
		expect(broker.complete(second, job.id, { ok: true, data: {} })).toBe(false);
		expect(
			broker.complete(first, job.id, {
				ok: true,
				data: { projectId: "project-a" },
			}),
		).toBe(true);
		expect(broker.complete(first, job.id, { ok: true, data: {} })).toBe(false);
		expect(await pending).toEqual({
			ok: true,
			data: { projectId: "project-a" },
		});
	});
	it("revoking editing cancels writes while preserving queued reads", async () => {
		const broker = new McpBroker();
		const connection = broker.create({ ...input, allowEdits: true });
		const write = broker.dispatch(connection, "catchim_history", {
			action: "undo",
		});
		const read = broker.dispatch(connection, "catchim_get_project", {});
		broker.setAllowEdits(connection, false);
		expect(await write).toMatchObject({ ok: false });
		const job = broker.takeJob(connection)!;
		expect(job.name).toBe("catchim_get_project");
		broker.complete(connection, job.id, { ok: true, data: {} });
		expect(await read).toMatchObject({ ok: true });
	});
	it("removes cancelled jobs before the browser can execute them", async () => {
		const broker = new McpBroker();
		const connection = broker.create({ ...input, allowEdits: true });
		const controller = new AbortController();
		const pending = broker.dispatch(
			connection,
			"catchim_history",
			{ action: "undo" },
			controller.signal,
		);
		controller.abort();
		expect(await pending).toMatchObject({ ok: false });
		expect(broker.takeJob(connection)).toBeNull();
	});
	it("expires queued work and rejects late acknowledgements", async () => {
		vi.useFakeTimers();
		const broker = new McpBroker({ timeoutMs: 100 });
		const connection = broker.create(input);
		const pending = broker.dispatch(connection, "catchim_get_project", {});
		const job = broker.takeJob(connection)!;
		await vi.advanceTimersByTimeAsync(101);
		expect(await pending).toMatchObject({ ok: false });
		expect(broker.complete(connection, job.id, { ok: true, data: {} })).toBe(
			false,
		);
		expect(broker.takeJob(connection)).toBeNull();
	});
	it("browser inactivity expires credentials and settles pending requests", async () => {
		let now = 0;
		const broker = new McpBroker({ now: () => now, ttlMs: 100 });
		const connection = broker.create(input);
		const pending = broker.dispatch(connection, "catchim_get_project", {});
		now = 101;
		expect(broker.authorize(connection.accessToken, "access")).toBeNull();
		expect(await pending).toMatchObject({ ok: false });
	});
});
