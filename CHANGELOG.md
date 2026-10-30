# Changelog

What changed in each release of RedGem. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Every release so far is experimental:
until 1.0, a release may change what games rely on (see
[KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md)).

## [Unreleased]

### Added

- **Persistent object lookup**: `GameObject.stable_id` and `Scene.find_by_id` reconnect saved
  references to scene-file nodes after renames or reordering, without aliasing live copies.

- **Named save slots**: `Save.slot` selects isolated progress while the default keeps the existing
  save; `Save.slots` lists known names. Local slots reuse durable files and backups, server rooms
  select their own SQLite views, and game-test restarts copy/reset named slots too.

- **Immediate world diagnostics**: `Debug.line` and `Debug.box` draw bounded, frame-local wire
  primitives with scene depth or explicit x-ray, across player, editor Play and recordings.

- **Foreground frame profiler and statistics overlay**: opt-in real-clock phase timings, a bounded
  rolling summary and FPS panel shared by player, editor Play and recorded frames.

- **Asynchronous scene reads**: `Game.load_scene_async`, a shared bounded background parser,
  cancellation, last-error inspection and an immediate-mode loading-screen callback. The old
  world survives failed reads, while other rooms can keep ticking. Fixed-step offline runs have
  deterministic activation. Physics creation, script setup and GPU upload still happen on the
  owning thread and may hitch at activation.

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

[Unreleased]: https://github.com/carlosdanielpohlod/redgem/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/carlosdanielpohlod/redgem/releases/tag/v0.1.0
