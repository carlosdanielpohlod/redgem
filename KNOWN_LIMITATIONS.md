# Known limitations

RedGem is experimental. This is what is missing or known to be wrong today: good places to
start if you want to [contribute](CONTRIBUTING.md). If you fix one, remove it from this list in
the same pull request.

## Platforms

- The editor is developed and tested on Linux. The Windows download is compiled from Linux. On a
  real Windows machine (GitHub's, which has no graphics card) the engine is built with the same
  compiler and its unit, integration and Ruby tests run on every change, but no window has been
  opened there: the editor and the games have been seen running on Windows under Wine only.
- The macOS downloads have only run on GitHub's virtual Macs, which have no graphics card OpenGL
  can use: there the editor is drawn by Apple's software renderer (its system test takes about
  an hour instead of a minute), and the games' system tests are not run on the Mac. Never
  tried on a real Mac. The app is not signed by a registered Apple developer nor notarized.
- The editor on a Mac builds games for the Mac only: it carries no players for other systems.
  A built game's Mac app has been checked by unit tests on Linux and GitHub's Macs, never opened
  on a real Mac.
- A game's icon is put into a copy of the Windows player by the build; Windows' own loader of
  icons has been seen reading it under Wine only.
- The Linux programs need a processor with AVX2 and glibc 2.35 or newer (Ubuntu 22.04, Debian 12,
  Fedora 36 and later): they are built on Ubuntu 22.04, so no older system runs them (Debian 11,
  RHEL 9 and its kind refuse them). The CI runs them on a bare Ubuntu 22.04 without opening a
  window. The AppImage needs FUSE to start (`--appimage-extract-and-run` without it), and its
  demos are read only inside it. Neither has been tried on a real desktop of another
  distribution, nor on a Steam Deck.
- Android games have been run on an emulator only, not on phones. The touch controls have one
  fixed layout (a stick and three buttons). The continuous integration builds the Android player
  but runs nothing on Android.
- Once, right after an emulator cold boot, an Android game stopped in its audio thread. It did
  not happen again in the runs that followed and has no fix.

## Rendering and animation

- Animation graphs support clip states, one-dimensional blends and rectangular two-dimensional grids. State fades complete
  before another transition is evaluated; there are no hierarchical trees or irregular two-dimensional point clouds.
- Particle shadow casting is opt-in sun-facing alpha-cutout sprites (`cast_shadow: true`), not
  volumetric density shadows. Lit particles receive filtered scene sun shadows. Camera-depth soft
  fading does not affect the cast shadow silhouette; particles cast no shadows from point or spot
  lights.
- Grass and other `Scatter` copies stop at the base template's `visible_to`. Distance meshes
  can be supplied with `lods`, but simplified models and impostor atlases are not generated
  automatically. Beyond the final range the ground is still its texture alone.
- Temporal anti-aliasing (`antialias = "temporal"`) smears detail when the camera moves fast,
  such as a chase camera behind a fast car. Local high-motion and opening-edge rejection reduces
  history accumulation. Previous-frame depth rejects history from hidden or newly revealed
  surfaces, with previous skin and object poses included. The depth check accepts a match within a
  texel, so history from a surface that disappeared can survive along its outline for a frame,
  held back only by color clipping. Thin cut-outs and fast details can trade smear for shimmer
  when too few valid temporal samples remain.
  It is off in the editor's scene view, and turns itself
  off, with a warning, on graphics cards that cannot draw into float textures.
- Water supports animated horizontal surfaces, depth absorption, screen-space refraction,
  screen-space reflections with an environment fallback, shore foam and local impact/wake ripples
  from physical bodies. It has no underwater camera mode, fluid simulation,
  buoyancy, breaking waves or planar reflections. Shore foam estimates horizontal distance
  to the intersection of visible bottom and water tangent planes, with filtered pores and partial transmission.
  It is a local screen-space approximation, not a global shoreline-distance field. Missing geometry,
  depth discontinuities and nearly parallel planes suppress coverage; curved shores can deviate
  from the nearest-shore distance. Its procedural pore pattern is stationary.
  It does not accumulate, advect or follow breaking crests.
  Refraction sees opaque geometry on screen
  only; overlapping water layers and transparent objects are not captured. Absorption affects
  the path from the bottom to the camera; incident lighting of submerged objects is not attenuated by water.
- Images support PNG, JPEG and static WebP, at most 8192 pixels a side (a sky picture may also
  be Radiance `.hdr`). Animated WebP and KTX2/Basis textures are not read. WebP is decoded on the CPU and uploaded as
  ordinary pixels; it does not provide GPU texture compression.
- Point and spot lights cast shadows only when their object sets `cast_shadow = true`, at most
  four a frame (fewer under lower quality presets) and at most two of them point lights, each in
  512-texel tiles of one atlas: a large room lit by one point light gets coarse shadow edges, and
  the filter does not blend across a point light's cube faces, so a faint seam can show where two
  faces meet. A light that drops out of the budget as the camera moves loses its shadow at once
  (no fade), and every shadowed light is drawn again each frame, moving or not. Lights beyond the
  budget, and every light on a card that cannot make the atlas, light through walls. Particles
  are lit by spots within their cones but receive no point or spot shadows.
- An object mirrored with a negative scale in Blender is drawn inside out (the editor and the
  command-line tools refuse negative scales).
- Morph targets (shape keys) are not read: their weights and names are dropped. Animation keys
  with cubic-spline interpolation are skipped with a warning (export linear or step keys).
- Of glTF's material extensions the engine reads clearcoat, emissive strength and WebP textures.
  Transmission, sheen, specular, IOR, anisotropy, volume and texture transforms are ignored.
- Indirect light comes from the sky and one live capture around the camera: no lightmaps, light
  probes or baked lighting, so interiors and deep shade are lit by the sky. Exposure is a fixed
  environment property; there is no eye adaptation.
- A sky picture, shadow cascades, `msaa` and temporal anti-aliasing have been seen on desktop
  OpenGL only, not on a phone.
  On OpenGL ES 3 they rely on what the engine already needs there (half-float textures that can
  be drawn into): a phone that cannot draw several samples a pixel draws one, and says so.

## Multiplayer

- A dedicated server runs each room on a thread of its own, up to `--threads` (the machine's
  cores by default). With more rooms open than threads, rooms share them, and a level loading in
  one holds up the others on its thread for that moment.
- Messages travel over TCP: a packet lost on the way holds up the ones after it. What others
  move is shown a moment behind them (interpolated), and nothing rewinds time to judge a hit where
  its shooter saw the target (no lag compensation): fast competitive games will feel the delay.
- Players behind one shared address (a campus, some mobile networks) count as one address: by
  default a relay or a dedicated server lets it connect 24 machines (a relay, open 8 rooms), and a
  game hosted directly lets 4 of them be joining at once. Whoever runs the relay, the server or
  the game has to raise these (`--per-address`, `--rooms-per-address`,
  `Network.host(waiting_per_address:)`).

## Web requests (HTTP)

- There are no cookies, no streaming, no uploads of files: a request and its answer are each
  held whole in memory (at most 8 MB, decompressed). Answers compressed with anything but gzip
  or deflate (br, zstd) are not decompressed (the engine does not ask for them).
- Redirects stop after 10. When one leaves the origin, only `Authorization`, `Cookie` and
  `Proxy-Authorization` are dropped: other headers a game sets (an `X-Api-Key`) go along. Pass
  `redirects: false` to see the redirect and decide.
- Looking a name up cannot be cut short: a request to a name the network takes long to answer
  for can outlast its timeout by that long (and a game that ends waits for nobody: the request's
  thread ends on its own).
- On Windows the certificates come from the system's ROOT store, which Windows fills on demand:
  an authority no program on the machine has used yet may be missing. Not tried on a real Windows
  machine, nor on a real Android device (whose system certificates the engine reads).
- `http://` names are checked once connected: a public address is never sent anything, but the
  connection to it is made.

## Scripting

- Scripts run on mruby, not on the Ruby you install on your machine: no gems, no `require`, no
  files, sockets or `eval` (on purpose: a game cannot touch the player's machine).
- `a&.b.c` does not stop at `a` being nil as it does in other Rubies: write `a&.b&.c`.
- All text the engine and editor show is English. Games translate their own texts.
- `Scene.spawn` cannot name or parent the copy, and an object's parent cannot be changed from a
  script. The scene's first camera is the game's camera: a script cannot switch to another.
- Custom properties that are lists or objects in the scene file are dropped with a warning: a
  property is a number, a text or true/false.
- Limits a game can reach: 64 hits per physics query, 4096 live particles and 256 emitters, 4096
  `Debug` shapes a frame, 262,144 scattered copies, 16 collision layers, 64 named save slots
  opened in one run, 1024 bones a skeleton, a 4 GB memory estimate per scene.

## Editor

- Undo keeps at most 200 steps and 256 MB, and lets go of the vertices, pictures and file data of
  what no step can bring back. What an imported model put on the graphics card, its skeletons and
  animations, and a few hundred bytes of description per object still stay until the editor is
  closed.
- In Docker the editor has no sound.
- A pasted object carries its own copy of its meshes and pictures, even in the scene it was copied
  from (Duplicate shares them). A copy bigger than 64 MB pastes only in the editor window it was
  copied in.
- Importing the same model twice appends its meshes and textures again, and deleting an import
  leaves its data in the scene's file (only pictures no material shows are dropped on save).
- A model whose glTF keeps its data in several buffers cannot be imported or pasted: export a
  single `.glb` from Blender.
- A character duplicated in the editor keeps the pose it was copied in.
- Terrain is sculpted only: one 100 m square with one color and roughness, no texture painting,
  no holes cut through it, and no joining of several terrains. A terrain re-exported from Blender
  is an ordinary mesh.

## Tools

- Recording a video (`redgem <game> --record out.mp4`) needs `ffmpeg` on the PATH, is only on the
  development program `redgem` (not on a built game's `redgem-player`), and cannot start on
  Android.

- The Blender scene builders do not write the same bytes twice (vertex and UV order change
  between runs), so regenerating a demo scene shows up as a change.
- The showcase demo is built from downloaded assets that may not be passed on, so it is not in
  the downloads and its tests are skipped without it.
- The scripts that record the site's video are not part of the repository.
- The CI example every new game gets (`.github/workflows/test.yml`) is for GitHub Actions on
  Linux only, and downloads the latest release: a game made from templates newer than that
  release (the `third-person`, `top-down` and `menu-only` ones, until the next release) may use
  scripting the release does not have yet. Its system tests run with no window: they take no
  pictures, and a click lands on no screen (a template's own tests call `walk_to` instead of
  clicking).
- A game's `tests/` folder goes into its builds with the rest of the game (the tests are not run
  there, and the player has no test code).
- The template scenes are written by `tools/templates/build_template_scenes.py` from the editor's
  primitives in the `empty` and `platformer` templates: a template is not yet made from the
  editor's own primitive code.

## Before 1.0

What a game made outside this project would run into first, roughly in order. Each line is a gap,
not a promise of how it will be closed.

### Editor

- A picture put on a material from the editor goes inside the scene's `.glb`: changing the file in
  `images/` afterwards does not change the scene (pick it again). The Inspector shows a slot's
  picture by name, without a preview, and sets neither a texture's sampler nor its UV set.
- [Prefab file](docs/prefabs.md) instances cannot override inherited children separately, a
  script cannot spawn a prefab file by its path (only a placement in the scene), and a running
  scene does not see a changed prefab file until reloaded. Export needs complete skeletons and
  descendant-only animation; it does not replace the original selection with a reference or
  overwrite an existing source file. (Loading, nesting, root overrides, previews, placement from
  the Project panel and export of a selected hierarchy are there.)
- [Live script reload](docs/scripting/live_reload.md) updates plain behaviour methods and literal
  constants during editor Play, preserving instance fields. Executable setup, computed constants,
  class methods/mixins, changed inheritance and initially failed scripts still require a restart;
  existing blocks keep their old code. The script editor has no autocomplete, and console
  backtraces do not jump to their lines.
- The editor can show actual collider wireframes during Play/Pause; it does not yet draw
  contacts, joints or velocities, or preview colliders before Play.
- Scene recovery writes a separate copy at safe editing boundaries about once a minute. It is
  synchronous, limited to 64 MiB and embedded GLB buffers, and needs the original scene to remain
  loadable when reopening. Linked images keep their existing paths; scripts and external assets
  are not backed up. See [scene recovery](docs/editor.md#scene-recovery).
- No animation preview; no preferences
  beyond the graphics card (key bindings, camera speed, snap sizes); dark theme only.
- Automatic monitor scaling applies to editor text and spacing. The scene preview renders at
  logical viewport resolution, and there is no manual scale override.

### What a shipped game offers its players

- The menu's controls page binds one key and one button to a row: rebinding a cell replaces all it
  had, so a second key for one action comes only from a game's script (`Input.bind`). It lists
  every action and axis the game declared. Gamepads cannot rumble (neither the
  desktops' window library nor Android offers it), and a capture catches keys, mouse buttons, the
  wheel and gamepad buttons, not sticks, triggers or mouse movement. Keys are named for their place
  on a US keyboard, and the page shows them by those names whatever the layout types.
- Text fields take single key presses: there is no input-method composition, so languages typed
  through a composition window cannot be entered even with a game font that draws them.
- `UI` lays texts out a line at a time: word wrap breaks only at spaces and between letters (no
  hyphenation, no line-breaking rules of scripts written without spaces), and anchors place things
  by fixed pixel sizes (sizes do not scale with the screen; `GameMenu` scales its own). There is
  no layout engine (rows, columns, stacks) and no scroll region for anything other than rows of
  one height.
- On Android the on-screen keyboard types the letters that have a key of their own on it (with
  Shift, digits and signs). Letters a keyboard sends as a whole text (some accented letters, emoji,
  words written by swiping or picked from suggestions on some keyboards) do not arrive.
  `UI.keyboard_visible?` says whether the engine asked for the keyboard: when the system closes it
  by its own means (the back key, the keyboard's own key to hide it), the game does not hear of it,
  and it stays true until the field goes, is tapped, or the back key is pressed again. The
  keyboard covers the bottom of the screen and nothing moves out of its way: a game keeps its
  fields above it.

### Audio

- At most 64 sounds with ids play at once, flat and positional together. Positional sound provides
  stereo panning and distance falloff; games update source positions and the listener explicitly;
  there is no HRTF, occlusion or Doppler.
- One music track at a time, two while they crossfade. The mix groups are fixed (master, music,
  effects, voice): a game cannot add its own, and there are no effects on a group (reverb,
  ducking, filters). Fades are linear in volume, and only the music fades by itself; a sound's
  levels change at once.

### Text

- Text uses individual Unicode glyphs without bidirectional layout, complex-script shaping or
  automatic font fallback chains. The built-in font supports its own glyphs; other scripts, such as
  CJK, need a game-provided font. Each face caches at most 2048 codepoints; four named game fonts
  can be cached at once, with at most 32 MB per font file. Unsupported glyphs show a question mark.

### Gameplay API

- Joints are made from scripts only: a level cannot set one up in Blender or the editor.
- Animation clips have no events (footsteps, hit frames), no root motion and no IK.
- No navigation mesh or pathfinding.
- A game's vehicle drives every axle, steers the front wheels and brakes the rear ones: drive
  layout, torque and suspension cannot be set from properties.
- A scene holds at most 4096 physics bodies at once (a fixed size, not set per game). An object
  spawned past that gets no body: no collisions, and raycasts miss it. The engine logs one error
  when that first happens. A game that keeps spawning (bullets, debris) has to destroy what it no
  longer needs.
- `Save` supports named slots and per-object pose/appearance checkpoints. Whole-world snapshots,
  automatic spawned-instance/behaviour persistence and game-data migration hooks remain future
  work. Slot selection and storage calls are synchronous.
- `Game.load_scene` is synchronous. `Game.load_scene_async` reads and parses in a bounded shared
  worker, with a loading-screen callback, but physics creation, scripts, texture decoding and GPU
  upload still block the owning thread at activation. Old and new scenes coexist in memory.
  Offline fixed-step runs intentionally wait for the reader for reproducible activation frames.
- `Debug` supports an opt-in foreground phase profiler, FPS/frame-time overlay and immediate
  world-space lines/wire boxes. World-space labels, GPU timing and per-function sampling remain
  future work.

### Safety

- Running out of memory is handled only where it reaches the top of a built game (a clear message
  and a report): memory running out inside a library that does not throw (the graphics driver,
  the physics, the interpreter's own allocations, which a script sees as `NoMemoryError`) can
  still end the game with only a crash report, or none.
- A crash report's backtrace is offsets into each program (`Arena.exe+0x1a2b`): function names
  need `redgem symbolize` with the release's `redgem-symbols-<system>.zip` and an addr2line on the
  developer's machine. The symbols name functions; file and line only where the player was built
  with debugging information, which the release's are not. Mac reports name no build id, so
  `redgem symbolize` cannot read them, and no Mac symbols are kept.
- A built game for Android shows its player no message when it crashes: the report is written to
  the app's files and to `adb logcat`, and the next run only says so in its log. The report's
  backtrace on Android is what the unwinder could walk from the signal handler, after the address
  the crash happened at.
- The scripts' time budget stops Ruby code only: one call of the engine's own that takes long
  (a huge `Array#sort`, a request to the engine for millions of things) is not cut off, and the
  budget is wall time: a call paused in a debugger for longer than it is stopped when it resumes.

### Security

- The TLS private key of a relay, a server or a hosted game is in memory as PEM text while it
  starts, and those copies are freed without being wiped first.
- On a dedicated server, a game's plain `http://` reaches anything on the server's own private
  networks (only the clouds' known metadata addresses are refused).
- On Windows a server's save database takes its folder's permissions: it is kept to its owner
  only on Linux and macOS.
- A script that sets an engine class's name to something else (`Vector3 = 5`) breaks the
  engine's calls that use that class with Ruby errors, instead of being stopped from doing so.
- No unit tests for the room codes and their guessers (`room_codes.cpp` is tested only through
  the relay and server integration tests), the socket layer or the web requests' TLS stream on
  its own (only through requests; game connections' TLS is tested over loopback sockets only);
  no test of a TLS name that does not match its certificate, nor of a server
  sending interim (1xx) answers for ever.

### Shipping

- The splash picture shows once the window is open: reading the first level's file, before
  that, shows nothing. The Linux menu entry is for a package to install: the build does not add
  it to a menu.
- A built game is the player program beside a folder of loose files, scripts as plain `.rb`
  included: no packed or compressed archive.
- No installers: no Windows installer or code signing, no Linux AppImage or Flatpak. A game's Mac
  app (and the editor's) is signed ad hoc only, with no Developer ID nor notarization: a Mac that
  downloads it warns that its developer cannot be verified.
- An App Bundle (`--bundle`) needs Google's bundletool installed apart. Bundles have been checked
  with bundletool, never uploaded to Google Play. Whether an app asks for the network, when
  `game.json` does not say, is guessed from its scripts' text (`Network.` or `HTTP.`): a script
  that reaches them some other way is not seen.
- A game's folder is named after its studio and title: a game renamed in an update starts with no
  save, in a new folder, and nothing moves the old one. Two games of the same studio and title
  share it.
- Saves, settings and bindings are plain JSON with no integrity check: a player can edit them.
- The downloads are built and packed by the continuous integration, on GitHub's runners, and
  checked there only by listing them and running their programs' `--version` and `--help`: no
  one opens the Windows and Linux editors from a download before a release is published.

### Engineering

- Rendering, physics and scripts run on one thread; physics uses a single-threaded job system.
- Textures are kept uncompressed on the graphics card (no KTX2/Basis, BCn or ASTC), which costs
  most on phones.
- The continuous integration has no real graphics card. The sanitizers (ASan, UBSan, TSan) run the tests
  that open no window, not the system tests; the coverage report is of those tests too, and no
  share of it is required. The load test and the benchmarks print numbers but fail on none.
- UBSan finds a read before a table in raylib's bundled `stb_image_resize2` (scaling icons),
  listed in `tools/sanitizers/ubsan.supp`. LeakSanitizer finds 161 bytes mruby allocated still
  held when the unit tests end; mruby is not built with the sanitizers, so whose they are is
  unknown, and every leak of mruby's memory is listed as known.
- About forty unit and integration tests wait on the real clock, a source of rare timing failures.
- Some constants are defined more than once (Ruby limits that repeat C++ ones), as are a few
  string helpers.
- A connection's buffers never give memory back once grown, and copy what is left on every
  partial send (`frame_buffer.cpp`, `socket_io.cpp`).
- `Save.update`'s block runs under one lock for the whole database, which every room of a
  dedicated server shares: a slow block holds up the saves of all rooms.
- The threads of web requests are detached: one still running at exit may outlive the program's
  static objects.
- Water is expensive in coastal views: when refraction's 8-step estimate is rejected near
  shorelines and rocks it falls back to a 16-step screen search refined by 9 halvings,
  reflections march up to 32 steps, and the shore foam's distance is computed for every water
  pixel.

### Multiplayer

- A dropped player cannot reconnect, and when the host of a hosted game leaves, the game ends for
  everyone (dedicated servers do not have this problem).
- Game connections are encrypted, but who answers is checked only when the game gives its
  fingerprint (`fingerprint:`, or `"servers"` in `game.json`): without one, someone who can
  redirect the players' traffic can stand in for the host, the relay or the server. Nothing
  remembers a fingerprint seen once (no trust on first use).
- A player hosting is checked by a 10-digit code (40 bits of its fingerprint), not the whole
  fingerprint (64 digits, too long to read out loud): an attacker who can redirect the traffic
  and spends about 2^40 certificate generations while that game is open could match it. A short
  code is taken for any address joined directly, a dedicated server's too, where an attacker has
  all the time it wants: give a server its whole fingerprint (a room takes nothing less).
- Through a relay, the connections are encrypted to the relay, not from player to player: the
  relay reads what the players of its rooms say.
- No certificate rotation or revocation: a relay or server that loses its data folder (or its key)
  gets a new fingerprint, and games that pinned the old one refuse it until they are updated.
  Its certificate is self-signed, valid until 2099, and not checked against any authority.
- A TLS handshake costs the relay and the server some CPU (an ECDHE key exchange and an ECDSA
  signature): a flood of handshakes is limited only by the connections each address may hold.
- Finding games on the local network (`Network.search`) is not authenticated: any machine there
  can announce a game.
- Plaintext is kept only inside the engine, for its own tests of the protocol: a game, the relay
  and the server have no way to turn encryption off.
- Clients own the positions of their objects and decide their own collisions: a server cannot
  check movement, so a modified client can move anywhere.
- Players who are not on the same network need a relay or a dedicated server: there is no direct
  connection through home routers (no STUN or hole punching).
- A room holds at most 16 machines (a dedicated server's `--max-players` stops at 15), set by
  the wire format, not by a game.
- A dedicated server's status (`--status-port`) is one JSON document of the moment: no history,
  no Prometheus-style metrics, and no IPv6 address to bind it to.

### Documentation

- No migration notes between versions: CHANGELOG.md says what changed, not how to move a game
  across.
- The API reference (`docs/scripting/api.json`, made into `docs/scripting/api.md` and `redgem api`)
  is checked against the engine's classes and methods, but written by hand: arguments are written
  as text, with no types, and there are no stubs for code editors.
- Templates are `empty` and `platformer` only; there is no example of running game tests in a
  game's own continuous integration.

### Working with AI assistants

What is there today is the command line (`redgem scene`, `run`, `shot`, `check`, `api`), the MCP
tools (`redgem mcp`), the editor's Agent panel, the `AGENTS.md` guide of new games and `llms.txt`;
see [docs/assistants.md](docs/assistants.md) and [docs/mcp.md](docs/mcp.md). Not there yet:

- Headless input replay plays one finger, as the pointer a phone gives the game; multi-touch
  gestures and the on-screen touch controls of a phone are not replayed.
- Reading history from an unrelated running game: MCP's `events` tool reads its own process or
  its connected editor, using a process-local `since` cursor.
- `redgem scene diff` compares loaded object content against an earlier scene file, but does
  not read a revision directly from Git or compare unused assets and unsupported glTF data.
- A search of the game's assets and of the templates. `api` covers the engine's classes only, not
  the game's own behaviours: read the game's scripts for those.
- The opt-in MCP editor bridge ([docs/mcp.md](docs/mcp.md)) supports live inspection, atomic node
  set/add/remove, selection and framing, undo/redo, save, play controls, single stepping, bounded
  viewport images, runtime queries and event history. Live add creates empty objects only;
  asset-backed duplication (`from`) and model importing still use files or the editor UI. Native
  calls run serially and cannot be interrupted by a cancellation notification.
- The Agent panel ([docs/assistants.md](docs/assistants.md#the-agent-panel)) sends and shows text
  only: no pictures in the chat, and no terminal of the editor's for the agent (the adapters run
  commands themselves). Logging in to an agent happens in a terminal, with the agent's own CLI.
  Slash commands and session modes are not offered, and answers are not selectable as text (code
  blocks have a Copy button). Past conversations are reopened through the agent (`session/load`): an
  agent that cannot do that starts anew, and a conversation the agent no longer keeps is gone.
- An opt-in Ruby eval in a running game (localhost and a token, a timeout, compiled out of
  release builds). `game_run` expressions cover headless runs; `game_query` covers inspection
  and physics queries.
- MCP adds one object per `scene_edit` call; a row of copies takes one call each.
- Objects made since a scene was first saved get 16-character hexadecimal ids, longer to read
  and pass back than the numeric ids of older objects.
- `game_shot` needs a display (Xvfb works on a server).
- External exporters that drop the private `REDGEM_object_id` glTF extension also drop persistent
  scene object identities.
