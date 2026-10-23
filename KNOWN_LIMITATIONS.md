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
  A built Mac game is a plain program beside its files, not an .app.
- The Linux programs need a processor with AVX2 and glibc 2.39 or newer (Ubuntu 24.04 and later).
- Android games have been run on an emulator only, not on phones. The touch controls have one
  fixed layout (a stick and three buttons). The continuous integration does not build for Android:
  an Android-only break shows up when a release is packed.
- Once, right after an emulator cold boot, an Android game stopped in its audio thread. It did
  not happen again in the runs that followed and has no fix.

## Rendering and animation

- Animation graphs support clip states and one-dimensional blends only. State fades complete
  before another transition is evaluated; there are no hierarchical or two-dimensional trees.
- Particle shadows are opt-in sun-facing alpha-cutout sprites (`cast_shadow: true`), not volumetric density shadows. They do not receive shadows, and camera-depth soft fading does not affect their shadow silhouette.
- Grass and other `Scatter` copies stop at the base template's `visible_to`. Distance meshes
  can be supplied with `lods`, but simplified models and impostor atlases are not generated
  automatically. Beyond the final range the ground is still its texture alone.
- Temporal anti-aliasing (`antialias = "temporal"`) smears detail when the camera moves fast,
  such as a chase camera behind a fast car. Local high-motion and opening-edge rejection reduces
  history accumulation, but disocclusion is estimated from current neighboring motion rather
  than validated against previous-frame depth. Fast details can trade smear for shimmer.
  It is off in the editor's scene view, and turns itself
  off, with a warning, on graphics cards that cannot draw into float textures.
- Water supports animated horizontal surfaces, depth absorption, screen-space refraction,
  screen-space reflections with an environment fallback, shore foam and local impact/wake ripples
  from physical bodies. It has no underwater camera mode, fluid simulation,
  buoyancy, breaking waves or planar reflections. Refraction sees opaque geometry on screen
  only; overlapping water layers and transparent objects are not captured. Absorption affects
  the path from the bottom to the camera; incident lighting of submerged objects is not attenuated by water.
- Images must be PNG or JPEG, at most 8192 pixels a side (a sky picture may also be Radiance
  `.hdr`). Blender's exporter can write WebP and KTX2 textures: neither is read (no decoder for
  them is among the libraries the engine is built with).
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

- Only `scene.glb` can be opened and edited. Other scenes in `scenes/` can be loaded from a script
  (`Game.load_scene`) but not edited, and Play always starts from `scene.glb`.
- One object is selected at a time: no multi-select, box select or group transform, and no copy and
  paste (duplicate and delete only).
- The material inspector edits color, metallic, roughness and emission, but cannot assign or swap a
  texture. Images cannot be dropped onto the editor, and the Project panel has no `images/` folder.
- Templates live inside one scene: there are no prefab files shared by several scenes, nor nesting
  or overrides.
- Scripts reload when Play is pressed, not while the game runs. The script editor has no
  autocomplete, and a backtrace in the console does not jump to its line.
- No autosave or crash recovery of the scene being edited; no animation preview or physics debug
  view; no preferences beyond the graphics card (key bindings, camera speed, snap sizes); no HiDPI
  scaling; dark theme only.

### What a shipped game offers its players

- The window has a fixed size from `game.json`: no fullscreen or borderless mode, no resolution or
  render scale choice, vsync always on, a fixed frame-rate cap.
- No graphics quality presets a game can expose, and no general fallback for weaker graphics cards
  beyond TAA and `msaa` turning themselves off.
- Input actions are fixed (`jump`, `fire`, `run` and two axes): games cannot declare their own, and
  players cannot rebind keys. One gamepad, no rumble.
- `UI` has no sliders, checkboxes, key capture, word wrap, anchors, scroll lists or gamepad focus
  navigation (a gamepad's A picks the first button), so settings screens have to be built by hand.
  Android has no on-screen keyboard.

### Audio

- Sounds play once with a volume and a pitch: no positional (3D) sound or listener, no handle to
  stop or loop one sound, one music track at a time with no crossfade, no mixing groups.

### Text

- The built-in font covers Latin-1 only (code points 32 to 255): no "€", no Polish or Turkish
  letters, no Cyrillic, Greek or CJK. Games cannot load their own fonts, so most translations
  cannot be shown.

### Gameplay API

- Physics has no collision layers or masks, joints, shape casts or overlap queries, forces or
  torque, and mass or kinematic state cannot change while the game runs. `Physics.raycast` can only
  ignore one object.
- Animation clips have no events (footsteps, hit frames), no root motion and no IK.
- No navigation mesh or pathfinding.
- Cameras have no follow, orbit or shake helpers, and no `world_to_screen`.
- Particles come in bursts only: no emitter that stays attached to an object.
- `Save` is one key/value store per game: no slots, no saving of objects' state, no version number
  or migration hook.
- `Game.load_scene` loads in the middle of a frame: there is no loading screen or loading in the
  background. On a dedicated server, a room loading a level holds up the rooms sharing its thread.
- `Debug` writes to the log only: no on-screen lines, boxes or text, no frame-time overlay, no
  profiler.

### Safety

- A script that never returns (`loop {}`) freezes the game, the editor or a server room: there is
  no time or instruction limit. (A script that raises is switched off and the game goes on.)
- A crash in a built game closes it with a line in the log: no message to the player, no crash
  report, no backtrace. Running out of memory is not handled.
- Saves are written to a new file and renamed over the old one, but not flushed to disk first, and
  a damaged save is dropped silently with no backup kept.
- A missing texture draws nothing, with a warning in the log, instead of a placeholder pattern.

### Shipping

- A built game has no icon (Windows, macOS, Linux or Android), no splash picture and no version
  number outside Android.
- A built game is the player program beside a folder of loose files, scripts as plain `.rb`
  included: no packed or compressed archive.
- No installers: no Windows installer or code signing, no Linux AppImage or Flatpak, no `.app`
  bundle on macOS (and the editor's is not notarized).
- Android builds are APKs; Google Play needs an App Bundle. The network permission is always asked
  for, even by games that never go online.
- Saves live under RedGem's own folder, not under the game's or its studio's name.
- Except the Mac's, the downloads are packed by hand, not by the continuous integration.

### Engineering

- Rendering, physics and scripts run on one thread; physics uses a single-threaded job system.
- Textures are kept uncompressed on the graphics card (no KTX2/Basis, BCn or ASTC), which costs
  most on phones.
- The continuous integration has no sanitizers (ASan, UBSan, TSan), no fuzzing of the file and
  network parsers (they have hostile-input tests), no coverage report and no real graphics card.
  Compiler warnings do not fail the build. The load test and the benchmarks print numbers but fail
  on none.
- About forty unit and integration tests wait on the real clock, a source of rare timing failures.
- One fetched dependency (`nlohmann_json`) is downloaded without a checksum.

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

- No changelog or migration notes between versions; the release notes on GitHub are the record.
- `docs/scripting/api.md` is a hand-written summary, not a full reference generated from the code,
  and there are no type signatures or stubs for code editors.
- Templates are `empty` and `platformer` only; there is no example of running game tests in a
  game's own continuous integration.
