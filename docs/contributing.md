# Working on the engine

For people changing RedGem itself. To make games with it, the rest of this manual is enough.

## Demos

Demos: `games/city` (a crossroad with Ruby-driven traffic: lights, braking, spinning wheels; Kenney
CC0 models), `games/platformer` (WASD/arrows, Space jumps: grab 5 coins, reach the flag) and
`games/driving` (W/S throttle, A/D steer, Space hand brake, R reset: drive laps).

**Showcase** (`games/showcase`, not committed: it is built from downloaded assets). A street at
dusk with a drivable car, traffic that keeps its distance and overtakes, parked cars, shop fronts
and cafe tables, a crowd on the sidewalks, potholes and falling leaves, made to push the renderer. Blender only builds the scene; everything
that happens in it is set up in `games/showcase/scripts/main_scene.rb`:

```bash
python3 -I tools/assets/fetch_showcase.py      # street: Poly Haven and ambientCG (CC0)
python3 -I tools/assets/fetch_ad_assets.py     # car and people: BlenderKit (see showcase/CREDITS.md)
SHOWCASE_RENDER=0 BLENDER_MEMORY_MAX=8500M tools/blender/run.sh build_showcase.py showcase
./build/src/redgem games/showcase           # then Play: WASD drives, Space brakes
```

The export needs about 7 GB of memory (close the editor first); without `SHOWCASE_RENDER=0` it
also renders `showcase/renders/ad.png` with Cycles, which needs an NVIDIA RTX card.
`games/city` is rebuilt
with `tools/assets/fetch_kenney.sh` and `tools/blender/run.sh build_city.py games/city`.

## Tests

```bash
cd build && ctest --output-on-failure        # everything
ctest -L system                              # only the system tests
```

Tests are run by a program of their own, `redgem-test`, built beside `redgem`. A system test is
a Ruby file run against a game:
`redgem-test <game_dir> <file.rb> [--screenshots <dir>] [--headless]` (`--headless` runs
without a window; `redgem-test --help` lists every option).

The engine knows nothing of tests. `redgem`, which is also what every built game ships as, has no
test code in it: what tests are written with (`TestInput`, `SystemTest`, `RedGemTest`, the runners
and their command line) lives in `src/testing` and `ruby/testing`, a library that only
`redgem-test` links. The engine gives it nothing but general options (`EngineOptions`: a fixed
step per frame, an input fed by code, a save file, no FPS counter...). Keep it that way: the
`app.ships_no_test_code` test looks into the built `redgem` and fails when test code got in.

* **C++ unit + integration** (doctest): math, extras parser, glTF loading from Blender fixtures,
  scene hierarchy, physics world, Ruby VM, and a headless engine running the `sandbox` fixture game.
* **Tests of the test tool** (`tests/testing/`, doctest): its command line, its save file, what it
  adds to a game's Ruby, and whole runs of passing and failing tests.
* **Ruby tests** run inside the engine's mruby: `redgem-test --ruby tests/ruby/vector3_test.rb`.
* **Editor system test** (`tests/editor_system/`) opens the editor window on a copy of the platformer,
  picks, edits, plays, stops, undoes and saves.
* **System tests** (`tests/system/<game>/*_test.rb`, run against `games/<game>`) open the real game
  window and drive it frame by frame with simulated input:

```ruby
SystemTest.define do |t|
  t.hold(:w, frames: 60)
  t.wait_until(frames: 30) { Scene.find("Coin1").nil? }
  t.assert("score counts the coin") { GameManager.instance.coins == 1 }
  t.screenshot("collect_coin_after")        # saved to build/screenshots/<game>/
end
```

Test fixtures are regenerated with
`tools/blender/run.sh build_test_fixtures.py tests/fixtures`.

## Layout

```
src/core      what everything shares: math, logging, the fixed timestep clock, files, the platform
              layer (the only place, with src/net/socket.cpp, that asks which system this is),
              embedding files in the binary, the game folder's layout (config.hpp)
src/scene     glTF loader (meshes, skeletons, animations), Blender extras parser, scene graph,
              animation playback, particles; the custom properties scenes carry (props.hpp)
src/physics   PhysicsWorld interface + Jolt implementation
src/input     keys, axes/actions, keyboard, mouse, gamepad and scripted input sources
src/net       sockets (the only place with OS socket calls), message framing and wire format, the
              host/guest session, discovery on the local network, the relay
src/audio     sound effects and music (raylib, or silence when there is no audio device)
src/storage   what a game keeps between runs (the save store)
src/render    renderer (HDR, shadows, reflections, post-processing), shaders, environment,
              culling, graphics card choice, mesh conversion, what differs per graphics API
              (gpu_profile.hpp), the font texts are drawn with
src/script    mruby VM, behaviour host, native bindings
src/app       what plays a game: the engine loop, game.json, the game beside a built executable,
              where saves go, user settings, the Android entry point
src/editor    editor: scene document (undo, save, import), session (play mode), camera, picking,
              script tabs, ImGui panels and theme (src/editor/ui)
src/project   the tools around a game: new games from templates, builds (desktop and Android),
              recent games and the project hub, the command line, the program's entry point
src/testing   what runs tests, apart from the engine: TestInput, the system test and Ruby test
              runners, the audio tests listen with, redgem-test and its command line
ruby/engine   Ruby prelude embedded in the binary (Behaviour, Vector3, UI, Network...)
ruby/testing  the Ruby tests are written with (assertions, test cases, the system test DSL),
              embedded in redgem-test only
templates     scripts of the new-game templates (embedded in the binary)
assets/editor fonts of the editor (Inter, JetBrains Mono; SIL Open Font License, see the license files)
games         demo games: scene.glb, scripts and the .blend each scene was built from
showcase      renders of the showcase scene and the credits of its assets
tools/blender scene builders (run with tools/blender/run.sh), walk_rig.py
tools/assets  downloaders of the free assets the demos use
tests         unit, integration, Ruby and system tests, with their fixtures
cmake         dependencies (pinned versions) and file embedding
```

The folders of `src` are layers, in the order above: each includes only itself and what is below
it (`core`; then `scene`, `net`, `input`, `audio`, `physics`, `storage`; then `render` and
`script`; then `app`; then `editor`, `project`; `testing` sees everything and nothing sees it).
Each layer keeps its own numbers in a `<layer>_config.hpp` next to its code.
`tests/include_directions.sh` holds the table and fails the test suite on an include that goes up.

## Not there yet

* Blend trees and animation events. Characters copied with Duplicate in the editor keep the pose
  they were copied in.
* Spot light cones, and camera clip planes from the scene (the engine uses fixed ones).
* Reusing data when the same model is imported twice, and dropping the data of deleted imports
  from `scene.glb` (each import appends its meshes and textures).
* Mirrored objects (negative scale) are drawn inside out.
* More vehicle options from Blender properties (drive layout, engine torque); hot reload of
  scripts while playing.
