---
description: "Working on the engine itself: building, testing, the rules of the code."
---

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
`redgem-test <game_dir> <file.rb> [--screenshots <dir>] [--headless] [--work <dir>] [--after <test.rb>]`
(`--headless` runs without a window, `--work` gives the run a folder of its own for its save file
and notes, `--after` goes on from an earlier test's run in that folder as if the game were started
again; `redgem-test --help` lists every option).

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

What a game keeps between runs is tested by starting it again:
`tests/system_restarts/<game>/<name>_restarted_test.rb` plays `games/<game>` after
`tests/system/<game>/<name>_test.rb` did, on copies of the save, display settings and bindings that
run left. The first run is a ctest fixture of the second (`redgem-test --work <dir>`, then
`--work <dir> --after <first_test.rb>`), so asking for the second runs the first once before it.

Test fixtures are regenerated with
`tools/blender/run.sh build_test_fixtures.py tests/fixtures`.

## Continuous integration

Every pull request runs these workflows (`.github/workflows/`), side by side:

* **Linux** (`linux.yml`): the build, every test (the system tests in windows drawn by Mesa's
  software renderer on a virtual display) and every linter.
* **Windows** (`windows.yml`): the build with llvm-mingw and the tests that open no window.
* **macOS** (`macos.yml`): the build and the tests that open no window on Apple Silicon and Intel,
  and the editor system test.
* **Android** (`android.yml`): the player built for arm64-v8a and x86_64 by
  `tools/android/build_runtime.sh`, with the NDK `tools/android/fetch_toolchain.sh` pins (kept
  between runs). Nothing runs on Android there: it catches code that only breaks that build.

Every fetched archive is pinned by its SHA-256 (`cmake/Dependencies.cmake`); a dependency fetched
from git is pinned by its tag or commit. `THIRD_PARTY_LICENSES.md` is generated: after adding or
updating a dependency, run `python3 -I tools/licenses/collect_notices.py` (after a build) and
commit what it writes. The test `tools.license_notices_are_up_to_date` fails when it is stale.

## Layout

```
src/core      what everything shares: math, logging, the fixed timestep clock, files, the platform
              layer (the only place, with the sockets of src/net, that asks which system this is),
              the words of a command line, embedding files in the binary, the game folder's layout
              (config.hpp)
src/scene     scene graph (objects, transforms, copies), particles, the custom properties scenes
              carry (props.hpp); in folders of their own: gltf/ (the loader: meshes, skeletons,
              animations, Blender extras), animation/ (clips and their playback), shapes/
              (primitives and terrain)
src/physics   PhysicsWorld interface + Jolt implementation
src/input     keys, axes/actions, keyboard, mouse, gamepad and scripted input sources; a touch
              screen's on-screen controls (touch_input_source.hpp), which only the program for
              a touch screen puts in front of the others
src/net       message framing and wire format, the host/guest session and its connection approval;
              socket/ (the only place with OS socket calls, what the systems spell differently being
              in socket_native), discovery/ (games on the local network), rooms/ (the relay and the
              entry point of its program, relay_main.cpp, and what a dedicated server's rooms share
              with it: codes, the lobby, guessers kept out), http/ (the games' web requests:
              HTTP/1.1 over Mbed TLS, one thread per request)
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
              the player (player_main.cpp; on Android, android/android_main.cpp)
src/editor    editor: session (play mode), camera, picking, the app and its panels; document/ (the
              scene document: undo, save, the glb it edits, importing models), scripts/ (the script
              tabs), ui/ (ImGui panels and theme)
src/project   the tools around a game: new games from templates, builds (desktop, and Android in
              android/), recent games and the project hub, the command line, the entry point of
              redgem (main.cpp)
src/testing   what runs tests, apart from the engine: TestInput, the pictures a test takes of the
              game (TestScreen), the system test and Ruby test runners, the audio tests listen
              with, redgem-test and its command line
ruby/engine   Ruby prelude embedded in the runtime (Behaviour, Vector3, UI, Network...)
ruby/testing  the Ruby tests are written with (assertions, test cases, the system test DSL),
              embedded in redgem-test only
templates     the new-game templates: each folder is a whole game (scene, scripts, sounds) and a
              template.json about it, embedded in redgem as it is; a new game is a copy of one.
              A template is edited like any game: redgem templates/platformer
locales      the texts users read, one translation file per language (see Texts users read)
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
redgem-relay    src/net/rooms/relay_main.cpp     redgem_net       the meeting point for games over the
                                                            internet: no window, for servers
redgem-server   src/server/server_main.cpp redgem_game_server
                                                            a game run on a server for players to
                                                            join, in rooms: no window, no display
redgem-test     src/testing/main.cpp       redgem_testing   runs a game's tests
libredgem.so    src/app/android/android_main.cpp   redgem_runtime   the player on Android, loaded by the app
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

## Texts users read

What a user of RedGem reads is not written in the code: it is in translation files, one per
language, under `locales/<part>/<language>.json`, and the code names it by a key
(`tr("editor.menu.file")`, see `src/core/translations.hpp`). Today there is English (`en.json`)
alone; a translation is another file beside it, with the same keys.

- **Into a translation file**: everything a user reads in a window or at a terminal. The
  editor and the project hub (menus, buttons, panels, tooltips, dialogs, hints, the status bar,
  the inspector's labels), the help, usage and errors of the command lines of `redgem`,
  `redgem-server`, `redgem-relay` and `redgem-test` that are about what the user typed, and the
  texts the engine shows players (the labels of `GameMenu`).
- **In English in the code**: the log, and the messages of errors that are for whoever works on
  the engine or a game (a broken file, a failed call). Those are searched for, quoted in bug
  reports and read next to the code; they stay in one language, as the code does.

Each part is the texts of one library, built into it (`redgem_embed_texts` in
`src/CMakeLists.txt`), so that a program carries only what it shows: `core` (what a command
line says of an option it cannot read), `game` (the runtime: what players read), `editor`, `project` (the hub and `redgem`'s command line), `server`, `relay` and
`testing` (the command lines of the other programs). A program adds its parts and picks the
language as it starts: the one in the user's settings (`"language"` in `settings.json`), else the
system's; a key a translation lacks is shown in English.

A key is the part's name, then where the text is, then what it is: `editor.menu.file`. The
first word says which file it is in. A text with values in it names them, `Created {game}`, and
the code fills them in: `tr("project.cli.created", {{"game", name}})`. A long text (a command's
usage) may be a list of lines. `tests/unit/translation_keys_test.cpp` fails when the code shows a
key English has no text for, when an English text is shown nowhere, or when a translation has a
key English lacks; keys are written whole in the code, as string literals, for it to find them.

A game's texts are not the engine's: they are in the game's own `locales/<language>.json`, and its
scripts show them with `I18n.t("hud.coins")` (see [Texts in several languages](scripting/i18n.md)),
behind which are the engine's `game` part (the prelude's `I18n.t` calls, in `ruby/engine/`), so a
game's files may translate those too. The same test checks the games and templates of this
repository: every key their scripts show has a text in their `locales/en.json` or the `game` part,
and every text of theirs is shown.

## Not there yet

* Blend trees and animation events. Characters copied with Duplicate in the editor keep the pose
  they were copied in.
* Spot light cones, and camera clip planes from the scene (the engine uses fixed ones).
* Reusing data when the same model is imported twice, and dropping the data of deleted imports
  from `scene.glb` (each import appends its meshes and textures).
* Mirrored objects (negative scale) are drawn inside out.
* More vehicle options from Blender properties (drive layout, engine torque); hot reload of
  scripts while playing.
