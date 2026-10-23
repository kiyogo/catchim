import { mcpBroker } from "@/mcp/broker.server";
import { bearerToken, jsonNoStore, localRequestError } from "@/mcp/http.server";
import { handleMcpRequest } from "@/mcp/server";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";
export const maxDuration = 30;

export async function POST(request: Request): Promise<Response> {
	const denied = localRequestError(request);
	if (denied) return denied;
	const connection = mcpBroker.authorize(bearerToken(request), "access");
	if (!connection)
		return jsonNoStore(
			{ error: "Enable MCP in the editor and supply its access token" },
			401,
		);
	return handleMcpRequest(request, connection);
}
function methodNotAllowed(request: Request): Response {
	return (
		localRequestError(request) ??
		new Response(null, {
			status: 405,
			headers: { Allow: "POST", "Cache-Control": "no-store" },
		})
	);
}
export const GET = methodNotAllowed;
export const DELETE = methodNotAllowed;
