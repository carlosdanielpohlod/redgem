---
description: "MCP over stdio: scene tools, headless runs, bounded events and explicit permissions for one game."
---

# MCP tools

Start a local MCP server for one game:

```sh
redgem mcp /path/to/game --read-only
```

Configure your MCP client to launch the `redgem` executable with those arguments. It reads
newline-delimited JSON-RPC from standard input and writes only protocol responses to standard
output. Native library output goes to standard error. Closing input stops the server.
It opens no listening socket and does not attach to an editor.

The server supports the MCP `2026-07-28` per-request metadata protocol and the `2025-11-25`
initialization handshake. Modern clients can discover capabilities with `server/discover` and
must supply their protocol version and client capabilities in each request's `params._meta`.
Legacy clients send `initialize`, then `notifications/initialized`, before using tools.

## Tools and permissions

| Tool | What it does |
|------|--------------|
| `scene_list` | Lists objects, taking optional `scene`, `limit` and `depth`. |
| `scene_get` | Reads one `object` by persistent `#id`, unique name or hierarchy path. |
| `scene_edit` | Takes `action` (`set`, `add`, `remove`), `object` and optional `assignments` (`key=value` strings), `from`, `parent`, `scene`. Removal requires `confirm: true`. Saves the scene file. |
| `scene_diff` | Compares with an earlier scene file named by `against`, inside the game. |
| `game_run` | Plays headlessly with `frames` (default 60, maximum 600), `seed`, `input`, `assert`, `until`, `print`, `every`, `scene`. |
| `game_check` | Checks scripts, scenes and assets; does not run the game's test files. |
| `api` | Reads the API index, a `class`, or a `search`. |
| `events` | Reads this server's log history with `since`, `limit` (default 20, maximum 32), `level`. |

`--read-only` exposes only `scene_list`, `scene_get`, `scene_diff`, `api` and `events`. It also
rejects calls to disabled tools, even if the client remembers an earlier tool list. For a smaller
surface, add an exact comma-separated allow list:

```sh
redgem mcp /path/to/game --read-only --allow scene_list,scene_get,api,events
```

Without `--read-only`, the tools can save edits and execute the configured game's Ruby scripts.
Run only games you trust. A run expression can mutate state or use networking; read-only mode
disables execution entirely. The configured game directory stays fixed for the life of the
process. Input replay and comparison files must be regular files inside it, without symbolic
links. These restrictions protect tooling paths; they do not sandbox native game code.

Edits go through the same scene commands as the CLI. Close the editor while editing through
MCP: there is no live synchronization or editor undo entry yet. Keep an earlier save and inspect
`scene_diff` after an edit. `from` and `parent` are available only when adding an object.

## Bounded results

Each request is at most 64 KiB with at most 16 JSON nesting levels. Ordinary tool text is limited
to 8 KiB with a truncation notice; narrow the scene list or API query for more detail. Invalid
arguments return JSON-RPC errors. A valid command that fails returns `isError: true` with the
engine's diagnostic.

Event results also contain `structuredContent`: `events`, `cursor`, `dropped`, `more` and
`cursor_ahead`. Pass the returned cursor in the next `since`. Each event's text is capped at 256
UTF-8 bytes, with `truncated` when shortened. The cursor belongs to this server process; it is
not a cursor into another running editor or game. Events include engine log messages, not raw
standard output. Process history is bounded, so `dropped` identifies overwritten messages.

Requests run one at a time. A cancellation notification cannot interrupt a native command already
running; frame limits and the game's script budget bound headless work. A slow native operation
can still delay completion. Clients can terminate and restart a server that stops responding.

The protocol follows the official [stdio binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio)
and [tool messages](https://modelcontextprotocol.io/specification/2026-07-28/server/tools).
