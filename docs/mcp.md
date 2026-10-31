---
description: "redgem mcp: MCP over stdio for one game, with scene tools, headless runs, game_query and game_shot, the live editor, bounded events and explicit permissions."
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

The server supports the MCP `2026-07-28` per-request metadata protocol and the `2025-11-25`,
`2025-06-18` and `2025-03-26` initialization handshakes (a client asking for one of them gets it). Modern clients can discover capabilities with `server/discover` and
must supply their protocol version and client capabilities in each request's `params._meta`.
Legacy clients send `initialize`, then `notifications/initialized`, before using tools.

## Tools and permissions

| Tool | What it does |
|------|--------------|
| `scene_list` | Lists objects, taking optional `scene`, `limit` (default 50, `0` for all) and `depth`. |
| `scene_get` | Reads one `object` by persistent `#id`, unique name or hierarchy path. |
| `scene_edit` | Takes `action` (`set`, `add`, `remove`), `object` and optional `assignments` (`key=value` strings), `from`, `parent`, `scene`. Removal requires `confirm: true`. Saves the scene file. |
| `scene_diff` | Compares with an earlier scene file named by `against`, inside the game. |
| `game_run` | Plays headlessly with `frames` (default 60, maximum 600), `seed`, `input`, `assert`, `until`, `print`, `every`, `scene`. |
| `game_check` | Checks scripts, scenes and assets; does not run the game's test files. |
| `game_shot` | Returns a PNG picture of the saved game at `at` seconds (default 1), `width` pixels wide (default 512, 16:9), optionally through another `camera`. |
| `game_query` | Asks the playing game: `inspect` an `object`, `raycast` `from` one point `to` another, or `overlap` a sphere `at` a point with a `radius`. |
| `api` | Reads the API index, a `class`, or a `search`. |
| `events` | Reads this server's log history with `since`, `limit` (default 20, maximum 32), `level`. |

`game_run` expressions run in the game after each frame and can use anything scripts can, for
example `print: "Scene.find('Player').transform.position"` or
`assert: "Physics.raycast(Vector3.new(0, 5, 0), Vector3.down)"`.

`game_query` answers in short lines with rounded numbers. `inspect` gives the object's world
position and rotation, its body, velocity and whether a character is grounded, and only those
props that scripts changed since the scene loaded. Points are `x,y,z` strings in meters. Without
an editor in Play, it plays the saved game for `frames` (default 60) first, as `game_run` does:

```json
{"action": "inspect", "object": "Player"}
{"action": "raycast", "from": "0,5,0", "to": "0,-5,0"}
{"action": "overlap", "at": "4,1,-3", "radius": 1.5}
```

```text
after 60 frames: #5 Player pos=0,0.8,0 body=character vel=0,-0.33,0 grounded
after 60 frames: 2 hits, nearest first:
#5 Player at 0,1.62,0 normal=0,1,0 dist=3.38
#0 Ground at 0,0,0 normal=0,1,0 dist=5
```

`game_shot` draws in a separate `redgem shot` process with a hidden window. A graphics driver
that crashes takes only that process down, and the call reports it. It needs a display. Without one (no `DISPLAY` on Linux)
it returns a tool error that says so, and the server keeps answering. Some MCP clients start
servers with only a few environment variables; pass `DISPLAY` in the server's configuration
if pictures are wanted. Prefer `game_run` to find out whether something works and use pictures
for how it looks.

Invalid arguments come back as tool errors (`isError: true`) naming the argument and what it
accepts, such as `frames: must be at most 600, not 601`, so the model can correct the call.
Unknown or disabled tools remain JSON-RPC errors.

`--read-only` exposes only `scene_list`, `scene_get`, `scene_diff`, `api` and `events`; the
`game_*` tools execute the game's scripts and are disabled. It also
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
With `--editor auto`, check `editor` `status` to know whether calls reach an editor.

## Live editor

Start the editor with its bridge explicitly enabled:

```sh
redgem /path/to/game --editor-bridge
```

The terminal prints the path of a private `connection.json` in the user's RedGem data folder.
Let the MCP server find it on every call, so one client configuration survives editor restarts:

```sh
redgem mcp /path/to/game --editor auto
```

With `auto`, each call uses the newest running editor of that game. When none answers, the
scene tools, `game_query` and `events` work on the saved files and this server instead, and
their answer starts with `(no editor is running: answered from the saved files)`; the `editor`
tool returns an error saying how to start an editor with its bridge. A connection file whose
port refuses connections belongs to an editor that is gone and is deleted. A listener that does
not prove itself (below) is skipped for a minute, so it delays at most one call a minute by about
a second. To pin one editor instance, pass its file:

```sh
redgem mcp /path/to/game --editor /path/to/connection.json
```

If that editor exits, calls fail with an error that says how to restart it. A pinned editor
always answers scene edits live, so its `scene_edit` schema leaves out `from`.

With `auto`, an `events` cursor belongs to whichever process answered: the editor, or this server
when no editor is open. After an editor opens or closes, start again from `since: 0`.

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
| `frame` | Point the editor camera at `object` (selecting it), or at the selection. |
| `undo`, `redo`, `save` | Apply the corresponding editor command. Saving is explicit. |
| `play`, `pause`, `resume`, `stop` | Control the editor's game, using its normal input, audio and viewport. |
| `step` | Advance a paused game by exactly one fixed frame, keeping it paused. |
| `capture` | Return the last completed viewport as an MCP PNG image, without writing a file. |

`capture` accepts `max_size` (default 512, range 128–1024), the longest output side. Images are
reduced further when necessary for the reply, with the PNG as base64 text, to stay within the
bridge's 256 KiB limit (about 188 KiB of PNG). Only the scene
viewport is captured, never the desktop or other applications. `object` is accepted only by
`select` and `frame`; `max_size` only by `capture`. Read-only mode permits `status` and `capture` only.

Edits and document commands are refused while playing, during gestures/dialogs, or with an
external file conflict pending. Play starts a copy of the unsaved scene; Stop discards that
runtime and restores the edited document. Pause/resume/stop/step control that runtime, not the
file. Read operations still describe the edited document, not runtime-mutated objects. An
explicit `scene` must match the open scene; requests never switch or discard scenes.

`events` reads the editor's log history. `game_query` asks the game the editor is playing (the
runtime, not the edited document); it refuses `frames` there, so pause and `step` to advance.
While editing, it reports that the editor is not playing; it does not fall back to the saved
files, which may differ from the open document. `api`, `game_run`, `game_check` and
`game_shot` remain ordinary CLI tools: they read saved files rather than the unsaved editor scene.
Arbitrary live eval is not exposed.

The bridge binds only to IPv4 loopback and requires a random credential on every request, with
the game directory checked at both ends. Its connection file stays outside the repository and
is removed on clean shutdown. On POSIX it is owner-only inside an owner-only directory; on
Windows it inherits the user's profile access controls. Treat the file as a credential granting
control of that editor. MCP read-only and allow-list options restrict that client's calls.

At most four connections are handled at once, on the editor thread between frames. Requests
are limited to 64 KiB and replies to 256 KiB. Transport waits expire after five seconds. If an
edit times out, it may have completed: inspect status and the object before retrying. A pinned
connection (`--editor connection.json`) never falls back to the files.

Before it sends the credential or a request, the client sends a random nonce, and the editor must
answer within one second with an HMAC-SHA256 of it keyed by the credential. A program that took a
crashed editor's port therefore receives only that nonce. Until that proof, a failure means no
editor took the call; after it, any failure (a timeout, or a reset after the request went out)
fails the call. It is never retried elsewhere or applied to the files, because the edit may
already have happened.

## Bounded results

Each request is at most 64 KiB with at most 16 JSON nesting levels. Ordinary tool text is limited
to 8 KiB with a notice saying it was cut; narrow the scene list (`depth`, `limit`, then
`scene_get`) or the API query for more detail. Invalid arguments and a valid command that fails
both return `isError: true`, with what to change or the engine's diagnostic. Unknown or disabled
tools are JSON-RPC errors.

Event text starts with `cursor=N`, the value to pass as the next `since` (followed by `more`,
`dropped` or `cursor_ahead` when they apply), then one line per event, `#id level text`:

```text
cursor=2
#2 error scripts/coin.rb:12: Coin#update on 'Coin1': undefined method 'positon=' for Transform (NoMethodError)
```

Each event is its message's first line, led by the place in the game's files its backtrace names,
with the game's folder left out of paths. Each event's text is capped at 256 UTF-8 bytes and ends
with `...` when shortened. The cursor belongs to this server process; it is not a cursor into an
unrelated editor or game (with `--editor`, it belongs to that editor).
Events include engine log messages, not raw
standard output. Process history is bounded, so `dropped` identifies overwritten messages.

Requests run one at a time. A cancellation notification cannot interrupt a native command already
running; frame limits and the game's script budget bound headless work. A slow native operation
can still delay completion. Clients can terminate and restart a server that stops responding.

The protocol follows the official [stdio binding](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports/stdio)
and [tool messages](https://modelcontextprotocol.io/specification/2026-07-28/server/tools).
