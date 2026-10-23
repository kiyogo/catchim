"use client";

import { create } from "zustand";
import type { McpCredentials } from "./contracts";

export interface McpActivity {
	id: string;
	name: string;
	time: number;
	ok: boolean;
	error?: string;
}
interface ConnectionState {
	connection: McpCredentials | null;
	status: "off" | "connecting" | "connected" | "error";
	allowEdits: boolean;
	error: string | null;
	activity: McpActivity[];
}
export const useMcpConnection = create<ConnectionState>(() => ({
	connection: null,
	status: "off",
	allowEdits: false,
	error: null,
	activity: [],
}));
let generation = 0;
export function bridgeHeaders(connection: McpCredentials) {
	return {
		Authorization: `Bearer ${connection.bridgeToken}`,
		"Content-Type": "application/json",
	};
}
export function isLocalMcpHost(hostname: string): boolean {
	return ["localhost", "127.0.0.1", "[::1]"].includes(hostname);
}

async function revoke(connection: McpCredentials) {
	await fetch("/api/mcp/bridge", {
		method: "DELETE",
		headers: bridgeHeaders(connection),
		keepalive: true,
		signal: AbortSignal.timeout(3000),
	}).catch(() => {});
}
export async function disableMcp(error: string | null = null): Promise<void> {
	generation++;
	const { connection } = useMcpConnection.getState();
	useMcpConnection.setState({
		connection: null,
		status: error ? "error" : "off",
		allowEdits: false,
		error,
	});
	if (connection) await revoke(connection);
}
export async function enableMcp(project: {
	id: string;
	name: string;
}): Promise<void> {
	await disableMcp();
	const current = ++generation;
	useMcpConnection.setState({
		status: "connecting",
		error: null,
		activity: [],
	});
	try {
		const response = await fetch("/api/mcp/bridge", {
			method: "POST",
			signal: AbortSignal.timeout(8000),
			headers: { "Content-Type": "application/json" },
			body: JSON.stringify({
				projectId: project.id,
				projectName: project.name,
				allowEdits: false,
			}),
		});
		if (!response.ok)
			throw new Error(
				`MCP ${response.status}: use the Next.js server on localhost.`,
			);
		const connection = (await response.json()) as McpCredentials;
		if (current !== generation) {
			await revoke(connection);
			return;
		}
		useMcpConnection.setState({
			connection,
			status: "connected",
			allowEdits: false,
		});
	} catch (error) {
		if (current === generation)
			useMcpConnection.setState({
				status: "error",
				error: error instanceof Error ? error.message : "MCP connection failed",
			});
	}
}
export async function setMcpEditing(allowEdits: boolean): Promise<void> {
	const { connection } = useMcpConnection.getState();
	if (!connection) return;
	// Revocation applies in the tab immediately, before the HTTP acknowledgement.
	if (!allowEdits) useMcpConnection.setState({ allowEdits: false });
	try {
		const response = await fetch("/api/mcp/bridge", {
			method: "PATCH",
			signal: AbortSignal.timeout(8000),
			headers: bridgeHeaders(connection),
			body: JSON.stringify({ allowEdits }),
		});
		if (!response.ok)
			throw new Error("Could not update MCP editing permission");
		if (useMcpConnection.getState().connection?.id === connection.id)
			useMcpConnection.setState({ allowEdits });
	} catch (error) {
		if (useMcpConnection.getState().connection?.id === connection.id) {
			await disableMcp(
				error instanceof Error ? error.message : "MCP permission update failed",
			);
		}
	}
}
export function logMcpActivity(activity: McpActivity): void {
	useMcpConnection.setState((state) => ({
		activity: [
			activity,
			...state.activity.filter((item) => item.id !== activity.id),
		].slice(0, 30),
	}));
}
