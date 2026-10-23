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
  buoyancy, breaking waves or planar reflections. Shore foam estimates horizontal distance
  to the intersection of visible bottom and water tangent planes, with filtered pores and partial transmission.
  It is a local screen-space approximation, not a global shoreline-distance field. Missing geometry,
  depth discontinuities and nearly parallel planes suppress coverage; curved shores can deviate
  from the nearest-shore distance. Its procedural pore pattern is stationary.
  It does not accumulate, advect or follow breaking crests.
  Refraction sees opaque geometry on screen
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
