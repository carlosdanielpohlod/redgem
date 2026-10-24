# Changelog

What changed in each release of RedGem. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Every release so far is experimental:
until 1.0, a release may change what games rely on (see
[KNOWN_LIMITATIONS.md](KNOWN_LIMITATIONS.md)).

## [Unreleased]

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
- **Display settings for players**: windowed, fullscreen and borderless modes, a resolution from
  the monitor's sizes, a render scale (50% to 200%), vsync and a frame-rate cap, quality presets
  (low to ultra, and custom) that cap what a level's environment asks for, and a master volume.
  `game.json` gives the defaults; the `Settings` module reads and changes them, and the built
  game keeps them per game (the editor's Play, servers and tests keep nothing). The player's
  volume multiplies the game's own.
- **Settings screen and widgets**: `GameMenu` has a settings screen reached from its title and
  pause screens, built from the new `UI.toggle`, `UI.choice` and `UI.slider`. The keyboard and a
  gamepad move a focus between widgets and buttons (`UI::Focus`).
- **Game icon, version and splash picture**: `game.json` can give a game an `icon`, a `version`
  (read by scripts as `Game.version`) and a `splash` picture shown while it loads. Builds turn the
  icon into each system's own: inside the Windows `.exe`, a menu entry and icon on Linux, an
  `.icns` in the Mac app, launcher icons on Android.
- **Mac apps**: a build for the Mac is an `.app` with the game, its icon and its description
  inside, signed ad hoc.
- **Android App Bundles**: `redgem build ... --target android --bundle` makes an `.aab` for Google
  Play (with Google's bundletool).
- **`redgem --version`** says which version of RedGem it is.
- **CHANGELOG.md**: this file.

### Changed

- **An Android app asks for the network only when its game says `"network": true`** in its
  `game.json` (the platformer demo and template do).
- **Saves are flushed to the disk** before they replace the previous one, and the previous save is
  kept beside it as `<name>.bak`. A damaged save is moved aside to `<name>.damaged` (never written
  over), the backup is loaded and put back in its place, and the log names both files. A save that
  cannot be read is left alone, and the game runs without saving.
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

[Unreleased]: https://github.com/carlosdanielpohlod/redgem/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/carlosdanielpohlod/redgem/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/carlosdanielpohlod/redgem/releases/tag/v0.1.0
