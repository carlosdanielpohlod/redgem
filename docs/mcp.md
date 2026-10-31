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
By default it opens no listening socket and works on files. Add `--editor` to connect to an
editor that explicitly enabled its local bridge (see below).

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

Without `--editor`, edits go through the file-based scene commands. Close the editor while
editing its files this way. Keep an earlier save and inspect `scene_diff` after an edit.
`from` is available only in file mode; `parent` also works when adding a live empty object.

## Live editor

Start the editor with its bridge explicitly enabled:

```sh
redgem /path/to/game --editor-bridge
```

The terminal prints the path of a private `connection.json` in the user's RedGem data folder.
Pass that file to the MCP server for the same game:

```sh
redgem mcp /path/to/game --editor /path/to/connection.json
```

Scene listing, object details and scene comparison now read the editor's unsaved document.
`scene_edit` supports `set`, `add` and `remove`. `add` creates an empty object, optionally under
`parent`, with initial `assignments`; attach scripts and properties as with the CLI. `from`
(asset-backed duplication) remains file-only. `remove` deletes a subtree and requires
`confirm: true`. Every call is one undo step: invalid assignments or exceptions roll it back
without losing earlier undo/redo. Changes update the viewport and remain unsaved until `save`.

The `editor` tool takes these actions:

| Action | Result |
|--------|--------|
| `status` | Scene, play state, dirty flag, selection, undo/redo and external-conflict state. |
| `select` | Select the requested `object`. |
| `undo`, `redo`, `save` | Apply the corresponding editor command. Saving is explicit. |
| `play`, `pause`, `resume`, `stop` | Control the editor's game, using its normal input, audio and viewport. |
| `step` | Advance a paused game by exactly one fixed frame, keeping it paused. |
| `capture` | Return the last completed viewport as an MCP PNG image, without writing a file. |

`capture` accepts `max_size` (default 512, range 128–1024), the longest output side. Images are
reduced further when necessary for the reply, with the PNG as base64 text, to stay within the
bridge's 256 KiB limit (about 188 KiB of PNG). Only the scene
viewport is captured, never the desktop or other applications. `object` is accepted only by
`select`; `max_size` only by `capture`. Read-only mode permits `status` and `capture` only.

Edits and document commands are refused while playing, during gestures/dialogs, or with an
external file conflict pending. Play starts a copy of the unsaved scene; Stop discards that
runtime and restores the edited document. Pause/resume/stop/step control that runtime, not the
file. Read operations still describe the edited document, not runtime-mutated objects. An
explicit `scene` must match the open scene; requests never switch or discard scenes.

`events` reads the editor's log history. `api`, `game_run` and `game_check` remain ordinary CLI
tools: runs/checks read saved files rather than the unsaved editor scene. Runtime object
inspection and arbitrary live eval are not exposed by this release.

The bridge binds only to IPv4 loopback and requires a random credential on every request, with
the game directory checked at both ends. Its connection file stays outside the repository and
is removed on clean shutdown. On POSIX it is owner-only inside an owner-only directory; on
Windows it inherits the user's profile access controls. Treat the file as a credential granting
control of that editor. MCP read-only and allow-list options restrict that client's calls.

At most four connections are handled at once, on the editor thread between frames. Requests
are limited to 64 KiB and replies to 256 KiB. Transport waits expire after five seconds. If an
edit times out, it may have completed: inspect status and the object before retrying. There is
no automatic retry or fallback to editing the file when an editor connection fails. After a
crash a stale connection file can remain; reconnect using the new editor instance's file.

## Bounded results

Each request is at most 64 KiB with at most 16 JSON nesting levels. Ordinary tool text is limited
to 8 KiB with a truncation notice; narrow the scene list or API query for more detail. Invalid
arguments return JSON-RPC errors. A valid command that fails returns `isError: true` with the
engine's diagnostic.

Event results also contain `structuredContent`: `events`, `cursor`, `dropped`, `more` and
`cursor_ahead`. Pass the returned cursor in the next `since`. Each event's text is capped at 256
UTF-8 bytes, with `truncated` when shortened. The cursor belongs to this server process; it is
not a cursor into an unrelated editor or game (with `--editor`, it belongs to that editor).
Events include engine log messages, not raw
standard output. Process history is bounded, so `dropped` identifies overwritten messages.

Requests run one at a time. A cancellation notification cannot interrupt a native command already
running; frame limits and the game's script budget bound headless work. A slow native operation
can still delay completion. Clients can terminate and restart a server that stops responding.

The protocol follows the official [stdio binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio)
and [tool messages](https://modelcontextprotocol.io/specification/2026-07-28/server/tools).
