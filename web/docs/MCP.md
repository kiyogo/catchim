# Catchim Web MCP

Catchim exposes the project in one explicitly connected browser tab to an MCP client. This implementation belongs to `web/`; it does not modify the Qt application or the saved project format.

## Setup

1. Run the Next.js editor at `http://localhost:3300` and open a project.
2. Open **Settings → MCP → Enable MCP**. The connection starts in read-only mode.
3. Copy the endpoint and access token, or use **Copy MCP config**. Configure a client that supports Streamable HTTP with an `Authorization: Bearer <token>` header. Client configuration formats differ; the copied JSON uses the common `mcpServers` / `url` / `headers` shape.
4. Enable **Allow editing** if the assistant should change the project. Read the timeline first to obtain current track/clip IDs.
5. Keep the editor tab active. **Disable**, leaving the editor, or changing projects closes the connection. Reloading the page or restarting the server requires a new connection/token.

Example configuration (replace the placeholder with the token shown in the editor):

```json
{
  "mcpServers": {
    "catchim": {
      "url": "http://localhost:3300/api/mcp",
      "headers": {
        "Authorization": "Bearer <token-from-settings>"
      }
    }
  }
}
```

## Architecture and scope

```text
MCP client → Next POST /api/mcp → in-memory connection broker
                                       ↕ queued, bounded requests/results
Browser MCP bridge → EditorCore → existing commands → Rust/WASM + render/storage
```

- The official TypeScript SDK `@modelcontextprotocol/sdk@1.32.1` handles protocol validation, negotiation and JSON response transport. The supported protocol family is the initialize-handshake family through `2025-11-25`; modern-only clients must support negotiation to this family. This is a stateless MCP transport: no MCP session ID, standalone SSE stream or subscriptions. [SDK v1 server guide](https://ts.sdk.modelcontextprotocol.io/server), [Streamable HTTP specification](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports).
- The broker's browser connection is separate from an MCP transport session. Browser and external-client credentials are independently generated random 256-bit tokens. Only the browser credential can poll/complete jobs or change permissions. The external token only grants access to tools/resources for its connected project.
- The MCP routes validate loopback URL/Host and same-origin Origin headers. Browser connection creation and changes require a same-origin Origin. No cross-origin browser access is enabled. This version targets the local single-process Next.js Node server; it is unavailable in the standalone Vite build and is not a multi-worker/cloud service.
- The browser remains the owner of project/media state. No media bytes, browser object URLs or thumbnails are copied to MCP responses. Read tools return project/timeline data and imported media metadata, with clip/media pagination.
- Editing permission is enforced both before server dispatch and again before browser execution. Existing track locks, placement rules, source mapping, history and Rust/WASM planners remain authoritative. API times are seconds, converted at the existing WASM time boundary. Audio gain uses the editor's dB field.
- Requests are delivered at most once, capped at 16 pending calls per connection, and expire after 20 seconds. Undelivered HTTP-aborted/expired work cannot execute later. Browser connections expire after 90 seconds without polling. If a delivered operation times out or loses its acknowledgement, inspect current state before retrying; a completed command is not automatically replayed or rolled back.
- Tokens are held in memory and never persisted in project data. There is no arbitrary JavaScript, filesystem access, network import, AI provider call or video export tool. The UI records the latest 30 tool outcomes without retaining arguments/results.

## Tools and resources

| Tool | Behavior |
| --- | --- |
| `catchim_get_project` | Project settings, scene summaries, playhead, duration, history availability |
| `catchim_list_media` | Paginated imported media metadata |
| `catchim_get_timeline` | Track summaries and paginated clip snapshots, optional track filter |
| `catchim_get_selection` | Selected clip/keyframe references |
| `catchim_seek` | Seek to seconds; return actual clamped time |
| `catchim_select_elements` | Select existing clips or clear selection |
| `catchim_add_text` | Insert text with optional font size/color and explicit track |
| `catchim_add_media` | Insert an imported source using normal source duration/placement |
| `catchim_update_clip` | Name/start time/text/opacity/volume dB; return actual values and `applied` |
| `catchim_split_clip` | Source-aware split inside a clip |
| `catchim_duplicate_clips` | Duplicate clips and return actual IDs/placement |
| `catchim_delete_clips` | Undoable timeline deletion; sources remain imported |
| `catchim_set_track_state` | Lock/mute/hide as supported by track type |
| `catchim_close_track_gaps` | Existing Rust/WASM interval planner + undo command |
| `catchim_set_clip_color` | Add/update color grading as one undoable change |
| `catchim_history` | One undo/redo in the shared history, including human edits |
| `catchim_save_project` | Save to browser storage and propagate failures |

Read-only resources: `catchim://project`, `catchim://timeline`, `catchim://media`. Paginated resources return the first page; use tools for subsequent pages.

## Validation

```powershell
npx vitest run src/mcp
npx tsc --noEmit
npm run build
# With the production server running:
node scripts/check-mcp.mjs
```

The HTTP smoke script creates its own synthetic browser connection and runs the official client against the live Next.js routes. It does not access an open user project. Browser controls, actual editing workflows, storage and render output still need browser QA separately.

Validated on 2026-10-23: **4 test files / 20 tests PASS**, standalone TypeScript check PASS, final Next.js/Rust-WASM production build PASS, and live-server `node scripts/check-mcp.mjs` PASS. The HTTP check covers SDK negotiation, 17-tool/3-resource discovery, browser request/result relay, read-only denial, enabling edits, credential separation, foreign-origin rejection and token revocation. The Next.js internal `0.0.0.0` request URL with a loopback public Host is covered by a regression test. Direct browser GUI verification was unavailable: the browser tool returned `Local browser use is disabled in Work settings`.
