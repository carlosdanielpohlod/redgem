---
description: "redgem mcp: MCP over stdio for one game, with scene tools, asset and template search, scene diffs against Git, headless runs with input replay, game_query, game_shot and game_eval, the live editor, bounded events and explicit permissions."
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
| `scene_edit` | Changes objects (see [Editing objects](#editing-objects)): `action` `set`, `add`, `duplicate`, `group` or `remove`, on `object` or several `objects`, with `assignments` (`key=value` strings), `kind`, `from`, `parent`, `scene`. Removal requires `confirm: true`. Saves the scene file. |
| `scene_diff` | Compares with an earlier scene file named by `against` (a `.glb` inside the game), or with the scene as a Git `revision` saved it (`HEAD`, `HEAD~1`, `main`, a commit hash). Takes one of the two. |
| `asset_search` | Finds the game's assets by the words of `query` in their name or folder, best match first: a line each with the kind and the path. `kind` keeps one kind (`model`, `texture`, `sound`, `script`, `prefab`: the folders `models/`, `images/`, `sounds/`, `scripts/`, `prefabs/`), `tag` the assets inside a subfolder of that name; `limit` defaults to 20 (`0` for all). |
| `template_search` | Finds the templates new games start from by the words of `query` in their name or description. |
| `game_run` | Plays headlessly with `frames` (default 60, maximum 600), `seed`, `input`, `assert`, `until`, `print`, `every`, `scene`. `input` replays keys, the mouse, gamepads and touch (see [Repeating a headless session](assistants.md#repeating-a-headless-session)). |
| `game_check` | Checks scripts, scenes and assets; does not run the game's test files. |
| `game_shot` | Returns a PNG picture of the saved game at `at` seconds (default 1), `width` pixels wide (default 512, 16:9), optionally through another `camera`. |
| `game_query` | Asks the playing game: `inspect` an `object`, `raycast` `from` one point `to` another, or `overlap` a sphere `at` a point with a `radius`. |
| `project` | Reads and changes the game beyond its scenes, as files (see [The project](#the-project)): `action` `settings`, `set_settings`, `texts`, `set_texts`, `story`, `set_story`, `scenes`, `new_scene` or `new_game`. |
| `build` | Makes a standalone game from the saved files, on a thread of its own (see [Building the game](#building-the-game)): `action` `start` or `status`. |
| `terrain` | A terrain's ground layers and the rules that paint them, as files (see [Terrain layers](#terrain-layers)): `action` `layers`, `set_layers` or `paint`. |
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

`--read-only` exposes only `scene_list`, `scene_get`, `scene_diff`, `asset_search`,
`template_search`, `api`, `events` and `project` with its reading actions (`settings`, `texts`,
`story`, `scenes`); the
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
editing its files this way. Keep an earlier save and inspect `scene_diff` after an edit, or, in a
game kept in Git, compare with the last commit (`revision: "HEAD"`).
`from` is available only in file mode; `parent` also works when adding a live empty object.
With `--editor auto`, check `editor` `status` to know whether calls reach an editor.

## Editing objects

`scene_edit` changes the scene the same way on the files and in a live editor:

| Action | What it does |
|--------|--------------|
| `set` | Sets `assignments` on `object`, or on each of `objects`. |
| `add` | Makes one object named `object`: empty, of a `kind`, or a copy of `from` (with its mesh, children, scripts and properties; a copy of a template is no template), under `parent` when given. |
| `duplicate` | Copies `object` or `objects`, each with its children, named apart (`Crate.001`); `assignments` are set on each copy. |
| `group` | Makes an empty object named `object` (under `parent`, else where the first member is) and moves `objects` under it, keeping where they are in the world. |
| `remove` | Removes `object` or `objects` with their children. Needs `confirm: true`. |

`kind` is one of the GameObject menu's objects: `empty`, `cube`, `sphere`, `plane`, `cylinder`,
`capsule` (shapes with colliders), `sun`, `point_light`, `spot_light`, `camera`, `terrain` and
`environment` (one a scene; a second is refused). A name already taken gets a number.

The assignments are those of `redgem scene set` (see [the command line](command-line.md)):
`position`, `rotation` (degrees), `scale`, `name`, `parent`, `script+=`, `script-=`, `tint`, and
`prop.<key>`. The Inspector's cards are properties too: `prop.visible=false` hides an object,
`prop.tag`, `prop.prefab=true` (a template), `prop.body`, `prop.collider`, `prop.trigger`,
`prop.mass`, `prop.layer` and `prop.cast_shadow`. A light takes `light=sun|point|spot`,
`light_color=#rrggbb` (or `r,g,b`, each 0 to 1), `light_intensity` (watts, W/m² for a sun),
`light_range` (meters, 0 for the engine's own) and `spot_cone=inner,outer`; a camera takes
`camera_fov` (degrees, 5 to 150), `camera_near` and `camera_far` (0 for none).

```json
{"action": "add", "object": "Lamp", "kind": "point_light", "parent": "Hall", "assignments": ["position=0,3,0", "light_color=#ffcc88"]}
{"action": "duplicate", "objects": ["Crate1", "Crate2"], "assignments": ["prop.tag=spare"]}
{"action": "group", "object": "Crates", "objects": ["Crate1", "Crate2", "Crate1.001", "Crate2.001"]}
```

Objects are named as `scene_get` names them. Each answer gives a line per object it made or
changed, with its id, so the next call can name it by `#id`.

## The project

`project` works on the game's files, with or without an editor:

| Action | Result |
|--------|--------|
| `settings` | `game.json`, or only its `section` (such as `menu`), then the problems the game would have with it. |
| `set_settings` | Merges `changes` into `game.json`: a key replaces the file's, an object merges into its object, `null` removes a key. |
| `texts` | The texts of `locales/<language>.json` (`language` defaults to `en`) whose keys start with `prefix`, `limit` of them (default 50). |
| `set_texts` | Sets `texts` (`{"key": "text"}`) in that language; `null` removes one. |
| `story` | Lists the files of `kind` (`dialogue`, `quest`, `item`), or reads one by `name` (`npcs/baker`). |
| `set_story` | Writes `content` as the whole file of `kind` and `name`. Replacing a file needs `confirm: true`. |
| `scenes` | The names of the game's scenes (`main` is `scene.glb`). |
| `new_scene` | Makes `scenes/<scene>.glb` with a ground, a sun and a camera. |
| `new_game` | Makes a game from `template` (default `empty`) in a new folder `path`, next to this game's folder. |

```json
{"action": "set_settings", "changes": {"window": {"title": "Coast"}, "input": {"actions": {"jump": ["key:space", "pad:a"]}}}}
{"action": "set_settings", "changes": {"menu": {"theme": {"colors": {"button": "#203040"}, "transition": {"kind": "fade", "seconds": 0.25}}}}}
{"action": "set_texts", "language": "pt-BR", "texts": {"hud.coins": "Moedas {coins}/{total}"}}
{"action": "set_story", "kind": "dialogue", "name": "npcs/baker", "content": {"start": "hi", "nodes": {"hi": {"speaker": "baker.name", "text": "baker.hi", "end": true}}}}
```

Every write is checked as `game_check` checks the game: `game.json` for its physics, input and
menu sections (the menu theme's keys and values, and the fonts, pictures and sounds it names; see
[Giving the menu your game's look](scripting/gameplay.md#giving-the-menu-your-games-look)), the locales, dialogues and quests for what they name of each other. A write that brings an
error the game did not have is refused, with the errors, and the file stays as it was. The
warnings it brings, such as texts a new language still lacks, come with the answer. An item is
checked only through the quests that name it. The
`changes`, `texts` and `content` objects are free-form; the checks, not the schema, judge them.

## Terrain layers

A terrain object blends up to eight ground layers when it has `prop.terrain_layers=<name>`: the
layers file `terrains/<name>.json` (see [Terrain](levels.md#terrain)). `terrain` reads and writes
those files and paints them:

| Action | Result |
|--------|--------|
| `layers` | The game's layers files; with `name`, that file; with `object`, the file the object names. |
| `set_layers` | Writes `content` as `terrains/<name>.json`. A file the game could not read is refused, and nothing is written. Replacing a file needs `confirm: true`. |
| `paint` | Paints the `object`'s layers by the rules of its file, on its saved mesh, and writes the weights beside the file (`terrains/<name>.splat0.png`), as `redgem scene paint` does. |

```json
{"action": "set_layers", "name": "beach", "content": {"layers": [{"name": "sand", "albedo": "textures/sand/Color.jpg"}, {"name": "grass"}], "rules": [{"layer": "sand"}, {"layer": "grass", "height": {"min": 2, "soft": 1.5}}]}}
{"action": "paint", "object": "Ground"}
```

Name the layers with `scene_edit` (`prop.terrain_layers=beach`) and save the scene before `paint`:
it works on the saved files. `--read-only` keeps `layers`.

## Building the game

`build` makes the game a player can run, as the editor's Build menu does: `start` returns at once,
and `status` says how it goes.

```json
{"action": "start", "target": "android", "format": "bundle"}
{"action": "status", "wait": 25}
```

| Argument | Meaning |
|----------|---------|
| `target` | `linux`, `windows`, `android` or `macos` (default: this system; a Mac build only on a Mac). |
| `format` | `apk` (default) or `bundle` (an App Bundle for Google Play), for Android. |
| `loose` | `true` keeps the game's files in a `game/` folder instead of `game.rgpak`, to look into a build. |
| `folder` | The folder next to the game it goes into; default `<game>-build`, or `<game>-build-<target>` for another system. |
| `wait` | For `status`: how many seconds to wait for the build to end first (up to 25; call it again for a longer build). |

`status` gives the build's step and time while it runs. Once it ended, it gives what it made (the
executable, the app, the APK or bundle) or why not, with the last lines the build logged; these
also reach `events`. A build works from the files on disk: save the editor's edits first. It
replaces only what an earlier build wrote in that folder. One build runs at a time.

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

If that editor exits, calls fail with an error that says how to restart it.

With `auto`, an `events` cursor belongs to whichever process answered: the editor, or this server
when no editor is open. After an editor opens or closes, start again from `since: 0`.

Scene listing, object details and scene comparison now read the editor's unsaved document.
`scene_edit` works there as on the files. Every call is one undo step: invalid assignments or
exceptions roll it back without losing earlier undo/redo. Changes update the viewport, select
what the call made or changed, and remain unsaved until `save`.

The `editor` tool takes these actions:

| Action | Result |
|--------|--------|
| `status` | Scene, play state, dirty flag, selection, undo/redo, external-conflict state, and whether a copy of an earlier session's unsaved edits waits for an answer (`recovery_pending`: the editor offers it when no other dialog is open). |
| `select` | Select the requested `object`, or every one of `objects`. |
| `frame` | Point the editor camera at `object` (selecting it), or at the selection. |
| `undo`, `redo`, `save` | Apply the corresponding editor command. Saving is explicit. |
| `play`, `pause`, `resume`, `stop` | Control the editor's game, using its normal input, audio and viewport. |
| `step` | Advance a paused game by exactly one fixed frame, keeping it paused. |
| `capture` | Return the last completed viewport as an MCP PNG image, without writing a file. |
| `open_scene` | Open the game's `scene` by name. A scene with unsaved edits is refused: `save` first, or pass `discard: true` to drop them. |
| `save_as` | Save the open scene as `scenes/<scene>.glb`, which is then the scene edited. |

`capture` accepts `max_size` (default 512, range 128–1024), the longest output side. Images are
reduced further when necessary for the reply, with the PNG as base64 text, to stay within the
bridge's 256 KiB limit (about 188 KiB of PNG). Only the scene
viewport is captured, never the desktop or other applications. `object` is accepted only by
`select` and `frame`, `objects` only by `select`; `max_size` only by `capture`. Read-only mode permits `status` and `capture` only.

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
Evaluating Ruby in the playing game is a separate opt-in (below).

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

## Ruby eval in the playing game

A development build of `redgem` (one built with its tests, as from source: the CMake option
`REDGEM_RUBY_EVAL` follows `REDGEM_BUILD_TESTS`) can evaluate Ruby inside the game an editor
plays. Release builds, and so the downloads, leave it out entirely: no option, no tool, none of
its code. Start the editor with it:

```sh
redgem /path/to/game --ruby-eval
```

`--ruby-eval` enables the bridge (as `--editor-bridge` does) and prints a line
`ruby eval token: <64 hex digits>`, new each time the editor starts. Connected to an editor, the
MCP server lists `game_eval`, which takes that `token` and `code`:

```json
{"token": "<the printed token>", "code": "Scene.find('Player').transform.position"}
```

The answer is the result's `inspect`, or the exception with its backtrace (the snippet's lines
are `(game_eval):<line>`). The code runs at the top level of the game's interpreter, between two
frames, so it sees and changes what the game's scripts see: the game is not reset afterwards.
It runs on the game's script budget, capped at 0.4 seconds (so that it always answers before the
bridge's five-second wait): code that never returns ends with a `ScriptTimeout`, as any script
call would. The result's `inspect` runs within the same budget. Code is limited to 16 KiB, and the answer is cut at
8 KiB with a notice, as any tool's text (see below).

It is refused, without running anything, when the token is missing or wrong, when the editor was
not started with `--ruby-eval` (the error says how to start it), and while the editor is editing
rather than playing (start the game with `editor` `action=play`). Read-only mode leaves the tool
out. The bridge's protections apply as well: IPv4 loopback only, and the connection's credential.

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
