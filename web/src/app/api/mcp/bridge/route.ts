import { z } from "zod";
import { mcpBroker } from "@/mcp/broker.server";
import {
	bearerToken,
	jsonNoStore,
	limitedJson,
	localRequestError,
} from "@/mcp/http.server";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";
const createInput = z
	.object({
		projectId: z.string().min(1).max(200),
		projectName: z.string().max(300),
		allowEdits: z.boolean().default(false),
	})
	.strict();
const resultInput = z
	.object({
		jobId: z.string().uuid(),
		result: z.discriminatedUnion("ok", [
			z
				.object({
					ok: z.literal(true),
					data: z.record(z.string(), z.unknown()),
				})
				.strict(),
			z.object({ ok: z.literal(false), error: z.string().max(2000) }).strict(),
		]),
	})
	.strict();

export async function POST(request: Request): Promise<Response> {
	const denied = localRequestError(request, true);
	if (denied) return denied;
	try {
		const input = await limitedJson(request);
		if (!request.headers.has("authorization")) {
			const connection = mcpBroker.create(createInput.parse(input));
			return jsonNoStore(
				{
					id: connection.id,
					projectId: connection.projectId,
					allowEdits: connection.allowEdits,
					accessToken: connection.accessToken,
					bridgeToken: connection.bridgeToken,
				},
				201,
			);
		}
		const connection = mcpBroker.authorize(bearerToken(request), "bridge");
		if (!connection)
			return jsonNoStore({ error: "Editor connection expired" }, 401);
		const { jobId, result } = resultInput.parse(input);
		return jsonNoStore({
			accepted: mcpBroker.complete(connection, jobId, result),
		});
	} catch (error) {
		return jsonNoStore(
			{ error: error instanceof Error ? error.message : "Invalid request" },
			400,
		);
	}
}
export async function GET(request: Request): Promise<Response> {
	const denied = localRequestError(request);
	if (denied) return denied;
	const connection = mcpBroker.authorize(bearerToken(request), "bridge");
	if (!connection)
		return jsonNoStore({ error: "Editor connection expired" }, 401);
	return jsonNoStore({
		job: mcpBroker.takeJob(connection),
		allowEdits: connection.allowEdits,
	});
}
export async function PATCH(request: Request): Promise<Response> {
	const denied = localRequestError(request, true);
	if (denied) return denied;
	const connection = mcpBroker.authorize(bearerToken(request), "bridge");
	if (!connection)
		return jsonNoStore({ error: "Editor connection expired" }, 401);
	try {
		const { allowEdits } = z
			.object({ allowEdits: z.boolean() })
			.strict()
			.parse(await limitedJson(request, 1024));
		mcpBroker.setAllowEdits(connection, allowEdits);
		return jsonNoStore({ allowEdits });
	} catch {
		return jsonNoStore({ error: "Invalid permission setting" }, 400);
	}
}
export async function DELETE(request: Request): Promise<Response> {
	const denied = localRequestError(request, true);
	if (denied) return denied;
	const connection = mcpBroker.authorize(bearerToken(request), "bridge");
	if (connection) mcpBroker.disconnect(connection);
	return new Response(null, {
		status: 204,
		headers: { "Cache-Control": "no-store" },
	});
}
