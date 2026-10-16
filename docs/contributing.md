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

Tests are run by a program of their own, `redgem-test`, built beside `redgem` (see
[Libraries and programs](#libraries-and-programs)). A system test is
a Ruby file run against a game:
`redgem-test <game_dir> <file.rb> [--screenshots <dir>] [--headless] [--work <dir>]` (`--headless`
runs without a window, `--work` gives the run a folder of its own for its save file and notes;
`redgem-test --help` lists every option).

The engine knows nothing of tests. Neither `redgem` nor `redgem-player`, which is what every
built game ships as, has test code in it: what tests are written with (`TestInput`, `SystemTest`,
`RedGemTest`, the runners and their command line) lives in `src/testing` and `ruby/testing`, a library that only
`redgem-test` links. The engine gives it nothing but general options (`EngineOptions`: a fixed
step per frame, an input fed by code, a save file, who shows the game) and the window a game is
shown in tells whoever asks when a frame is drawn; the pictures a test takes (`t.screenshot`) are
written by the test tool from there. Keep it that way: the `app.ships_no_test_code` test looks
into the built `redgem-player` and `redgem` and fails when test code got in.

* **C++ unit + integration** (doctest): math, extras parser, glTF loading from Blender fixtures,
  scene hierarchy, physics world, Ruby VM, and an engine nobody shows running the `sandbox` fixture
  game (and one shown by a presenter that records what it is handed).
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

A template is tested as the game it makes: `tests/system_templates/<template>/*_test.rb` run
against a game created from it by `redgem new`. Every template needs at least one.

Test fixtures are regenerated with
`tools/blender/run.sh build_test_fixtures.py tests/fixtures`.

## Layout

```
src/core      what everything shares: math, logging, the fixed timestep clock, files, the platform
              layer (the only place, with the sockets of src/net, that asks which system this is),
              the words of a command line, embedding files in the binary, the game folder's layout
              (config.hpp)
src/scene     glTF loader (meshes, skeletons, animations), Blender extras parser, scene graph,
              animation playback, particles; the custom properties scenes carry (props.hpp)
src/physics   PhysicsWorld interface + Jolt implementation
src/input     keys, axes/actions, keyboard, mouse, gamepad and scripted input sources; a touch
              screen's on-screen controls (touch_input_source.hpp), which only the program for
              a touch screen puts in front of the others
src/net       sockets (socket*.cpp and datagrams.cpp: the only place with OS socket calls, what the
              systems spell differently being in socket_native), message framing and wire format, the
              host/guest session and its connection approval, discovery on the local network,
              the relay and the entry point of its program (relay_main.cpp), and the games' web
              requests (http_*: HTTP/1.1 over Mbed TLS, one thread per request)
src/audio     sound effects and music (raylib, or silence when there is no audio device)
src/storage   what a game keeps between runs (the save store)
src/render    the window (window.hpp: the only place that opens one, with its graphics card, frame
              pace and pointer), the renderer that draws in it and owns what is on the card (HDR,
              shadows, reflections, post-processing), shaders, environment, culling, mesh
              conversion, what differs per graphics API (gpu_profile.hpp), the font texts are
              drawn with
src/script    mruby VM, behaviour host, native bindings
src/app       what plays a game: the engine loop, which draws nothing and hands each frame
              (FrameView) to a presenter (presenter.hpp); the presenter that shows a game in a
              window of its own and the painter every drawing presenter shares; game.json, the
              game beside a built executable, where saves go, user settings, the entry points of
              the player (player_main.cpp; on Android, android_main.cpp)
src/editor    editor: scene document (undo, save, import), session (play mode), camera, picking,
              script tabs, ImGui panels and theme (src/editor/ui)
src/project   the tools around a game: new games from templates, builds (desktop and Android),
              recent games and the project hub, the command line, the entry point of redgem
              (main.cpp)
src/testing   what runs tests, apart from the engine: TestInput, the pictures a test takes of the
              game (TestScreen), the system test and Ruby test runners, the audio tests listen
              with, redgem-test and its command line
ruby/engine   Ruby prelude embedded in the runtime (Behaviour, Vector3, UI, Network...)
ruby/testing  the Ruby tests are written with (assertions, test cases, the system test DSL),
              embedded in redgem-test only
templates     the new-game templates: each folder is a whole game (scene, scripts, sounds) and a
              template.json about it, embedded in redgem as it is; a new game is a copy of one.
              A template is edited like any game: redgem templates/platformer
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
`script`; then `app`; then `server`, `editor`, `project`; `testing` sees everything and nothing
sees it).
Each layer keeps its own numbers in a `<layer>_config.hpp` next to its code.

### Objects, and keeping hold of one

A scene object's `ObjectId` is a slot. The loaded scene's objects keep their slots (an id below
the node count is that node); the slot of an object made while the game runs (`Scene::clone`,
`Scene.spawn`) holds a new object once it is destroyed. Every object also gets an `ObjectSerial`
no other object ever had, in any scene.

- What keeps an object past the step it was handed it (a skin's bones, a vehicle's wheels, a
  character's own clip, a behaviour, a Ruby `GameObject` or `Transform`) keeps an `ObjectHandle`
  and asks `Scene::resolve` for it each time: that finds the object while it is alive, and
  nothing afterwards, whoever sits in its slot.
- Only a registry the engine updates in the very step an object is destroyed may key by bare id:
  the physics bodies (`Engine::process_destroyed` removes them) and the scene's own hierarchy and
  copies.
- Destruction is two steps: `request_destroy` marks an object and what is under it, its
  `on_destroy` runs with everything still there, then the batch is torn down children first.

## Libraries and programs

The folders are built into a few static libraries, one on top of the other, and each program
links only the ones it is made of (`src/CMakeLists.txt`):

```
redgem_core     src/core                                           links nothing of the engine's
redgem_net      src/net                                            redgem_core, the JSON library,
                                                                   Mbed TLS
redgem_runtime  scene, physics, audio, input, storage, render,     redgem_net, raylib, Jolt, mruby,
                script, app, and the Ruby prelude                  cgltf, the JSON library
redgem_game_server src/server (without server_main.cpp)            redgem_runtime
redgem_editor   src/editor                                         redgem_runtime, Dear ImGui
redgem_project  src/project (without main.cpp), the templates      redgem_editor
                and the licenses
redgem_testing  src/testing, ruby/testing                          redgem_runtime
```

```
redgem-player   src/app/player_main.cpp    redgem_runtime   what players receive: plays the game in
                                                            the game/ folder beside it, nothing else
redgem          src/project/main.cpp       redgem_project   the editor, the project hub, the command
                                                            line (new, build, --play)
redgem-relay    src/net/relay_main.cpp     redgem_net       the meeting point for games over the
                                                            internet: no window, for servers
redgem-server   src/server/server_main.cpp redgem_game_server
                                                            a game run on a server for players to
                                                            join, in rooms: no window, no display
redgem-test     src/testing/main.cpp       redgem_testing   runs a game's tests
libredgem.so    src/app/android_main.cpp   redgem_runtime   the player on Android, loaded by the app
```

A built game is a copy of the player, never of the program that builds it: `redgem build` and
the editor's Build take `redgem-player` from beside `redgem`, or `runtimes/<system>/` there for
another system (`tools/windows/build_runtime.sh`, `tools/android/build_runtime.sh` build those).
`cmake --install build --component redgem --prefix <folder>` puts the five programs in
`<folder>/bin` and the players for other systems in `<folder>/bin/runtimes`, where the same
lookups find them.

Keep what players receive small: nothing the player links may include `editor/` or `project/`
(the include directions below say so, and linking proves it). The tests `app.player_ships_no_tools`,
`app.relay_ships_no_game` and `app.server_ships_no_tools` look into the built programs (`tests/ships_no_tools.sh`) for texts
only the tools, or only a game, have.

A program opens one window, through `Window`, and makes what draws in it afterwards (a `Renderer`,
the editor's `ui::Interface`): the project hub, the editor and a played game each do. The engine
never does: it is given a `Presenter`. `WindowPresenter` shows a game in its own window
(`redgem-player`, `redgem --play`, `redgem-test`), the editor's Scene view is another (`EditorApp::GameView`),
and without one nobody shows the game (`NullPresenter`: tests, servers). What a game can show is
a field of `FrameView`, drawn in `FramePainter`: added there, it reaches every presenter.
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
