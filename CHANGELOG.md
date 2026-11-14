# Changelog

What changed in each release of RedGem. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Every release so far is experimental:
until 1.0, a release may change what games rely on (see
[KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md)).

## [Unreleased]

### Added

- **Asset and template search** for assistants: `redgem assets <game> [words] [--kind
  model|texture|sound|script|prefab] [--tag name] [--limit N]` lists the game's assets whose name
  or folder has every word, best match first (a name that is the word, then one that starts with
  it, holds it, a subfolder tag of that name, and the rest of the path); `redgem templates
  [words]` does the same for the templates new games start from. MCP has both as the read-only
  tools `asset_search` and `template_search`.
- **Scene diff against Git**: `redgem scene diff <game> --against <revision>` (or `--revision`;
  `HEAD`, `HEAD~1`, `main`, a commit hash) compares the scene with the one that commit saved,
  read through `git show`; MCP's `scene_diff` takes the same as `revision`, and a connected editor compares
  its open, unsaved document with it. A missing revision, or a game in no Git repository, is an
  error that says so.

- **Benchmarks and a server load test with thresholds**: `redgem_bench` times the hot paths that
  need no window (script updates, the physics step, loading a `.glb`, the network frame, JSON
  saves, HTTP parsing) and reports each one's median and 95th percentile; `redgem_server_load` fills
  a dedicated server's rooms with bots and reads its status port for its frame time, bytes a second
  and dropped clients. A new `Benchmarks` CI workflow runs both on Linux, keeps the results as an
  artifact, and fails only when a result clearly passes its ceiling in `tests/bench/thresholds.json`
  (see the contributing guide).

- **Mirrored objects**: an object placed with a negative scale on one axis (or three), as
  Blender's mirror leaves it, is drawn the right way out in every pass (the lit picture, the
  sun's shadow cascades, the point and spot shadow atlas, motion vectors and see-through
  surfaces). Mirrored copies of a mesh are batched apart, so the winding changes only between
  runs of them. A node given as a mirroring matrix is read with a negative x scale.

- **Duplicated characters animate**: duplicating an object in the editor copies the channels of
  every clip that moves it onto the copy, so a duplicated character plays the original's clips
  (Wave, Walk...) instead of standing in the pose it was copied in; the copy itself stays where
  it is put. Undoing the duplicate drops them again.

- **Shape keys (morph targets)**: a mesh's shape keys are read from glTF with their names, starting
  weights, position and normal deltas, and clips' `weights` channels play them. The vertex shader
  applies them as it does bones: every part's deltas in one float texture and, per drawn copy,
  its eight heaviest keys in the frame's texels, so copies drawn in one call each show their own
  weights (lit, shadow, motion and see-through passes alike, desktop and OpenGL ES). Scripts set
  them by name: `face.morph(:Smile, 0.7)`, `face.morph(:Smile)`, `face.morph_names`. Imported
  models keep their shape keys in the editor.

- **Dedicated server status**: `redgem-server --status-port <port>` answers a local HTTP GET, or
  a bare TCP connection, with one JSON document: rooms open and the limit, players per room and
  in total, frame time (average and worst over the last five seconds, and the budget), bytes a
  second in and out, the threads, uptime and the engine's version. It listens on `127.0.0.1`
  unless `--status-bind` names another address, holds counts only (no room codes or players),
  and never holds up the server: few connections, small requests, short deadlines.
  `--status-log <seconds>`, and `SIGUSR1` on Linux and macOS, write the same summary to the log.
- **Three more templates** for `redgem new --template` and the project hub: `third-person` (a hero
  that walks the way the camera looks and jumps, the camera orbiting behind it with the right
  mouse button or the right stick), `top-down` (a hero under a tilted camera, walked by WASD or the
  left stick, or sent to a spot by a left click) and `menu-only` (a title screen, the engine's
  settings screen and controls page, and credits that roll in a scroll list; no 3D play). Each
  comes with its own tests in `tests/`, which `redgem check --run-tests` runs and which pass as
  the game is made.
- **A CI example for games**: every new game comes with `.github/workflows/test.yml`, a GitHub
  Actions workflow that downloads the latest RedGem for Linux and runs `redgem check --run-tests`
  on pushes to main and on pull requests (see docs/testing.md). The files every new game gets live in
  `templates/_shared/`.

- **Replaying the mouse, gamepads and touch**: an event of `redgem run --input` (and MCP
  `game_run`'s `input`) may now move the mouse, hold, click and release its buttons and turn its
  wheel (`mouse`), hold buttons and push sticks and triggers of gamepads 0 to 3 (`pads`), and put
  a finger down, move it and lift it, or tap (`touch`, `tap`). Points are in screen pixels, or
  fractions of the screen with `normalized`. It all goes through the input the game reads, so
  `Input.mouse_*`, actions bound to `mouse:left` or `pad:a`, and gamepad reads see it as a
  player's.
- **Navigation**: `Navigation.path(from, to)` finds the way around walls, along floors and up
  stairs and gentle slopes: Vector3 waypoints pulled straight, or nil when there is no way there.
  `Navigation.nearest(point)`, `Navigation.walkable?(point)`, `Navigation.draw` (the walkable cells
  around a point, for a frame), `Navigation.bake` and `Navigation.stats`. The walkable area is a
  2.5D grid baked from the scene's static bodies as each scene loads (or when a script first asks),
  sized by game.json's `navigation` section (`cell_size`, `agent_radius`, `agent_height`,
  `max_step`, `max_slope`). Bakes and searches are bounded; moving bodies are not obstacles yet
  (see [KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md)).

- **Camera switching**: `Camera.main = camera` sees the game through another camera object of the
  scene (security cameras, a car's cameras, a cutscene's shots), and `Camera.main = nil` goes back
  to the scene's first. `Camera.cut_to(camera, fade: seconds)` cuts through black: the picture
  darkens over the first half, the cut happens at the darkest frame, and it comes back over the
  second half, also while the game is paused, under the UI; `Camera.cutting?` says whether one is
  playing. A camera switched away from keeps its follow or orbit for when it is back. The
  view natives (`screen_ray`, `world_to_screen`, `fov`) and the helpers (`follow`, `orbit`,
  `shake`) act on the camera switched to. A scene loaded anew starts at its own first camera.
  Temporal anti-aliasing and motion blur take any switch for a cut, however near the cameras are,
  so nothing of the last shot smears into the next.
- **Parents from scripts**: `Scene.spawn(..., name:, parent:)` names the copy (so `Scene.find`
  finds it) and puts it under an object, keeping its place in the world. `GameObject#parent =`
  moves an object under another one at runtime, keeping its place in the world, or at the root
  with `nil`; `set_parent(object, keep_world: false)` keeps its local transform instead. An object
  never goes under itself or what it carries, nor under a template, and a vehicle's wheel stays on
  it; a part taken out of a spawned copy leaves that copy's animation. A kinematic body under a parent
  goes where it takes it; a dynamic one keeps its own motion, and is taken to its new place
  (stopped, as when a script sets its position) when `keep_world: false` moves it.
- **Packed games**: `redgem build` packs a game's scripts, scenes, pictures, sounds, music, fonts,
  `game.json` and translations into one file, `game.rgpak`, beside the executable (inside the Mac
  app, and as one asset of the Android app), compressed where that makes a file smaller. The
  player reads every file of the game out of it (raylib's own loaders and music streams included)
  and refuses a damaged or tampered archive with a reason (a file's checksum is checked when it
  is read). `--loose` keeps the files loose in a
  `game/` folder, as before, to look into a build. The editor's Play and `redgem run` still read
  the project folder. The format is documented in `docs/shipping.md`, and the reader is fuzzed.

- **Typing on Android**: while a `UI.text_field` is drawn, the phone's on-screen keyboard is up;
  it goes when no field is drawn (the field or its menu closed, the scene changed) or when the
  back key puts it away, and a tap on the field brings it back. What it types reaches `Input.text`
  and the text field like a desktop keyboard's letters, and its Backspace and Enter reach
  `Input.pressed?`. `UI.keyboard_visible?` says whether it covers the bottom of the screen, for a
  layout that moves the field above it.

- **Inspector collision layers**: assign a body or trigger to a layer defined in `game.json`
  from the Physics card.

- **UI at any screen size**: `anchor:` on `UI.text`, `rect`, `image`, `button`, `text_field`,
  `toggle`, `choice` and `slider` (`:top_left`, `:top`, `:top_right`, `:left`, `:center`,
  `:right`, `:bottom_left`, `:bottom`, `:bottom_right`) makes x and y offsets from that point of
  the screen, with the matching corner of what is drawn there, so a HUD keeps to its corner at
  every resolution. `UI.anchored` gives the box's top-left corner for a layout of a game's own.
- **Word wrap**: `UI.text(..., width:)` wraps at word boundaries (a word longer than a line is cut
  between whole UTF-8 letters), and every text breaks at its own `"\n"`; `UI.text_height`,
  `UI.wrap` and `UI.line_height` measure and lay out dialog boxes and credits.
- **Scroll lists and clipping**: `UI::ScrollList` draws rows in a box that scrolls with the mouse
  wheel, Page Up and Page Down, a gamepad's bumpers and right stick, and follows a `UI::Focus` to
  keep the focused row in view. `UI.clip { }` cuts what its block draws to a box (and the pointer
  outside it is over nothing in it). The menu's controls page scrolls with the wheel too.

- **Sound handles**: `Audio.play(name, loop:, volume:, pitch:, group:)` returns the sound's id (or
  `nil`), in the same id space as `Audio.play_at`, so `Audio.stop(id)`, `Audio.playing?(id)` and the
  new `Audio.set(id, volume:, pitch:)` work on flat sounds too: a looping hum, an alarm that stops.
  Each play is a voice of its own, so plays of one sound overlap instead of restarting it.

- **Music crossfade**: `Audio.music(name, fade: seconds)` fades the new track in while the one
  playing fades out, and `Audio.stop_music(fade:)` fades it out.

- **Mix groups**: every sound plays in `:music`, `:effects` (the default) or `:voice`
  (`group:` on `Audio.play` and `Audio.play_at`). `Settings.music_volume`, `effects_volume` and
  `voice_volume` are the player's volume of each, kept with the master volume, and the settings
  screen has a slider for each.

- **Editor display scaling**: monitor-aware text and interface spacing, native Retina windows,
  and stable font handles while moving between monitors.

- **The whole keyboard**: F1-F12, Insert, Delete, Home, End, Page Up and Page Down, the keypad
  (`:kp_0` to `:kp_9`, `:kp_enter`, `:kp_add`...), Caps, Scroll and Num Lock, Print Screen, Pause,
  Menu, Right Alt, the Super keys, and `;` `=` `[` `]` `\` and the grave key, for games and for
  players rebinding, each with a label on the controls page.

- **Mouse bindings**: `mouse:left`, `mouse:right`, `mouse:middle`, the side buttons (`mouse:side`,
  `mouse:extra`) and the wheel (`mouse:wheel_up`, `mouse:wheel_down`, a press on each frame it
  turns) trigger actions and axes from `game.json`, `Input.define_action(..., mouse: [...])` and
  `Input.bind(..., mouse: ...)`. `Input.capture_next` and the controls page catch them too; the
  click that starts a capture is never taken for the binding, and the click caught clicks nothing
  else. `mouse: false` leaves the mouse out of a capture.

- **Live script reload**: saved plain behaviour classes update during editor Play while retaining
  instance fields and physics state; unsupported setup keeps its previous code until a restart.
- **Physics collider view**: inspect actual collision shapes during editor Play/Pause, with
  colored wireframes, trigger and character shapes, and bounded geometry collection.

- **Scene recovery**: periodic durable copies of unsaved editor scenes, with a recovery prompt,
  original-file conflict detection and cleanup on Save or Discard.

- **Prefab creation**: save a selected hierarchy as a reusable GLB from the Project panel,
  preserving complete skeletons and nested references while validating dependencies before publication.

- **Prefab placement**: double-click or drag files from the Project panel into the scene or
  hierarchy to create linked instances, with dependency validation and undo/redo.

- **Prefab previews in the editor**: inherited geometry and components appear while editing,
  with reference selection, cached placement edits and preserved links on save.

- **Release downloads from CI**: a `release-*` pull request, a pushed `v*` tag or a manual run
  builds all four downloads on pinned toolchains and checks each one (contents, and its programs
  run on its own system). A tag's run attaches them to the tag's draft release, all four or none.

- **Spot cones**: spot lights shine within their glTF inner and outer cone angles, fading
  smoothly between them, on surfaces and on lit particles (they used to light every way).
- **Spot light editing**: adjust the inner and outer beam angles in the Light card and save them
  with the scene.
- **Shadows from point and spot lights**: a light whose object sets `cast_shadow = true` casts
  shadows (off unless said, so existing scenes draw as fast as before). Of those, the ones that
  matter most to the camera cast them each frame, up to the environment's `local_shadows` (0 to
  4, at most two of them point lights): a spot draws one perspective view, a point light the six
  faces of a cube, all into one depth atlas, filtered like the sun's with a normal offset and a bias that follows the texel size. The
  quality presets cap them (ultra 4, high 2, medium 1, low 0; `local_shadows` in a custom quality
  and `Settings.quality_details`), and a graphics card that cannot make the atlas draws none.
- **Web requests follow redirects**: `HTTP` follows 301, 302, 303, 307 and 308, up to 10 of them,
  reading a relative `Location` against the address that sent it. A 303, and a 301 or 302 to a
  POST, go on as a GET without the body; 307 and 308 keep both. A redirect from `https://` to
  `http://` is refused, every hop is checked as the first address was (`http://` only nearby, no
  metadata address), and a hop to another origin drops `Authorization`, `Cookie` and
  `Proxy-Authorization`. A `Location` with spaces or UTF-8 is percent-encoded. `response.url` is
  where the answer came from; `redirects: false` returns the redirect itself.
- **Compressed web answers**: requests send `Accept-Encoding: gzip, deflate`, and gzip or deflate
  answers (chunked or not) reach the block decompressed with zlib 1.3.2, the 8 MB limit applying
  to the decompressed body (a decompression bomb is refused). A corrupt or cut body is an `error`,
  with the answer's status and headers kept.

- **Linux AppImage**: `redgem-linux-x86_64.AppImage`, a fifth download beside the tarball, holds
  the same editor, tools, players and demos in one file with the RedGem menu entry and icon. Run
  it as `bin/redgem`; name another program first (`redgem-server --help`) to run that one.
- **Crash reports on Android**: a built game for Android writes its crash report in `crashes/` of
  the app's files and in `adb logcat` (tag `redgem`), and hands the crash on to the system, which
  keeps its own tombstone too. The next run of any built game says in its log that the run
  before crashed, naming the report.
- **`redgem symbolize <report> [--symbols <dir>]`**: a crash report's frames with their functions
  (and file:line where the symbols have them), looked up with `llvm-addr2line` or `addr2line`.
  Every report now names the build of the player that crashed (`Build: <id> <program>`, the
  linker's build id on Linux, Windows and Android), and the symbols of that very build are found
  by it; a report of another build is said to be so, with the builds found. Each release carries
  the players as they were linked, before stripping, as `redgem-symbols-linux.zip`,
  `redgem-symbols-windows.zip` and `redgem-symbols-android.zip`, apart from the downloads.

### Changed

- **Game connections are encrypted** (TLS 1.2 or 1.3, no renegotiation), to a player hosting, a
  relay or a dedicated server alike, and a machine that talks in the clear is let go: games of
  this version cannot play with earlier ones, nor use an earlier relay or server. A handshake not
  over in 3 seconds is let go without holding up anyone else. The relay and the server make a TLS
  identity the first time they start and keep it in their data folder (`redgem-relay --data`,
  `./redgem-relay-data` by default; the server's `--data`), print its fingerprint when they start,
  and show it with `--fingerprint`. A game pins it with `fingerprint:` on `Network.join` and
  `Network.host(room: true)`, or for every join with `"servers"` in `game.json`, and refuses any
  other machine. A player hosting proves itself with an identity made for the game, whose short
  code `Network.host_code` (and whole `Network.fingerprint`) the others join with. Without a
  fingerprint the connection is encrypted but whoever answers is believed. `docker compose up
  relay` keeps the relay's identity in a volume.

- **The Linux download runs on glibc 2.35 or newer** (Ubuntu 22.04, Debian 12, Fedora 36 and
  later) instead of 2.39: it is built on Ubuntu 22.04 with libstdc++ linked in, and so are the
  players inside it, so games built for Linux run on those systems too. The release workflow
  measures the floor and runs both Linux downloads on a bare Ubuntu 22.04.
- **Crash report backtraces on Linux and Android are offsets into each program**
  (`Arena+0x1a2b`), as on Windows, so that `redgem symbolize` reads them; where the loader knows
  a frame's symbol, it follows in brackets.
- **Each game keeps its data in a folder of its own, named after its studio and title**:
  `game.json` gains `"studio"`, and a game's save, settings, bindings and crash reports are kept
  together in `<studio>/<title>/` where the player's system keeps programs' data
  (`$XDG_DATA_HOME` or `~/.local/share` on Linux, `%APPDATA%` on Windows,
  `~/Library/Application Support` on macOS, the app's files folder on Android), as `save.json`,
  `settings.json`, `bindings.json` and `crashes/`. The names are made safe folder names on every
  system; a game with no studio goes under `RedGem Games`, and a build warns of a game with no
  studio, no title or the `"My Studio"` every new game starts with. `redgem new`
  writes `"studio": "My Studio"`. The editor's Play and `redgem play` keep theirs in
  `game-data/<studio>/<title>/` of the RedGem folder and `redgem-test` in `test-saves/`, never in
  the players' folder; a built game no longer reads RedGem's own `settings.json`. **Saves,
  bindings and display settings kept under `redgem/saves`, `redgem/bindings` and RedGem's
  `settings.json` are not moved**: a game starts from nothing in its new folder, and the old
  files stay where they were. The `id` no longer names the save.
- **`Audio.play` returns an id or `nil`** instead of `true` or `false`; a call that ignores it, or
  only tests it, works as before. Its pitch must be from 0.01 to 8, as `play_at`'s.
- **`Audio.music` with a missing file** leaves the track playing as it was, instead of stopping it.
- **`Audio.stop_sources`** stops every sound with an id, flat ones included. A game's sounds stop
  when it ends (the editor's Stop included), and so do its player's group volumes.
- **The network tests no longer time the real clock**: the relay, a dedicated server's turns and
  rooms, and HTTP timeouts read time from a small `Clock` (`core/clock.hpp`, the steady clock by
  default), which tests replace with a fake one they move by hand. A server room thread now stops
  within one turn's wait instead of at once.

### Fixed

- **Android build staging** uses a private per-user cache. Existing folders next to the APK or AAB
  stay intact, and interrupted builds are cleaned up by later builds.
- **Adding a vehicle takes the physics locks in the order a step takes them**: the chassis's lock
  is released before the vehicle joins the simulation, so ThreadSanitizer no longer finds a
  lock-order inversion there and its suppression is gone (the ground for physics on several
  threads). The largest port, the bits in a byte and the byte mask are each defined once, and
  the port errors of `Network` and of HTTP addresses say the range from that one definition.
- **A very long script name no longer crashes `redgem run`, `shot` and the MCP server**: the
  error hints read at most the first 512 bytes of a message, so the standard library's matcher
  no longer runs out of stack on a 100 000-character name. A line number too big for a file no
  longer throws.
- **`Game.load_scene` with a name too long for the system** raises `no scene named ...` instead
  of throwing a C++ exception through the interpreter and ending the game (or the editor's Play).
- **Past 4096 physics bodies** an object gets no body, as before, but the engine now logs one
  error naming the limit instead of saying nothing. The limit is listed in KNOWN_LIMITATIONS.md.
- **A game that spawned tens of thousands of objects with bodies no longer hangs on close**: the
  script budget did stop the spawning loop, but taking the physics world apart looked at every
  object for each object destroyed. Destroying an object now only looks at the triggers.
- **`redgem scene set` and MCP `scene_edit` refuse placements a scene cannot hold**: a position,
  rotation or scale past ±1 000 000 (the same reach as `game_query` points) is refused with the
  range. Before, `1e40` was saved as `null`, which is not valid glTF, and the object moved to the
  origin. A scale below 0.001 (the Inspector's smallest), zero or negative is refused too.
- **Usage errors exit with 2 everywhere**: `redgem mcp` with no game, a folder that is not there
  or an unknown `--allow` tool exited 1. `redgem <word>` naming no folder (a mistyped command)
  now says so, with the usage, instead of failing to parse `<word>/scene.glb`.
- **The editor bridge names a pending external change**: once the scene changed on disk while
  the editor had unsaved edits, every MCP action said "editor is busy". It now says the scene
  changed on disk and has to be resolved in the editor.
- **Saves no longer write through a planted link**: a game folder carrying a symbolic link named
  like a file being written (`scene.glb.tmp`, `*.redgem-writing`, `*.part`) made the editor's Save,
  `redgem scene set|add|remove`, MCP `scene_edit` and the agent's file writes overwrite the file
  the link pointed to, anywhere on the disk. Every write-then-rename now makes its temporary file
  new (`O_CREAT|O_EXCL|O_NOFOLLOW`; `CREATE_NEW` on Windows) and removes a link in its way instead
  of following it.

## [0.5.0] - 2026-10-31

### Added

- **Agent panel**: a chat with a coding agent in the editor, through the Agent Client Protocol.
  Claude Code and Codex come as presets, run by their open-source adapters
  (`@agentclientprotocol/claude-agent-acp`, `@agentclientprotocol/codex-acp`), and you can add your
  own agents. The panel streams the answer as markdown and shows tool calls with their diffs, the
  agent's plan, permission questions (allow once, allow always, reject) and a Stop button. The
  selected objects can be attached as context. The agent works in the game's folder: the editor
  reads and writes files for it only there. It also gets the editor's live MCP tools, so it can
  list and move objects, play, read errors and look at the viewport. Each game's past conversations
  are listed, and the last one comes back when the editor opens the game again.
- **MCP `game_query` and `game_shot`**: inspect an object's live state (world pose, body,
  velocity, grounded, props changed while playing), raycast between two points or overlap a
  sphere, in the editor's running game or after headless frames; and a small PNG picture of the
  saved game, returned as MCP image content.
- **MCP editor discovery**: `redgem mcp <game> --editor auto` finds this game's running editor on
  every call, skipping stale connection files, and works on the files when none is open. The
  `editor` tool gains `frame`.
- **Ruby colors in the script tabs**: keywords, strings and their `#{}` interpolations, symbols,
  comments, numbers, constants and instance variables are colored while typing, heredocs,
  `%w[]`, regexes and `=begin` blocks included.
- **Builds in the background**: Build in the editor no longer freezes the window while it works
  (a build for Android takes minutes). A box in the Scene view shows what is being built, for how
  long, and the step it is at (copying the game's files, packing the app, signing it), then how
  it ended: the folder it filled, with an Open folder button, or the error. One build runs at a
  time, and closing the editor during one waits for it to end, saying so.
- **Camera helpers**: `Camera.follow` (an offset in the world's or the target's axes, smoothing
  by half-life, look-at, a dead zone, and pulled in front of walls with a raycast),
  `Camera.orbit` (yaw, pitch and distance turned by the right mouse button or the right stick and
  zoomed by the wheel, within limits; `Camera::Orbit#turn` and `#zoom` for scripts) and
  `Camera.shake` (trauma that stacks up to 1 and fades over its duration, along seeded noise, on
  top of either without drifting the camera). The same at any frame rate; a destroyed target is
  let go; a camera with a parent is placed in the world.
- **`Camera.world_to_screen`**: where a point of the world is in the pixels the interface draws
  in, at any window shape and render scale (nil behind the camera), the inverse of
  `Camera.screen_ray`.
- **`Time.unscaled_delta`**: the frame's seconds whatever `Time.scale`, for what moves while the
  game is paused or slowed (an orbit the player turns goes by it).
- The platformer follows its hero with `Camera.follow`, shakes the camera when the hero falls off,
  and marks the coin nearest the camera with `Camera.world_to_screen`.
- **Named save slots**: `Save.slot` selects isolated progress while the default keeps the existing
  save; `Save.slots` lists known names. Local slots reuse durable files and backups, server rooms
  select their own SQLite views, and game-test restarts copy/reset named slots too.
- **Object checkpoints**: capture and restore local pose, tint and visibility through Save,
  with complete validation before writes and consistent physics teleports for moved hierarchies.
- **Persistent object lookup**: `GameObject.stable_id` and `Scene.find_by_id` reconnect saved
  references to scene-file nodes after renames or reordering, without aliasing live copies.
- **Immediate world diagnostics**: `Debug.line` and `Debug.box` draw bounded, frame-local wire
  primitives with scene depth or explicit x-ray, across player, editor Play and recordings.
- **Foreground frame profiler and statistics overlay**: opt-in real-clock phase timings, a bounded
  rolling summary and FPS panel shared by player, editor Play and recorded frames.
- **Prefab file loading**: reusable GLB references with nested instances, root property overrides,
  persistent descendant identities and shared geometry, available at runtime and in editor Play.
- **Asynchronous scene reads**: `Game.load_scene_async`, a shared bounded background parser,
  cancellation, last-error inspection and an immediate-mode loading-screen callback. The old
  world survives failed reads, while other rooms can keep ticking. Fixed-step offline runs have
  deterministic activation. Physics creation, script setup and GPU upload still happen on the
  owning thread and may hitch at activation.

### Changed

- MCP argument errors are tool results naming the argument and its range, so models can
  correct them; unknown tools stay protocol errors. Tool descriptions say what expressions and
  assignments look like, and `scene_edit`'s lists the words its assignments take (`tint=#rrggbb`
  among them).
- MCP events read as one line each, `#id level scripts/file.rb:line: message`, after the next
  cursor, instead of a JSON dump with absolute backtraces.
- MCP `events` answers in text alone, its cursor first.
- MCP `scene_list` shows 50 objects by default in file mode too, and `limit: 0` shows all.
- The editor bridge's wire version is 2: before a client sends the credential or a request, the
  editor proves with an HMAC of a client nonce that it holds the credential. Discovery deletes
  connection files whose port refuses and skips unproven listeners for a minute.
- A viewport capture is sized against what the bridge carries (the whole reply, the PNG as base64
  text included), drops the alpha channel the viewport does not use, and shrinks a quarter at a
  time from the original picture, so a 512-pixel request no longer comes back 256 wide.
- A script calling `puts`, `print` or `p` is told to use `Debug.log`.
- `Camera.screen_ray` follows the world's picture as the window shows it, render scale and
  recording included (presenters say the size they draw the world at).

### Fixed

- **The Agent panel's tools with a game opened by a relative path**: the editor keeps the game's
  folder absolute, so the agent's MCP server, started in the game's folder, finds it.
- **The agent's scene edits refused as "editor is busy"** while the cursor sat in the Agent
  panel's message box: typing to the agent no longer counts as using the editor.
- **Play pressed by an agent took the keyboard** from the message box the user was typing in:
  the Scene view gets the keyboard only when nobody is typing in a text field.
- **The Agent panel's transcript stopped following** an answer that kept growing after its first
  turn; it now follows while its very end is in view, and a view scrolled up stays where it is.
- **An agent command that is a missing file** was reported as "not found on PATH" (and run through
  `npx` when it named a package); it is now reported as a file that does not exist.
- **"Conflicting ID" warning on the toolbar**: the Hierarchy and Inspector toggles share an icon
  and no longer share an ImGui ID.
- `redgem shot` and MCP `game_shot` crashed when no display was available; they now report it.
  `game_shot` draws in a child process, so a renderer crash cannot stop the MCP server.
- MCP initialization answered `2025-06-18` and `2025-03-26` clients with an unrequested version.
- A missing game folder for `redgem mcp` is reported plainly instead of as a filesystem error.

## [0.4.1] - 2026-10-28

### Added

- **Complete editor MCP controls**: create empty objects and remove subtrees with atomic undo,
  play/pause/resume/stop, advance paused games one fixed frame, and return bounded PNG viewport
  images directly as MCP image content.
- **Live editor tooling**: `--editor-bridge` enables an authenticated loopback endpoint; connect
  with `redgem mcp --editor`. Read unsaved scene state, set existing objects atomically as one
  undo step, select, undo, redo, save explicitly and read the editor's event history. File edits
  remain the default when no editor connection is requested.
- **MCP stdio tools**: `redgem mcp <game>` exposes scene inspection, editing and comparison,
  headless runs, checks, API lookup and cursor-based process events. It supports read-only and
  allow-list options, bounded messages and results, modern per-request metadata and legacy
  initialization. Add `--editor` to attach to the opt-in live bridge.
- **Cursor event history for tools**: process logs can be read from an increasing cursor, with
  bounded pages, level filters and explicit reports of overwritten messages. Custom console
  sinks keep working alongside the history.

- **Reproducible headless runs**: `redgem run` seeds Ruby and particle randomness before scripts
  load, replays keyboard state from a JSON file, checks assertions and stop conditions every frame,
  and prints selected state at a chosen frame interval.

- **Scene comparison**: `redgem scene diff --against` reports added, removed and changed objects
  by persistent identity, including placement, hierarchy, properties, materials and asset content,
  without rewriting either scene file.
- **Stable scene object identities**: command-line object IDs survive saves, renames, reparenting
  and deletion of other objects. New objects, duplicates, pasted objects and imported models get
  their own identities, persisted in the scene file and restored by undo and redo.

- **Several objects at once in the editor**: Ctrl+click and Shift+click in the Hierarchy, Ctrl or
  Shift + click and rectangle selection in the Scene view. The gizmo moves, turns and scales the
  whole selection around its middle or its first object; the Inspector edits what they have in
  common (a dash where they differ); Duplicate and Delete act on all of it, each one undo step.
- **Material pictures in the Inspector**: the color, surface (metallic-roughness), normal,
  occlusion and glow maps of a material are picked from the game's `images/` folder, swapped or
  cleared, and saved inside the scene's `.glb`.
- **Pictures dropped on the editor**: PNG, JPEG and WebP files are copied into the game's
  `images/` folder, which the Project panel lists.
- **Cut, copy and paste** of objects with everything under them, their scripts, properties,
  meshes, materials and pictures: in the same scene, into another scene, or into another editor
  window through the system's clipboard. Paste puts them next to the selection, Paste as Child
  under it.
- **Continuous particle emitters**: attach local emitters to scene objects, with fair shared
  capacity, interpolated movement, teleport resets and correctly aged births. Lit particles can
  receive the sun's shadows.
- **Two-dimensional animation blending**: rectangular blend grids mix clips across two runtime
  parameters, including synchronized normalized playback and networked blend poses.
- **Collision layers**: up to 16 named layers in `game.json`'s `physics` section, with what each
  collides with; a body or trigger goes on one with the `layer` property or `rigidbody.layer =`.
- **Physics queries**: `Physics.raycast_all`, `sphere_cast`, `box_cast`, `capsule_cast`,
  `overlap_sphere`, `overlap_box` and `overlap_capsule`; every query, `raycast` included, takes
  `layers:` and a list of objects to `ignore:`.
- **Forces and body state**: `Rigidbody#add_force` (at a point too), `add_torque`,
  `angular_velocity`, `mass`, `gravity_scale`, `sleep`/`wake`, and switching a body between
  dynamic and kinematic while the game runs.
- **Joints**: `Physics.joint` makes fixed, hinge (limits and a motor), slider, point and distance
  (a rope, or a spring) joints between two bodies or a body and the world; a joint can break past
  a force, be disabled or removed, and goes with its objects.
- **Android in the continuous integration**: every pull request builds the Android player for
  arm64-v8a and x86_64 with the pinned NDK, so an Android-only compile break fails it.
- **Sanitizers in the continuous integration**: the unit, integration, Ruby and command line
  tests run in a build with AddressSanitizer, LeakSanitizer and UndefinedBehaviorSanitizer and in
  one with ThreadSanitizer (`REDGEM_SANITIZERS`, `tools/sanitizers/`), and a third build reports
  the coverage of `src/` as an artifact.
- **Fuzzing**: libFuzzer targets for the glTF loader, picture headers, MessagePack messages, HTTP
  answers and the JSON of glTF extras (`tests/fuzz/`, `REDGEM_FUZZ`), run for a minute each on
  every pull request and longer every night; every build replays their corpora as tests.
- **Warnings as errors**: every workflow builds with `REDGEM_WARNINGS_AS_ERRORS=ON`, so a compiler
  warning in the engine's own code or tests fails it. The dependencies' headers are included as
  system headers.
- **License notices check**: `tools/licenses/collect_notices.py --check` (run by ctest as
  `tools.license_notices_are_up_to_date`) fails when `THIRD_PARTY_LICENSES.md` is not what the
  fetched dependencies make.

### Changed

- The JSON library (`nlohmann_json`) is downloaded with a pinned SHA-256 checksum, as every other
  fetched archive is.

## [0.4.0] - 2026-10-25

### Added

- **Scatter distance LODs**: a scattered model can name simpler versions of itself for the
  distance, drawn in its place farther away.
- **Clearcoat materials**: the glTF clearcoat layer, with its coating maps, imported, edited and
  saved.
- **Animated water**: a horizontal water material with moving waves, depth absorption,
  screen-space refraction and reflections, and foam along the shore.
- **Water ripples**: bodies falling into, leaving or moving along water make ripples by
  themselves, with no splash call or water collider.
- **Animation state graphs**: clip states and one-dimensional blend trees declared in a script,
  each character's graph its own, with the blended pose sent over the network.
- **Particle sun shadows**: particles can cast the sun's shadow (opt-in), following their fading
  and flipbook cells.
- **Several scenes in the editor**: open a scene from the Scenes folder or File > Open Scene,
  make one with File > New Scene, save it under another name with File > Save Scene As. Play
  starts from the open scene.
- **Input actions and rebinding**: a game declares its actions and axes in `game.json` or with
  `Input.define_action` and `Input.define_axis`; players rebind them (`Input.bind`,
  `Input.capture_next`), and their changes are kept per game. Up to four gamepads, each action
  and axis read per pad.
- **Positional audio**: `Audio.play_at` places a sound in the world, panned and fading with its
  distance from the listener (`Audio.listener`), moved with `Audio.move`.
- **Unicode text and game fonts**: UI text draws letters beyond Latin-1 (the euro sign, Polish,
  Turkish, Greek, Cyrillic), and a game can use its own font from `fonts/` with `UI.font`.
- **WebP images** in scene materials, the HUD, particles and sky pictures, including glTF's
  `EXT_texture_webp`.
- **Runaway scripts are stopped**: a script call that runs longer than `script_budget_ms` (from
  `game.json`, 1 s by default) is stopped with a `ScriptTimeout`, and the game goes on.
- **Crash reports**: when a built game crashes, it writes a report beside its save
  (`<id>-crashes/`).
- **Display settings for players**: windowed, fullscreen and borderless modes, a resolution from
  the monitor's sizes, a render scale (50% to 200%), vsync and a frame-rate cap, quality presets
  (low to ultra, and custom) that cap what a level's environment asks for, and a master volume.
  `game.json` gives the defaults; the `Settings` module reads and changes them, and the built
  game keeps them per game (the editor's Play, servers and tests keep nothing). The player's
  volume multiplies the game's own.
- **Settings screen and widgets**: `GameMenu` has a settings screen reached from its title and
  pause screens, built from the new `UI.toggle`, `UI.choice` and `UI.slider`. The keyboard and a
  gamepad move a focus between widgets and buttons (`UI::Focus`).
- **Controls page**: the settings screen leads to each player's keys and gamepad buttons for
  every action and axis, rebound by pressing the new one, with keys bound twice marked and a
  reset to the game's. `Input.axis_stick` says which stick moves an axis; `redgem-test --after`
  starts a game again after an earlier test's run, with what that run kept.
- **Game icon, version and splash picture**: `game.json` can give a game an `icon`, a `version`
  (read by scripts as `Game.version`) and a `splash` picture shown while it loads. Builds turn the
  icon into each system's own: inside the Windows `.exe`, a menu entry and icon on Linux, an
  `.icns` in the Mac app, launcher icons on Android.
- **Mac apps**: a build for the Mac is an `.app` with the game, its icon and its description
  inside, signed ad hoc.
- **Android App Bundles**: `redgem build ... --target android --bundle` makes an `.aab` for Google
  Play (with Google's bundletool).
- **`redgem --version`** says which version of RedGem it is.
- **`redgem scene list|get|set|add|remove`**: a scene read and changed from the command line as
  short text, one line per object, objects named by a short id (`#12`), a path or a name; saved
  as the editor saves it.
- **`redgem run`** plays a game with no window for a while and says each script error once, with
  its file and line; **`redgem shot`** saves a picture of a game at a moment.
- **`redgem check`** looks a game over (scripts compile, scenes read, attached scripts are there
  and define their class) and, with `--run-tests`, runs its tests: **`redgem-test --all`** runs
  every test of a game, a line per file.
- **`redgem api`**: the scripting API as an index, a class or a search, and as JSON for tools,
  from `docs/scripting/api.json`, which tests keep in step with the engine; the manual's API page
  is made from it, a section per class.
- **A guide in every new game**: `AGENTS.md` (layout, commands, conventions, the 25 calls used
  most) and a `CLAUDE.md` that points at it; see "Making games with an AI assistant".
- **Hints with common errors**: a mistyped method of an engine class, a class no script defines,
  a scene the game lacks.
- **`llms.txt` and `llms-full.txt`**: the manual for coding assistants, written by the docs build.
- **CHANGELOG.md**: this file.

### Changed

- **An Android app asks for the network only when its game goes online**: when its `game.json`
  says `"network": true`, or says nothing and its scripts call `Network` or `HTTP` (the build then
  warns to say so). The platformer demo and template say it.
- **Saves are flushed to the disk** before they replace the previous one, and the previous save is
  kept beside it as `<name>.bak`. A damaged save is moved aside to `<name>.damaged` (never written
  over), the backup is loaded and put back in its place, and the log names both files. A save that
  cannot be read is left alone, and the game runs without saving.
- **A missing or unreadable image** shows as a magenta-and-black checkerboard, with one warning,
  instead of disappearing.
- **Water looks closer to real water**: shore foam measured along the surface, broader fine
  detail on the waves, smoother highlights, and better refraction and wave filtering.
- **KNOWN_LIMITATIONS.md** lists what stands between the engine and a 1.0.
- **A gamepad's A picks the focused option** of a menu, not always the first one.
- **What the graphics card cannot do** (several samples a pixel, temporal anti-aliasing) is dropped
  in one place, as a quality that does not allow it would drop it.

### Fixed

- **Water refraction on sloped floors**: the bottom seen through water is traced along the
  refracted ray, so a slope or a nearby rock is no longer sampled in the wrong place.
- **Temporal anti-aliasing under fast motion**: history that no longer matches is dropped when the
  camera moves fast or an edge opens up, instead of smearing.
- **Temporal anti-aliasing when an object goes away**: history is checked against the previous
  frame's depth, so a removed object no longer leaves a ghost behind.
- **Security fixes from an audit**: scripts can no longer free objects the engine still uses; a
  build refuses to replace a folder that holds the game, and never ships signing keys; vectors
  out of range from the network arrive as plain hashes; `Network.authenticate` is asked about a
  server room's first player too; every host limits floods of messages; the relay finds a room's
  members without scanning every connection; a cut HTTPS answer is no longer taken as whole;
  login tokens are hidden in message logs; the server's save database is readable only by its
  owner.
- **A guest that keeps playing while its host stalls** is no longer dropped as a flood when its
  backlog arrives late.

## [0.3.0] - 2026-10-18

### Added

- **Dedicated server**: `redgem-server` runs a game with no display, for players to join. Each
  room runs on its own thread, and the server's `Save` is a SQLite database. A game can approve
  each player who joins (after a login, for example), in relay rooms too.
- **Big outdoor worlds**: skeletons of up to 1024 bones (no more fixed shader limit), thousands of
  copies of a model drawn at once with `Scatter.place`, and animations kept in step across
  machines.
- **Texts in translation files**: every text the editor and games show lives in a locale file
  (English for now), so a game can be translated.
- **Effects and materials**: particles that change size, color and transparency over their life,
  flipbook particles, soft and lit particles, lights whose color and brightness scripts change,
  and blended or dithered (see-through) materials.
- **Outdoor lighting**: the sky picture (HDR) lights the scene, cascaded soft shadows from the
  sun, MSAA, and Scatter copies with tints, fading in the distance and normals that follow the
  ground.
- **Temporal anti-aliasing**: `antialias = "temporal"` smooths edges and shimmering foliage.
- **Record a game to a video**: `redgem <game> --record out.mp4` plays the game frame by frame
  into a video, at any size (needs ffmpeg).
- **macOS 11** is the oldest macOS the downloads run on.

### Changed

- **Multiplayer messages in a compact binary format**: smaller and faster than before, with
  positions sent as compact changes. Games of 0.2.0 and 0.3.0 do not play together, and say so.
- **The Linux download** needs glibc 2.38 with GCC 13's libstdc++ or newer (Ubuntu 24.04 and
  later).

### Fixed

- **Many fixes** from reviews of the work above, of multiplayer and the dedicated server, and of
  the engine's correctness (object identity, handles, number rules, a native `Vector3`).

## [0.2.0] - 2026-10-15

### Added

- **The editor on macOS**, Apple Silicon and Intel (built and tested on machines without a
  graphics card, not yet on a real Mac).
- **Your own models and sounds**: drop `.glb` files and sounds from your file manager onto the
  editor window.
- **A game has an `id`** in `game.json`, so its save survives moving the folder.

### Changed

- **Terrain is easier to find and use**: new ground comes with the brush on, and the Scene view
  says how to sculpt.
- **Templates are plain folders**, and the title, pause and end screens exist once (`GameMenu`).
- **A built game is `redgem-player`**, the small program that plays a game and nothing else; the
  relay is its own program, `redgem-relay`.
- **The engine's code was reorganised** into small files with one job each, with many new tests
  (783 unit cases). Games written for 0.1.0 work unchanged.

### Fixed

- **Multiplayer**: a spawned object its owner destroys is gone on every machine; messages that
  arrive just before a disconnect are delivered; resend requests are rationed.

## [0.1.0] - 2026-10-12

First experimental release.

### Added

- **Linux (x86_64)**: unpack and run `bin/redgem`. Needs OpenGL 3.3, a processor with AVX2 and
  glibc 2.39 or newer (Ubuntu 24.04 and later). Builds games for Linux, Windows and Android.
- **Windows (x86_64)**: unpack and run `redgem.exe`. Compiled from Linux and only tried under
  Wine so far. Builds games for Windows.
- The platformer, driving and city demos.

[Unreleased]: https://github.com/carlosdanielpohlod/redgem/compare/v0.5.0...HEAD
[0.5.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.4.1...v0.5.0
[0.4.1]: https://github.com/carlosdanielpohlod/redgem/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/carlosdanielpohlod/redgem/releases/tag/v0.1.0
