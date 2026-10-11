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
SHOWCASE_RENDER=0 BLENDER_MEMORY_MAX=6500M tools/blender/run.sh build_showcase.py showcase
./build/src/redgem games/showcase           # then Play: WASD drives, Space brakes
```

The export needs about 4.5 GB of memory (close the editor first); without `SHOWCASE_RENDER=0` it
also renders `showcase/renders/ad.png` with Cycles, which needs an NVIDIA RTX card.
`games/city` is rebuilt
with `tools/assets/fetch_kenney.sh` and `tools/blender/run.sh build_city.py games/city`.

## Tests

```bash
cd build && ctest --output-on-failure        # everything
ctest -L system                              # only the system tests
```

A system test is a Ruby file run against a game:
`redgem <game_dir> --test <file.rb> [--screenshots <dir>] [--headless]` (`--headless` runs
without a window; `redgem --help` lists every option).

* **C++ unit + integration** (doctest): math, extras parser, glTF loading from Blender fixtures,
  scene hierarchy, physics world, Ruby VM, and a headless engine running the `sandbox` fixture game.
* **Ruby tests** run inside the engine's mruby: `redgem --ruby-test tests/ruby/vector3_test.rb`.
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
src/core      constants (config.hpp), math, logging, fixed timestep clock
src/scene     glTF loader (meshes, skeletons, animations), Blender extras parser, scene graph,
              animation playback
src/physics   PhysicsWorld interface + Jolt implementation
src/input     keys, axes/actions, keyboard and scripted input sources
src/audio     sound effects and music (raylib, or a recorder for tests)
src/script    mruby VM, behaviour host, native bindings
src/render    renderer (HDR, shadows, reflections, post-processing), shaders, environment,
              culling, graphics card choice, mesh conversion
src/editor    editor: scene document (undo, save, import), session (play mode), camera, picking,
              project hub, script tabs, ImGui panels and theme (src/editor/ui)
src/app       engine loop, game projects (templates, build, recent games), user settings and
              command line
ruby/engine   Ruby prelude embedded in the binary (Behaviour, Vector3, test DSLs)
templates     scripts of the new-game templates (embedded in the binary)
assets/editor fonts of the editor (Inter, JetBrains Mono; SIL Open Font License, see the license files)
games         demo games: scene.glb, scripts and the .blend each scene was built from
showcase      renders of the showcase scene and the credits of its assets
tools/blender scene builders (run with tools/blender/run.sh), walk_rig.py
tools/assets  downloaders of the free assets the demos use
tests         unit, integration, Ruby and system tests, with their fixtures
cmake         dependencies (pinned versions) and file embedding
```

## Not there yet

* Choosing which animation plays: every animation in a scene loops at once (fine for one clip per
  character). Characters copied with Duplicate in the editor keep the pose they were copied in.
* Spot light cones, and camera clip planes from the scene (the engine uses fixed ones).
* Reusing data when the same model is imported twice, and dropping the data of deleted imports
  from `scene.glb` (each import appends its meshes and textures).
* Mirrored objects (negative scale) are drawn inside out.
* More vehicle options from Blender properties (drive layout, engine torque); hot reload of
  scripts while playing.
