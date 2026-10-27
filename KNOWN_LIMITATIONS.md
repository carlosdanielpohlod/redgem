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
  half an hour instead of a minute), and the games' system tests are not run on the Mac. Never
  tried on a real Mac. The app is not signed by a registered Apple developer nor notarized.
- The editor on a Mac builds games for the Mac only: it carries no players for other systems.
  A built game's Mac app has been checked by unit tests on Linux and GitHub's Macs, never opened
  on a real Mac.
- A game's icon is put into a copy of the Windows player by the build; Windows' own loader of
  icons has been seen reading it under Wine only.
- The Linux programs need a processor with AVX2 and glibc 2.39 or newer (Ubuntu 24.04 and later).
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
  fading does not affect the cast shadow silhouette; point lights do not cast shadows.
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

- Redirects are not followed; there are no cookies, no compression, no streaming, no uploads of
  files: a request and its answer are each held whole in memory (at most 8 MB).
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

## Editor

- Undo keeps at most 200 steps and 256 MB, and lets go of the vertices, pictures and file data of
  what no step can bring back. What an imported model put on the graphics card, its skeletons and
  animations, and a few hundred bytes of description per object still stay until the editor is
  closed.
- In Docker the editor has no sound.
- A pasted object carries its own copy of its meshes and pictures, even in the scene it was copied
  from (Duplicate shares them). A copy bigger than 64 MB pastes only in the editor window it was
  copied in.

## Tools

- Recording a video (`redgem <game> --record out.mp4`) needs `ffmpeg` on the PATH, is only on the
  development program `redgem` (not on a built game's `redgem-player`), and cannot start on
  Android.

- The Blender scene builders do not write the same bytes twice (vertex and UV order change
  between runs), so regenerating a demo scene shows up as a change.
- The showcase demo is built from downloaded assets that may not be passed on, so it is not in
  the downloads and its tests are skipped without it.
- The scripts that record the site's video are not part of the repository.

## Before 1.0

What a game made outside this project would run into first, roughly in order. Each line is a gap,
not a promise of how it will be closed.

### Editor

- A picture put on a material from the editor goes inside the scene's `.glb`: changing the file in
  `images/` afterwards does not change the scene (pick it again). The Inspector shows a slot's
  picture by name, without a preview, and sets neither a texture's sampler nor its UV set.
- Templates live inside one scene: there are no prefab files shared by several scenes, nor nesting
  or overrides.
- Scripts reload when Play is pressed, not while the game runs. The script editor has no
  autocomplete, and a backtrace in the console does not jump to its line.
- No autosave or crash recovery of the scene being edited; no animation preview or physics debug
  view; no preferences beyond the graphics card (key bindings, camera speed, snap sizes); no HiDPI
  scaling; dark theme only.

### What a shipped game offers its players

- The menu's controls page binds one key and one button to a row: rebinding a cell replaces all it
  had, so a second key for one action comes only from a game's script (`Input.bind`). It lists
  every action and axis the game declared. Gamepads cannot rumble (neither the
  desktops' window library nor Android offers it), the mouse cannot be bound to an action, and a
  capture catches keys and buttons, not sticks or triggers.
- `UI` has no word wrap, anchors or scroll lists. Android has no on-screen keyboard.

### Audio

- Positional sound provides stereo panning and distance falloff for at most 64 independent sources.
  Games update source positions and the listener explicitly; there is no HRTF, occlusion or Doppler.
  One music track at a time with no crossfade, no mixing groups.

### Text

- Text uses individual Unicode glyphs without bidirectional layout, complex-script shaping or
  automatic font fallback chains. The built-in font supports its own glyphs; other scripts, such as
  CJK, need a game-provided font. Each face caches at most 2048 codepoints; four named game fonts
  can be cached at once, with at most 32 MB per font file. Unsupported glyphs show a question mark.

### Gameplay API

- Joints are made from scripts only: a level cannot set one up in Blender or the editor. The
  editor's Physics card has no collision layer picker (`layer` is set as a custom property).
- Animation clips have no events (footsteps, hit frames), no root motion and no IK.
- No navigation mesh or pathfinding.
- Cameras have no follow, orbit or shake helpers, and no `world_to_screen`.
- `Save` is one key/value store per game: no slots, no saving of objects' state, no version number
  or migration hook.
- `Game.load_scene` loads in the middle of a frame: there is no loading screen or loading in the
  background. On a dedicated server, a room loading a level holds up the rooms sharing its thread.
- `Debug` writes to the log only: no on-screen lines, boxes or text, no frame-time overlay, no
  profiler.

### Safety

- Running out of memory is handled only where it reaches the top of a built game (a clear message
  and a report): memory running out inside a library that does not throw (the graphics driver,
  the physics, the interpreter's own allocations, which a script sees as `NoMemoryError`) can
  still end the game with only a crash report, or none.
- A built game for Android writes no crash report and shows no message (only `adb logcat`). On
  Windows the report's backtrace is module offsets (`redgem-player.exe+0x1a2b`), not function names.
- The scripts' time budget stops Ruby code only: one call of the engine's own that takes long
  (a huge `Array#sort`, a request to the engine for millions of things) is not cut off, and the
  budget is wall time: a call paused in a debugger for longer than it is stopped when it resumes.

### Security

- On a dedicated server, a game's plain `http://` reaches anything on the server's own private
  networks (only the clouds' known metadata addresses are refused).
- On Windows a server's save database takes its folder's permissions: it is kept to its owner
  only on Linux and macOS.
- A script that sets an engine class's name to something else (`Vector3 = 5`) breaks the
  engine's calls that use that class with Ruby errors, instead of being stopped from doing so.
- An Android build empties its staging folder (`.redgem-apk-stage` in the output folder) without
  checking that a build made it.
- No unit tests for the room-code guessers, room codes, `ByteAllowance`, saved values, the socket
  layer or the TLS stream on its own (only through requests); no test of a TLS name that does not
  match its certificate, nor of a server sending interim (1xx) answers for ever.

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
- Saves live under RedGem's own folder, not under the game's or its studio's name.
- Except the Mac's, the downloads are packed by hand, not by the continuous integration.

### Engineering

- Rendering, physics and scripts run on one thread; physics uses a single-threaded job system.
- Textures are kept uncompressed on the graphics card (no KTX2/Basis, BCn or ASTC), which costs
  most on phones.
- The continuous integration has no real graphics card. The sanitizers (ASan, UBSan, TSan) run the tests
  that open no window, not the system tests; the coverage report is of those tests too, and no
  share of it is required. The load test and the benchmarks print numbers but fail on none.
- ThreadSanitizer finds a lock-order inversion in the physics: adding a vehicle holds a body's
  lock while it takes Jolt's step-listener lock, which a physics step takes the other way round.
  Physics runs on one thread, so they never meet today; it is listed in
  `tools/sanitizers/tsan.supp` until it is fixed. UBSan finds a read before a table in raylib's
  bundled `stb_image_resize2` (scaling icons), listed in `tools/sanitizers/ubsan.supp`. LeakSanitizer
  finds 161 bytes mruby allocated still held when the unit tests end; mruby is not built with the
  sanitizers, so whose they are is unknown, and every leak of mruby's memory is listed as known.
- About forty unit and integration tests wait on the real clock, a source of rare timing failures.
- Some constants are defined more than once (`kLargestPort` in four places, `kBitsPerByte` in
  four, Ruby limits that repeat C++ ones), as are a few string helpers.
- A connection's buffers never give memory back once grown, and copy what is left on every
  partial send (`frame_buffer.cpp`, `socket_io.cpp`).
- `Save.update`'s block runs under one lock for the whole database, which every room of a
  dedicated server shares: a slow block holds up the saves of all rooms.
- The threads of web requests are detached: one still running at exit may outlive the program's
  static objects.
- Water is expensive in coastal views: refraction can fall back to a 32-step screen search with
  refinement near shorelines and rocks, and the shore foam's distance is computed for every water
  pixel.

### Multiplayer

- Game connections are not encrypted. A dropped player cannot reconnect, and when the host of a
  hosted game leaves, the game ends for everyone (dedicated servers do not have this problem).
- Clients own the positions of their objects and decide their own collisions: a server cannot
  check movement, so a modified client can move anywhere.
- Players who are not on the same network need a relay or a dedicated server: there is no direct
  connection through home routers (no STUN or hole punching).
- A dedicated server writes logs only: no status page or metrics (rooms, players, frame time,
  bandwidth).

### Documentation

- No migration notes between versions: CHANGELOG.md says what changed, not how to move a game
  across.
- The API reference (`docs/scripting/api.json`, made into `docs/scripting/api.md` and `redgem api`)
  is checked against the engine's classes and methods, but written by hand: arguments are written
  as text, with no types, and there are no stubs for code editors.
- Templates are `empty` and `platformer` only; there is no example of running game tests in a
  game's own continuous integration.

### Working with AI assistants

What is there today is the command line (`redgem scene`, `run`, `shot`, `check`, `api`), the
`AGENTS.md` guide of new games and `llms.txt`; see [docs/assistants.md](docs/assistants.md). Not
there yet:

- Headless input replay covers keyboard states only; mouse, gamepad and touch input are not yet replayed.
- An event log read with a `since` cursor (only what is new), and `redgem scene diff` to show what
  an edit changed.
- A search of the game's assets and of the templates.
- `redgem mcp`: an MCP server of about 8 to 10 tools wrapping the command line, and a live bridge
  to an open editor (a localhost socket with a token) to list, select and move objects, set
  properties, play, stop and step, read the console's errors and take small screenshots, with
  read-only and allow-list flags.
- Inspecting the objects of a game while it plays, and an opt-in Ruby eval in it (localhost and a
  token, a timeout, compiled out of release builds).
- Physics queries (raycasts and overlaps) through the same tools.
- `redgem scene` ids are places in the scene file: adding or removing objects renumbers the ones
  after them.
